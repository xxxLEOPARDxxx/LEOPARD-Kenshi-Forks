param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [string]$KenshiPath = ""
)

$ErrorActionPreference = 'Stop'

$code = @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public static class HubPhase3BoolKeybindHarness
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetApiRaw(uint requestedVersion, uint callerApiSize, IntPtr outApi, IntPtr outApiSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterModRaw(IntPtr desc, IntPtr outHandle);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterBoolRaw(IntPtr mod, IntPtr def);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterKeybindRaw(IntPtr mod, IntPtr def);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterActionRaw(IntPtr mod, IntPtr def);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void TestSetRegistrationLockedRaw(int isLocked);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void TestSetRegistryAttachEnabledRaw(int isEnabled);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void MenuSetHubEnabledRaw(int isEnabled);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void MenuOpenRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void MenuSaveRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void MenuCloseRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiSetPendingBoolRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, int value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiBeginKeybindCaptureRaw(IntPtr nsId, IntPtr modId, IntPtr settingId);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiApplyCapturedKeycodeRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, int keycode);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiInvokeActionRowRaw(IntPtr nsId, IntPtr modId, IntPtr settingId);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void CommitGetSummaryRaw(out uint attempted, out uint succeeded, out uint failed, out uint skipped, out int skipReason);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetBoolCb(IntPtr userData, IntPtr outValue);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int SetBoolCb(IntPtr userData, int value, IntPtr errBuf, uint errBufSize);

    [StructLayout(LayoutKind.Sequential)]
    public struct EMC_KeybindValueV1
    {
        public int keycode;
        public uint modifiers;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetKeybindCb(IntPtr userData, IntPtr outValue);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int SetKeybindCb(IntPtr userData, EMC_KeybindValueV1 value, IntPtr errBuf, uint errBufSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int ActionCb(IntPtr userData, IntPtr errBuf, uint errBufSize);

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
    public struct EMC_KeybindSettingDefV1
    {
        public IntPtr setting_id;
        public IntPtr label;
        public IntPtr description;
        public IntPtr user_data;
        public IntPtr get_value;
        public IntPtr set_value;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct EMC_ActionRowDefV1
    {
        public IntPtr setting_id;
        public IntPtr label;
        public IntPtr description;
        public IntPtr user_data;
        public uint action_flags;
        public IntPtr on_action;
    }

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
    private const int EMC_ERR_INVALID_ARGUMENT = 1;
    private const int HUB_COMMIT_SKIP_REASON_NONE = 0;
    private const int HUB_COMMIT_SKIP_REASON_KEYBIND_CAPTURE_ACTIVE = 1;
    private const uint DONT_RESOLVE_DLL_REFERENCES = 0x00000001u;

    private static readonly List<Delegate> CallbackRoots = new List<Delegate>();
    private static readonly List<string> CommitSetOrder = new List<string>();

    private static int CurrentBoolValue = 1;
    private static EMC_KeybindValueV1 CurrentKeybindValue = new EMC_KeybindValueV1 { keycode = 30, modifiers = 7 };
    private static int BoolGetCount = 0;
    private static int BoolSetCount = 0;
    private static int KeybindGetCount = 0;
    private static int KeybindSetCount = 0;
    private static int ActionCount = 0;

    private static void Assert(bool condition, string message)
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
            throw new Exception(message + " (expected=" + expected + ", actual=" + actual + ")");
        }
    }

    private static IntPtr AllocAnsi(List<IntPtr> allocations, string value)
    {
        IntPtr ptr = Marshal.StringToHGlobalAnsi(value);
        allocations.Add(ptr);
        return ptr;
    }

    private static IntPtr AllocStruct<T>(List<IntPtr> allocations, T value)
    {
        IntPtr ptr = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(T)));
        Marshal.StructureToPtr(value, ptr, false);
        allocations.Add(ptr);
        return ptr;
    }

    private static IntPtr ReadFnPtr(IntPtr apiPtr, int offset)
    {
        return Marshal.ReadIntPtr(apiPtr, offset);
    }

    private static int BoolGet(IntPtr userData, IntPtr outValue)
    {
        BoolGetCount += 1;
        Marshal.WriteInt32(outValue, CurrentBoolValue);
        return EMC_OK;
    }

    private static int BoolSet(IntPtr userData, int value, IntPtr errBuf, uint errBufSize)
    {
        BoolSetCount += 1;
        CommitSetOrder.Add("bool");
        CurrentBoolValue = value;
        return EMC_OK;
    }

    private static int KeybindGet(IntPtr userData, IntPtr outValue)
    {
        KeybindGetCount += 1;
        Marshal.StructureToPtr(CurrentKeybindValue, outValue, false);
        return EMC_OK;
    }

    private static int KeybindSet(IntPtr userData, EMC_KeybindValueV1 value, IntPtr errBuf, uint errBufSize)
    {
        KeybindSetCount += 1;
        CommitSetOrder.Add("keybind");
        CurrentKeybindValue = value;
        return EMC_OK;
    }

    private static int Action(IntPtr userData, IntPtr errBuf, uint errBufSize)
    {
        ActionCount += 1;
        CurrentBoolValue = CurrentBoolValue == 0 ? 1 : 0;
        return EMC_OK;
    }

    private static void ReadSummary(CommitGetSummaryRaw readSummary, out uint attempted, out uint succeeded, out uint failed, out uint skipped, out int skipReason)
    {
        readSummary(out attempted, out succeeded, out failed, out skipped, out skipReason);
    }

    public static string Run(string dllPath, string kenshiPath)
    {
        List<IntPtr> allocations = new List<IntPtr>();
        IntPtr module = IntPtr.Zero;
        IntPtr outApi = IntPtr.Zero;
        IntPtr outApiSize = IntPtr.Zero;
        TestSetRegistrationLockedRaw setRegistrationLocked = null;
        TestSetRegistryAttachEnabledRaw setRegistryAttachEnabled = null;
        MenuSetHubEnabledRaw setHubEnabled = null;
        MenuOpenRaw openOptions = null;
        MenuSaveRaw saveOptions = null;
        MenuCloseRaw closeOptions = null;

        try
        {
            CommitSetOrder.Clear();
            CurrentBoolValue = 1;
            CurrentKeybindValue = new EMC_KeybindValueV1 { keycode = 30, modifiers = 7u };
            BoolGetCount = 0;
            BoolSetCount = 0;
            KeybindGetCount = 0;
            KeybindSetCount = 0;
            ActionCount = 0;

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

            IntPtr getApiProc = GetProcAddress(module, "EMC_ModHub_GetApi");
            Assert(getApiProc != IntPtr.Zero, "Missing EMC_ModHub_GetApi export");
            GetApiRaw getApi = (GetApiRaw)Marshal.GetDelegateForFunctionPointer(getApiProc, typeof(GetApiRaw));

            IntPtr setLockedProc = GetProcAddress(module, "EMC_ModHub_Test_SetRegistrationLocked");
            Assert(setLockedProc != IntPtr.Zero, "Missing EMC_ModHub_Test_SetRegistrationLocked export");
            setRegistrationLocked = (TestSetRegistrationLockedRaw)Marshal.GetDelegateForFunctionPointer(
                setLockedProc, typeof(TestSetRegistrationLockedRaw));

            IntPtr setAttachProc = GetProcAddress(module, "EMC_ModHub_Test_SetRegistryAttachEnabled");
            Assert(setAttachProc != IntPtr.Zero, "Missing EMC_ModHub_Test_SetRegistryAttachEnabled export");
            setRegistryAttachEnabled = (TestSetRegistryAttachEnabledRaw)Marshal.GetDelegateForFunctionPointer(
                setAttachProc, typeof(TestSetRegistryAttachEnabledRaw));

            IntPtr setHubEnabledProc = GetProcAddress(module, "EMC_ModHub_Test_Menu_SetHubEnabled");
            IntPtr openOptionsProc = GetProcAddress(module, "EMC_ModHub_Test_Menu_OpenOptionsWindow");
            IntPtr saveOptionsProc = GetProcAddress(module, "EMC_ModHub_Test_Menu_SaveOptionsWindow");
            IntPtr closeOptionsProc = GetProcAddress(module, "EMC_ModHub_Test_Menu_CloseOptionsWindow");
            IntPtr setPendingBoolProc = GetProcAddress(module, "EMC_ModHub_Test_UI_SetPendingBool");
            IntPtr beginCaptureProc = GetProcAddress(module, "EMC_ModHub_Test_UI_BeginKeybindCapture");
            IntPtr applyCaptureProc = GetProcAddress(module, "EMC_ModHub_Test_UI_ApplyCapturedKeycode");
            IntPtr invokeActionProc = GetProcAddress(module, "EMC_ModHub_Test_UI_InvokeActionRow");
            IntPtr getSummaryProc = GetProcAddress(module, "EMC_ModHub_Test_Commit_GetLastSummary");

            Assert(setHubEnabledProc != IntPtr.Zero, "Missing EMC_ModHub_Test_Menu_SetHubEnabled export");
            Assert(openOptionsProc != IntPtr.Zero, "Missing EMC_ModHub_Test_Menu_OpenOptionsWindow export");
            Assert(saveOptionsProc != IntPtr.Zero, "Missing EMC_ModHub_Test_Menu_SaveOptionsWindow export");
            Assert(closeOptionsProc != IntPtr.Zero, "Missing EMC_ModHub_Test_Menu_CloseOptionsWindow export");
            Assert(setPendingBoolProc != IntPtr.Zero, "Missing EMC_ModHub_Test_UI_SetPendingBool export");
            Assert(beginCaptureProc != IntPtr.Zero, "Missing EMC_ModHub_Test_UI_BeginKeybindCapture export");
            Assert(applyCaptureProc != IntPtr.Zero, "Missing EMC_ModHub_Test_UI_ApplyCapturedKeycode export");
            Assert(invokeActionProc != IntPtr.Zero, "Missing EMC_ModHub_Test_UI_InvokeActionRow export");
            Assert(getSummaryProc != IntPtr.Zero, "Missing EMC_ModHub_Test_Commit_GetLastSummary export");

            setHubEnabled = (MenuSetHubEnabledRaw)Marshal.GetDelegateForFunctionPointer(setHubEnabledProc, typeof(MenuSetHubEnabledRaw));
            openOptions = (MenuOpenRaw)Marshal.GetDelegateForFunctionPointer(openOptionsProc, typeof(MenuOpenRaw));
            saveOptions = (MenuSaveRaw)Marshal.GetDelegateForFunctionPointer(saveOptionsProc, typeof(MenuSaveRaw));
            closeOptions = (MenuCloseRaw)Marshal.GetDelegateForFunctionPointer(closeOptionsProc, typeof(MenuCloseRaw));
            UiSetPendingBoolRaw setPendingBool = (UiSetPendingBoolRaw)Marshal.GetDelegateForFunctionPointer(setPendingBoolProc, typeof(UiSetPendingBoolRaw));
            UiBeginKeybindCaptureRaw beginCapture = (UiBeginKeybindCaptureRaw)Marshal.GetDelegateForFunctionPointer(beginCaptureProc, typeof(UiBeginKeybindCaptureRaw));
            UiApplyCapturedKeycodeRaw applyCapture = (UiApplyCapturedKeycodeRaw)Marshal.GetDelegateForFunctionPointer(applyCaptureProc, typeof(UiApplyCapturedKeycodeRaw));
            UiInvokeActionRowRaw invokeAction = (UiInvokeActionRowRaw)Marshal.GetDelegateForFunctionPointer(invokeActionProc, typeof(UiInvokeActionRowRaw));
            CommitGetSummaryRaw getSummary = (CommitGetSummaryRaw)Marshal.GetDelegateForFunctionPointer(getSummaryProc, typeof(CommitGetSummaryRaw));

            setRegistrationLocked(0);
            setRegistryAttachEnabled(-1);
            setHubEnabled(1);

            outApi = Marshal.AllocHGlobal(IntPtr.Size);
            outApiSize = Marshal.AllocHGlobal(4);
            Marshal.WriteIntPtr(outApi, IntPtr.Zero);
            Marshal.WriteInt32(outApiSize, 0);

            int r = getApi(1u, 56u, outApi, outApiSize);
            ExpectResult(r, EMC_OK, "GetApi failed");

            IntPtr apiPtr = Marshal.ReadIntPtr(outApi);
            Assert(apiPtr != IntPtr.Zero, "GetApi returned null api pointer");

            RegisterModRaw registerMod = (RegisterModRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, 8), typeof(RegisterModRaw));
            RegisterBoolRaw registerBool = (RegisterBoolRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, 16), typeof(RegisterBoolRaw));
            RegisterKeybindRaw registerKeybind = (RegisterKeybindRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, 24), typeof(RegisterKeybindRaw));
            RegisterActionRaw registerAction = (RegisterActionRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, 48), typeof(RegisterActionRaw));

            GetBoolCb getBool = BoolGet;
            SetBoolCb setBool = BoolSet;
            GetKeybindCb getKeybind = KeybindGet;
            SetKeybindCb setKeybind = KeybindSet;
            ActionCb action = Action;

            CallbackRoots.Add(getBool);
            CallbackRoots.Add(setBool);
            CallbackRoots.Add(getKeybind);
            CallbackRoots.Add(setKeybind);
            CallbackRoots.Add(action);

            IntPtr getBoolPtr = Marshal.GetFunctionPointerForDelegate(getBool);
            IntPtr setBoolPtr = Marshal.GetFunctionPointerForDelegate(setBool);
            IntPtr getKeybindPtr = Marshal.GetFunctionPointerForDelegate(getKeybind);
            IntPtr setKeybindPtr = Marshal.GetFunctionPointerForDelegate(setKeybind);
            IntPtr actionPtr = Marshal.GetFunctionPointerForDelegate(action);

            IntPtr nsId = AllocAnsi(allocations, "emkej.qol");
            IntPtr nsDisplay = AllocAnsi(allocations, "QoL");
            IntPtr modId = AllocAnsi(allocations, "phase3_test_mod");
            IntPtr modDisplay = AllocAnsi(allocations, "Phase3 Test Mod");
            IntPtr boolSettingId = AllocAnsi(allocations, "enabled");
            IntPtr boolLabel = AllocAnsi(allocations, "Enabled");
            IntPtr boolDesc = AllocAnsi(allocations, "Enable feature");
            IntPtr keybindSettingId = AllocAnsi(allocations, "toggle_hotkey");
            IntPtr keybindLabel = AllocAnsi(allocations, "Toggle hotkey");
            IntPtr keybindDesc = AllocAnsi(allocations, "Toggle with key");
            IntPtr actionSettingId = AllocAnsi(allocations, "refresh_action");
            IntPtr actionLabel = AllocAnsi(allocations, "Refresh");
            IntPtr actionDesc = AllocAnsi(allocations, "Refresh values");

            EMC_ModDescriptorV1 modDesc = new EMC_ModDescriptorV1
            {
                namespace_id = nsId,
                namespace_display_name = nsDisplay,
                mod_id = modId,
                mod_display_name = modDisplay,
                mod_user_data = IntPtr.Zero
            };
            IntPtr modDescPtr = AllocStruct(allocations, modDesc);

            EMC_BoolSettingDefV1 boolDef = new EMC_BoolSettingDefV1
            {
                setting_id = boolSettingId,
                label = boolLabel,
                description = boolDesc,
                user_data = new IntPtr(1001),
                get_value = getBoolPtr,
                set_value = setBoolPtr
            };
            IntPtr boolDefPtr = AllocStruct(allocations, boolDef);

            EMC_KeybindSettingDefV1 keybindDef = new EMC_KeybindSettingDefV1
            {
                setting_id = keybindSettingId,
                label = keybindLabel,
                description = keybindDesc,
                user_data = new IntPtr(2001),
                get_value = getKeybindPtr,
                set_value = setKeybindPtr
            };
            IntPtr keybindDefPtr = AllocStruct(allocations, keybindDef);

            EMC_ActionRowDefV1 actionDef = new EMC_ActionRowDefV1
            {
                setting_id = actionSettingId,
                label = actionLabel,
                description = actionDesc,
                user_data = new IntPtr(3001),
                action_flags = 0u,
                on_action = actionPtr
            };
            IntPtr actionDefPtr = AllocStruct(allocations, actionDef);

            IntPtr outHandle = Marshal.AllocHGlobal(IntPtr.Size);
            allocations.Add(outHandle);
            Marshal.WriteIntPtr(outHandle, IntPtr.Zero);

            r = registerMod(modDescPtr, outHandle);
            ExpectResult(r, EMC_OK, "register_mod failed");
            IntPtr modHandle = Marshal.ReadIntPtr(outHandle);
            Assert(modHandle != IntPtr.Zero, "register_mod returned null handle");

            r = registerBool(modHandle, boolDefPtr);
            ExpectResult(r, EMC_OK, "register_bool failed");
            r = registerKeybind(modHandle, keybindDefPtr);
            ExpectResult(r, EMC_OK, "register_keybind failed");
            r = registerAction(modHandle, actionDefPtr);
            ExpectResult(r, EMC_OK, "register_action_row failed");

            openOptions();
            Assert(BoolGetCount == 1, "Expected one initial bool get on options open");
            Assert(KeybindGetCount == 1, "Expected one initial keybind get on options open");

            saveOptions();
            uint attempted;
            uint succeeded;
            uint failed;
            uint skipped;
            int skipReason;
            ReadSummary(getSummary, out attempted, out succeeded, out failed, out skipped, out skipReason);
            Assert(attempted == 0u && succeeded == 0u && failed == 0u && skipped == 0u, "Expected empty summary when nothing is dirty");
            Assert(skipReason == HUB_COMMIT_SKIP_REASON_NONE, "Expected skip reason NONE when nothing is dirty");
            Assert(BoolSetCount == 0 && KeybindSetCount == 0, "Save with no dirty rows must not call set callbacks");

            r = setPendingBool(nsId, modId, boolSettingId, 0);
            ExpectResult(r, EMC_OK, "set_pending_bool failed");
            r = beginCapture(nsId, modId, keybindSettingId);
            ExpectResult(r, EMC_OK, "begin_keybind_capture failed");

            saveOptions();
            ReadSummary(getSummary, out attempted, out succeeded, out failed, out skipped, out skipReason);
            Assert(attempted == 0u && succeeded == 0u && failed == 0u && skipped == 1u, "Capture-active save must skip commit");
            Assert(skipReason == HUB_COMMIT_SKIP_REASON_KEYBIND_CAPTURE_ACTIVE, "Capture-active save must set keybind capture skip reason");
            Assert(BoolSetCount == 0 && KeybindSetCount == 0, "Skipped commit must not call set callbacks");

            r = applyCapture(nsId, modId, keybindSettingId, 44);
            ExpectResult(r, EMC_OK, "apply_captured_keycode failed");

            saveOptions();
            ReadSummary(getSummary, out attempted, out succeeded, out failed, out skipped, out skipReason);
            Assert(attempted == 2u && succeeded == 2u && failed == 0u && skipped == 0u, "Dirty bool+keybind save must apply two settings");
            Assert(skipReason == HUB_COMMIT_SKIP_REASON_NONE, "Expected skip reason NONE after successful commit");
            Assert(BoolSetCount == 1 && KeybindSetCount == 1, "Expected one bool and one keybind set call");
            Assert(CommitSetOrder.Count >= 2, "Expected commit set order to contain two entries");
            Assert(CommitSetOrder[0] == "bool" && CommitSetOrder[1] == "keybind", "Commit set order must follow registration order");
            Assert(CurrentBoolValue == 0, "Bool value did not commit expected pending value");
            Assert(CurrentKeybindValue.keycode == 44 && CurrentKeybindValue.modifiers == 0u, "Keybind value did not commit expected pending value");

            r = beginCapture(nsId, modId, keybindSettingId);
            ExpectResult(r, EMC_OK, "begin_keybind_capture before action failed");
            r = invokeAction(nsId, modId, actionSettingId);
            ExpectResult(r, EMC_OK, "invoke_action_row should work during active capture");
            Assert(ActionCount == 1, "Expected exactly one action callback invocation");

            saveOptions();
            ReadSummary(getSummary, out attempted, out succeeded, out failed, out skipped, out skipReason);
            Assert(skipped == 1u && skipReason == HUB_COMMIT_SKIP_REASON_KEYBIND_CAPTURE_ACTIVE, "Save after action during capture should still skip commit");

            r = applyCapture(nsId, modId, keybindSettingId, 55);
            ExpectResult(r, EMC_OK, "apply_captured_keycode second capture failed");
            saveOptions();
            ReadSummary(getSummary, out attempted, out succeeded, out failed, out skipped, out skipReason);
            Assert(attempted == 1u && succeeded == 1u && failed == 0u && skipped == 0u, "Expected single keybind commit after second capture");
            Assert(CurrentKeybindValue.keycode == 55 && CurrentKeybindValue.modifiers == 0u, "Second keybind capture did not commit expected value");

            closeOptions();
            return "PASS: phase3 bool/keybind commit matrix completed";
        }
        finally
        {
            if (setRegistrationLocked != null)
            {
                setRegistrationLocked(0);
            }

            if (setRegistryAttachEnabled != null)
            {
                setRegistryAttachEnabled(-1);
            }

            if (setHubEnabled != null)
            {
                setHubEnabled(1);
            }

            if (closeOptions != null)
            {
                closeOptions();
            }

            for (int i = allocations.Count - 1; i >= 0; --i)
            {
                Marshal.FreeHGlobal(allocations[i]);
            }

            if (outApiSize != IntPtr.Zero)
            {
                Marshal.FreeHGlobal(outApiSize);
            }

            if (outApi != IntPtr.Zero)
            {
                Marshal.FreeHGlobal(outApi);
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
$resolvedKenshiPath = $KenshiPath
if ([string]::IsNullOrWhiteSpace($resolvedKenshiPath)) {
    $resolvedKenshiPath = $env:KENSHI_PATH
}
if ([string]::IsNullOrWhiteSpace($resolvedKenshiPath) -and (Test-Path "H:\SteamLibrary\steamapps\common\Kenshi")) {
    $resolvedKenshiPath = "H:\SteamLibrary\steamapps\common\Kenshi"
}

$result = [HubPhase3BoolKeybindHarness]::Run($DllPath, $resolvedKenshiPath)
Write-Host $result
