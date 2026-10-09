param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [string]$KenshiPath = ""
)

$ErrorActionPreference = 'Stop'

$code = @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public static class HubPhase2RegistrationHarness
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
    public delegate int RegisterIntRaw(IntPtr mod, IntPtr def);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterFloatRaw(IntPtr mod, IntPtr def);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterActionRaw(IntPtr mod, IntPtr def);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void TestSetRegistrationLockedRaw(int isLocked);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void TestSetRegistryAttachEnabledRaw(int isEnabled);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetBoolCb(IntPtr userData, IntPtr outValue);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int SetBoolCb(IntPtr userData, int value, IntPtr errBuf, uint errBufSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetKeybindCb(IntPtr userData, IntPtr outValue);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int SetKeybindCb(IntPtr userData, EMC_KeybindValueV1 value, IntPtr errBuf, uint errBufSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetIntCb(IntPtr userData, IntPtr outValue);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int SetIntCb(IntPtr userData, int value, IntPtr errBuf, uint errBufSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetFloatCb(IntPtr userData, IntPtr outValue);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int SetFloatCb(IntPtr userData, float value, IntPtr errBuf, uint errBufSize);

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
    public struct EMC_KeybindValueV1
    {
        public int keycode;
        public uint modifiers;
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
    public struct EMC_IntSettingDefV1
    {
        public IntPtr setting_id;
        public IntPtr label;
        public IntPtr description;
        public IntPtr user_data;
        public int min_value;
        public int max_value;
        public int step;
        public IntPtr get_value;
        public IntPtr set_value;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct EMC_FloatSettingDefV1
    {
        public IntPtr setting_id;
        public IntPtr label;
        public IntPtr description;
        public IntPtr user_data;
        public float min_value;
        public float max_value;
        public float step;
        public uint display_decimals;
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
    private const int EMC_ERR_CONFLICT = 4;
    private const int EMC_ERR_INTERNAL = 7;
    private const uint DONT_RESOLVE_DLL_REFERENCES = 0x00000001u;

    private static readonly List<Delegate> CallbackRoots = new List<Delegate>();

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

    private static int BoolGetA(IntPtr userData, IntPtr outValue)
    {
        Marshal.WriteInt32(outValue, 1);
        return EMC_OK;
    }

    private static int BoolSetA(IntPtr userData, int value, IntPtr errBuf, uint errBufSize)
    {
        return EMC_OK;
    }

    private static int BoolGetB(IntPtr userData, IntPtr outValue)
    {
        Marshal.WriteInt32(outValue, 0);
        return EMC_OK;
    }

    private static int BoolSetB(IntPtr userData, int value, IntPtr errBuf, uint errBufSize)
    {
        return EMC_OK;
    }

    private static int KeybindGetA(IntPtr userData, IntPtr outValue)
    {
        EMC_KeybindValueV1 value = new EMC_KeybindValueV1 { keycode = 42, modifiers = 0 };
        Marshal.StructureToPtr(value, outValue, false);
        return EMC_OK;
    }

    private static int KeybindSetA(IntPtr userData, EMC_KeybindValueV1 value, IntPtr errBuf, uint errBufSize)
    {
        return EMC_OK;
    }

    private static int KeybindGetB(IntPtr userData, IntPtr outValue)
    {
        EMC_KeybindValueV1 value = new EMC_KeybindValueV1 { keycode = 43, modifiers = 1 };
        Marshal.StructureToPtr(value, outValue, false);
        return EMC_OK;
    }

    private static int KeybindSetB(IntPtr userData, EMC_KeybindValueV1 value, IntPtr errBuf, uint errBufSize)
    {
        return EMC_OK;
    }

    private static int IntGetA(IntPtr userData, IntPtr outValue)
    {
        Marshal.WriteInt32(outValue, 10);
        return EMC_OK;
    }

    private static int IntSetA(IntPtr userData, int value, IntPtr errBuf, uint errBufSize)
    {
        return EMC_OK;
    }

    private static int IntGetB(IntPtr userData, IntPtr outValue)
    {
        Marshal.WriteInt32(outValue, 20);
        return EMC_OK;
    }

    private static int IntSetB(IntPtr userData, int value, IntPtr errBuf, uint errBufSize)
    {
        return EMC_OK;
    }

    private static int FloatGetA(IntPtr userData, IntPtr outValue)
    {
        byte[] bytes = BitConverter.GetBytes(1.25f);
        Marshal.Copy(bytes, 0, outValue, 4);
        return EMC_OK;
    }

    private static int FloatSetA(IntPtr userData, float value, IntPtr errBuf, uint errBufSize)
    {
        return EMC_OK;
    }

    private static int FloatGetB(IntPtr userData, IntPtr outValue)
    {
        byte[] bytes = BitConverter.GetBytes(2.50f);
        Marshal.Copy(bytes, 0, outValue, 4);
        return EMC_OK;
    }

    private static int FloatSetB(IntPtr userData, float value, IntPtr errBuf, uint errBufSize)
    {
        return EMC_OK;
    }

    private static int ActionA(IntPtr userData, IntPtr errBuf, uint errBufSize)
    {
        return EMC_OK;
    }

    private static int ActionB(IntPtr userData, IntPtr errBuf, uint errBufSize)
    {
        return EMC_OK;
    }

    public static string Run(string dllPath, string kenshiPath)
    {
        List<IntPtr> allocations = new List<IntPtr>();
        IntPtr module = IntPtr.Zero;
        IntPtr outApi = IntPtr.Zero;
        IntPtr outApiSize = IntPtr.Zero;
        TestSetRegistrationLockedRaw setRegistrationLocked = null;
        TestSetRegistryAttachEnabledRaw setRegistryAttachEnabled = null;

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

            IntPtr getApiProc = GetProcAddress(module, "EMC_ModHub_GetApi");
            Assert(getApiProc != IntPtr.Zero, "GetProcAddress failed for EMC_ModHub_GetApi");

            GetApiRaw getApi = (GetApiRaw)Marshal.GetDelegateForFunctionPointer(getApiProc, typeof(GetApiRaw));

            IntPtr setLockedProc = GetProcAddress(module, "EMC_ModHub_Test_SetRegistrationLocked");
            Assert(setLockedProc != IntPtr.Zero, "GetProcAddress failed for EMC_ModHub_Test_SetRegistrationLocked");
            setRegistrationLocked = (TestSetRegistrationLockedRaw)Marshal.GetDelegateForFunctionPointer(
                setLockedProc, typeof(TestSetRegistrationLockedRaw));

            IntPtr setAttachProc = GetProcAddress(module, "EMC_ModHub_Test_SetRegistryAttachEnabled");
            Assert(setAttachProc != IntPtr.Zero, "GetProcAddress failed for EMC_ModHub_Test_SetRegistryAttachEnabled");
            setRegistryAttachEnabled = (TestSetRegistryAttachEnabledRaw)Marshal.GetDelegateForFunctionPointer(
                setAttachProc, typeof(TestSetRegistryAttachEnabledRaw));

            setRegistrationLocked(0);
            setRegistryAttachEnabled(-1);

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
            RegisterIntRaw registerInt = (RegisterIntRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, 32), typeof(RegisterIntRaw));
            RegisterFloatRaw registerFloat = (RegisterFloatRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, 40), typeof(RegisterFloatRaw));
            RegisterActionRaw registerAction = (RegisterActionRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, 48), typeof(RegisterActionRaw));

            GetBoolCb getBoolA = BoolGetA;
            SetBoolCb setBoolA = BoolSetA;
            GetBoolCb getBoolB = BoolGetB;
            SetBoolCb setBoolB = BoolSetB;
            GetKeybindCb getKeybindA = KeybindGetA;
            SetKeybindCb setKeybindA = KeybindSetA;
            GetKeybindCb getKeybindB = KeybindGetB;
            SetKeybindCb setKeybindB = KeybindSetB;
            GetIntCb getIntA = IntGetA;
            SetIntCb setIntA = IntSetA;
            GetIntCb getIntB = IntGetB;
            SetIntCb setIntB = IntSetB;
            GetFloatCb getFloatA = FloatGetA;
            SetFloatCb setFloatA = FloatSetA;
            GetFloatCb getFloatB = FloatGetB;
            SetFloatCb setFloatB = FloatSetB;
            ActionCb actionA = ActionA;
            ActionCb actionB = ActionB;

            CallbackRoots.Add(getBoolA);
            CallbackRoots.Add(setBoolA);
            CallbackRoots.Add(getBoolB);
            CallbackRoots.Add(setBoolB);
            CallbackRoots.Add(getKeybindA);
            CallbackRoots.Add(setKeybindA);
            CallbackRoots.Add(getKeybindB);
            CallbackRoots.Add(setKeybindB);
            CallbackRoots.Add(getIntA);
            CallbackRoots.Add(setIntA);
            CallbackRoots.Add(getIntB);
            CallbackRoots.Add(setIntB);
            CallbackRoots.Add(getFloatA);
            CallbackRoots.Add(setFloatA);
            CallbackRoots.Add(getFloatB);
            CallbackRoots.Add(setFloatB);
            CallbackRoots.Add(actionA);
            CallbackRoots.Add(actionB);

            IntPtr getBoolAPtr = Marshal.GetFunctionPointerForDelegate(getBoolA);
            IntPtr setBoolAPtr = Marshal.GetFunctionPointerForDelegate(setBoolA);
            IntPtr getBoolBPtr = Marshal.GetFunctionPointerForDelegate(getBoolB);
            IntPtr setBoolBPtr = Marshal.GetFunctionPointerForDelegate(setBoolB);
            IntPtr getKeybindAPtr = Marshal.GetFunctionPointerForDelegate(getKeybindA);
            IntPtr setKeybindAPtr = Marshal.GetFunctionPointerForDelegate(setKeybindA);
            IntPtr getKeybindBPtr = Marshal.GetFunctionPointerForDelegate(getKeybindB);
            IntPtr setKeybindBPtr = Marshal.GetFunctionPointerForDelegate(setKeybindB);
            IntPtr getIntAPtr = Marshal.GetFunctionPointerForDelegate(getIntA);
            IntPtr setIntAPtr = Marshal.GetFunctionPointerForDelegate(setIntA);
            IntPtr getIntBPtr = Marshal.GetFunctionPointerForDelegate(getIntB);
            IntPtr setIntBPtr = Marshal.GetFunctionPointerForDelegate(setIntB);
            IntPtr getFloatAPtr = Marshal.GetFunctionPointerForDelegate(getFloatA);
            IntPtr setFloatAPtr = Marshal.GetFunctionPointerForDelegate(setFloatA);
            IntPtr getFloatBPtr = Marshal.GetFunctionPointerForDelegate(getFloatB);
            IntPtr setFloatBPtr = Marshal.GetFunctionPointerForDelegate(setFloatB);
            IntPtr actionAPtr = Marshal.GetFunctionPointerForDelegate(actionA);
            IntPtr actionBPtr = Marshal.GetFunctionPointerForDelegate(actionB);

            IntPtr nsId = AllocAnsi(allocations, "emkej.qol");
            IntPtr nsDisplayA = AllocAnsi(allocations, "QoL");
            IntPtr nsDisplayB = AllocAnsi(allocations, "QoL Drift");
            IntPtr modId = AllocAnsi(allocations, "phase2_test_mod");
            IntPtr modDisplayA = AllocAnsi(allocations, "Phase2 Test Mod");
            IntPtr modDisplayB = AllocAnsi(allocations, "Phase2 Test Mod Drift");
            IntPtr mod2Id = AllocAnsi(allocations, "phase2_test_mod_2");
            IntPtr mod2Display = AllocAnsi(allocations, "Phase2 Test Mod 2");
            IntPtr invalidModId = AllocAnsi(allocations, "Phase2-Invalid");

            IntPtr boolSettingId = AllocAnsi(allocations, "enabled");
            IntPtr boolSetting2Id = AllocAnsi(allocations, "enabled_two");
            IntPtr boolLabelA = AllocAnsi(allocations, "Enabled");
            IntPtr boolLabelB = AllocAnsi(allocations, "Enabled Drift");
            IntPtr boolDescA = AllocAnsi(allocations, "Enable the feature");
            IntPtr boolDescB = AllocAnsi(allocations, "Enable the feature drift");

            IntPtr keybindSettingId = AllocAnsi(allocations, "toggle_hotkey");
            IntPtr keybindLabel = AllocAnsi(allocations, "Toggle hotkey");
            IntPtr keybindLabelB = AllocAnsi(allocations, "Toggle hotkey drift");
            IntPtr keybindDesc = AllocAnsi(allocations, "Toggle with key");
            IntPtr keybindDescB = AllocAnsi(allocations, "Toggle with key drift");

            IntPtr intSettingId = AllocAnsi(allocations, "distance_limit");
            IntPtr intLabel = AllocAnsi(allocations, "Distance");
            IntPtr intDesc = AllocAnsi(allocations, "Distance limit");

            IntPtr floatSettingId = AllocAnsi(allocations, "multiplier");
            IntPtr floatLabel = AllocAnsi(allocations, "Multiplier");
            IntPtr floatDesc = AllocAnsi(allocations, "Multiplier value");

            IntPtr actionSettingId = AllocAnsi(allocations, "reset_defaults");
            IntPtr actionLabel = AllocAnsi(allocations, "Reset");
            IntPtr actionDesc = AllocAnsi(allocations, "Reset to defaults");

            EMC_ModDescriptorV1 modDesc = new EMC_ModDescriptorV1
            {
                namespace_id = nsId,
                namespace_display_name = nsDisplayA,
                mod_id = modId,
                mod_display_name = modDisplayA,
                mod_user_data = new IntPtr(1001)
            };
            IntPtr modDescPtr = AllocStruct(allocations, modDesc);

            EMC_ModDescriptorV1 modDescDrift = new EMC_ModDescriptorV1
            {
                namespace_id = nsId,
                namespace_display_name = nsDisplayB,
                mod_id = modId,
                mod_display_name = modDisplayB,
                mod_user_data = new IntPtr(1002)
            };
            IntPtr modDescDriftPtr = AllocStruct(allocations, modDescDrift);

            EMC_ModDescriptorV1 modDesc2 = new EMC_ModDescriptorV1
            {
                namespace_id = nsId,
                namespace_display_name = nsDisplayA,
                mod_id = mod2Id,
                mod_display_name = mod2Display,
                mod_user_data = IntPtr.Zero
            };
            IntPtr modDesc2Ptr = AllocStruct(allocations, modDesc2);

            EMC_ModDescriptorV1 invalidModDesc = new EMC_ModDescriptorV1
            {
                namespace_id = nsId,
                namespace_display_name = nsDisplayA,
                mod_id = invalidModId,
                mod_display_name = modDisplayA,
                mod_user_data = IntPtr.Zero
            };
            IntPtr invalidModDescPtr = AllocStruct(allocations, invalidModDesc);

            IntPtr outHandle = Marshal.AllocHGlobal(IntPtr.Size);
            allocations.Add(outHandle);

            Marshal.WriteIntPtr(outHandle, new IntPtr(0x1234));
            r = registerMod(IntPtr.Zero, outHandle);
            ExpectResult(r, EMC_ERR_INVALID_ARGUMENT, "register_mod should reject null desc");
            Assert(Marshal.ReadIntPtr(outHandle) == IntPtr.Zero, "register_mod must clear out_handle on failure");

            r = registerMod(modDescPtr, IntPtr.Zero);
            ExpectResult(r, EMC_ERR_INVALID_ARGUMENT, "register_mod should reject null out_handle");

            Marshal.WriteIntPtr(outHandle, IntPtr.Zero);
            r = registerMod(invalidModDescPtr, outHandle);
            ExpectResult(r, EMC_ERR_INVALID_ARGUMENT, "register_mod should reject invalid mod id");
            Assert(Marshal.ReadIntPtr(outHandle) == IntPtr.Zero, "register_mod invalid id must clear out_handle");

            Marshal.WriteIntPtr(outHandle, IntPtr.Zero);
            r = registerMod(modDescPtr, outHandle);
            ExpectResult(r, EMC_OK, "register_mod valid call failed");
            IntPtr modHandle = Marshal.ReadIntPtr(outHandle);
            Assert(modHandle != IntPtr.Zero, "register_mod returned null handle");

            Marshal.WriteIntPtr(outHandle, IntPtr.Zero);
            r = registerMod(modDescPtr, outHandle);
            ExpectResult(r, EMC_OK, "register_mod exact duplicate should succeed");
            Assert(Marshal.ReadIntPtr(outHandle) == modHandle, "register_mod exact duplicate should return canonical handle");

            Marshal.WriteIntPtr(outHandle, IntPtr.Zero);
            r = registerMod(modDescDriftPtr, outHandle);
            ExpectResult(r, EMC_OK, "register_mod drift duplicate should succeed");
            Assert(Marshal.ReadIntPtr(outHandle) == modHandle, "register_mod drift duplicate should return canonical handle");

            EMC_BoolSettingDefV1 boolDef = new EMC_BoolSettingDefV1
            {
                setting_id = boolSettingId,
                label = boolLabelA,
                description = boolDescA,
                user_data = new IntPtr(2001),
                get_value = getBoolAPtr,
                set_value = setBoolAPtr
            };
            IntPtr boolDefPtr = AllocStruct(allocations, boolDef);

            EMC_BoolSettingDefV1 boolDefDrift = new EMC_BoolSettingDefV1
            {
                setting_id = boolSettingId,
                label = boolLabelB,
                description = boolDescB,
                user_data = new IntPtr(2002),
                get_value = getBoolBPtr,
                set_value = setBoolBPtr
            };
            IntPtr boolDefDriftPtr = AllocStruct(allocations, boolDefDrift);

            EMC_BoolSettingDefV1 boolDefNullCb = new EMC_BoolSettingDefV1
            {
                setting_id = boolSetting2Id,
                label = boolLabelA,
                description = boolDescA,
                user_data = IntPtr.Zero,
                get_value = IntPtr.Zero,
                set_value = setBoolAPtr
            };
            IntPtr boolDefNullCbPtr = AllocStruct(allocations, boolDefNullCb);

            r = registerBool(IntPtr.Zero, boolDefPtr);
            ExpectResult(r, EMC_ERR_INVALID_ARGUMENT, "register_bool should reject null mod");
            r = registerBool(modHandle, IntPtr.Zero);
            ExpectResult(r, EMC_ERR_INVALID_ARGUMENT, "register_bool should reject null def");
            r = registerBool(modHandle, boolDefNullCbPtr);
            ExpectResult(r, EMC_ERR_INVALID_ARGUMENT, "register_bool should reject null callbacks");
            r = registerBool(new IntPtr(0x987654), boolDefPtr);
            ExpectResult(r, EMC_ERR_INVALID_ARGUMENT, "register_bool should reject invalid handle");
            r = registerBool(modHandle, boolDefPtr);
            ExpectResult(r, EMC_OK, "register_bool valid call failed");
            r = registerBool(modHandle, boolDefPtr);
            ExpectResult(r, EMC_OK, "register_bool exact duplicate should succeed");
            r = registerBool(modHandle, boolDefDriftPtr);
            ExpectResult(r, EMC_OK, "register_bool drift duplicate should succeed");

            EMC_KeybindSettingDefV1 keybindDef = new EMC_KeybindSettingDefV1
            {
                setting_id = keybindSettingId,
                label = keybindLabel,
                description = keybindDesc,
                user_data = new IntPtr(3001),
                get_value = getKeybindAPtr,
                set_value = setKeybindAPtr
            };
            IntPtr keybindDefPtr = AllocStruct(allocations, keybindDef);

            EMC_KeybindSettingDefV1 keybindDefDrift = new EMC_KeybindSettingDefV1
            {
                setting_id = keybindSettingId,
                label = keybindLabelB,
                description = keybindDescB,
                user_data = new IntPtr(3002),
                get_value = getKeybindBPtr,
                set_value = setKeybindBPtr
            };
            IntPtr keybindDefDriftPtr = AllocStruct(allocations, keybindDefDrift);

            EMC_KeybindSettingDefV1 keybindDefNullCb = new EMC_KeybindSettingDefV1
            {
                setting_id = keybindSettingId,
                label = keybindLabel,
                description = keybindDesc,
                user_data = IntPtr.Zero,
                get_value = IntPtr.Zero,
                set_value = setKeybindAPtr
            };
            IntPtr keybindDefNullCbPtr = AllocStruct(allocations, keybindDefNullCb);

            r = registerKeybind(modHandle, keybindDefNullCbPtr);
            ExpectResult(r, EMC_ERR_INVALID_ARGUMENT, "register_keybind should reject null callbacks");
            r = registerKeybind(modHandle, keybindDefPtr);
            ExpectResult(r, EMC_OK, "register_keybind valid call failed");
            r = registerKeybind(modHandle, keybindDefDriftPtr);
            ExpectResult(r, EMC_OK, "register_keybind drift duplicate should succeed");

            EMC_IntSettingDefV1 intDef = new EMC_IntSettingDefV1
            {
                setting_id = intSettingId,
                label = intLabel,
                description = intDesc,
                user_data = new IntPtr(4001),
                min_value = 0,
                max_value = 50,
                step = 1,
                get_value = getIntAPtr,
                set_value = setIntAPtr
            };
            IntPtr intDefPtr = AllocStruct(allocations, intDef);

            EMC_IntSettingDefV1 intDefDrift = new EMC_IntSettingDefV1
            {
                setting_id = intSettingId,
                label = intLabel,
                description = intDesc,
                user_data = new IntPtr(4002),
                min_value = 0,
                max_value = 100,
                step = 5,
                get_value = getIntBPtr,
                set_value = setIntBPtr
            };
            IntPtr intDefDriftPtr = AllocStruct(allocations, intDefDrift);

            EMC_IntSettingDefV1 intDefInvalid = new EMC_IntSettingDefV1
            {
                setting_id = intSettingId,
                label = intLabel,
                description = intDesc,
                user_data = IntPtr.Zero,
                min_value = 10,
                max_value = 0,
                step = 0,
                get_value = getIntAPtr,
                set_value = setIntAPtr
            };
            IntPtr intDefInvalidPtr = AllocStruct(allocations, intDefInvalid);

            EMC_IntSettingDefV1 intConflictDef = new EMC_IntSettingDefV1
            {
                setting_id = boolSettingId,
                label = intLabel,
                description = intDesc,
                user_data = IntPtr.Zero,
                min_value = 0,
                max_value = 10,
                step = 1,
                get_value = getIntAPtr,
                set_value = setIntAPtr
            };
            IntPtr intConflictDefPtr = AllocStruct(allocations, intConflictDef);

            r = registerInt(modHandle, intDefInvalidPtr);
            ExpectResult(r, EMC_ERR_INVALID_ARGUMENT, "register_int should reject invalid metadata");
            r = registerInt(modHandle, intDefPtr);
            ExpectResult(r, EMC_OK, "register_int valid call failed");
            r = registerInt(modHandle, intDefDriftPtr);
            ExpectResult(r, EMC_OK, "register_int drift duplicate should succeed");
            r = registerInt(modHandle, intConflictDefPtr);
            ExpectResult(r, EMC_ERR_CONFLICT, "register_int should conflict for same id with different kind");

            EMC_FloatSettingDefV1 floatDef = new EMC_FloatSettingDefV1
            {
                setting_id = floatSettingId,
                label = floatLabel,
                description = floatDesc,
                user_data = new IntPtr(5001),
                min_value = 0.0f,
                max_value = 10.0f,
                step = 0.5f,
                display_decimals = 2u,
                get_value = getFloatAPtr,
                set_value = setFloatAPtr
            };
            IntPtr floatDefPtr = AllocStruct(allocations, floatDef);

            EMC_FloatSettingDefV1 floatDefDrift = new EMC_FloatSettingDefV1
            {
                setting_id = floatSettingId,
                label = floatLabel,
                description = floatDesc,
                user_data = new IntPtr(5002),
                min_value = -5.0f,
                max_value = 20.0f,
                step = 1.0f,
                display_decimals = 1u,
                get_value = getFloatBPtr,
                set_value = setFloatBPtr
            };
            IntPtr floatDefDriftPtr = AllocStruct(allocations, floatDefDrift);

            EMC_FloatSettingDefV1 floatDefInvalid = new EMC_FloatSettingDefV1
            {
                setting_id = floatSettingId,
                label = floatLabel,
                description = floatDesc,
                user_data = IntPtr.Zero,
                min_value = 0.0f,
                max_value = 1.0f,
                step = 0.1f,
                display_decimals = 4u,
                get_value = getFloatAPtr,
                set_value = setFloatAPtr
            };
            IntPtr floatDefInvalidPtr = AllocStruct(allocations, floatDefInvalid);

            r = registerFloat(modHandle, floatDefInvalidPtr);
            ExpectResult(r, EMC_ERR_INVALID_ARGUMENT, "register_float should reject invalid display_decimals");
            r = registerFloat(modHandle, floatDefPtr);
            ExpectResult(r, EMC_OK, "register_float valid call failed");
            r = registerFloat(modHandle, floatDefDriftPtr);
            ExpectResult(r, EMC_OK, "register_float drift duplicate should succeed");

            EMC_ActionRowDefV1 actionDef = new EMC_ActionRowDefV1
            {
                setting_id = actionSettingId,
                label = actionLabel,
                description = actionDesc,
                user_data = new IntPtr(6001),
                action_flags = 0u,
                on_action = actionAPtr
            };
            IntPtr actionDefPtr = AllocStruct(allocations, actionDef);

            EMC_ActionRowDefV1 actionDefDrift = new EMC_ActionRowDefV1
            {
                setting_id = actionSettingId,
                label = actionLabel,
                description = actionDesc,
                user_data = new IntPtr(6002),
                action_flags = 1u,
                on_action = actionBPtr
            };
            IntPtr actionDefDriftPtr = AllocStruct(allocations, actionDefDrift);

            EMC_ActionRowDefV1 actionDefNullCb = new EMC_ActionRowDefV1
            {
                setting_id = actionSettingId,
                label = actionLabel,
                description = actionDesc,
                user_data = IntPtr.Zero,
                action_flags = 0u,
                on_action = IntPtr.Zero
            };
            IntPtr actionDefNullCbPtr = AllocStruct(allocations, actionDefNullCb);

            r = registerAction(modHandle, actionDefNullCbPtr);
            ExpectResult(r, EMC_ERR_INVALID_ARGUMENT, "register_action_row should reject null callback");
            r = registerAction(modHandle, actionDefPtr);
            ExpectResult(r, EMC_OK, "register_action_row valid call failed");
            r = registerAction(modHandle, actionDefDriftPtr);
            ExpectResult(r, EMC_OK, "register_action_row drift duplicate should succeed");

            setRegistrationLocked(1);
            r = registerBool(modHandle, boolDefPtr);
            ExpectResult(r, EMC_ERR_INVALID_ARGUMENT, "register_bool should reject when options-open lock is active");
            Marshal.WriteIntPtr(outHandle, new IntPtr(0x3333));
            r = registerMod(modDesc2Ptr, outHandle);
            ExpectResult(r, EMC_ERR_INVALID_ARGUMENT, "register_mod should reject when options-open lock is active");
            Assert(Marshal.ReadIntPtr(outHandle) == IntPtr.Zero, "register_mod locked must clear out_handle");
            setRegistrationLocked(0);

            setRegistryAttachEnabled(0);
            Marshal.WriteIntPtr(outHandle, new IntPtr(0x4444));
            r = registerMod(modDesc2Ptr, outHandle);
            ExpectResult(r, EMC_ERR_INTERNAL, "register_mod should return internal when registry attach is disabled");
            Assert(Marshal.ReadIntPtr(outHandle) == IntPtr.Zero, "register_mod gated-off must clear out_handle");
            r = registerBool(modHandle, boolDefPtr);
            ExpectResult(r, EMC_ERR_INTERNAL, "register_bool should return internal when registry attach is disabled");
            setRegistryAttachEnabled(1);

            return "PASS: phase2 registration matrix completed";
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

$result = [HubPhase2RegistrationHarness]::Run($DllPath, $resolvedKenshiPath)
Write-Host $result
