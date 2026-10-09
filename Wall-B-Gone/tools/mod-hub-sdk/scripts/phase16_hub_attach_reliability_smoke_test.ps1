param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [string]$KenshiPath = "",
    [ValidateRange(1, 10)][int]$RepeatCount = 2
)

$ErrorActionPreference = "Stop"

$DllPath = (Resolve-Path -Path $DllPath).ProviderPath
if ($KenshiPath) {
    $KenshiPath = (Resolve-Path -Path $KenshiPath).ProviderPath
}

$code = @"
using System;
using System.Runtime.InteropServices;

public static class Phase16HubAttachReliabilitySmokeHarness
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
    private static extern bool SetDllDirectory(string lpPathName);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr LoadLibrary(string lpFileName);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr GetProcAddress(IntPtr hModule, string lpProcName);

    [DllImport("kernel32", SetLastError = true)]
    private static extern bool FreeLibrary(IntPtr hModule);

    private const int EMC_OK = 0;
    private const int EMC_ERR_NOT_FOUND = 5;
    private const int EMC_ERR_INTERNAL = 7;

    private const int ATTACH_SUCCESS = 0;
    private const int ATTACH_FAILED = 1;
    private const int REGISTRATION_FAILED = 2;

    private const int GET_API_MODE_SUCCESS = 0;
    private const int GET_API_MODE_FAILURE = 1;

    private const int FORCE_ATTACH_NONE = 0;
    private const int FORCE_ATTACH_STARTUP_ONLY = 1;
    private const int FORCE_ATTACH_ALWAYS = 2;

    private const int LOOKUP_MODE_AUTO = 0;
    private const int LOOKUP_MODE_MISSING = 2;

    private const uint HUB_API_V1 = 1u;
    private const uint HUB_API_V1_MIN_SIZE = 56u;

    private static void Assert(bool condition, string message)
    {
        if (!condition)
        {
            throw new Exception(message);
        }
    }

    private static string Context(string caseName, string cause, string detail)
    {
        return "case=" + caseName + " cause=" + cause + " " + detail;
    }

    private static T Bind<T>(IntPtr module, string exportName)
    {
        IntPtr proc = GetProcAddress(module, exportName);
        Assert(proc != IntPtr.Zero, "Missing export: " + exportName);
        return (T)(object)Marshal.GetDelegateForFunctionPointer(proc, typeof(T));
    }

    private static void AssertState(
        GetIntRaw useHubUi,
        GetIntRaw isRetryPending,
        GetIntRaw hasRetried,
        int expectedUseHubUi,
        int expectedRetryPending,
        int expectedRetried,
        string caseName,
        string cause)
    {
        Assert(
            useHubUi() == expectedUseHubUi,
            Context(caseName, cause, "use_hub_ui mismatch"));
        Assert(
            isRetryPending() == expectedRetryPending,
            Context(caseName, cause, "retry_pending mismatch"));
        Assert(
            hasRetried() == expectedRetried,
            Context(caseName, cause, "retry_attempted mismatch"));
    }

    private static void PrintPass(string caseName, int iteration, string cause)
    {
        Console.WriteLine("PASS case=" + caseName + " iteration=" + iteration + " cause=" + cause);
    }

    private static void ExpectLookupNotFound(GetApiRaw fn, string caseName, string cause)
    {
        IntPtr outApi = Marshal.AllocHGlobal(IntPtr.Size);
        IntPtr outApiSize = Marshal.AllocHGlobal(4);
        try
        {
            Marshal.WriteIntPtr(outApi, new IntPtr(0x1357));
            Marshal.WriteInt32(outApiSize, unchecked((int)0x24682468));

            int r = fn(HUB_API_V1, HUB_API_V1_MIN_SIZE, outApi, outApiSize);
            Assert(
                r == EMC_ERR_NOT_FOUND,
                Context(caseName, cause, "default lookup should return not_found"));
            Assert(
                Marshal.ReadIntPtr(outApi) == IntPtr.Zero,
                Context(caseName, cause, "default lookup should clear out_api"));
            Assert(
                Marshal.ReadInt32(outApiSize) == 0,
                Context(caseName, cause, "default lookup should clear out_api_size"));
        }
        finally
        {
            Marshal.FreeHGlobal(outApi);
            Marshal.FreeHGlobal(outApiSize);
        }
    }

    public static void Run(string dllPath, string kenshiPath, int repeatCount)
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
                throw new Exception(
                    "LoadLibrary failed for " + dllPath +
                    ". Provide -KenshiPath when Kenshi runtime dependencies are outside PATH.");
            }

            VoidRaw clientReset = Bind<VoidRaw>(module, "EMC_ModHub_Test_Client_Reset");
            SetIntRaw setGetApiMode = Bind<SetIntRaw>(module, "EMC_ModHub_Test_Client_SetGetApiMode");
            SetIntRaw setGetApiFailureResult = Bind<SetIntRaw>(module, "EMC_ModHub_Test_Client_SetGetApiFailureResult");
            SetIntRaw setRegisterResult = Bind<SetIntRaw>(module, "EMC_ModHub_Test_Client_SetRegisterResult");
            SetIntRaw setForceAttachMode = Bind<SetIntRaw>(module, "EMC_ModHub_Test_Client_SetForceAttachFailureMode");
            GetIntRaw onStartup = Bind<GetIntRaw>(module, "EMC_ModHub_Test_Client_OnStartup");
            GetIntRaw useHubUi = Bind<GetIntRaw>(module, "EMC_ModHub_Test_Client_UseHubUi");
            GetIntRaw isRetryPending = Bind<GetIntRaw>(module, "EMC_ModHub_Test_Client_IsAttachRetryPending");
            GetIntRaw hasRetried = Bind<GetIntRaw>(module, "EMC_ModHub_Test_Client_HasAttachRetryAttempted");
            GetIntRaw lastFailure = Bind<GetIntRaw>(module, "EMC_ModHub_Test_Client_LastAttemptFailureResult");
            VoidRaw openOptionsWindow = Bind<VoidRaw>(module, "EMC_ModHub_Test_Menu_OpenOptionsWindow");
            GetIntRaw getObserverCount = Bind<GetIntRaw>(module, "EMC_ModHub_Test_GetOptionsWindowInitObserverCount");
            VoidRaw defaultLookupReset = Bind<VoidRaw>(module, "EMC_ModHub_Test_Client_DefaultLookup_Reset");
            SetIntRaw defaultLookupSetMode = Bind<SetIntRaw>(module, "EMC_ModHub_Test_Client_DefaultLookup_SetMode");
            GetApiRaw defaultLookupCallGetApi = Bind<GetApiRaw>(module, "EMC_ModHub_Test_Client_DefaultLookup_CallGetApi");

            for (int iteration = 1; iteration <= repeatCount; ++iteration)
            {
                clientReset();
                defaultLookupReset();

                // Matrix 1: attach succeeds at startup.
                setGetApiMode(GET_API_MODE_SUCCESS);
                setRegisterResult(EMC_OK);
                setForceAttachMode(FORCE_ATTACH_NONE);
                Assert(
                    onStartup() == ATTACH_SUCCESS,
                    Context("startup_attach_success", "attach", "startup result mismatch"));
                AssertState(
                    useHubUi,
                    isRetryPending,
                    hasRetried,
                    1,
                    0,
                    0,
                    "startup_attach_success",
                    "attach");
                Assert(
                    lastFailure() == EMC_OK,
                    Context("startup_attach_success", "attach", "last_failure mismatch"));
                Assert(
                    getObserverCount() == 0,
                    Context("startup_attach_success", "attach", "observer registry should stay empty"));
                PrintPass("startup_attach_success", iteration, "attach");

                // Matrix 2: attach fails at startup, retry succeeds at options-init.
                clientReset();
                setGetApiMode(GET_API_MODE_SUCCESS);
                setRegisterResult(EMC_OK);
                setForceAttachMode(FORCE_ATTACH_STARTUP_ONLY);
                Assert(
                    onStartup() == ATTACH_FAILED,
                    Context("startup_retry_success", "retry", "startup result mismatch"));
                AssertState(
                    useHubUi,
                    isRetryPending,
                    hasRetried,
                    0,
                    1,
                    0,
                    "startup_retry_success",
                    "retry");
                Assert(
                    getObserverCount() == 1,
                    Context("startup_retry_success", "retry", "expected one pending observer"));
                openOptionsWindow();
                AssertState(
                    useHubUi,
                    isRetryPending,
                    hasRetried,
                    1,
                    0,
                    1,
                    "startup_retry_success",
                    "retry");
                Assert(
                    getObserverCount() == 0,
                    Context("startup_retry_success", "retry", "observer should unregister after success"));
                PrintPass("startup_retry_success", iteration, "retry");

                // Matrix 3: attach fails at startup and retry, local fallback stays active.
                clientReset();
                setGetApiMode(GET_API_MODE_SUCCESS);
                setRegisterResult(EMC_OK);
                setForceAttachMode(FORCE_ATTACH_ALWAYS);
                Assert(
                    onStartup() == ATTACH_FAILED,
                    Context("startup_retry_fail_local_fallback", "retry", "startup result mismatch"));
                AssertState(
                    useHubUi,
                    isRetryPending,
                    hasRetried,
                    0,
                    1,
                    0,
                    "startup_retry_fail_local_fallback",
                    "retry");
                Assert(
                    getObserverCount() == 1,
                    Context("startup_retry_fail_local_fallback", "retry", "expected one pending observer"));
                openOptionsWindow();
                AssertState(
                    useHubUi,
                    isRetryPending,
                    hasRetried,
                    0,
                    0,
                    1,
                    "startup_retry_fail_local_fallback",
                    "retry");
                Assert(
                    getObserverCount() == 0,
                    Context("startup_retry_fail_local_fallback", "retry", "observer should unregister after failed retry"));
                PrintPass("startup_retry_fail_local_fallback", iteration, "retry");

                // Matrix 4: attach succeeds but registration fails, local fallback stays active.
                clientReset();
                setGetApiMode(GET_API_MODE_SUCCESS);
                setRegisterResult(EMC_ERR_INTERNAL);
                setForceAttachMode(FORCE_ATTACH_NONE);
                Assert(
                    onStartup() == REGISTRATION_FAILED,
                    Context("registration_failure_local_fallback", "registration", "startup result mismatch"));
                AssertState(
                    useHubUi,
                    isRetryPending,
                    hasRetried,
                    0,
                    0,
                    0,
                    "registration_failure_local_fallback",
                    "registration");
                Assert(
                    lastFailure() == EMC_ERR_INTERNAL,
                    Context("registration_failure_local_fallback", "registration", "last_failure mismatch"));
                Assert(
                    getObserverCount() == 0,
                    Context("registration_failure_local_fallback", "registration", "observer registry should stay empty"));
                PrintPass("registration_failure_local_fallback", iteration, "registration");

                // Matrix 5: export symbol lookup fails deterministically and local fallback remains active.
                defaultLookupSetMode(LOOKUP_MODE_MISSING);
                ExpectLookupNotFound(
                    defaultLookupCallGetApi,
                    "symbol_lookup_missing_fallback",
                    "symbol_lookup");
                clientReset();
                setGetApiMode(GET_API_MODE_FAILURE);
                setGetApiFailureResult(EMC_ERR_NOT_FOUND);
                setRegisterResult(EMC_OK);
                setForceAttachMode(FORCE_ATTACH_NONE);
                Assert(
                    onStartup() == ATTACH_FAILED,
                    Context("symbol_lookup_missing_fallback", "symbol_lookup", "startup result mismatch"));
                AssertState(
                    useHubUi,
                    isRetryPending,
                    hasRetried,
                    0,
                    1,
                    0,
                    "symbol_lookup_missing_fallback",
                    "symbol_lookup");
                Assert(
                    lastFailure() == EMC_ERR_NOT_FOUND,
                    Context("symbol_lookup_missing_fallback", "symbol_lookup", "last_failure mismatch"));
                defaultLookupReset();
                PrintPass("symbol_lookup_missing_fallback", iteration, "symbol_lookup");

                // Restore defaults before the next loop.
                clientReset();
                setGetApiMode(GET_API_MODE_SUCCESS);
                setRegisterResult(EMC_OK);
                setForceAttachMode(FORCE_ATTACH_NONE);
                defaultLookupSetMode(LOOKUP_MODE_AUTO);
            }
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

try {
    [Phase16HubAttachReliabilitySmokeHarness]::Run($DllPath, $KenshiPath, $RepeatCount)
    Write-Host "PASS"
}
catch {
    $failure = $_.Exception
    while ($failure.InnerException) {
        $failure = $failure.InnerException
    }

    Write-Host ("FAIL " + $failure.Message)
    exit 1
}
