param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [string]$KenshiPath = ""
)

$ErrorActionPreference = "Stop"

$code = @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class HubPhase27HoverHintHarness
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetApiRaw(uint requestedVersion, uint callerApiSize, IntPtr outApi, IntPtr outApiSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterModRaw(IntPtr desc, IntPtr outHandle);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterBoolV2Raw(IntPtr mod, IntPtr def);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterKeybindV2Raw(IntPtr mod, IntPtr def);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterSelectV2Raw(IntPtr mod, IntPtr def);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterTextV2Raw(IntPtr mod, IntPtr def);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterActionV2Raw(IntPtr mod, IntPtr def);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void SetIntRaw(int value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void VoidRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int CountSettingsForModRaw(IntPtr nsId, IntPtr modId);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiSetPendingSelectRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, int value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiGetPendingSelectStateRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, out int outValue);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiSetPendingTextRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, IntPtr text);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiGetPendingTextStateRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, IntPtr outText, uint outTextSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiInvokeActionRowRaw(IntPtr nsId, IntPtr modId, IntPtr settingId);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiGetStringStateRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, IntPtr outText, uint outTextSize);

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
    public delegate int GetSelectCb(IntPtr userData, IntPtr outValue);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int SetSelectCb(IntPtr userData, int value, IntPtr errBuf, uint errBufSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetTextCb(IntPtr userData, IntPtr outValue, uint outValueSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int SetTextCb(IntPtr userData, IntPtr value, IntPtr errBuf, uint errBufSize);

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
    public struct EMC_BoolSettingDefV2
    {
        public IntPtr setting_id;
        public IntPtr label;
        public IntPtr description;
        public IntPtr user_data;
        public IntPtr get_value;
        public IntPtr set_value;
        public IntPtr hover_hint;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct EMC_KeybindSettingDefV2
    {
        public IntPtr setting_id;
        public IntPtr label;
        public IntPtr description;
        public IntPtr user_data;
        public IntPtr get_value;
        public IntPtr set_value;
        public IntPtr hover_hint;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct EMC_SelectOptionV1
    {
        public int value;
        public IntPtr label;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct EMC_SelectSettingDefV2
    {
        public IntPtr setting_id;
        public IntPtr label;
        public IntPtr description;
        public IntPtr user_data;
        public IntPtr options;
        public uint option_count;
        public IntPtr get_value;
        public IntPtr set_value;
        public IntPtr hover_hint;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct EMC_TextSettingDefV2
    {
        public IntPtr setting_id;
        public IntPtr label;
        public IntPtr description;
        public IntPtr user_data;
        public uint max_length;
        public IntPtr get_value;
        public IntPtr set_value;
        public IntPtr hover_hint;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct EMC_ActionRowDefV2
    {
        public IntPtr setting_id;
        public IntPtr label;
        public IntPtr description;
        public IntPtr user_data;
        public uint action_flags;
        public IntPtr on_action;
        public IntPtr hover_hint;
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

    private const uint DONT_RESOLVE_DLL_REFERENCES = 0x00000001u;
    private const int EMC_OK = 0;
    private const int EMC_ERR_INVALID_ARGUMENT = 1;
    private const uint HUB_API_V1 = 1u;
    private const uint HUB_API_V1_SIZE = 160u;
    private const int REGISTER_MOD_OFFSET = 8;
    private const int REGISTER_BOOL_V2_OFFSET = 112;
    private const int REGISTER_KEYBIND_V2_OFFSET = 120;
    private const int REGISTER_SELECT_V2_OFFSET = 128;
    private const int REGISTER_TEXT_V2_OFFSET = 136;
    private const int REGISTER_ACTION_V2_OFFSET = 144;
    private const uint TEXT_MAX_LENGTH = 24u;

    private static readonly List<Delegate> CallbackRoots = new List<Delegate>();

    private static int BoolValue = 1;
    private static EMC_KeybindValueV1 KeybindValue = new EMC_KeybindValueV1 { keycode = 42, modifiers = 0u };
    private static int SelectValue = 1;
    private static string TextValue = "Alpha";
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

    private static IntPtr AllocArray<T>(List<IntPtr> allocations, T[] values)
    {
        int elementSize = Marshal.SizeOf(typeof(T));
        IntPtr ptr = Marshal.AllocHGlobal(elementSize * values.Length);
        allocations.Add(ptr);
        for (int index = 0; index < values.Length; ++index)
        {
            Marshal.StructureToPtr(values[index], IntPtr.Add(ptr, index * elementSize), false);
        }
        return ptr;
    }

    private static IntPtr ReadFnPtr(IntPtr apiPtr, int offset)
    {
        return Marshal.ReadIntPtr(apiPtr, offset);
    }

    private static string ReadAnsiBuffer(IntPtr buffer)
    {
        return Marshal.PtrToStringAnsi(buffer) ?? string.Empty;
    }

    private static void AssertStringExport(
        UiGetStringStateRaw getter,
        IntPtr nsId,
        IntPtr modId,
        IntPtr settingId,
        string expected,
        string context)
    {
        IntPtr buffer = Marshal.AllocHGlobal(256);
        try
        {
            ExpectResult(getter(nsId, modId, settingId, buffer, 256u), EMC_OK, context + " export failed");
            Assert(ReadAnsiBuffer(buffer) == expected, context + " mismatch");
        }
        finally
        {
            Marshal.FreeHGlobal(buffer);
        }
    }

    private static int BoolGet(IntPtr userData, IntPtr outValue)
    {
        Marshal.WriteInt32(outValue, BoolValue);
        return EMC_OK;
    }

    private static int BoolSet(IntPtr userData, int value, IntPtr errBuf, uint errBufSize)
    {
        BoolValue = value != 0 ? 1 : 0;
        return EMC_OK;
    }

    private static int KeybindGet(IntPtr userData, IntPtr outValue)
    {
        Marshal.StructureToPtr(KeybindValue, outValue, false);
        return EMC_OK;
    }

    private static int KeybindSet(IntPtr userData, EMC_KeybindValueV1 value, IntPtr errBuf, uint errBufSize)
    {
        KeybindValue = value;
        return EMC_OK;
    }

    private static int SelectGet(IntPtr userData, IntPtr outValue)
    {
        Marshal.WriteInt32(outValue, SelectValue);
        return EMC_OK;
    }

    private static int SelectSet(IntPtr userData, int value, IntPtr errBuf, uint errBufSize)
    {
        SelectValue = value;
        return EMC_OK;
    }

    private static int TextGet(IntPtr userData, IntPtr outValue, uint outValueSize)
    {
        byte[] bytes = Encoding.ASCII.GetBytes(TextValue);
        if ((uint)bytes.Length + 1u > outValueSize)
        {
            return EMC_ERR_INVALID_ARGUMENT;
        }

        Marshal.Copy(bytes, 0, outValue, bytes.Length);
        Marshal.WriteByte(outValue, bytes.Length, 0);
        return EMC_OK;
    }

    private static int TextSet(IntPtr userData, IntPtr value, IntPtr errBuf, uint errBufSize)
    {
        string next = Marshal.PtrToStringAnsi(value) ?? string.Empty;
        if ((uint)next.Length > TEXT_MAX_LENGTH)
        {
            return EMC_ERR_INVALID_ARGUMENT;
        }

        TextValue = next;
        return EMC_OK;
    }

    private static int ActionInvoke(IntPtr userData, IntPtr errBuf, uint errBufSize)
    {
        ActionCount += 1;
        return EMC_OK;
    }

    public static string Run(string dllPath, string kenshiPath)
    {
        IntPtr module = IntPtr.Zero;
        List<IntPtr> allocations = new List<IntPtr>();
        SetIntRaw setRegistrationLocked = null;
        SetIntRaw setRegistryAttachEnabled = null;
        SetIntRaw setHubEnabled = null;
        VoidRaw openOptionsWindow = null;
        VoidRaw closeOptionsWindow = null;

        try
        {
            CallbackRoots.Clear();
            BoolValue = 1;
            KeybindValue = new EMC_KeybindValueV1 { keycode = 42, modifiers = 0u };
            SelectValue = 1;
            TextValue = "Alpha";
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

            GetApiRaw getApi = (GetApiRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_GetApi"),
                typeof(GetApiRaw));

            setRegistrationLocked = (SetIntRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_SetRegistrationLocked"), typeof(SetIntRaw));
            setRegistryAttachEnabled = (SetIntRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_SetRegistryAttachEnabled"), typeof(SetIntRaw));
            setHubEnabled = (SetIntRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_Menu_SetHubEnabled"), typeof(SetIntRaw));
            openOptionsWindow = (VoidRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_Menu_OpenOptionsWindow"), typeof(VoidRaw));
            closeOptionsWindow = (VoidRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_Menu_CloseOptionsWindow"), typeof(VoidRaw));

            CountSettingsForModRaw countSettingsForMod = (CountSettingsForModRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_UI_CountSettingsForMod"), typeof(CountSettingsForModRaw));
            UiSetPendingSelectRaw setPendingSelect = (UiSetPendingSelectRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_UI_SetPendingSelect"), typeof(UiSetPendingSelectRaw));
            UiGetPendingSelectStateRaw getPendingSelect = (UiGetPendingSelectStateRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_UI_GetPendingSelectState"), typeof(UiGetPendingSelectStateRaw));
            UiSetPendingTextRaw setPendingText = (UiSetPendingTextRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_UI_SetPendingText"), typeof(UiSetPendingTextRaw));
            UiGetPendingTextStateRaw getPendingText = (UiGetPendingTextStateRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_UI_GetPendingTextState"), typeof(UiGetPendingTextStateRaw));
            UiInvokeActionRowRaw invokeAction = (UiInvokeActionRowRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_UI_InvokeActionRow"), typeof(UiInvokeActionRowRaw));
            UiGetStringStateRaw getHoverHint = (UiGetStringStateRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_UI_GetHoverHint"), typeof(UiGetStringStateRaw));
            UiGetStringStateRaw getDescription = (UiGetStringStateRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_UI_GetDescription"), typeof(UiGetStringStateRaw));

            Assert(getApi != null, "Missing EMC_ModHub_GetApi export");

            setRegistrationLocked(0);
            setRegistryAttachEnabled(1);
            setHubEnabled(1);
            closeOptionsWindow();

            IntPtr apiPtrStorage = Marshal.AllocHGlobal(IntPtr.Size);
            IntPtr apiSizeStorage = Marshal.AllocHGlobal(sizeof(uint));
            allocations.Add(apiPtrStorage);
            allocations.Add(apiSizeStorage);

            ExpectResult(getApi(HUB_API_V1, HUB_API_V1_SIZE, apiPtrStorage, apiSizeStorage), EMC_OK, "phase27 get_api failed");
            IntPtr apiPtr = Marshal.ReadIntPtr(apiPtrStorage);
            uint apiSize = unchecked((uint)Marshal.ReadInt32(apiSizeStorage));
            Assert(apiPtr != IntPtr.Zero, "phase27 api pointer should not be null");
            Assert(apiSize >= HUB_API_V1_SIZE, "phase27 api size too small");

            RegisterModRaw registerMod = (RegisterModRaw)Marshal.GetDelegateForFunctionPointer(ReadFnPtr(apiPtr, REGISTER_MOD_OFFSET), typeof(RegisterModRaw));
            RegisterBoolV2Raw registerBoolV2 = (RegisterBoolV2Raw)Marshal.GetDelegateForFunctionPointer(ReadFnPtr(apiPtr, REGISTER_BOOL_V2_OFFSET), typeof(RegisterBoolV2Raw));
            RegisterKeybindV2Raw registerKeybindV2 = (RegisterKeybindV2Raw)Marshal.GetDelegateForFunctionPointer(ReadFnPtr(apiPtr, REGISTER_KEYBIND_V2_OFFSET), typeof(RegisterKeybindV2Raw));
            RegisterSelectV2Raw registerSelectV2 = (RegisterSelectV2Raw)Marshal.GetDelegateForFunctionPointer(ReadFnPtr(apiPtr, REGISTER_SELECT_V2_OFFSET), typeof(RegisterSelectV2Raw));
            RegisterTextV2Raw registerTextV2 = (RegisterTextV2Raw)Marshal.GetDelegateForFunctionPointer(ReadFnPtr(apiPtr, REGISTER_TEXT_V2_OFFSET), typeof(RegisterTextV2Raw));
            RegisterActionV2Raw registerActionV2 = (RegisterActionV2Raw)Marshal.GetDelegateForFunctionPointer(ReadFnPtr(apiPtr, REGISTER_ACTION_V2_OFFSET), typeof(RegisterActionV2Raw));

            GetBoolCb boolGet = BoolGet;
            SetBoolCb boolSet = BoolSet;
            GetKeybindCb keybindGet = KeybindGet;
            SetKeybindCb keybindSet = KeybindSet;
            GetSelectCb selectGet = SelectGet;
            SetSelectCb selectSet = SelectSet;
            GetTextCb textGet = TextGet;
            SetTextCb textSet = TextSet;
            ActionCb actionCb = ActionInvoke;
            CallbackRoots.Add(boolGet);
            CallbackRoots.Add(boolSet);
            CallbackRoots.Add(keybindGet);
            CallbackRoots.Add(keybindSet);
            CallbackRoots.Add(selectGet);
            CallbackRoots.Add(selectSet);
            CallbackRoots.Add(textGet);
            CallbackRoots.Add(textSet);
            CallbackRoots.Add(actionCb);

            IntPtr nsId = AllocAnsi(allocations, "phase27.hover");
            IntPtr nsDisplay = AllocAnsi(allocations, "Phase27 Hover");
            IntPtr modId = AllocAnsi(allocations, "phase27_mod");
            IntPtr modDisplay = AllocAnsi(allocations, "Phase27 Mod");

            EMC_ModDescriptorV1 modDescriptor = new EMC_ModDescriptorV1
            {
                namespace_id = nsId,
                namespace_display_name = nsDisplay,
                mod_id = modId,
                mod_display_name = modDisplay,
                mod_user_data = IntPtr.Zero
            };

            IntPtr modDescriptorPtr = AllocStruct(allocations, modDescriptor);
            IntPtr modHandlePtr = Marshal.AllocHGlobal(IntPtr.Size);
            allocations.Add(modHandlePtr);
            ExpectResult(registerMod(modDescriptorPtr, modHandlePtr), EMC_OK, "phase27 register_mod failed");
            IntPtr modHandle = Marshal.ReadIntPtr(modHandlePtr);
            Assert(modHandle != IntPtr.Zero, "phase27 mod handle should not be null");

            EMC_SelectOptionV1[] options = new EMC_SelectOptionV1[]
            {
                new EMC_SelectOptionV1 { value = 0, label = AllocAnsi(allocations, "Default") },
                new EMC_SelectOptionV1 { value = 1, label = AllocAnsi(allocations, "Warm") },
                new EMC_SelectOptionV1 { value = 2, label = AllocAnsi(allocations, "Cool") }
            };
            IntPtr optionsPtr = AllocArray(allocations, options);

            EMC_BoolSettingDefV2 boolDef = new EMC_BoolSettingDefV2
            {
                setting_id = AllocAnsi(allocations, "enabled"),
                label = AllocAnsi(allocations, "Enabled"),
                description = AllocAnsi(allocations, "Enable feature"),
                user_data = IntPtr.Zero,
                get_value = Marshal.GetFunctionPointerForDelegate(boolGet),
                set_value = Marshal.GetFunctionPointerForDelegate(boolSet),
                hover_hint = AllocAnsi(allocations, "Turn feature on or off.")
            };
            EMC_BoolSettingDefV2 boolDriftDef = boolDef;
            boolDriftDef.hover_hint = AllocAnsi(allocations, "Changed hint should be ignored.");
            EMC_BoolSettingDefV2 boolNullHintDef = new EMC_BoolSettingDefV2
            {
                setting_id = AllocAnsi(allocations, "null_hint"),
                label = AllocAnsi(allocations, "Null hint"),
                description = AllocAnsi(allocations, "Null hint row"),
                user_data = IntPtr.Zero,
                get_value = Marshal.GetFunctionPointerForDelegate(boolGet),
                set_value = Marshal.GetFunctionPointerForDelegate(boolSet),
                hover_hint = IntPtr.Zero
            };
            EMC_KeybindSettingDefV2 keybindDef = new EMC_KeybindSettingDefV2
            {
                setting_id = AllocAnsi(allocations, "hotkey"),
                label = AllocAnsi(allocations, "Hotkey"),
                description = AllocAnsi(allocations, "Primary hotkey"),
                user_data = IntPtr.Zero,
                get_value = Marshal.GetFunctionPointerForDelegate(keybindGet),
                set_value = Marshal.GetFunctionPointerForDelegate(keybindSet),
                hover_hint = AllocAnsi(allocations, "Capture a replacement hotkey.")
            };
            EMC_SelectSettingDefV2 selectDef = new EMC_SelectSettingDefV2
            {
                setting_id = AllocAnsi(allocations, "palette"),
                label = AllocAnsi(allocations, "Palette"),
                description = AllocAnsi(allocations, "Choose palette"),
                user_data = IntPtr.Zero,
                options = optionsPtr,
                option_count = (uint)options.Length,
                get_value = Marshal.GetFunctionPointerForDelegate(selectGet),
                set_value = Marshal.GetFunctionPointerForDelegate(selectSet),
                hover_hint = AllocAnsi(allocations, "Choose the palette style.")
            };
            EMC_TextSettingDefV2 textDef = new EMC_TextSettingDefV2
            {
                setting_id = AllocAnsi(allocations, "title"),
                label = AllocAnsi(allocations, "Title"),
                description = AllocAnsi(allocations, "Overlay title"),
                user_data = IntPtr.Zero,
                max_length = TEXT_MAX_LENGTH,
                get_value = Marshal.GetFunctionPointerForDelegate(textGet),
                set_value = Marshal.GetFunctionPointerForDelegate(textSet),
                hover_hint = AllocAnsi(allocations, "Edit the overlay title.")
            };
            EMC_TextSettingDefV2 textEmptyHintDef = new EMC_TextSettingDefV2
            {
                setting_id = AllocAnsi(allocations, "empty_hint"),
                label = AllocAnsi(allocations, "Empty hint"),
                description = AllocAnsi(allocations, "Empty hint row"),
                user_data = IntPtr.Zero,
                max_length = TEXT_MAX_LENGTH,
                get_value = Marshal.GetFunctionPointerForDelegate(textGet),
                set_value = Marshal.GetFunctionPointerForDelegate(textSet),
                hover_hint = AllocAnsi(allocations, "")
            };
            EMC_ActionRowDefV2 actionDef = new EMC_ActionRowDefV2
            {
                setting_id = AllocAnsi(allocations, "refresh_now"),
                label = AllocAnsi(allocations, "Refresh now"),
                description = AllocAnsi(allocations, "Refresh runtime values"),
                user_data = IntPtr.Zero,
                action_flags = 1u,
                on_action = Marshal.GetFunctionPointerForDelegate(actionCb),
                hover_hint = AllocAnsi(allocations, "Re-sync values immediately.")
            };

            IntPtr boolDefPtr = AllocStruct(allocations, boolDef);
            IntPtr boolDriftDefPtr = AllocStruct(allocations, boolDriftDef);
            IntPtr boolNullHintDefPtr = AllocStruct(allocations, boolNullHintDef);
            IntPtr keybindDefPtr = AllocStruct(allocations, keybindDef);
            IntPtr selectDefPtr = AllocStruct(allocations, selectDef);
            IntPtr textDefPtr = AllocStruct(allocations, textDef);
            IntPtr textEmptyHintDefPtr = AllocStruct(allocations, textEmptyHintDef);
            IntPtr actionDefPtr = AllocStruct(allocations, actionDef);

            ExpectResult(registerBoolV2(modHandle, boolDefPtr), EMC_OK, "phase27 register bool failed");
            ExpectResult(registerBoolV2(modHandle, boolDriftDefPtr), EMC_OK, "phase27 bool hover drift should be ignored");
            ExpectResult(registerBoolV2(modHandle, boolNullHintDefPtr), EMC_OK, "phase27 register bool null hover failed");
            ExpectResult(registerKeybindV2(modHandle, keybindDefPtr), EMC_OK, "phase27 register keybind failed");
            ExpectResult(registerSelectV2(modHandle, selectDefPtr), EMC_OK, "phase27 register select failed");
            ExpectResult(registerTextV2(modHandle, textDefPtr), EMC_OK, "phase27 register text failed");
            ExpectResult(registerTextV2(modHandle, textEmptyHintDefPtr), EMC_OK, "phase27 register text empty hover failed");
            ExpectResult(registerActionV2(modHandle, actionDefPtr), EMC_OK, "phase27 register action failed");

            Assert(countSettingsForMod(nsId, modId) == 7, "phase27 setting count mismatch");

            openOptionsWindow();

            AssertStringExport(getHoverHint, nsId, modId, boolDef.setting_id, "Turn feature on or off.", "phase27 bool hover");
            AssertStringExport(getDescription, nsId, modId, boolDef.setting_id, "Enable feature", "phase27 bool description");
            AssertStringExport(getHoverHint, nsId, modId, keybindDef.setting_id, "Capture a replacement hotkey.", "phase27 keybind hover");
            AssertStringExport(getDescription, nsId, modId, keybindDef.setting_id, "Primary hotkey", "phase27 keybind description");
            AssertStringExport(getHoverHint, nsId, modId, selectDef.setting_id, "Choose the palette style.", "phase27 select hover");
            AssertStringExport(getDescription, nsId, modId, selectDef.setting_id, "Choose palette", "phase27 select description");
            AssertStringExport(getHoverHint, nsId, modId, textDef.setting_id, "Edit the overlay title.", "phase27 text hover");
            AssertStringExport(getDescription, nsId, modId, textDef.setting_id, "Overlay title", "phase27 text description");
            AssertStringExport(getHoverHint, nsId, modId, actionDef.setting_id, "Re-sync values immediately.", "phase27 action hover");
            AssertStringExport(getDescription, nsId, modId, actionDef.setting_id, "Refresh runtime values", "phase27 action description");
            AssertStringExport(getHoverHint, nsId, modId, boolNullHintDef.setting_id, "", "phase27 null hover");
            AssertStringExport(getDescription, nsId, modId, boolNullHintDef.setting_id, "Null hint row", "phase27 null description");
            AssertStringExport(getHoverHint, nsId, modId, textEmptyHintDef.setting_id, "", "phase27 empty hover");
            AssertStringExport(getDescription, nsId, modId, textEmptyHintDef.setting_id, "Empty hint row", "phase27 empty description");

            ExpectResult(setPendingSelect(nsId, modId, selectDef.setting_id, 2), EMC_OK, "phase27 set_pending_select failed");
            int pendingSelectValue;
            ExpectResult(getPendingSelect(nsId, modId, selectDef.setting_id, out pendingSelectValue), EMC_OK, "phase27 pending select readback failed");
            Assert(pendingSelectValue == 2, "phase27 pending select mismatch");

            IntPtr gamma = AllocAnsi(allocations, "Gamma");
            ExpectResult(setPendingText(nsId, modId, textDef.setting_id, gamma), EMC_OK, "phase27 set_pending_text failed");
            IntPtr textBuffer = Marshal.AllocHGlobal((int)TEXT_MAX_LENGTH + 1);
            try
            {
                ExpectResult(getPendingText(nsId, modId, textDef.setting_id, textBuffer, TEXT_MAX_LENGTH + 1u), EMC_OK, "phase27 pending text readback failed");
                Assert(ReadAnsiBuffer(textBuffer) == "Gamma", "phase27 pending text mismatch");
            }
            finally
            {
                Marshal.FreeHGlobal(textBuffer);
            }

            ExpectResult(invokeAction(nsId, modId, actionDef.setting_id), EMC_OK, "phase27 invoke action failed");
            Assert(ActionCount == 1, "phase27 action count mismatch");

            closeOptionsWindow();
            setHubEnabled(1);
            setRegistryAttachEnabled(1);
            setRegistrationLocked(0);
            return "PASS";
        }
        finally
        {
            if (closeOptionsWindow != null)
            {
                closeOptionsWindow();
            }
            if (setHubEnabled != null)
            {
                setHubEnabled(1);
            }
            if (setRegistryAttachEnabled != null)
            {
                setRegistryAttachEnabled(1);
            }
            if (setRegistrationLocked != null)
            {
                setRegistrationLocked(0);
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
$result = [HubPhase27HoverHintHarness]::Run($DllPath, $KenshiPath)
Write-Host $result
