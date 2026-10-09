param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [string]$KenshiPath = ""
)

$ErrorActionPreference = "Stop"

$code = @"
using System;
using System.Runtime.InteropServices;

public static class Phase14OptionsObserverHarness
{
    [StructLayout(LayoutKind.Sequential)]
    public struct HubApiV1
    {
        public uint api_version;
        public uint api_size;
        public IntPtr register_mod;
        public IntPtr register_bool_setting;
        public IntPtr register_keybind_setting;
        public IntPtr register_int_setting;
        public IntPtr register_float_setting;
        public IntPtr register_action_row;
        public IntPtr register_options_window_init_observer;
        public IntPtr unregister_options_window_init_observer;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetApiRaw(uint requestedVersion, uint callerApiSize, out IntPtr outApi, out uint outApiSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterObserverRaw(IntPtr observerFn, IntPtr userData);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ObserverRaw(IntPtr userData);

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
    private const int EMC_ERR_CONFLICT = 4;
    private const int EMC_ERR_NOT_FOUND = 5;
    private const int ATTACH_SUCCESS = 0;
    private const int ATTACH_FAILED = 1;

    private const int GET_API_MODE_SUCCESS = 0;
    private const int GET_API_MODE_LEGACY_NO_OBSERVER = 4;

    private const int FORCE_ATTACH_NONE = 0;
    private const int FORCE_ATTACH_STARTUP_ONLY = 1;

    private const uint HUB_API_V1 = 1u;
    private const uint HUB_API_V1_MIN_SIZE = 56u;
    private const uint HUB_API_V1_OPTIONS_OBSERVER_MIN_SIZE = 72u;

    private static int s_directObserverCallbackCount = 0;
    private static readonly ObserverRaw s_directObserver = new ObserverRaw(DirectObserverCallback);

    private static void DirectObserverCallback(IntPtr userData)
    {
        s_directObserverCallbackCount += 1;
    }

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

    private static void AssertClientState(
        GetIntRaw useHubUi,
        GetIntRaw isRetryPending,
        GetIntRaw hasRetried,
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
            Assert(module != IntPtr.Zero, "LoadLibrary failed for " + dllPath);

            GetApiRaw getApi = Bind<GetApiRaw>(module, "EMC_ModHub_GetApi");
            VoidRaw openOptionsWindow = Bind<VoidRaw>(module, "EMC_ModHub_Test_Menu_OpenOptionsWindow");
            GetIntRaw getObserverCount = Bind<GetIntRaw>(module, "EMC_ModHub_Test_GetOptionsWindowInitObserverCount");

            VoidRaw clientReset = Bind<VoidRaw>(module, "EMC_ModHub_Test_Client_Reset");
            SetIntRaw clientSetGetApiMode = Bind<SetIntRaw>(module, "EMC_ModHub_Test_Client_SetGetApiMode");
            SetIntRaw clientSetRegisterResult = Bind<SetIntRaw>(module, "EMC_ModHub_Test_Client_SetRegisterResult");
            SetIntRaw clientSetForceAttachMode = Bind<SetIntRaw>(module, "EMC_ModHub_Test_Client_SetForceAttachFailureMode");
            GetIntRaw clientOnStartup = Bind<GetIntRaw>(module, "EMC_ModHub_Test_Client_OnStartup");
            GetIntRaw clientOnOptionsWindowInit = Bind<GetIntRaw>(module, "EMC_ModHub_Test_Client_OnOptionsWindowInit");
            GetIntRaw clientUseHubUi = Bind<GetIntRaw>(module, "EMC_ModHub_Test_Client_UseHubUi");
            GetIntRaw clientIsRetryPending = Bind<GetIntRaw>(module, "EMC_ModHub_Test_Client_IsAttachRetryPending");
            GetIntRaw clientHasRetried = Bind<GetIntRaw>(module, "EMC_ModHub_Test_Client_HasAttachRetryAttempted");

            clientReset();
            Assert(getObserverCount() == 0, "observer registry should start empty");

            IntPtr apiPtr;
            uint apiSize;
            Assert(getApi(HUB_API_V1, HUB_API_V1_MIN_SIZE, out apiPtr, out apiSize) == EMC_OK, "EMC_ModHub_GetApi should succeed");
            Assert(apiPtr != IntPtr.Zero, "EMC_ModHub_GetApi returned null api");
            Assert(apiSize >= HUB_API_V1_OPTIONS_OBSERVER_MIN_SIZE, "Hub API size should expose observer callbacks");

            HubApiV1 api = (HubApiV1)Marshal.PtrToStructure(apiPtr, typeof(HubApiV1));
            Assert(api.register_options_window_init_observer != IntPtr.Zero, "register_options_window_init_observer pointer missing");
            Assert(api.unregister_options_window_init_observer != IntPtr.Zero, "unregister_options_window_init_observer pointer missing");

            RegisterObserverRaw registerObserver = (RegisterObserverRaw)Marshal.GetDelegateForFunctionPointer(
                api.register_options_window_init_observer, typeof(RegisterObserverRaw));
            RegisterObserverRaw unregisterObserver = (RegisterObserverRaw)Marshal.GetDelegateForFunctionPointer(
                api.unregister_options_window_init_observer, typeof(RegisterObserverRaw));

            s_directObserverCallbackCount = 0;
            IntPtr observerPtr = Marshal.GetFunctionPointerForDelegate(s_directObserver);
            Assert(registerObserver(observerPtr, IntPtr.Zero) == EMC_OK, "direct observer register should succeed");
            Assert(registerObserver(observerPtr, IntPtr.Zero) == EMC_ERR_CONFLICT, "duplicate observer register should conflict");
            Assert(getObserverCount() == 1, "direct observer should be present");
            openOptionsWindow();
            Assert(s_directObserverCallbackCount == 1, "direct observer should fire once");
            Assert(unregisterObserver(observerPtr, IntPtr.Zero) == EMC_OK, "direct observer unregister should succeed");
            Assert(unregisterObserver(observerPtr, IntPtr.Zero) == EMC_ERR_NOT_FOUND, "missing observer unregister should return not_found");
            Assert(getObserverCount() == 0, "direct observer should be removed");
            openOptionsWindow();
            Assert(s_directObserverCallbackCount == 1, "direct observer should stay removed");

            // Observer-enabled helper path: startup failure auto-registers, hub options init triggers retry.
            clientReset();
            clientSetGetApiMode(GET_API_MODE_SUCCESS);
            clientSetRegisterResult(EMC_OK);
            clientSetForceAttachMode(FORCE_ATTACH_STARTUP_ONLY);
            Assert(clientOnStartup() == ATTACH_FAILED, "observer path startup should fail once");
            Assert(getObserverCount() == 1, "helper should auto-register observer when available");
            AssertClientState(clientUseHubUi, clientIsRetryPending, clientHasRetried, 0, 1, 0, "observer_path_before_retry");
            openOptionsWindow();
            AssertClientState(clientUseHubUi, clientIsRetryPending, clientHasRetried, 1, 0, 1, "observer_path_after_retry");
            Assert(getObserverCount() == 0, "helper observer should unregister after retry");

            // Observer-unavailable fallback: old explicit OnOptionsWindowInit path still works.
            clientReset();
            clientSetGetApiMode(GET_API_MODE_LEGACY_NO_OBSERVER);
            clientSetRegisterResult(EMC_OK);
            clientSetForceAttachMode(FORCE_ATTACH_STARTUP_ONLY);
            Assert(clientOnStartup() == ATTACH_FAILED, "legacy fallback startup should fail once");
            Assert(getObserverCount() == 0, "legacy api size should not register observer");
            openOptionsWindow();
            AssertClientState(clientUseHubUi, clientIsRetryPending, clientHasRetried, 0, 1, 0, "legacy_fallback_before_manual_retry");
            Assert(clientOnOptionsWindowInit() == ATTACH_SUCCESS, "legacy fallback manual retry should succeed");
            AssertClientState(clientUseHubUi, clientIsRetryPending, clientHasRetried, 1, 0, 1, "legacy_fallback_after_manual_retry");

            // Reset simulates shutdown cleanup for a pending observer registration.
            clientReset();
            clientSetGetApiMode(GET_API_MODE_SUCCESS);
            clientSetRegisterResult(EMC_OK);
            clientSetForceAttachMode(FORCE_ATTACH_STARTUP_ONLY);
            Assert(clientOnStartup() == ATTACH_FAILED, "shutdown cleanup startup should fail once");
            Assert(getObserverCount() == 1, "pending retry should keep observer registered");
            clientReset();
            Assert(getObserverCount() == 0, "client reset should unregister observer");

            clientSetForceAttachMode(FORCE_ATTACH_NONE);
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
$result = [Phase14OptionsObserverHarness]::Run($DllPath, $KenshiPath)
Write-Host $result
