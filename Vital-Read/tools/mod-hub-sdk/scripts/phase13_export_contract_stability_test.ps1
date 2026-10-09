param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [string]$KenshiPath = ""
)

$ErrorActionPreference = "Stop"

function Get-DllExportSet {
    param(
        [Parameter(Mandatory = $true)][string]$Path
    )

    $exports = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::Ordinal)

    if (Get-Command objdump -ErrorAction SilentlyContinue) {
        $rawLines = & objdump -p $Path 2>$null | Select-String -Pattern 'EMC_ModHub_'
    } elseif (Get-Command llvm-nm -ErrorAction SilentlyContinue) {
        $rawLines = & llvm-nm -D --defined-only $Path 2>$null | Select-String -Pattern 'EMC_ModHub_'
    } elseif (Get-Command nm -ErrorAction SilentlyContinue) {
        $rawLines = & nm -D --defined-only $Path 2>$null | Select-String -Pattern 'EMC_ModHub_'
    } else {
        throw 'Unable to inspect DLL exports: install objdump, llvm-nm, or nm in PATH.'
    }

    foreach ($line in $rawLines) {
        if ($line -match '(EMC_ModHub_[A-Za-z0-9_]+)') {
            $null = $exports.Add($Matches[1])
        }
    }

    return $exports
}

function Assert-HasAllDebugHelpers {
    param(
        [Parameter(Mandatory = $true)]$ExportSet
    )

    $helperNames = @(
        'EMC_ModHub_Test_ResetGetApiAliasWarningCount',
        'EMC_ModHub_Test_GetApiAliasWarningCount',
        'EMC_ModHub_Test_Client_DefaultLookup_SetMode',
        'EMC_ModHub_Test_Client_DefaultLookup_Reset',
        'EMC_ModHub_Test_Client_DefaultLookup_CallGetApi'
    )

    $present = 0
    foreach ($helper in $helperNames) {
        if ($ExportSet.Contains($helper)) {
            $present += 1
        }
    }

    if (($present -gt 0) -and ($present -ne $helperNames.Count)) {
        throw "Expected phase 13 helper exports to be all-or-nothing. Found $present of $($helperNames.Count)."
    }
}

function Assert-Condition {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

$DllPath = (Resolve-Path -Path $DllPath).ProviderPath
if ($KenshiPath) {
    $KenshiPath = (Resolve-Path -Path $KenshiPath).ProviderPath
}

if (-not $IsWindows) {
    $DllPath = (Resolve-Path -Path $DllPath).ProviderPath
    $exports = Get-DllExportSet -Path $DllPath

    $required = @(
        'EMC_ModHub_GetApi',
        'EMC_ModHub_GetApi_v1_compat'
    )
    foreach ($name in $required) {
        if (-not $exports.Contains($name)) {
            throw "Missing required export in WSL smoke check: $name"
        }
    }

    Assert-HasAllDebugHelpers -ExportSet $exports
    Write-Host 'PASS'
    return
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

    private static IntPtr FindExport(IntPtr module, string exportName)
    {
        return GetProcAddress(module, exportName);
    }

    private static T Bind<T>(IntPtr module, string exportName)
    {
        IntPtr proc = FindExport(module, exportName);
        Assert(proc != IntPtr.Zero, "Missing export: " + exportName);
        return (T)(object)Marshal.GetDelegateForFunctionPointer(proc, typeof(T));
    }

    private static T TryBind<T>(IntPtr module, string exportName) where T : class
    {
        IntPtr proc = FindExport(module, exportName);
        if (proc == IntPtr.Zero)
        {
            return null;
        }

        return Marshal.GetDelegateForFunctionPointer(proc, typeof(T)) as T;
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

            VoidRaw resetAliasWarningCount = TryBind<VoidRaw>(module, "EMC_ModHub_Test_ResetGetApiAliasWarningCount");
            GetIntRaw getAliasWarningCount = TryBind<GetIntRaw>(module, "EMC_ModHub_Test_GetApiAliasWarningCount");

            SetIntRaw setDefaultLookupMode = TryBind<SetIntRaw>(module, "EMC_ModHub_Test_Client_DefaultLookup_SetMode");
            VoidRaw resetDefaultLookup = TryBind<VoidRaw>(module, "EMC_ModHub_Test_Client_DefaultLookup_Reset");
            GetApiRaw callDefaultLookupGetApi = TryBind<GetApiRaw>(module, "EMC_ModHub_Test_Client_DefaultLookup_CallGetApi");

            int availableDebugHelpers = 0;
            foreach (Delegate helper in new Delegate[] {
                resetAliasWarningCount,
                getAliasWarningCount,
                setDefaultLookupMode,
                resetDefaultLookup,
                callDefaultLookupGetApi
            })
            {
                if (helper != null)
                {
                    availableDebugHelpers += 1;
                }
            }

            // Canonical export path.
            ExpectGetApiSuccess(getApiCanonical, "canonical_export");

            ExpectGetApiSuccess(getApiAlias, "alias_export_first");
            ExpectGetApiSuccess(getApiAlias, "alias_export_second");

            Assert(
                availableDebugHelpers == 0 || availableDebugHelpers == 5,
                "Expected either zero or all phase13 debug helper exports.");

            if (availableDebugHelpers == 0)
            {
                return "PASS";
            }

            // Debug-only helper path assertions.
            resetAliasWarningCount();
            ExpectGetApiSuccess(getApiAlias, "alias_export_debug_first");
            ExpectGetApiSuccess(getApiAlias, "alias_export_debug_second");
            Assert(getAliasWarningCount() == 1, "alias export should increment warning count once");

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
Write-Host "PASS"
