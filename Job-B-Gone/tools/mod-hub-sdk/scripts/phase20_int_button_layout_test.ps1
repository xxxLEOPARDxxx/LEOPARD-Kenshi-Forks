param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [string]$KenshiPath = ""
)

$ErrorActionPreference = 'Stop'

$code = @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public static class HubPhase20IntButtonLayoutHarness
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetApiRaw(uint requestedVersion, uint callerApiSize, out IntPtr outApi, out uint outApiSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterModRaw(IntPtr desc, out IntPtr outHandle);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterIntRaw(IntPtr mod, IntPtr def);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterIntV2Raw(IntPtr mod, IntPtr def);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void SetIntRaw(int value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void VoidRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiAdjustPendingIntStepRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, int stepDelta);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiAdjustPendingIntDeltaRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, int delta);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiGetPendingIntStateRaw(
        IntPtr nsId,
        IntPtr modId,
        IntPtr settingId,
        out int outValue,
        out int outParseError,
        IntPtr textBuffer,
        uint textBufferSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiGetIntButtonLayoutRaw(
        IntPtr nsId,
        IntPtr modId,
        IntPtr settingId,
        out int outUseCustomButtons,
        IntPtr outDecButtonDeltas,
        uint decButtonDeltasCount,
        IntPtr outIncButtonDeltas,
        uint incButtonDeltasCount);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetIntCb(IntPtr userData, IntPtr outValue);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int SetIntCb(IntPtr userData, int value, IntPtr errBuf, uint errBufSize);

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
        public IntPtr register_int_setting_v2;
    }

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
    public struct EMC_IntSettingDefV2
    {
        public IntPtr setting_id;
        public IntPtr label;
        public IntPtr description;
        public IntPtr user_data;
        public int min_value;
        public int max_value;
        public int step;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)] public int[] dec_button_deltas;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)] public int[] inc_button_deltas;
        public IntPtr get_value;
        public IntPtr set_value;
    }

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

    private const int EMC_OK = 0;
    private const int EMC_ERR_INVALID_ARGUMENT = 1;
    private const uint DONT_RESOLVE_DLL_REFERENCES = 0x00000001u;

    private static readonly List<Delegate> CallbackRoots = new List<Delegate>();
    private static int V1Value = 50;
    private static int V2SimpleValue = 4;
    private static int V2SparseValue = 10;

    private static void Assert(bool condition, string message)
    {
        if (!condition)
        {
            throw new Exception(message);
        }
    }

    private static void ExpectResult(int actual, int expected, string context)
    {
        if (actual != expected)
        {
            throw new Exception(context + " (expected=" + expected + ", actual=" + actual + ")");
        }
    }

    private static T Bind<T>(IntPtr module, string exportName)
    {
        IntPtr proc = GetProcAddress(module, exportName);
        Assert(proc != IntPtr.Zero, "Missing export: " + exportName);
        return (T)(object)Marshal.GetDelegateForFunctionPointer(proc, typeof(T));
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

    private static string ReadAnsiBuffer(IntPtr buffer)
    {
        return Marshal.PtrToStringAnsi(buffer) ?? string.Empty;
    }

    private static int GetV1(IntPtr userData, IntPtr outValue)
    {
        Marshal.WriteInt32(outValue, V1Value);
        return EMC_OK;
    }

    private static int SetV1(IntPtr userData, int value, IntPtr errBuf, uint errBufSize)
    {
        V1Value = value;
        return EMC_OK;
    }

    private static int GetV2Simple(IntPtr userData, IntPtr outValue)
    {
        Marshal.WriteInt32(outValue, V2SimpleValue);
        return EMC_OK;
    }

    private static int SetV2Simple(IntPtr userData, int value, IntPtr errBuf, uint errBufSize)
    {
        V2SimpleValue = value;
        return EMC_OK;
    }

    private static int GetV2Sparse(IntPtr userData, IntPtr outValue)
    {
        Marshal.WriteInt32(outValue, V2SparseValue);
        return EMC_OK;
    }

    private static int SetV2Sparse(IntPtr userData, int value, IntPtr errBuf, uint errBufSize)
    {
        V2SparseValue = value;
        return EMC_OK;
    }

    private static void ReadLayout(UiGetIntButtonLayoutRaw getLayout, IntPtr nsId, IntPtr modId, IntPtr settingId, out int useCustom, out int[] dec, out int[] inc)
    {
        IntPtr decPtr = Marshal.AllocHGlobal(sizeof(int) * 3);
        IntPtr incPtr = Marshal.AllocHGlobal(sizeof(int) * 3);
        try
        {
            ExpectResult(getLayout(nsId, modId, settingId, out useCustom, decPtr, 3u, incPtr, 3u), EMC_OK, "get_int_button_layout failed");
            dec = new int[3];
            inc = new int[3];
            Marshal.Copy(decPtr, dec, 0, 3);
            Marshal.Copy(incPtr, inc, 0, 3);
        }
        finally
        {
            Marshal.FreeHGlobal(decPtr);
            Marshal.FreeHGlobal(incPtr);
        }
    }

    private static void AssertArray(int[] actual, int[] expected, string context)
    {
        Assert(actual.Length == expected.Length, context + " length mismatch");
        for (int i = 0; i < actual.Length; i++)
        {
            Assert(actual[i] == expected[i], context + " mismatch at index " + i);
        }
    }

    public static string Run(string dllPath, string kenshiPath)
    {
        List<IntPtr> allocations = new List<IntPtr>();
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

            GetApiRaw getApi = Bind<GetApiRaw>(module, "EMC_ModHub_GetApi");
            SetIntRaw setRegistrationLocked = Bind<SetIntRaw>(module, "EMC_ModHub_Test_SetRegistrationLocked");
            SetIntRaw setRegistryAttachEnabled = Bind<SetIntRaw>(module, "EMC_ModHub_Test_SetRegistryAttachEnabled");
            SetIntRaw setHubEnabled = Bind<SetIntRaw>(module, "EMC_ModHub_Test_Menu_SetHubEnabled");
            VoidRaw openOptionsWindow = Bind<VoidRaw>(module, "EMC_ModHub_Test_Menu_OpenOptionsWindow");
            VoidRaw closeOptionsWindow = Bind<VoidRaw>(module, "EMC_ModHub_Test_Menu_CloseOptionsWindow");
            UiAdjustPendingIntStepRaw adjustIntStep = Bind<UiAdjustPendingIntStepRaw>(module, "EMC_ModHub_Test_UI_AdjustPendingIntStep");
            UiAdjustPendingIntDeltaRaw adjustIntDelta = Bind<UiAdjustPendingIntDeltaRaw>(module, "EMC_ModHub_Test_UI_AdjustPendingIntDelta");
            UiGetPendingIntStateRaw getIntState = Bind<UiGetPendingIntStateRaw>(module, "EMC_ModHub_Test_UI_GetPendingIntState");
            UiGetIntButtonLayoutRaw getIntLayout = Bind<UiGetIntButtonLayoutRaw>(module, "EMC_ModHub_Test_UI_GetIntButtonLayout");

            IntPtr apiPtr;
            uint outApiSize;
            ExpectResult(getApi(1u, 80u, out apiPtr, out outApiSize), EMC_OK, "get_api failed");
            Assert(apiPtr != IntPtr.Zero, "api pointer is null");
            Assert(outApiSize >= 80u, "api size does not expose int_v2 support");

            HubApiV1 api = (HubApiV1)Marshal.PtrToStructure(apiPtr, typeof(HubApiV1));
            Assert(api.register_mod != IntPtr.Zero, "register_mod pointer missing");
            Assert(api.register_int_setting != IntPtr.Zero, "register_int_setting pointer missing");
            Assert(api.register_int_setting_v2 != IntPtr.Zero, "register_int_setting_v2 pointer missing");

            RegisterModRaw registerMod = (RegisterModRaw)Marshal.GetDelegateForFunctionPointer(api.register_mod, typeof(RegisterModRaw));
            RegisterIntRaw registerInt = (RegisterIntRaw)Marshal.GetDelegateForFunctionPointer(api.register_int_setting, typeof(RegisterIntRaw));
            RegisterIntV2Raw registerIntV2 = (RegisterIntV2Raw)Marshal.GetDelegateForFunctionPointer(api.register_int_setting_v2, typeof(RegisterIntV2Raw));

            GetIntCb getV1 = new GetIntCb(GetV1);
            SetIntCb setV1 = new SetIntCb(SetV1);
            GetIntCb getV2Simple = new GetIntCb(GetV2Simple);
            SetIntCb setV2Simple = new SetIntCb(SetV2Simple);
            GetIntCb getV2Sparse = new GetIntCb(GetV2Sparse);
            SetIntCb setV2Sparse = new SetIntCb(SetV2Sparse);
            CallbackRoots.Add(getV1);
            CallbackRoots.Add(setV1);
            CallbackRoots.Add(getV2Simple);
            CallbackRoots.Add(setV2Simple);
            CallbackRoots.Add(getV2Sparse);
            CallbackRoots.Add(setV2Sparse);

            IntPtr nsId = AllocAnsi(allocations, "phase20.numeric");
            IntPtr nsDisplay = AllocAnsi(allocations, "Phase20 Numeric");
            IntPtr modId = AllocAnsi(allocations, "phase20_mod");
            IntPtr modDisplay = AllocAnsi(allocations, "Phase20 Mod");
            IntPtr modDescPtr = AllocStruct(allocations, new EMC_ModDescriptorV1 {
                namespace_id = nsId,
                namespace_display_name = nsDisplay,
                mod_id = modId,
                mod_display_name = modDisplay,
                mod_user_data = IntPtr.Zero
            });

            IntPtr modHandle;
            ExpectResult(registerMod(modDescPtr, out modHandle), EMC_OK, "register_mod failed");
            Assert(modHandle != IntPtr.Zero, "mod handle is null");

            IntPtr v1SettingId = AllocAnsi(allocations, "legacy_int");
            IntPtr v2SimpleSettingId = AllocAnsi(allocations, "custom_simple");
            IntPtr v2SparseSettingId = AllocAnsi(allocations, "custom_sparse");

            IntPtr v1DefPtr = AllocStruct(allocations, new EMC_IntSettingDefV1 {
                setting_id = v1SettingId,
                label = AllocAnsi(allocations, "Legacy Int"),
                description = AllocAnsi(allocations, "Legacy int"),
                user_data = IntPtr.Zero,
                min_value = 0,
                max_value = 100,
                step = 5,
                get_value = Marshal.GetFunctionPointerForDelegate(getV1),
                set_value = Marshal.GetFunctionPointerForDelegate(setV1)
            });
            ExpectResult(registerInt(modHandle, v1DefPtr), EMC_OK, "register legacy int failed");

            IntPtr v2SimpleDefPtr = AllocStruct(allocations, new EMC_IntSettingDefV2 {
                setting_id = v2SimpleSettingId,
                label = AllocAnsi(allocations, "Custom Simple"),
                description = AllocAnsi(allocations, "Custom simple int"),
                user_data = IntPtr.Zero,
                min_value = 0,
                max_value = 9,
                step = 1,
                dec_button_deltas = new int[] { 1, 0, 0 },
                inc_button_deltas = new int[] { 1, 0, 0 },
                get_value = Marshal.GetFunctionPointerForDelegate(getV2Simple),
                set_value = Marshal.GetFunctionPointerForDelegate(setV2Simple)
            });
            ExpectResult(registerIntV2(modHandle, v2SimpleDefPtr), EMC_OK, "register custom simple int failed");

            IntPtr v2SparseDefPtr = AllocStruct(allocations, new EMC_IntSettingDefV2 {
                setting_id = v2SparseSettingId,
                label = AllocAnsi(allocations, "Custom Sparse"),
                description = AllocAnsi(allocations, "Custom sparse int"),
                user_data = IntPtr.Zero,
                min_value = 0,
                max_value = 20,
                step = 1,
                dec_button_deltas = new int[] { 3, 0, 1 },
                inc_button_deltas = new int[] { 1, 0, 7 },
                get_value = Marshal.GetFunctionPointerForDelegate(getV2Sparse),
                set_value = Marshal.GetFunctionPointerForDelegate(setV2Sparse)
            });
            ExpectResult(registerIntV2(modHandle, v2SparseDefPtr), EMC_OK, "register custom sparse int failed");

            IntPtr invalidNegativeId = AllocAnsi(allocations, "invalid_negative");
            IntPtr invalidNegativeDefPtr = AllocStruct(allocations, new EMC_IntSettingDefV2 {
                setting_id = invalidNegativeId,
                label = AllocAnsi(allocations, "Invalid Negative"),
                description = AllocAnsi(allocations, "Invalid negative"),
                user_data = IntPtr.Zero,
                min_value = 0,
                max_value = 20,
                step = 1,
                dec_button_deltas = new int[] { -1, 0, 0 },
                inc_button_deltas = new int[] { 1, 0, 0 },
                get_value = Marshal.GetFunctionPointerForDelegate(getV2Sparse),
                set_value = Marshal.GetFunctionPointerForDelegate(setV2Sparse)
            });
            ExpectResult(registerIntV2(modHandle, invalidNegativeDefPtr), EMC_ERR_INVALID_ARGUMENT, "invalid negative delta should fail");

            IntPtr invalidOrderId = AllocAnsi(allocations, "invalid_order");
            IntPtr invalidOrderDefPtr = AllocStruct(allocations, new EMC_IntSettingDefV2 {
                setting_id = invalidOrderId,
                label = AllocAnsi(allocations, "Invalid Order"),
                description = AllocAnsi(allocations, "Invalid order"),
                user_data = IntPtr.Zero,
                min_value = 0,
                max_value = 20,
                step = 5,
                dec_button_deltas = new int[] { 5, 10, 0 },
                inc_button_deltas = new int[] { 5, 10, 0 },
                get_value = Marshal.GetFunctionPointerForDelegate(getV1),
                set_value = Marshal.GetFunctionPointerForDelegate(setV1)
            });
            ExpectResult(registerIntV2(modHandle, invalidOrderDefPtr), EMC_ERR_INVALID_ARGUMENT, "out-of-order deltas should fail");

            IntPtr invalidDuplicateId = AllocAnsi(allocations, "invalid_duplicate");
            IntPtr invalidDuplicateDefPtr = AllocStruct(allocations, new EMC_IntSettingDefV2 {
                setting_id = invalidDuplicateId,
                label = AllocAnsi(allocations, "Invalid Duplicate"),
                description = AllocAnsi(allocations, "Invalid duplicate"),
                user_data = IntPtr.Zero,
                min_value = 0,
                max_value = 20,
                step = 5,
                dec_button_deltas = new int[] { 10, 5, 0 },
                inc_button_deltas = new int[] { 5, 5, 0 },
                get_value = Marshal.GetFunctionPointerForDelegate(getV1),
                set_value = Marshal.GetFunctionPointerForDelegate(setV1)
            });
            ExpectResult(registerIntV2(modHandle, invalidDuplicateDefPtr), EMC_ERR_INVALID_ARGUMENT, "duplicate deltas should fail");

            IntPtr invalidDivisibilityId = AllocAnsi(allocations, "invalid_divisibility");
            IntPtr invalidDivisibilityDefPtr = AllocStruct(allocations, new EMC_IntSettingDefV2 {
                setting_id = invalidDivisibilityId,
                label = AllocAnsi(allocations, "Invalid Divisibility"),
                description = AllocAnsi(allocations, "Invalid divisibility"),
                user_data = IntPtr.Zero,
                min_value = 0,
                max_value = 20,
                step = 5,
                dec_button_deltas = new int[] { 10, 5, 0 },
                inc_button_deltas = new int[] { 5, 7, 0 },
                get_value = Marshal.GetFunctionPointerForDelegate(getV1),
                set_value = Marshal.GetFunctionPointerForDelegate(setV1)
            });
            ExpectResult(registerIntV2(modHandle, invalidDivisibilityDefPtr), EMC_ERR_INVALID_ARGUMENT, "non-divisible deltas should fail");

            setRegistrationLocked(0);
            setRegistryAttachEnabled(1);
            setHubEnabled(1);
            openOptionsWindow();

            int useCustom;
            int[] dec;
            int[] inc;

            ReadLayout(getIntLayout, nsId, modId, v1SettingId, out useCustom, out dec, out inc);
            Assert(useCustom == 0, "legacy int should not use custom buttons");
            AssertArray(dec, new int[] { 10, 5, 1 }, "legacy dec layout");
            AssertArray(inc, new int[] { 1, 5, 10 }, "legacy inc layout");

            ReadLayout(getIntLayout, nsId, modId, v2SimpleSettingId, out useCustom, out dec, out inc);
            Assert(useCustom == 1, "simple v2 row should use custom buttons");
            AssertArray(dec, new int[] { 1, 0, 0 }, "simple v2 dec layout");
            AssertArray(inc, new int[] { 1, 0, 0 }, "simple v2 inc layout");

            ReadLayout(getIntLayout, nsId, modId, v2SparseSettingId, out useCustom, out dec, out inc);
            Assert(useCustom == 1, "sparse v2 row should use custom buttons");
            AssertArray(dec, new int[] { 3, 0, 1 }, "sparse v2 dec layout");
            AssertArray(inc, new int[] { 1, 0, 7 }, "sparse v2 inc layout");

            IntPtr textBuffer = Marshal.AllocHGlobal(128);
            try
            {
                int value;
                int parseError;

                ExpectResult(adjustIntStep(nsId, modId, v1SettingId, 1), EMC_OK, "legacy adjust step failed");
                ExpectResult(getIntState(nsId, modId, v1SettingId, out value, out parseError, textBuffer, 128u), EMC_OK, "legacy get state failed");
                Assert(value == 55, "legacy int should still use step multiplier semantics");
                Assert(parseError == 0, "legacy int parse error mismatch");
                Assert(ReadAnsiBuffer(textBuffer) == "55", "legacy int text mismatch");

                ExpectResult(adjustIntDelta(nsId, modId, v2SparseSettingId, 7), EMC_OK, "v2 exact delta +7 failed");
                ExpectResult(getIntState(nsId, modId, v2SparseSettingId, out value, out parseError, textBuffer, 128u), EMC_OK, "v2 get state after +7 failed");
                Assert(value == 17, "v2 int should apply exact +7 delta");
                Assert(ReadAnsiBuffer(textBuffer) == "17", "v2 int text after +7 mismatch");

                ExpectResult(adjustIntDelta(nsId, modId, v2SparseSettingId, -3), EMC_OK, "v2 exact delta -3 failed");
                ExpectResult(getIntState(nsId, modId, v2SparseSettingId, out value, out parseError, textBuffer, 128u), EMC_OK, "v2 get state after -3 failed");
                Assert(value == 14, "v2 int should apply exact -3 delta");
                Assert(ReadAnsiBuffer(textBuffer) == "14", "v2 int text after -3 mismatch");
            }
            finally
            {
                Marshal.FreeHGlobal(textBuffer);
                closeOptionsWindow();
            }

            return "PASS: phase20 int button layout completed";
        }
        finally
        {
            for (int i = allocations.Count - 1; i >= 0; i--)
            {
                Marshal.FreeHGlobal(allocations[i]);
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
$result = [HubPhase20IntButtonLayoutHarness]::Run($DllPath, $KenshiPath)
Write-Host $result
