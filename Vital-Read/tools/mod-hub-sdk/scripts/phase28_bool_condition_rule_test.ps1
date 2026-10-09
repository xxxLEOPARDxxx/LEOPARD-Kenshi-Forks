param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [string]$KenshiPath = ""
)

$ErrorActionPreference = "Stop"

$code = @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public static class Phase28BoolConditionRuleHarness
{
    [StructLayout(LayoutKind.Sequential)]
    public struct EMC_ModDescriptorV1
    {
        public IntPtr namespace_id;
        public IntPtr namespace_display_name;
        public IntPtr mod_id;
        public IntPtr mod_display_name;
        public IntPtr mod_user_data;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct EMC_BoolSettingDefV1
    {
        public IntPtr setting_id;
        public IntPtr label;
        public IntPtr description;
        public IntPtr user_data;
        public IntPtr get_value;
        public IntPtr set_value;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct EMC_BoolConditionRuleDefV1
    {
        public IntPtr target_setting_id;
        public IntPtr controller_setting_id;
        public uint effect;
        public int expected_bool_value;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetApiRaw(uint requestedVersion, uint callerApiSize, IntPtr outApi, IntPtr outApiSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterModRaw(IntPtr modDesc, IntPtr outHandle);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterBoolRaw(IntPtr modHandle, IntPtr boolDef);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterBoolConditionRuleRaw(IntPtr modHandle, IntPtr ruleDef);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void DummyResetRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void DummySetModeRaw(int mode);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int DummyOnStartupRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int DummyUseHubUiRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int DummyLastFailureRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int DummyCountRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiSetPendingBoolRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, int value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiGetConditionStateRaw(
        IntPtr nsId,
        IntPtr modId,
        IntPtr settingId,
        out int visible,
        out int enabled);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int CountSettingsForModRaw(IntPtr nsId, IntPtr modId);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ToggleRaw(int value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void VoidRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetBoolCb(IntPtr userData, IntPtr outValue);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int SetBoolCb(IntPtr userData, int value, IntPtr errBuf, uint errBufSize);

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
    private const uint HUB_API_V1 = 1u;
    private const uint HUB_API_V1_SIZE = 160u;
    private const int REGISTER_MOD_OFFSET = 8;
    private const int REGISTER_BOOL_OFFSET = 16;
    private const int REGISTER_BOOL_CONDITION_RULE_OFFSET = 152;
    private const int EMC_OK = 0;
    private const int EMC_ERR_API_SIZE_MISMATCH = 3;
    private const int ATTACH_SUCCESS = 0;
    private const int REGISTRATION_FAILED = 2;
    private const int MODE_BOOL_CONDITION_RULE = 20;
    private const int MODE_BOOL_CONDITION_RULE_LEGACY_API = 21;
    private const uint EFFECT_HIDE = 0u;
    private const uint EFFECT_DISABLE = 1u;

    private static readonly IntPtr ControllerUserData = new IntPtr(1);
    private static readonly IntPtr HiddenUserData = new IntPtr(2);
    private static readonly IntPtr DisabledUserData = new IntPtr(3);

    private static readonly List<Delegate> CallbackRoots = new List<Delegate>();

    private static int ControllerValue = 0;
    private static int HiddenValue = 1;
    private static int DisabledValue = 1;

    private static IntPtr AllocAnsi(List<IntPtr> allocations, string value)
    {
        IntPtr ptr = Marshal.StringToHGlobalAnsi(value);
        allocations.Add(ptr);
        return ptr;
    }

    private static IntPtr AllocStruct<T>(List<IntPtr> allocations, T value) where T : struct
    {
        IntPtr ptr = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(T)));
        allocations.Add(ptr);
        Marshal.StructureToPtr(value, ptr, false);
        return ptr;
    }

    private static IntPtr ReadFnPtr(IntPtr apiPtr, int offset)
    {
        return Marshal.ReadIntPtr(IntPtr.Add(apiPtr, offset));
    }

    private static T Bind<T>(IntPtr module, string exportName)
    {
        IntPtr proc = GetProcAddress(module, exportName);
        if (proc == IntPtr.Zero)
        {
            throw new Exception("Missing export: " + exportName);
        }

        return (T)(object)Marshal.GetDelegateForFunctionPointer(proc, typeof(T));
    }

    private static void Expect(bool condition, string message)
    {
        if (!condition)
        {
            throw new Exception(message);
        }
    }

    private static void ExpectResult(int actual, int expected, string message)
    {
        if (actual != expected)
        {
            throw new Exception(message + " (expected " + expected + ", got " + actual + ")");
        }
    }

    private static int GetBool(IntPtr userData, IntPtr outValue)
    {
        if (outValue == IntPtr.Zero)
        {
            return 1;
        }

        int value = 0;
        if (userData == ControllerUserData)
        {
            value = ControllerValue;
        }
        else if (userData == HiddenUserData)
        {
            value = HiddenValue;
        }
        else if (userData == DisabledUserData)
        {
            value = DisabledValue;
        }
        else
        {
            return 1;
        }

        Marshal.WriteInt32(outValue, value);
        return EMC_OK;
    }

    private static int SetBool(IntPtr userData, int value, IntPtr errBuf, uint errBufSize)
    {
        if (value != 0 && value != 1)
        {
            return 1;
        }

        if (userData == ControllerUserData)
        {
            ControllerValue = value;
            return EMC_OK;
        }

        if (userData == HiddenUserData)
        {
            HiddenValue = value;
            return EMC_OK;
        }

        if (userData == DisabledUserData)
        {
            DisabledValue = value;
            return EMC_OK;
        }

        return 1;
    }

    private static void ResetCallbackState()
    {
        ControllerValue = 0;
        HiddenValue = 1;
        DisabledValue = 1;
    }

    public static string Run(string dllPath, string kenshiPath)
    {
        IntPtr module = IntPtr.Zero;
        VoidRaw closeOptionsWindow = null;
        ToggleRaw setRegistrationLocked = null;
        ToggleRaw setRegistryAttachEnabled = null;
        ToggleRaw setHubEnabled = null;
        List<IntPtr> allocations = new List<IntPtr>();

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
            Expect(module != IntPtr.Zero, "LoadLibrary failed for " + dllPath);

            DummyResetRaw reset = Bind<DummyResetRaw>(module, "EMC_ModHub_Test_DummyConsumer_Reset");
            DummySetModeRaw setMode = Bind<DummySetModeRaw>(module, "EMC_ModHub_Test_DummyConsumer_SetMode");
            DummyOnStartupRaw onStartup = Bind<DummyOnStartupRaw>(module, "EMC_ModHub_Test_DummyConsumer_OnStartup");
            DummyUseHubUiRaw useHubUi = Bind<DummyUseHubUiRaw>(module, "EMC_ModHub_Test_DummyConsumer_UseHubUi");
            DummyLastFailureRaw lastFailure = Bind<DummyLastFailureRaw>(module, "EMC_ModHub_Test_DummyConsumer_LastAttemptFailureResult");
            DummyCountRaw getBoolCount = Bind<DummyCountRaw>(module, "EMC_ModHub_Test_DummyConsumer_GetRegisterBoolCalls");
            DummyCountRaw getRuleCount = Bind<DummyCountRaw>(module, "EMC_ModHub_Test_DummyConsumer_GetRegisterBoolConditionRuleCalls");
            GetApiRaw getApi = Bind<GetApiRaw>(module, "EMC_ModHub_GetApi");
            setRegistrationLocked = Bind<ToggleRaw>(module, "EMC_ModHub_Test_SetRegistrationLocked");
            setRegistryAttachEnabled = Bind<ToggleRaw>(module, "EMC_ModHub_Test_SetRegistryAttachEnabled");
            setHubEnabled = Bind<ToggleRaw>(module, "EMC_ModHub_Test_Menu_SetHubEnabled");
            CountSettingsForModRaw countSettingsForMod = Bind<CountSettingsForModRaw>(module, "EMC_ModHub_Test_UI_CountSettingsForMod");
            UiSetPendingBoolRaw setPendingBool = Bind<UiSetPendingBoolRaw>(module, "EMC_ModHub_Test_UI_SetPendingBool");
            UiGetConditionStateRaw getConditionState = Bind<UiGetConditionStateRaw>(module, "EMC_ModHub_Test_UI_GetBoolConditionState");
            VoidRaw openOptionsWindow = Bind<VoidRaw>(module, "EMC_ModHub_Test_Menu_OpenOptionsWindow");
            closeOptionsWindow = Bind<VoidRaw>(module, "EMC_ModHub_Test_Menu_CloseOptionsWindow");

            closeOptionsWindow();
            setRegistrationLocked(0);
            setRegistryAttachEnabled(1);
            setHubEnabled(1);

            reset();
            setMode(MODE_BOOL_CONDITION_RULE);
            Expect(onStartup() == ATTACH_SUCCESS, "bool_condition_rule startup result mismatch");
            Expect(useHubUi() == 1, "bool_condition_rule use_hub_ui mismatch");
            Expect(lastFailure() == EMC_OK, "bool_condition_rule last_failure mismatch");
            Expect(getBoolCount() == 3, "bool_condition_rule bool registration count mismatch");
            Expect(getRuleCount() == 2, "bool_condition_rule rule registration count mismatch");

            IntPtr outApi = Marshal.AllocHGlobal(IntPtr.Size);
            IntPtr outApiSize = Marshal.AllocHGlobal(4);
            allocations.Add(outApi);
            allocations.Add(outApiSize);
            ExpectResult(getApi(HUB_API_V1, HUB_API_V1_SIZE, outApi, outApiSize), EMC_OK, "bool_condition_rule get_api failed");

            IntPtr apiPtr = Marshal.ReadIntPtr(outApi);
            int apiSize = Marshal.ReadInt32(outApiSize);
            Expect(apiPtr != IntPtr.Zero, "bool_condition_rule api pointer should not be null");
            Expect(apiSize >= (int)HUB_API_V1_SIZE, "bool_condition_rule api size too small");

            RegisterModRaw registerMod = (RegisterModRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, REGISTER_MOD_OFFSET), typeof(RegisterModRaw));
            RegisterBoolRaw registerBool = (RegisterBoolRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, REGISTER_BOOL_OFFSET), typeof(RegisterBoolRaw));
            RegisterBoolConditionRuleRaw registerBoolConditionRule = (RegisterBoolConditionRuleRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, REGISTER_BOOL_CONDITION_RULE_OFFSET), typeof(RegisterBoolConditionRuleRaw));

            GetBoolCb getBool = new GetBoolCb(GetBool);
            SetBoolCb setBool = new SetBoolCb(SetBool);
            CallbackRoots.Add(getBool);
            CallbackRoots.Add(setBool);
            ResetCallbackState();

            IntPtr nsId = AllocAnsi(allocations, "phase28.bool_condition");
            IntPtr nsDisplayName = AllocAnsi(allocations, "Phase28 Bool Condition");
            IntPtr modId = AllocAnsi(allocations, "phase28_bool_mod");
            IntPtr modDisplayName = AllocAnsi(allocations, "Phase28 Bool Condition Mod");
            IntPtr controllerSettingId = AllocAnsi(allocations, "feature_enabled");
            IntPtr hiddenSettingId = AllocAnsi(allocations, "feature_hidden");
            IntPtr disabledSettingId = AllocAnsi(allocations, "feature_disabled");
            IntPtr modUserData = AllocAnsi(allocations, "phase28_user_data");

            EMC_ModDescriptorV1 modDesc = new EMC_ModDescriptorV1
            {
                namespace_id = nsId,
                namespace_display_name = nsDisplayName,
                mod_id = modId,
                mod_display_name = modDisplayName,
                mod_user_data = modUserData
            };

            EMC_BoolSettingDefV1 controllerDef = new EMC_BoolSettingDefV1
            {
                setting_id = controllerSettingId,
                label = AllocAnsi(allocations, "Enabled"),
                description = AllocAnsi(allocations, "Controller row"),
                user_data = ControllerUserData,
                get_value = Marshal.GetFunctionPointerForDelegate(getBool),
                set_value = Marshal.GetFunctionPointerForDelegate(setBool)
            };

            EMC_BoolSettingDefV1 hiddenDef = new EMC_BoolSettingDefV1
            {
                setting_id = hiddenSettingId,
                label = AllocAnsi(allocations, "Hidden feature"),
                description = AllocAnsi(allocations, "Target row hidden while disabled"),
                user_data = HiddenUserData,
                get_value = Marshal.GetFunctionPointerForDelegate(getBool),
                set_value = Marshal.GetFunctionPointerForDelegate(setBool)
            };

            EMC_BoolSettingDefV1 disabledDef = new EMC_BoolSettingDefV1
            {
                setting_id = disabledSettingId,
                label = AllocAnsi(allocations, "Disabled feature"),
                description = AllocAnsi(allocations, "Target row disabled while controller is off"),
                user_data = DisabledUserData,
                get_value = Marshal.GetFunctionPointerForDelegate(getBool),
                set_value = Marshal.GetFunctionPointerForDelegate(setBool)
            };

            EMC_BoolConditionRuleDefV1 hideRule = new EMC_BoolConditionRuleDefV1
            {
                target_setting_id = hiddenSettingId,
                controller_setting_id = controllerSettingId,
                effect = EFFECT_HIDE,
                expected_bool_value = 0
            };

            EMC_BoolConditionRuleDefV1 disableRule = new EMC_BoolConditionRuleDefV1
            {
                target_setting_id = disabledSettingId,
                controller_setting_id = controllerSettingId,
                effect = EFFECT_DISABLE,
                expected_bool_value = 0
            };

            IntPtr modDescPtr = AllocStruct(allocations, modDesc);
            IntPtr controllerDefPtr = AllocStruct(allocations, controllerDef);
            IntPtr hiddenDefPtr = AllocStruct(allocations, hiddenDef);
            IntPtr disabledDefPtr = AllocStruct(allocations, disabledDef);
            IntPtr hideRulePtr = AllocStruct(allocations, hideRule);
            IntPtr disableRulePtr = AllocStruct(allocations, disableRule);
            IntPtr outHandle = Marshal.AllocHGlobal(IntPtr.Size);
            allocations.Add(outHandle);

            ExpectResult(registerMod(modDescPtr, outHandle), EMC_OK, "bool_condition_rule register_mod failed");
            IntPtr modHandle = Marshal.ReadIntPtr(outHandle);
            Expect(modHandle != IntPtr.Zero, "bool_condition_rule mod handle should not be null");
            ExpectResult(registerBool(modHandle, controllerDefPtr), EMC_OK, "bool_condition_rule register controller bool failed");
            ExpectResult(registerBool(modHandle, hiddenDefPtr), EMC_OK, "bool_condition_rule register hidden bool failed");
            ExpectResult(registerBool(modHandle, disabledDefPtr), EMC_OK, "bool_condition_rule register disabled bool failed");
            ExpectResult(registerBoolConditionRule(modHandle, hideRulePtr), EMC_OK, "bool_condition_rule register hide rule failed");
            ExpectResult(registerBoolConditionRule(modHandle, disableRulePtr), EMC_OK, "bool_condition_rule register disable rule failed");

            openOptionsWindow();

            Expect(countSettingsForMod(nsId, modId) == 3, "bool_condition_rule setting count mismatch");

            int visible;
            int enabled;
            ExpectResult(
                getConditionState(nsId, modId, hiddenSettingId, out visible, out enabled),
                EMC_OK,
                "bool_condition_rule hidden state query failed");
            Expect(visible == 0, "bool_condition_rule hidden row should start hidden");
            Expect(enabled == 1, "bool_condition_rule hidden row should stay enabled");
            ExpectResult(
                getConditionState(nsId, modId, disabledSettingId, out visible, out enabled),
                EMC_OK,
                "bool_condition_rule disabled state query failed");
            Expect(visible == 1, "bool_condition_rule disabled row should stay visible");
            Expect(enabled == 0, "bool_condition_rule disabled row should start disabled");

            ExpectResult(
                setPendingBool(nsId, modId, controllerSettingId, 1),
                EMC_OK,
                "bool_condition_rule set pending bool mismatch");
            ExpectResult(
                getConditionState(nsId, modId, hiddenSettingId, out visible, out enabled),
                EMC_OK,
                "bool_condition_rule hidden state query after toggle failed");
            Expect(visible == 1 && enabled == 1, "bool_condition_rule hidden row should become visible and enabled");
            ExpectResult(
                getConditionState(nsId, modId, disabledSettingId, out visible, out enabled),
                EMC_OK,
                "bool_condition_rule disabled state query after toggle failed");
            Expect(visible == 1 && enabled == 1, "bool_condition_rule disabled row should become visible and enabled");

            closeOptionsWindow();

            reset();
            setMode(MODE_BOOL_CONDITION_RULE_LEGACY_API);
            Expect(onStartup() == REGISTRATION_FAILED, "bool_condition_rule legacy startup result mismatch");
            Expect(useHubUi() == 0, "bool_condition_rule legacy use_hub_ui mismatch");
            Expect(lastFailure() == EMC_ERR_API_SIZE_MISMATCH, "bool_condition_rule legacy failure mismatch");
            Expect(getBoolCount() == 3, "bool_condition_rule legacy bool registration count mismatch");
            Expect(getRuleCount() == 0, "bool_condition_rule legacy rule registration count mismatch");

            return "phase28 bool condition rule harness passed";
        }
        finally
        {
            if (closeOptionsWindow != null)
            {
                closeOptionsWindow();
            }
            if (setRegistrationLocked != null)
            {
                setRegistrationLocked(0);
            }
            if (setRegistryAttachEnabled != null)
            {
                setRegistryAttachEnabled(1);
            }
            if (setHubEnabled != null)
            {
                setHubEnabled(1);
            }

            for (int index = allocations.Count - 1; index >= 0; --index)
            {
                Marshal.FreeHGlobal(allocations[index]);
            }

            if (module != IntPtr.Zero)
            {
                FreeLibrary(module);
            }
        }
    }
}
"@

Add-Type -TypeDefinition $code -Language CSharp
$resolvedDllPath = (Resolve-Path $DllPath).Path
$resolvedKenshiPath = if ($KenshiPath) { (Resolve-Path $KenshiPath).Path } else { "" }
[Phase28BoolConditionRuleHarness]::Run($resolvedDllPath, $resolvedKenshiPath)
