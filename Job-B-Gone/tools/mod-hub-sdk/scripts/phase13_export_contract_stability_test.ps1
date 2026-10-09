param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [string]$KenshiPath = ""
)

$ErrorActionPreference = "Stop"

function Assert-Condition {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Invoke-KenshiAliasLogProbe {
    param(
        [Parameter(Mandatory = $true)][string]$DllPath,
        [Parameter(Mandatory = $false)][string]$KenshiPath
    )

    $runRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("emc_phase13_alias_log_" + [Guid]::NewGuid().ToString("N"))
    $probeExePath = Join-Path $runRoot "kenshi_x64.exe"
    $logPath = Join-Path $runRoot "RE_Kenshi_log.txt"
    $probeTypeName = "Phase13KenshiAliasLogProbe_" + [Guid]::NewGuid().ToString("N")

    $probeSource = @"
using System;
using System.Runtime.InteropServices;

public sealed class __PROBE_TYPE_NAME__
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int GetApiRaw(uint requestedVersion, uint callerApiSize, IntPtr outApi, IntPtr outApiSize);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern bool SetDllDirectory(string lpPathName);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr LoadLibrary(string lpFileName);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr GetProcAddress(IntPtr hModule, string lpProcName);

    [DllImport("kernel32", SetLastError = true)]
    private static extern bool FreeLibrary(IntPtr hModule);

    private const int EMC_OK = 0;
    private const uint HUB_API_V1 = 1u;
    private const uint HUB_API_V1_SIZE = 56u;

    public static int Main(string[] args)
    {
        IntPtr module = IntPtr.Zero;
        IntPtr outApi = IntPtr.Zero;
        IntPtr outApiSize = IntPtr.Zero;

        try
        {
            if (args.Length < 1 || args.Length > 2)
            {
                Console.Error.WriteLine("usage: kenshi_x64.exe <dllPath> [kenshiPath]");
                return 2;
            }

            string dllPath = args[0];
            string kenshiPath = args.Length == 2 ? args[1] : string.Empty;
            if (!string.IsNullOrEmpty(kenshiPath))
            {
                SetDllDirectory(kenshiPath);
            }

            module = LoadLibrary(dllPath);
            if (module == IntPtr.Zero)
            {
                Console.Error.WriteLine("LoadLibrary failed for " + dllPath);
                return 3;
            }

            IntPtr proc = GetProcAddress(module, "EMC_ModHub_GetApi_v1_compat");
            if (proc == IntPtr.Zero)
            {
                Console.Error.WriteLine("Missing export: EMC_ModHub_GetApi_v1_compat");
                return 4;
            }

            GetApiRaw getApiAlias = (GetApiRaw)Marshal.GetDelegateForFunctionPointer(proc, typeof(GetApiRaw));
            outApi = Marshal.AllocHGlobal(IntPtr.Size);
            outApiSize = Marshal.AllocHGlobal(4);

            for (int i = 0; i < 2; ++i)
            {
                Marshal.WriteIntPtr(outApi, IntPtr.Zero);
                Marshal.WriteInt32(outApiSize, 0);

                int r = getApiAlias(HUB_API_V1, HUB_API_V1_SIZE, outApi, outApiSize);
                if (r != EMC_OK)
                {
                    Console.Error.WriteLine("Alias export returned " + r);
                    return 5;
                }

                if (Marshal.ReadIntPtr(outApi) == IntPtr.Zero)
                {
                    Console.Error.WriteLine("Alias export did not populate out_api");
                    return 6;
                }
            }

            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex.ToString());
            return 10;
        }
        finally
        {
            if (outApi != IntPtr.Zero)
            {
                Marshal.FreeHGlobal(outApi);
            }

            if (outApiSize != IntPtr.Zero)
            {
                Marshal.FreeHGlobal(outApiSize);
            }

            if (module != IntPtr.Zero)
            {
                FreeLibrary(module);
            }
        }
    }
}
"@.Replace("__PROBE_TYPE_NAME__", $probeTypeName)

    New-Item -ItemType Directory -Path $runRoot -Force | Out-Null

    try {
        Add-Type -TypeDefinition $probeSource -Language CSharp -OutputAssembly $probeExePath -OutputType ConsoleApplication | Out-Null

        $probeArgs = @($DllPath)
        if ($KenshiPath) {
            $probeArgs += $KenshiPath
        }

        Push-Location $runRoot
        try {
            $probeOutput = & $probeExePath @probeArgs 2>&1
            $probeExitCode = $LASTEXITCODE
        }
        finally {
            Pop-Location
        }

        $probeOutputText = if ($probeOutput) { ($probeOutput | Out-String).Trim() } else { "" }
        Assert-Condition -Condition ($probeExitCode -eq 0) -Message ("kenshi_x64.exe alias probe failed with exit code {0}. {1}" -f $probeExitCode, $probeOutputText)
        Assert-Condition -Condition (Test-Path $logPath) -Message "Expected alias probe to create RE_Kenshi_log.txt."

        $logLines = @(Get-Content -Path $logPath)
        $aliasEvents = @($logLines | Where-Object { $_ -like "*event=hub_get_api_alias_deprecated*" })
        Assert-Condition -Condition ($aliasEvents.Count -eq 1) -Message ("Expected exactly one alias deprecation event, found {0}. Log:`n{1}" -f $aliasEvents.Count, (($logLines | Out-String).Trim()))

        $aliasEvent = [string]$aliasEvents[0]
        Assert-Condition -Condition ($aliasEvent.Contains("alias=EMC_ModHub_GetApi_v1_compat")) -Message "Alias deprecation event missing alias symbol."
        Assert-Condition -Condition ($aliasEvent.Contains("canonical=EMC_ModHub_GetApi")) -Message "Alias deprecation event missing canonical symbol."
        Assert-Condition -Condition ($aliasEvent.Contains("removal_target=v1.2.0")) -Message "Alias deprecation event missing removal target."
    }
    finally {
        if (Test-Path $runRoot) {
            Remove-Item -Path $runRoot -Recurse -Force
        }
    }
}

