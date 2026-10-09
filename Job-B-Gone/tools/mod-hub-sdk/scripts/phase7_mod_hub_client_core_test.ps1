param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [string]$KenshiPath = ""
)

$ErrorActionPreference = "Stop"

$code = @"
using System;
using System.Runtime.InteropServices;

public static class ModHubClientPhase7Harness
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ClientResetRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ClientSetIntRaw(int value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int ClientGetIntRaw();

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern bool SetDllDirectory(string lpPathName);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr LoadLibrary(string lpFileName);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr LoadLibraryEx(string lpFileName, IntPtr hFile, uint dwFlags);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr GetProcAddress(IntPtr hModule, string lpProcName);

    [DllImport("kernel32", SetLastError = true)]
    private static extern bool FreeLibrary(IntPtr hModule);

    private const uint DONT_RESOLVE_DLL_REFERENCES = 0x00000001u;

    private const int EMC_OK = 0;
    private const int EMC_ERR_INTERNAL = 7;

    private const int ATTACH_SUCCESS = 0;
    private const int ATTACH_FAILED = 1;
    private const int REGISTRATION_FAILED = 2;

    private const int GET_API_MODE_SUCCESS = 0;
    private const int GET_API_MODE_FAILURE = 1;

    private const int FORCE_ATTACH_NONE = 0;
    private const int FORCE_ATTACH_STARTUP_ONLY = 1;
    private const int FORCE_ATTACH_ALWAYS = 2;

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

    private static void AssertState(
        ClientGetIntRaw useHubUi,
        ClientGetIntRaw isRetryPending,
        ClientGetIntRaw hasRetried,
        int expectedUseHubUi,
        int expectedRetryPending,
        int expectedRetried,
        string context)
    {
        Assert(useHubUi() == expectedUseHubUi, context + " use_hub_ui mismatch");
        Assert(isRetryPending() == expectedRetryPending, context + " retry_pending mismatch");
        Assert(hasRetried() == expectedRetried, context + " retry_attempted mismatch");
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
                module = LoadLibraryEx(dllPath, IntPtr.Zero, DONT_RESOLVE_DLL_REFERENCES);
            }
            Assert(module != IntPtr.Zero, "LoadLibrary failed for " + dllPath);

            ClientResetRaw reset = Bind<ClientResetRaw>(module, "EMC_ModHub_Test_Client_Reset");
            ClientSetIntRaw setGetApiMode = Bind<ClientSetIntRaw>(module, "EMC_ModHub_Test_Client_SetGetApiMode");
            ClientSetIntRaw setGetApiFailureResult = Bind<ClientSetIntRaw>(module, "EMC_ModHub_Test_Client_SetGetApiFailureResult");
            ClientSetIntRaw setRegisterResult = Bind<ClientSetIntRaw>(module, "EMC_ModHub_Test_Client_SetRegisterResult");
            ClientSetIntRaw setForceAttachMode = Bind<ClientSetIntRaw>(module, "EMC_ModHub_Test_Client_SetForceAttachFailureMode");
            ClientGetIntRaw onStartup = Bind<ClientGetIntRaw>(module, "EMC_ModHub_Test_Client_OnStartup");
            ClientGetIntRaw onOptionsInit = Bind<ClientGetIntRaw>(module, "EMC_ModHub_Test_Client_OnOptionsWindowInit");
            ClientGetIntRaw useHubUi = Bind<ClientGetIntRaw>(module, "EMC_ModHub_Test_Client_UseHubUi");
            ClientGetIntRaw isRetryPending = Bind<ClientGetIntRaw>(module, "EMC_ModHub_Test_Client_IsAttachRetryPending");
            ClientGetIntRaw hasRetried = Bind<ClientGetIntRaw>(module, "EMC_ModHub_Test_Client_HasAttachRetryAttempted");
            ClientGetIntRaw lastFailure = Bind<ClientGetIntRaw>(module, "EMC_ModHub_Test_Client_LastAttemptFailureResult");

            // Case 1: attach success.
            reset();
            setGetApiMode(GET_API_MODE_SUCCESS);
            setRegisterResult(EMC_OK);
            setForceAttachMode(FORCE_ATTACH_NONE);
            Assert(onStartup() == ATTACH_SUCCESS, "startup_success result mismatch");
            AssertState(useHubUi, isRetryPending, hasRetried, 1, 0, 0, "startup_success");
            Assert(lastFailure() == EMC_OK, "startup_success last_failure mismatch");

            // Case 2: attach fail once, retry succeeds.
            reset();
            setGetApiMode(GET_API_MODE_SUCCESS);
            setRegisterResult(EMC_OK);
            setForceAttachMode(FORCE_ATTACH_STARTUP_ONLY);
            Assert(onStartup() == ATTACH_FAILED, "startup_fail_once result mismatch");
            AssertState(useHubUi, isRetryPending, hasRetried, 0, 1, 0, "startup_fail_once");
            Assert(lastFailure() == EMC_ERR_INTERNAL, "startup_fail_once last_failure mismatch");
            Assert(onOptionsInit() == ATTACH_SUCCESS, "retry_success result mismatch");
            AssertState(useHubUi, isRetryPending, hasRetried, 1, 0, 1, "retry_success");

            // Case 3: attach fail on startup and retry.
            reset();
            setGetApiMode(GET_API_MODE_SUCCESS);
            setRegisterResult(EMC_OK);
            setForceAttachMode(FORCE_ATTACH_ALWAYS);
            Assert(onStartup() == ATTACH_FAILED, "startup_fail_always result mismatch");
            AssertState(useHubUi, isRetryPending, hasRetried, 0, 1, 0, "startup_fail_always");
            Assert(onOptionsInit() == ATTACH_FAILED, "retry_fail result mismatch");
            AssertState(useHubUi, isRetryPending, hasRetried, 0, 0, 1, "retry_fail");

            // Case 4: attach succeeds, registration fails, no retry pending.
            reset();
            setGetApiMode(GET_API_MODE_SUCCESS);
            setRegisterResult(EMC_ERR_INTERNAL);
            setForceAttachMode(FORCE_ATTACH_NONE);
            Assert(onStartup() == REGISTRATION_FAILED, "register_fail result mismatch");
            AssertState(useHubUi, isRetryPending, hasRetried, 0, 0, 0, "register_fail");
            Assert(lastFailure() == EMC_ERR_INTERNAL, "register_fail last_failure mismatch");

            // Extra sanity: explicit get_api failure path.
            reset();
            setGetApiMode(GET_API_MODE_FAILURE);
            setGetApiFailureResult(EMC_ERR_INTERNAL);
            setRegisterResult(EMC_OK);
            setForceAttachMode(FORCE_ATTACH_NONE);
            Assert(onStartup() == ATTACH_FAILED, "get_api_fail result mismatch");
            AssertState(useHubUi, isRetryPending, hasRetried, 0, 1, 0, "get_api_fail");

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
$result = [ModHubClientPhase7Harness]::Run($DllPath, $KenshiPath)
Write-Host $result
