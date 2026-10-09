param(
    [Parameter(Mandatory = $true)][string]$DllPath
)

$ErrorActionPreference = 'Stop'
$DllPath = (Resolve-Path -Path $DllPath).ProviderPath

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

if (-not $IsWindows) {
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

    Write-Host "PASS: phase1 export smoke completed (WSL)"
    return
}

$code = @"
using System;
using System.Runtime.InteropServices;

public static class HubApiHarness
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetApiRaw(uint requestedVersion, uint callerApiSize, IntPtr outApi, IntPtr outApiSize);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr LoadLibrary(string lpFileName);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr LoadLibraryEx(string lpFileName, IntPtr hFile, uint dwFlags);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr GetProcAddress(IntPtr hModule, string lpProcName);

    [DllImport("kernel32", SetLastError = true)]
    private static extern bool FreeLibrary(IntPtr hModule);

    private const int EMC_OK = 0;
    private const int EMC_ERR_INVALID_ARGUMENT = 1;
    private const int EMC_ERR_UNSUPPORTED_VERSION = 2;
    private const int EMC_ERR_API_SIZE_MISMATCH = 3;
    private const uint DONT_RESOLVE_DLL_REFERENCES = 0x00000001u;
    private const uint HUB_API_V1_MIN_SIZE = 56u;
    private const uint HUB_API_V1_OPTIONS_WINDOW_INIT_OBSERVER_MIN_SIZE = 72u;
    private const uint HUB_API_V1_INT_SETTING_V2_MIN_SIZE = 80u;

    private static void Assert(bool condition, string message)
    {
        if (!condition)
        {
            throw new Exception(message);
        }
    }

    private static IntPtr AllocPointerBlock()
    {
        IntPtr mem = Marshal.AllocHGlobal(IntPtr.Size);
        Marshal.WriteIntPtr(mem, new IntPtr(0x7));
        return mem;
    }

    private static IntPtr AllocU32Block(int value)
    {
        IntPtr mem = Marshal.AllocHGlobal(4);
        Marshal.WriteInt32(mem, value);
        return mem;
    }

    public static string Run(string dllPath)
    {
        IntPtr module = LoadLibraryEx(dllPath, IntPtr.Zero, DONT_RESOLVE_DLL_REFERENCES);
        if (module == IntPtr.Zero)
        {
            module = LoadLibrary(dllPath);
        }
        Assert(module != IntPtr.Zero, "LoadLibrary failed for " + dllPath);

        try
        {
            IntPtr proc = GetProcAddress(module, "EMC_ModHub_GetApi");
            Assert(proc != IntPtr.Zero, "GetProcAddress failed for EMC_ModHub_GetApi");

            GetApiRaw fn = (GetApiRaw)Marshal.GetDelegateForFunctionPointer(proc, typeof(GetApiRaw));

            int r;

            // null out params
            r = fn(1u, HUB_API_V1_MIN_SIZE, IntPtr.Zero, IntPtr.Zero);
            Assert(r == EMC_ERR_INVALID_ARGUMENT, "Expected INVALID_ARGUMENT for both null out params");

            IntPtr outApiSizeOnly = AllocU32Block(1234);
            try
            {
                r = fn(1u, HUB_API_V1_MIN_SIZE, IntPtr.Zero, outApiSizeOnly);
                Assert(r == EMC_ERR_INVALID_ARGUMENT, "Expected INVALID_ARGUMENT for null out_api");
            }
            finally
            {
                Marshal.FreeHGlobal(outApiSizeOnly);
            }

            IntPtr outApiOnly = AllocPointerBlock();
            try
            {
                r = fn(1u, HUB_API_V1_MIN_SIZE, outApiOnly, IntPtr.Zero);
                Assert(r == EMC_ERR_INVALID_ARGUMENT, "Expected INVALID_ARGUMENT for null out_api_size");
            }
            finally
            {
                Marshal.FreeHGlobal(outApiOnly);
            }

            // unsupported version
            IntPtr outApi = AllocPointerBlock();
            IntPtr outApiSize = AllocU32Block(9999);
            try
            {
                r = fn(2u, HUB_API_V1_MIN_SIZE, outApi, outApiSize);
                Assert(r == EMC_ERR_UNSUPPORTED_VERSION, "Expected UNSUPPORTED_VERSION");
                Assert(Marshal.ReadIntPtr(outApi) == IntPtr.Zero, "out_api not cleared on unsupported version");
                Assert(Marshal.ReadInt32(outApiSize) == 0, "out_api_size not cleared on unsupported version");
            }
            finally
            {
                Marshal.FreeHGlobal(outApi);
                Marshal.FreeHGlobal(outApiSize);
            }

            // api size mismatch
            outApi = AllocPointerBlock();
            outApiSize = AllocU32Block(9999);
            try
            {
                r = fn(1u, HUB_API_V1_MIN_SIZE - 1u, outApi, outApiSize);
                Assert(r == EMC_ERR_API_SIZE_MISMATCH, "Expected API_SIZE_MISMATCH");
                Assert(Marshal.ReadIntPtr(outApi) == IntPtr.Zero, "out_api not cleared on size mismatch");
                Assert(Marshal.ReadInt32(outApiSize) == 0, "out_api_size not cleared on size mismatch");
            }
            finally
            {
                Marshal.FreeHGlobal(outApi);
                Marshal.FreeHGlobal(outApiSize);
            }

            // success
            outApi = AllocPointerBlock();
            outApiSize = AllocU32Block(0);
            try
            {
                r = fn(1u, HUB_API_V1_MIN_SIZE, outApi, outApiSize);
                Assert(r == EMC_OK, "Expected OK on success path");

                IntPtr apiPtr = Marshal.ReadIntPtr(outApi);
                int apiSizeValue = Marshal.ReadInt32(outApiSize);

                Assert(apiPtr != IntPtr.Zero, "API pointer is null on success");
                Assert(apiSizeValue >= (int)HUB_API_V1_MIN_SIZE, "out_api_size smaller than minimum contract");

                int apiVersion = Marshal.ReadInt32(apiPtr, 0);
                int apiStructSize = Marshal.ReadInt32(apiPtr, 4);
                Assert(apiVersion == 1, "api_version != 1");
                Assert(apiStructSize >= (int)HUB_API_V1_MIN_SIZE, "api_size field smaller than minimum contract");
                Assert(apiStructSize == apiSizeValue, "api_size field != out_api_size");

                int[] requiredFnOffsets = new int[] { 8, 16, 24, 32, 40, 48 };
                for (int i = 0; i < requiredFnOffsets.Length; i++)
                {
                    IntPtr fnPtr = Marshal.ReadIntPtr(apiPtr, requiredFnOffsets[i]);
                    Assert(fnPtr != IntPtr.Zero, "Function pointer at offset " + requiredFnOffsets[i] + " is null");
                }

                if (apiSizeValue >= (int)HUB_API_V1_OPTIONS_WINDOW_INIT_OBSERVER_MIN_SIZE)
                {
                    int[] observerFnOffsets = new int[] { 56, 64 };
                    for (int i = 0; i < observerFnOffsets.Length; i++)
                    {
                        IntPtr fnPtr = Marshal.ReadIntPtr(apiPtr, observerFnOffsets[i]);
                        Assert(fnPtr != IntPtr.Zero, "Observer function pointer at offset " + observerFnOffsets[i] + " is null");
                    }
                }

                if (apiSizeValue >= (int)HUB_API_V1_INT_SETTING_V2_MIN_SIZE)
                {
                    IntPtr fnPtr = Marshal.ReadIntPtr(apiPtr, 72);
                    Assert(fnPtr != IntPtr.Zero, "V2 int-setting function pointer at offset 72 is null");
                }
            }
            finally
            {
                Marshal.FreeHGlobal(outApi);
                Marshal.FreeHGlobal(outApiSize);
            }

            return "PASS: handshake matrix completed";
        }
        finally
        {
            FreeLibrary(module);
        }
    }
}
"@

Add-Type -TypeDefinition $code -Language CSharp
$result = [HubApiHarness]::Run($DllPath)
Write-Host $result