$DllPath = (Resolve-Path -Path $DllPath).ProviderPath
if ($KenshiPath) {
    $KenshiPath = (Resolve-Path -Path $KenshiPath).ProviderPath
}

$code = @"
using System;
using System.Runtime.InteropServices;

public static class Phase13ExportContractHarness
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetApiRaw(uint requestedVersion, uint callerApiSize, IntPtr outApi, IntPtr outApiSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void VoidRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void SetIntRaw(int value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetIntRaw();

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr LoadLibrary(string lpFileName);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern bool SetDllDirectory(string lpPathName);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr LoadLibraryEx(string lpFileName, IntPtr hFile, uint dwFlags);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr GetProcAddress(IntPtr hModule, string lpProcName);

    [DllImport("kernel32", SetLastError = true)]
    private static extern bool FreeLibrary(IntPtr hModule);

    private const int EMC_OK = 0;
    private const int EMC_ERR_NOT_FOUND = 5;
    private const uint DONT_RESOLVE_DLL_REFERENCES = 0x00000001u;
    private const uint HUB_API_V1 = 1u;
    private const uint HUB_API_V1_SIZE = 56u;

    private const int LOOKUP_MODE_AUTO = 0;
    private const int LOOKUP_MODE_ALIAS_ONLY = 1;
    private const int LOOKUP_MODE_MISSING = 2;

    private static void Assert(bool condition, string message)
    {
        if (!condition)
        {
            throw new Exception(message);
        }
    }

    private static T Bind<T>(IntPtr module, string exportName)
    {
        IntPtr proc = GetProcAddress(module, exportName);
        Assert(proc != IntPtr.Zero, "Missing export: " + exportName);
        return (T)(object)Marshal.GetDelegateForFunctionPointer(proc, typeof(T));
    }

    private static void ExpectGetApiSuccess(GetApiRaw fn, string context)
    {
        IntPtr outApi = Marshal.AllocHGlobal(IntPtr.Size);
        IntPtr outApiSize = Marshal.AllocHGlobal(4);
        try
        {
            Marshal.WriteIntPtr(outApi, new IntPtr(0x1111));
            Marshal.WriteInt32(outApiSize, unchecked((int)0x22222222));

            int r = fn(HUB_API_V1, HUB_API_V1_SIZE, outApi, outApiSize);
            Assert(r == EMC_OK, context + " expected EMC_OK");

            IntPtr apiPtr = Marshal.ReadIntPtr(outApi);
            int apiSize = Marshal.ReadInt32(outApiSize);
            Assert(apiPtr != IntPtr.Zero, context + " api pointer is null");
            Assert(apiSize >= (int)HUB_API_V1_SIZE, context + " api size too small");
        }
        finally
        {
            Marshal.FreeHGlobal(outApi);
            Marshal.FreeHGlobal(outApiSize);
        }
    }

    private static void ExpectGetApiNotFound(GetApiRaw fn, string context)
    {
        IntPtr outApi = Marshal.AllocHGlobal(IntPtr.Size);
        IntPtr outApiSize = Marshal.AllocHGlobal(4);
        try
        {
            Marshal.WriteIntPtr(outApi, new IntPtr(0x3333));
            Marshal.WriteInt32(outApiSize, unchecked((int)0x44444444));

            int r = fn(HUB_API_V1, HUB_API_V1_SIZE, outApi, outApiSize);
            Assert(r == EMC_ERR_NOT_FOUND, context + " expected EMC_ERR_NOT_FOUND");
            Assert(Marshal.ReadIntPtr(outApi) == IntPtr.Zero, context + " out_api not cleared on not-found");
            Assert(Marshal.ReadInt32(outApiSize) == 0, context + " out_api_size not cleared on not-found");
        }
        finally
        {
            Marshal.FreeHGlobal(outApi);
            Marshal.FreeHGlobal(outApiSize);
        }
    }

    public static string Run(string dllPath, string kenshiPath)
    {
        IntPtr module = IntPtr.Zero;
        try
        {
            if (!string.IsNullOrEmpty(kenshiPath))
            {
                SetDllDirectory(kenshiPath);
            }

            module = LoadLibrary(dllPath);
            if (module == IntPtr.Zero)
            {
                throw new Exception("LoadLibrary failed for " + dllPath + ". Provide -KenshiPath when Kenshi runtime dependencies are outside PATH.");
            }

            GetApiRaw getApiCanonical = Bind<GetApiRaw>(module, "EMC_ModHub_GetApi");
            GetApiRaw getApiAlias = Bind<GetApiRaw>(module, "EMC_ModHub_GetApi_v1_compat");

            VoidRaw resetAliasWarningCount = Bind<VoidRaw>(module, "EMC_ModHub_Test_ResetGetApiAliasWarningCount");
            GetIntRaw getAliasWarningCount = Bind<GetIntRaw>(module, "EMC_ModHub_Test_GetApiAliasWarningCount");

            SetIntRaw setDefaultLookupMode = Bind<SetIntRaw>(module, "EMC_ModHub_Test_Client_DefaultLookup_SetMode");
            VoidRaw resetDefaultLookup = Bind<VoidRaw>(module, "EMC_ModHub_Test_Client_DefaultLookup_Reset");
            GetApiRaw callDefaultLookupGetApi = Bind<GetApiRaw>(module, "EMC_ModHub_Test_Client_DefaultLookup_CallGetApi");

            // Canonical export path.
            ExpectGetApiSuccess(getApiCanonical, "canonical_export");

            // Alias export path + one-time deprecation warning event.
            resetAliasWarningCount();
            ExpectGetApiSuccess(getApiAlias, "alias_export_first");
            ExpectGetApiSuccess(getApiAlias, "alias_export_second");
            Assert(getAliasWarningCount() == 1, "alias export should emit warning once");

            // Helper default lookup: canonical first (no alias warning expected).
            resetAliasWarningCount();
            resetDefaultLookup();
            setDefaultLookupMode(LOOKUP_MODE_AUTO);
            ExpectGetApiSuccess(callDefaultLookupGetApi, "helper_default_canonical");
            Assert(getAliasWarningCount() == 0, "canonical-first helper path should not use alias");

            // Helper default lookup: alias fallback succeeds when canonical is skipped.
            resetAliasWarningCount();
            setDefaultLookupMode(LOOKUP_MODE_ALIAS_ONLY);
            ExpectGetApiSuccess(callDefaultLookupGetApi, "helper_alias_fallback");
            Assert(getAliasWarningCount() == 1, "alias fallback should log warning once");

            // Helper default lookup: deterministic missing-symbol fallback.
            setDefaultLookupMode(LOOKUP_MODE_MISSING);
            ExpectGetApiNotFound(callDefaultLookupGetApi, "helper_missing_symbol");

            // Restore default mode for future runs.
            resetDefaultLookup();

            return "PASS";
        }
        finally
        {
            if (module != IntPtr.Zero)
            {
                FreeLibrary(module);
            }
        }
    }
}
"@

Add-Type -TypeDefinition $code -Language CSharp
$result = [Phase13ExportContractHarness]::Run($DllPath, $KenshiPath)
Assert-Condition -Condition ($result -eq "PASS") -Message "Phase 13 in-process export harness failed."
Invoke-KenshiAliasLogProbe -DllPath $DllPath -KenshiPath $KenshiPath
Write-Host "PASS"
