param(
    [Parameter(Mandatory = $true)][string]$DllPath
)

$ErrorActionPreference = 'Stop'

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
            r = fn(1u, 56u, IntPtr.Zero, IntPtr.Zero);
            Assert(r == EMC_ERR_INVALID_ARGUMENT, "Expected INVALID_ARGUMENT for both null out params");

            IntPtr outApiSizeOnly = AllocU32Block(1234);
            try
            {
                r = fn(1u, 56u, IntPtr.Zero, outApiSizeOnly);
                Assert(r == EMC_ERR_INVALID_ARGUMENT, "Expected INVALID_ARGUMENT for null out_api");
            }
            finally
            {
                Marshal.FreeHGlobal(outApiSizeOnly);
            }

            IntPtr outApiOnly = AllocPointerBlock();
            try
            {
                r = fn(1u, 56u, outApiOnly, IntPtr.Zero);
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
                r = fn(2u, 56u, outApi, outApiSize);
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
                r = fn(1u, 55u, outApi, outApiSize);
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
                r = fn(1u, 56u, outApi, outApiSize);
                Assert(r == EMC_OK, "Expected OK on success path");

                IntPtr apiPtr = Marshal.ReadIntPtr(outApi);
                int apiSizeValue = Marshal.ReadInt32(outApiSize);

                Assert(apiPtr != IntPtr.Zero, "API pointer is null on success");
                Assert(apiSizeValue == 56, "out_api_size != sizeof(EMC_HubApiV1)");

                int apiVersion = Marshal.ReadInt32(apiPtr, 0);
                int apiStructSize = Marshal.ReadInt32(apiPtr, 4);
                Assert(apiVersion == 1, "api_version != 1");
                Assert(apiStructSize == 56, "api_size field != 56");

                int[] fnOffsets = new int[] { 8, 16, 24, 32, 40, 48 };
                for (int i = 0; i < fnOffsets.Length; i++)
                {
                    IntPtr fnPtr = Marshal.ReadIntPtr(apiPtr, fnOffsets[i]);
                    Assert(fnPtr != IntPtr.Zero, "Function pointer at offset " + fnOffsets[i] + " is null");
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
