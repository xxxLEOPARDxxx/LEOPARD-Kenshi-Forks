param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [string]$KenshiPath = ""
)

$ErrorActionPreference = "Stop"

$code = @"
using System;
using System.Runtime.InteropServices;

public static class ModHubClientPhase8TableHarness
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ClientTableResetRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ClientTableSetModeRaw(int mode);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int ClientTableGetIntRaw();

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
    private const int EMC_ERR_INVALID_ARGUMENT = 1;
    private const int EMC_ERR_INTERNAL = 7;

    private const int ATTACH_SUCCESS = 0;
    private const int REGISTRATION_FAILED = 2;

    private const int MODE_SUCCESS = 0;
    private const int MODE_FAIL_INT = 3;
    private const int MODE_FAIL_ACTION = 5;
    private const int MODE_INVALID_ROW_KIND = 6;
    private const int MODE_NULL_ROW_DEF = 7;
    private const int MODE_USE_INT_V2 = 8;
    private const int MODE_USE_INT_V2_LEGACY_API = 9;

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

    private static void AssertCounts(
        ClientTableGetIntRaw getMod,
        ClientTableGetIntRaw getBool,
        ClientTableGetIntRaw getKeybind,
        ClientTableGetIntRaw getInt,
        ClientTableGetIntRaw getFloat,
        ClientTableGetIntRaw getSelect,
        ClientTableGetIntRaw getText,
        ClientTableGetIntRaw getColor,
        ClientTableGetIntRaw getAction,
        int expectedMod,
        int expectedBool,
        int expectedKeybind,
        int expectedInt,
        int expectedFloat,
        int expectedSelect,
        int expectedText,
        int expectedColor,
        int expectedAction,
        string context)
    {
        Assert(getMod() == expectedMod, context + " register_mod count mismatch");
        Assert(getBool() == expectedBool, context + " bool count mismatch");
        Assert(getKeybind() == expectedKeybind, context + " keybind count mismatch");
        Assert(getInt() == expectedInt, context + " int count mismatch");
        Assert(getFloat() == expectedFloat, context + " float count mismatch");
        Assert(getSelect() == expectedSelect, context + " select count mismatch");
        Assert(getText() == expectedText, context + " text count mismatch");
        Assert(getColor() == expectedColor, context + " color count mismatch");
        Assert(getAction() == expectedAction, context + " action count mismatch");
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

            ClientTableResetRaw reset = Bind<ClientTableResetRaw>(module, "EMC_ModHub_Test_DummyConsumer_Reset");
            ClientTableSetModeRaw setMode = Bind<ClientTableSetModeRaw>(module, "EMC_ModHub_Test_DummyConsumer_SetMode");
            ClientTableGetIntRaw onStartup = Bind<ClientTableGetIntRaw>(module, "EMC_ModHub_Test_DummyConsumer_OnStartup");
            ClientTableGetIntRaw useHubUi = Bind<ClientTableGetIntRaw>(module, "EMC_ModHub_Test_DummyConsumer_UseHubUi");
            ClientTableGetIntRaw lastFailure = Bind<ClientTableGetIntRaw>(module, "EMC_ModHub_Test_DummyConsumer_LastAttemptFailureResult");

            ClientTableGetIntRaw getModCalls = Bind<ClientTableGetIntRaw>(module, "EMC_ModHub_Test_DummyConsumer_GetRegisterModCalls");
            ClientTableGetIntRaw getBoolCalls = Bind<ClientTableGetIntRaw>(module, "EMC_ModHub_Test_DummyConsumer_GetRegisterBoolCalls");
            ClientTableGetIntRaw getKeybindCalls = Bind<ClientTableGetIntRaw>(module, "EMC_ModHub_Test_DummyConsumer_GetRegisterKeybindCalls");
            ClientTableGetIntRaw getIntCalls = Bind<ClientTableGetIntRaw>(module, "EMC_ModHub_Test_DummyConsumer_GetRegisterIntCalls");
            ClientTableGetIntRaw getIntV2Calls = Bind<ClientTableGetIntRaw>(module, "EMC_ModHub_Test_DummyConsumer_GetRegisterIntV2Calls");
            ClientTableGetIntRaw getFloatCalls = Bind<ClientTableGetIntRaw>(module, "EMC_ModHub_Test_DummyConsumer_GetRegisterFloatCalls");
            ClientTableGetIntRaw getSelectCalls = Bind<ClientTableGetIntRaw>(module, "EMC_ModHub_Test_DummyConsumer_GetRegisterSelectCalls");
            ClientTableGetIntRaw getTextCalls = Bind<ClientTableGetIntRaw>(module, "EMC_ModHub_Test_DummyConsumer_GetRegisterTextCalls");
            ClientTableGetIntRaw getColorCalls = Bind<ClientTableGetIntRaw>(module, "EMC_ModHub_Test_DummyConsumer_GetRegisterColorCalls");
            ClientTableGetIntRaw getActionCalls = Bind<ClientTableGetIntRaw>(module, "EMC_ModHub_Test_DummyConsumer_GetRegisterActionCalls");
            ClientTableGetIntRaw getOrderChecks = Bind<ClientTableGetIntRaw>(module, "EMC_ModHub_Test_DummyConsumer_GetOrderChecksPassed");
            ClientTableGetIntRaw getDescriptorChecks = Bind<ClientTableGetIntRaw>(module, "EMC_ModHub_Test_DummyConsumer_GetDescriptorChecksPassed");

            // Case 1: success path includes all v1 row kinds in order.
            reset();
            setMode(MODE_SUCCESS);
            Assert(onStartup() == ATTACH_SUCCESS, "table_success startup result mismatch");
            Assert(useHubUi() == 1, "table_success use_hub_ui mismatch");
            Assert(lastFailure() == EMC_OK, "table_success last_failure mismatch");
            AssertCounts(getModCalls, getBoolCalls, getKeybindCalls, getIntCalls, getFloatCalls, getSelectCalls, getTextCalls, getColorCalls, getActionCalls,
                1, 1, 1, 1, 1, 1, 1, 1, 1, "table_success");
            Assert(getOrderChecks() == 1, "table_success order check mismatch");
            Assert(getDescriptorChecks() == 1, "table_success descriptor check mismatch");

            // Case 2: int registration failure is propagated and later rows are skipped.
            reset();
            setMode(MODE_FAIL_INT);
            Assert(onStartup() == REGISTRATION_FAILED, "table_fail_int startup result mismatch");
            Assert(useHubUi() == 0, "table_fail_int use_hub_ui mismatch");
            Assert(lastFailure() == EMC_ERR_INTERNAL, "table_fail_int last_failure mismatch");
            AssertCounts(getModCalls, getBoolCalls, getKeybindCalls, getIntCalls, getFloatCalls, getSelectCalls, getTextCalls, getColorCalls, getActionCalls,
                1, 1, 1, 1, 0, 0, 0, 0, 0, "table_fail_int");
            Assert(getOrderChecks() == 1, "table_fail_int order check mismatch");
            Assert(getDescriptorChecks() == 1, "table_fail_int descriptor check mismatch");

            // Case 3: action registration failure propagates after prior rows succeed.
            reset();
            setMode(MODE_FAIL_ACTION);
            Assert(onStartup() == REGISTRATION_FAILED, "table_fail_action startup result mismatch");
            Assert(useHubUi() == 0, "table_fail_action use_hub_ui mismatch");
            Assert(lastFailure() == EMC_ERR_INTERNAL, "table_fail_action last_failure mismatch");
            AssertCounts(getModCalls, getBoolCalls, getKeybindCalls, getIntCalls, getFloatCalls, getSelectCalls, getTextCalls, getColorCalls, getActionCalls,
                1, 1, 1, 1, 1, 1, 1, 1, 1, "table_fail_action");
            Assert(getOrderChecks() == 1, "table_fail_action order check mismatch");
            Assert(getDescriptorChecks() == 1, "table_fail_action descriptor check mismatch");

            // Case 4: helper rejects invalid table row kind before later registrations.
            reset();
            setMode(MODE_INVALID_ROW_KIND);
            Assert(onStartup() == REGISTRATION_FAILED, "table_invalid_kind startup result mismatch");
            Assert(useHubUi() == 0, "table_invalid_kind use_hub_ui mismatch");
            Assert(lastFailure() == EMC_ERR_INVALID_ARGUMENT, "table_invalid_kind last_failure mismatch");
            AssertCounts(getModCalls, getBoolCalls, getKeybindCalls, getIntCalls, getFloatCalls, getSelectCalls, getTextCalls, getColorCalls, getActionCalls,
                1, 1, 1, 0, 0, 0, 0, 0, 0, "table_invalid_kind");
            Assert(getOrderChecks() == 1, "table_invalid_kind order check mismatch");

            // Case 5: helper rejects null row descriptor pointer.
            reset();
            setMode(MODE_NULL_ROW_DEF);
            Assert(onStartup() == REGISTRATION_FAILED, "table_null_def startup result mismatch");
            Assert(useHubUi() == 0, "table_null_def use_hub_ui mismatch");
            Assert(lastFailure() == EMC_ERR_INVALID_ARGUMENT, "table_null_def last_failure mismatch");
            AssertCounts(getModCalls, getBoolCalls, getKeybindCalls, getIntCalls, getFloatCalls, getSelectCalls, getTextCalls, getColorCalls, getActionCalls,
                1, 1, 1, 0, 0, 0, 0, 0, 0, "table_null_def");
            Assert(getOrderChecks() == 1, "table_null_def order check mismatch");

            // Case 6: V2 int registration uses the new API entry point.
            reset();
            setMode(MODE_USE_INT_V2);
            Assert(onStartup() == ATTACH_SUCCESS, "table_int_v2_success startup result mismatch");
            Assert(useHubUi() == 1, "table_int_v2_success use_hub_ui mismatch");
            Assert(lastFailure() == EMC_OK, "table_int_v2_success last_failure mismatch");
            AssertCounts(getModCalls, getBoolCalls, getKeybindCalls, getIntCalls, getFloatCalls, getSelectCalls, getTextCalls, getColorCalls, getActionCalls,
                1, 1, 1, 0, 1, 1, 1, 1, 1, "table_int_v2_success");
            Assert(getIntV2Calls() == 1, "table_int_v2_success int_v2 count mismatch");
            Assert(getOrderChecks() == 1, "table_int_v2_success order check mismatch");
            Assert(getDescriptorChecks() == 1, "table_int_v2_success descriptor check mismatch");

            // Case 7: V2 rows fail cleanly against a legacy host API size.
            reset();
            setMode(MODE_USE_INT_V2_LEGACY_API);
            Assert(onStartup() == REGISTRATION_FAILED, "table_int_v2_legacy startup result mismatch");
            Assert(useHubUi() == 0, "table_int_v2_legacy use_hub_ui mismatch");
            Assert(lastFailure() == 3, "table_int_v2_legacy last_failure mismatch");
            AssertCounts(getModCalls, getBoolCalls, getKeybindCalls, getIntCalls, getFloatCalls, getSelectCalls, getTextCalls, getColorCalls, getActionCalls,
                1, 1, 1, 0, 0, 0, 0, 0, 0, "table_int_v2_legacy");
            Assert(getIntV2Calls() == 0, "table_int_v2_legacy int_v2 should not be called");
            Assert(getOrderChecks() == 1, "table_int_v2_legacy order check mismatch");

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
$result = [ModHubClientPhase8TableHarness]::Run($DllPath, $KenshiPath)
Write-Host $result
