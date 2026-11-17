param(
    [Parameter(Mandatory=$true)][string]$RunDirectory,
    [ValidateRange(30,600)][int]$MaxSeconds=360,
    [ValidateRange(1,32)][int]$MaxTempGiB=20,
    [string]$TempDirectory=''
)
$ErrorActionPreference='Stop'
$principal=New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if(-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Administrator token required; no recording started.'}
$runDir=[IO.Path]::GetFullPath($RunDirectory)
if(-not (Test-Path -LiteralPath $runDir -PathType Container)){throw 'Root must create the unique run folder first.'}
$profile=Join-Path $PSScriptRoot 'KeoSaveLoad.wprp'
$wpr=Join-Path $env:WINDIR 'System32\wpr.exe'
$instance='KEO-load-'+[Guid]::NewGuid().ToString('N')
if(-not $TempDirectory){$TempDirectory=Join-Path $runDir 'temporary'}
$TempDirectory=[IO.Path]::GetFullPath($TempDirectory)
New-Item -ItemType Directory -Path $TempDirectory -ErrorAction Stop | Out-Null
$started=$false;$saved=$false;$beginSeen=$false
$record=[ordered]@{instance=$instance;phase='starting';loggingMode='File';maxSeconds=$MaxSeconds;pid=$null;gameStartedUtc=$null;startedUtc=$null;startedQpc=$null;qpcFrequency=[Diagnostics.Stopwatch]::Frequency;stoppedUtc=$null;saved=$false;stopReason=$null;error=$null;recoveryError=$null;activeSessionNeedsRecovery=$false;tempDirectory=$TempDirectory;boundaries=@()}
function Store([string]$Name){$record|ConvertTo-Json -Depth 7|Set-Content -LiteralPath (Join-Path $runDir $Name) -Encoding UTF8}
function RevokeReady {
    try{@{ready=$false;instance=$instance;phase=$record.phase;utc=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $runDir 'ready.json') -Encoding UTF8}catch{$record.recoveryError+=' Readiness write failed: '+$_.Exception.Message}
}
function FreeBytes([string]$Path){$taskDrive=New-Object IO.DriveInfo([IO.Path]::GetPathRoot($Path));return $taskDrive.AvailableFreeSpace}
try{
    # Allow for the live trace and an uncompressed recovery merge on each volume.
    if((FreeBytes $TempDirectory) -lt (2*$MaxTempGiB+8)*1GB -or (FreeBytes $runDir) -lt (2*$MaxTempGiB+8)*1GB){throw 'Insufficient free space for bounded recording plus recovery merge.'}
    $games=@(Get-Process -Name 'Kenshi*' -ErrorAction SilentlyContinue)
    if($games.Count -ne 1){throw 'Expected exactly one running Kenshi at the menu.'}
    $game=$games[0];$record.pid=$game.Id;$record.gameStartedUtc=$game.StartTime.ToUniversalTime().ToString('o')
    Store 'recording.json'
    & $wpr -start ($profile+'!KeoSaveLoad') -filemode -recordtempto $TempDirectory -instancename $instance *> (Join-Path $runDir 'start.txt')
    if($LASTEXITCODE -ne 0){throw ('WPR start failed: '+$LASTEXITCODE)}
    $started=$true;$record.activeSessionNeedsRecovery=$true;$record.startedUtc=[DateTime]::UtcNow.ToString('o');$record.startedQpc=[Diagnostics.Stopwatch]::GetTimestamp()
    & $wpr -status profiles -instancename $instance *> (Join-Path $runDir 'profiles.txt')
    if($LASTEXITCODE -ne 0 -or -not ((Get-Content -LiteralPath (Join-Path $runDir 'profiles.txt') -Raw) -match 'KeoSaveLoad.Verbose.File')){throw 'WPR did not confirm sequential File profile; do not start loading.'}
    $record.phase='waiting_for_load';Store 'recording.json'
    @{ready=$true;pid=$record.pid;utc=[DateTime]::UtcNow.ToString('o');instance=$instance;mode='File'}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $runDir 'ready.json') -Encoding UTF8
    $timer=[Diagnostics.Stopwatch]::StartNew();$loadStartedSeconds=0.0
    while($true){
        if(-not $beginSeen -and (Test-Path -LiteralPath (Join-Path $runDir 'begin.request'))){$beginSeen=$true;$loadStartedSeconds=$timer.Elapsed.TotalSeconds;$record.phase='recording_load';$record.boundaries+=@{name='owner_told_to_load';utc=[DateTime]::UtcNow.ToString('o');qpc=[Diagnostics.Stopwatch]::GetTimestamp()};Store 'recording.json'}
        if(-not $beginSeen -and $timer.Elapsed.TotalSeconds -ge 120){$record.stopReason='load_handshake_timeout';break}
        if($beginSeen -and $timer.Elapsed.TotalSeconds-$loadStartedSeconds -ge $MaxSeconds){$record.stopReason='maximum_duration';break}
        if(Test-Path -LiteralPath (Join-Path $runDir 'stop.request')){$record.stopReason='owner_npcs_ready';break}
        $liveGame=Get-Process -Id $record.pid -ErrorAction SilentlyContinue
        if(-not $liveGame -or $liveGame.StartTime.ToUniversalTime().ToString('o') -ne $record.gameStartedUtc){$record.stopReason='game_process_exited';break}
        $bytes=(Get-ChildItem -LiteralPath $TempDirectory -File -Recurse -ErrorAction SilentlyContinue|Measure-Object Length -Sum).Sum
        if($bytes -gt $MaxTempGiB*1GB){$record.stopReason='temporary_file_size_limit';break}
        if((FreeBytes $TempDirectory) -lt ($MaxTempGiB+8)*1GB -or (FreeBytes $runDir) -lt ($MaxTempGiB+8)*1GB){$record.stopReason='disk_free_space_limit';break}
        Start-Sleep -Milliseconds 500
    }
    $record.phase='saving';RevokeReady;Store 'recording.json'
    $record.boundaries+=@{name=$record.stopReason;utc=[DateTime]::UtcNow.ToString('o');qpc=[Diagnostics.Stopwatch]::GetTimestamp()}
    & $wpr -status collectors -instancename $instance *> (Join-Path $runDir 'collectors-before-stop.txt')
    & $wpr -stop (Join-Path $runDir 'trace.etl') 'KEO save-load through NPC readiness; sequential complete recording' -skipPdbGen -compress -instancename $instance *> (Join-Path $runDir 'stop.txt')
    if($LASTEXITCODE -ne 0){throw ('WPR save failed: '+$LASTEXITCODE)}
    $saved=$true;$record.activeSessionNeedsRecovery=$false
}catch{
    $record.error=$_.Exception.Message;$record.phase='recovering';RevokeReady
    if($started -and $beginSeen -and -not $saved){
        try{
            & $wpr -stop (Join-Path $runDir 'recovery.etl') 'KEO recording recovery after capture error' -skipPdbGen -instancename $instance *> (Join-Path $runDir 'recovery-stop.txt')
            if($LASTEXITCODE -eq 0){$saved=$true;$record.activeSessionNeedsRecovery=$false}else{$record.recoveryError+=' Recovery save failed: '+$LASTEXITCODE}
        }catch{$record.recoveryError+=' Recovery invocation failed: '+$_.Exception.Message}
    }
}
finally{
    # Never discard an already requested load trace on a failed save. Root must
    # recover/stop the named session if activeSessionNeedsRecovery is true.
    if($started -and -not $saved -and -not $beginSeen){
        try{& $wpr -cancel -instancename $instance *> (Join-Path $runDir 'cancel-own-session.txt');if($LASTEXITCODE -eq 0){$record.activeSessionNeedsRecovery=$false}}catch{$record.recoveryError+=' Cancel invocation failed: '+$_.Exception.Message}
    }
    $record.phase=if($saved){'saved'}elseif($record.activeSessionNeedsRecovery){'recovery_needed'}else{'failed'}
    RevokeReady;$record.saved=$saved;$record.stoppedUtc=[DateTime]::UtcNow.ToString('o')
    try{Store 'result.json'}catch{[Console]::Error.WriteLine('Capture metadata write failed; inspect the named WPR instance: '+$instance)}
}
if(-not $saved){exit 1}
