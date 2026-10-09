param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [string]$KenshiPath = ""
)

$ErrorActionPreference = 'Stop'

# Requires a build that exports EMC_ModHub_Test_* symbols (Debug with EMC_ENABLE_TEST_EXPORTS).

$code = @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public static class HubPhase5NumericHarness
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetApiRaw(uint requestedVersion, uint callerApiSize, IntPtr outApi, IntPtr outApiSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterModRaw(IntPtr desc, IntPtr outHandle);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterIntRaw(IntPtr mod, IntPtr def);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterFloatRaw(IntPtr mod, IntPtr def);

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
    public delegate int UiAdjustPendingIntStepRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, int stepDelta);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiSetPendingIntFromTextRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, IntPtr text);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiNormalizePendingIntTextRaw(IntPtr nsId, IntPtr modId, IntPtr settingId);

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
    public delegate int UiAdjustPendingFloatStepRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, int stepDelta);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiSetPendingFloatFromTextRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, IntPtr text);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiNormalizePendingFloatTextRaw(IntPtr nsId, IntPtr modId, IntPtr settingId);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiGetPendingFloatStateRaw(
        IntPtr nsId,
        IntPtr modId,
        IntPtr settingId,
        out float outValue,
        out int outParseError,
        IntPtr textBuffer,
        uint textBufferSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void CommitGetSummaryRaw(out uint attempted, out uint succeeded, out uint failed, out uint skipped, out int skipReason);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetIntCb(IntPtr userData, IntPtr outValue);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int SetIntCb(IntPtr userData, int value, IntPtr errBuf, uint errBufSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetFloatCb(IntPtr userData, IntPtr outValue);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int SetFloatCb(IntPtr userData, float value, IntPtr errBuf, uint errBufSize);

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
    private const int HUB_COMMIT_SKIP_REASON_NONE = 0;
    private const uint DONT_RESOLVE_DLL_REFERENCES = 0x00000001u;

    private static readonly List<Delegate> CallbackRoots = new List<Delegate>();
    private static readonly List<string> CommitSetOrder = new List<string>();

    private static int CurrentIntValue = 0;
    private static float CurrentFloatValue = 0.0f;
    private static int IntGetCount = 0;
    private static int IntSetCount = 0;
    private static int FloatGetCount = 0;
    private static int FloatSetCount = 0;

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

    private static bool NearlyEqual(float lhs, float rhs, float epsilon)
    {
        return Math.Abs(lhs - rhs) <= epsilon;
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

    private static string ReadAnsiBuffer(IntPtr buffer)
    {
        return Marshal.PtrToStringAnsi(buffer) ?? string.Empty;
    }

    private static int IntGet(IntPtr userData, IntPtr outValue)
    {
        IntGetCount += 1;
        Marshal.WriteInt32(outValue, CurrentIntValue);
        return EMC_OK;
    }

    private static int IntSet(IntPtr userData, int value, IntPtr errBuf, uint errBufSize)
    {
        IntSetCount += 1;
        CommitSetOrder.Add("int");
        CurrentIntValue = value;
        return EMC_OK;
    }

    private static int FloatGet(IntPtr userData, IntPtr outValue)
    {
        FloatGetCount += 1;
        Marshal.StructureToPtr(CurrentFloatValue, outValue, false);
        return EMC_OK;
    }

    private static int FloatSet(IntPtr userData, float value, IntPtr errBuf, uint errBufSize)
    {
        FloatSetCount += 1;
        CommitSetOrder.Add("float");
        CurrentFloatValue = value;
        return EMC_OK;
    }

    private static void ReadSummary(
        CommitGetSummaryRaw readSummary,
        out uint attempted,
        out uint succeeded,
        out uint failed,
        out uint skipped,
        out int skipReason)
    {
        readSummary(out attempted, out succeeded, out failed, out skipped, out skipReason);
    }

    public static string Run(string dllPath, string kenshiPath)
    {
        List<IntPtr> allocations = new List<IntPtr>();
        IntPtr module = IntPtr.Zero;

        TestSetRegistrationLockedRaw setRegistrationLocked = null;
        TestSetRegistryAttachEnabledRaw setRegistryAttachEnabled = null;
        MenuSetHubEnabledRaw setHubEnabled = null;
        MenuOpenRaw openOptions = null;
        MenuSaveRaw saveOptions = null;
        MenuCloseRaw closeOptions = null;

        try
        {
            CommitSetOrder.Clear();
            CurrentIntValue = 0;
            CurrentFloatValue = 0.0f;
            IntGetCount = 0;
            IntSetCount = 0;
            FloatGetCount = 0;
            FloatSetCount = 0;

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
            IntPtr setAttachProc = GetProcAddress(module, "EMC_ModHub_Test_SetRegistryAttachEnabled");
            IntPtr setHubEnabledProc = GetProcAddress(module, "EMC_ModHub_Test_Menu_SetHubEnabled");
            IntPtr openOptionsProc = GetProcAddress(module, "EMC_ModHub_Test_Menu_OpenOptionsWindow");
            IntPtr saveOptionsProc = GetProcAddress(module, "EMC_ModHub_Test_Menu_SaveOptionsWindow");
            IntPtr closeOptionsProc = GetProcAddress(module, "EMC_ModHub_Test_Menu_CloseOptionsWindow");
            IntPtr adjustIntProc = GetProcAddress(module, "EMC_ModHub_Test_UI_AdjustPendingIntStep");
            IntPtr setIntTextProc = GetProcAddress(module, "EMC_ModHub_Test_UI_SetPendingIntFromText");
            IntPtr normalizeIntTextProc = GetProcAddress(module, "EMC_ModHub_Test_UI_NormalizePendingIntText");
            IntPtr getIntStateProc = GetProcAddress(module, "EMC_ModHub_Test_UI_GetPendingIntState");
            IntPtr adjustFloatProc = GetProcAddress(module, "EMC_ModHub_Test_UI_AdjustPendingFloatStep");
            IntPtr setFloatTextProc = GetProcAddress(module, "EMC_ModHub_Test_UI_SetPendingFloatFromText");
            IntPtr normalizeFloatTextProc = GetProcAddress(module, "EMC_ModHub_Test_UI_NormalizePendingFloatText");
            IntPtr getFloatStateProc = GetProcAddress(module, "EMC_ModHub_Test_UI_GetPendingFloatState");
            IntPtr getSummaryProc = GetProcAddress(module, "EMC_ModHub_Test_Commit_GetLastSummary");

            Assert(setLockedProc != IntPtr.Zero, "Missing EMC_ModHub_Test_SetRegistrationLocked export");
            Assert(setAttachProc != IntPtr.Zero, "Missing EMC_ModHub_Test_SetRegistryAttachEnabled export");
            Assert(setHubEnabledProc != IntPtr.Zero, "Missing EMC_ModHub_Test_Menu_SetHubEnabled export");
            Assert(openOptionsProc != IntPtr.Zero, "Missing EMC_ModHub_Test_Menu_OpenOptionsWindow export");
            Assert(saveOptionsProc != IntPtr.Zero, "Missing EMC_ModHub_Test_Menu_SaveOptionsWindow export");
            Assert(closeOptionsProc != IntPtr.Zero, "Missing EMC_ModHub_Test_Menu_CloseOptionsWindow export");
            Assert(adjustIntProc != IntPtr.Zero, "Missing EMC_ModHub_Test_UI_AdjustPendingIntStep export");
            Assert(setIntTextProc != IntPtr.Zero, "Missing EMC_ModHub_Test_UI_SetPendingIntFromText export");
            Assert(normalizeIntTextProc != IntPtr.Zero, "Missing EMC_ModHub_Test_UI_NormalizePendingIntText export");
            Assert(getIntStateProc != IntPtr.Zero, "Missing EMC_ModHub_Test_UI_GetPendingIntState export");
            Assert(adjustFloatProc != IntPtr.Zero, "Missing EMC_ModHub_Test_UI_AdjustPendingFloatStep export");
            Assert(setFloatTextProc != IntPtr.Zero, "Missing EMC_ModHub_Test_UI_SetPendingFloatFromText export");
            Assert(normalizeFloatTextProc != IntPtr.Zero, "Missing EMC_ModHub_Test_UI_NormalizePendingFloatText export");
            Assert(getFloatStateProc != IntPtr.Zero, "Missing EMC_ModHub_Test_UI_GetPendingFloatState export");
            Assert(getSummaryProc != IntPtr.Zero, "Missing EMC_ModHub_Test_Commit_GetLastSummary export");

            setRegistrationLocked = (TestSetRegistrationLockedRaw)Marshal.GetDelegateForFunctionPointer(
                setLockedProc, typeof(TestSetRegistrationLockedRaw));
            setRegistryAttachEnabled = (TestSetRegistryAttachEnabledRaw)Marshal.GetDelegateForFunctionPointer(
                setAttachProc, typeof(TestSetRegistryAttachEnabledRaw));
            setHubEnabled = (MenuSetHubEnabledRaw)Marshal.GetDelegateForFunctionPointer(
                setHubEnabledProc, typeof(MenuSetHubEnabledRaw));
            openOptions = (MenuOpenRaw)Marshal.GetDelegateForFunctionPointer(openOptionsProc, typeof(MenuOpenRaw));
            saveOptions = (MenuSaveRaw)Marshal.GetDelegateForFunctionPointer(saveOptionsProc, typeof(MenuSaveRaw));
            closeOptions = (MenuCloseRaw)Marshal.GetDelegateForFunctionPointer(closeOptionsProc, typeof(MenuCloseRaw));
            UiAdjustPendingIntStepRaw adjustIntStep = (UiAdjustPendingIntStepRaw)Marshal.GetDelegateForFunctionPointer(
                adjustIntProc, typeof(UiAdjustPendingIntStepRaw));
            UiSetPendingIntFromTextRaw setIntFromText = (UiSetPendingIntFromTextRaw)Marshal.GetDelegateForFunctionPointer(
                setIntTextProc, typeof(UiSetPendingIntFromTextRaw));
            UiNormalizePendingIntTextRaw normalizeIntText = (UiNormalizePendingIntTextRaw)Marshal.GetDelegateForFunctionPointer(
                normalizeIntTextProc, typeof(UiNormalizePendingIntTextRaw));
            UiGetPendingIntStateRaw getIntState = (UiGetPendingIntStateRaw)Marshal.GetDelegateForFunctionPointer(
                getIntStateProc, typeof(UiGetPendingIntStateRaw));
            UiAdjustPendingFloatStepRaw adjustFloatStep = (UiAdjustPendingFloatStepRaw)Marshal.GetDelegateForFunctionPointer(
                adjustFloatProc, typeof(UiAdjustPendingFloatStepRaw));
            UiSetPendingFloatFromTextRaw setFloatFromText = (UiSetPendingFloatFromTextRaw)Marshal.GetDelegateForFunctionPointer(
                setFloatTextProc, typeof(UiSetPendingFloatFromTextRaw));
            UiNormalizePendingFloatTextRaw normalizeFloatText = (UiNormalizePendingFloatTextRaw)Marshal.GetDelegateForFunctionPointer(
                normalizeFloatTextProc, typeof(UiNormalizePendingFloatTextRaw));
            UiGetPendingFloatStateRaw getFloatState = (UiGetPendingFloatStateRaw)Marshal.GetDelegateForFunctionPointer(
                getFloatStateProc, typeof(UiGetPendingFloatStateRaw));
            CommitGetSummaryRaw getSummary = (CommitGetSummaryRaw)Marshal.GetDelegateForFunctionPointer(
                getSummaryProc, typeof(CommitGetSummaryRaw));

            setRegistrationLocked(0);
            setRegistryAttachEnabled(-1);
            setHubEnabled(1);

            IntPtr outApi = Marshal.AllocHGlobal(IntPtr.Size);
            IntPtr outApiSize = Marshal.AllocHGlobal(4);
            allocations.Add(outApi);
            allocations.Add(outApiSize);
            Marshal.WriteIntPtr(outApi, IntPtr.Zero);
            Marshal.WriteInt32(outApiSize, 0);

            int r = getApi(1u, 56u, outApi, outApiSize);
            ExpectResult(r, EMC_OK, "GetApi failed");

            IntPtr apiPtr = Marshal.ReadIntPtr(outApi);
            Assert(apiPtr != IntPtr.Zero, "GetApi returned null api pointer");

            RegisterModRaw registerMod = (RegisterModRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, 8), typeof(RegisterModRaw));
            RegisterIntRaw registerInt = (RegisterIntRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, 32), typeof(RegisterIntRaw));
            RegisterFloatRaw registerFloat = (RegisterFloatRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, 40), typeof(RegisterFloatRaw));

            GetIntCb getInt = IntGet;
            SetIntCb setInt = IntSet;
            GetFloatCb getFloat = FloatGet;
            SetFloatCb setFloat = FloatSet;
            CallbackRoots.Add(getInt);
            CallbackRoots.Add(setInt);
            CallbackRoots.Add(getFloat);
            CallbackRoots.Add(setFloat);

            IntPtr getIntPtr = Marshal.GetFunctionPointerForDelegate(getInt);
            IntPtr setIntPtr = Marshal.GetFunctionPointerForDelegate(setInt);
            IntPtr getFloatPtr = Marshal.GetFunctionPointerForDelegate(getFloat);
            IntPtr setFloatPtr = Marshal.GetFunctionPointerForDelegate(setFloat);

            IntPtr nsId = AllocAnsi(allocations, "emkej.qol");
            IntPtr nsDisplay = AllocAnsi(allocations, "QoL");
            IntPtr modId = AllocAnsi(allocations, "phase5_test_mod");
            IntPtr modDisplay = AllocAnsi(allocations, "Phase5 Test Mod");
            IntPtr intSettingId = AllocAnsi(allocations, "distance_limit");
            IntPtr intLabel = AllocAnsi(allocations, "Distance limit");
            IntPtr intDesc = AllocAnsi(allocations, "Distance limit setting");
            IntPtr floatSettingId = AllocAnsi(allocations, "radius_scale");
            IntPtr floatLabel = AllocAnsi(allocations, "Radius scale");
            IntPtr floatDesc = AllocAnsi(allocations, "Radius scale setting");

            EMC_ModDescriptorV1 modDesc = new EMC_ModDescriptorV1
            {
                namespace_id = nsId,
                namespace_display_name = nsDisplay,
                mod_id = modId,
                mod_display_name = modDisplay,
                mod_user_data = IntPtr.Zero
            };
            IntPtr modDescPtr = AllocStruct(allocations, modDesc);

            EMC_IntSettingDefV1 intDef = new EMC_IntSettingDefV1
            {
                setting_id = intSettingId,
                label = intLabel,
                description = intDesc,
                user_data = new IntPtr(5001),
                min_value = -10,
                max_value = 10,
                step = 4,
                get_value = getIntPtr,
                set_value = setIntPtr
            };
            IntPtr intDefPtr = AllocStruct(allocations, intDef);

            EMC_FloatSettingDefV1 floatDef = new EMC_FloatSettingDefV1
            {
                setting_id = floatSettingId,
                label = floatLabel,
                description = floatDesc,
                user_data = new IntPtr(6001),
                min_value = -1.0f,
                max_value = 1.0f,
                step = 0.4f,
                display_decimals = 3u,
                get_value = getFloatPtr,
                set_value = setFloatPtr
            };
            IntPtr floatDefPtr = AllocStruct(allocations, floatDef);

            IntPtr outHandle = Marshal.AllocHGlobal(IntPtr.Size);
            allocations.Add(outHandle);
            Marshal.WriteIntPtr(outHandle, IntPtr.Zero);

            r = registerMod(modDescPtr, outHandle);
            ExpectResult(r, EMC_OK, "register_mod failed");
            IntPtr modHandle = Marshal.ReadIntPtr(outHandle);
            Assert(modHandle != IntPtr.Zero, "register_mod returned null handle");

            r = registerInt(modHandle, intDefPtr);
            ExpectResult(r, EMC_OK, "register_int failed");
            r = registerFloat(modHandle, floatDefPtr);
            ExpectResult(r, EMC_OK, "register_float failed");

            openOptions();
            Assert(IntGetCount == 1, "Expected one initial int get on options open");
            Assert(FloatGetCount == 1, "Expected one initial float get on options open");

            saveOptions();
            uint attempted;
            uint succeeded;
            uint failed;
            uint skipped;
            int skipReason;
            ReadSummary(getSummary, out attempted, out succeeded, out failed, out skipped, out skipReason);
            Assert(attempted == 0u && succeeded == 0u && failed == 0u && skipped == 0u, "Expected empty summary when nothing is dirty");
            Assert(skipReason == HUB_COMMIT_SKIP_REASON_NONE, "Expected skip reason NONE when nothing is dirty");

            IntPtr intTextBuffer = Marshal.AllocHGlobal(64);
            IntPtr floatTextBuffer = Marshal.AllocHGlobal(64);
            allocations.Add(intTextBuffer);
            allocations.Add(floatTextBuffer);

            int pendingIntValue;
            int intParseError;
            float pendingFloatValue;
            int floatParseError;

            IntPtr intUnnormalizedText = AllocAnsi(allocations, "3");
            IntPtr floatUnnormalizedText = AllocAnsi(allocations, "0.31");
            r = setIntFromText(nsId, modId, intSettingId, intUnnormalizedText);
            ExpectResult(r, EMC_OK, "set_pending_int_from_text unnormalized text failed");
            r = setFloatFromText(nsId, modId, floatSettingId, floatUnnormalizedText);
            ExpectResult(r, EMC_OK, "set_pending_float_from_text unnormalized text failed");

            r = getIntState(nsId, modId, intSettingId, out pendingIntValue, out intParseError, intTextBuffer, 64u);
            ExpectResult(r, EMC_OK, "get_pending_int_state after text edit failed");
            Assert(pendingIntValue == 2, "Int pending value should snap immediately after text edit");
            Assert(intParseError == 0, "Int parse error should remain clear for valid text edit");
            Assert(ReadAnsiBuffer(intTextBuffer) == "3", "Int pending text should remain user-entered until explicit normalization");

            r = getFloatState(nsId, modId, floatSettingId, out pendingFloatValue, out floatParseError, floatTextBuffer, 64u);
            ExpectResult(r, EMC_OK, "get_pending_float_state after text edit failed");
            Assert(NearlyEqual(pendingFloatValue, 0.2f, 1e-4f), "Float pending value should snap immediately after text edit");
            Assert(floatParseError == 0, "Float parse error should remain clear for valid text edit");
            Assert(ReadAnsiBuffer(floatTextBuffer) == "0.31", "Float pending text should remain user-entered until explicit normalization");

            r = normalizeIntText(nsId, modId, intSettingId);
            ExpectResult(r, EMC_OK, "normalize_pending_int_text failed");
            r = normalizeFloatText(nsId, modId, floatSettingId);
            ExpectResult(r, EMC_OK, "normalize_pending_float_text failed");

            r = getIntState(nsId, modId, intSettingId, out pendingIntValue, out intParseError, intTextBuffer, 64u);
            ExpectResult(r, EMC_OK, "get_pending_int_state after normalization failed");
            Assert(pendingIntValue == 2, "Int pending value should remain snapped after normalization");
            Assert(intParseError == 0, "Int normalize should clear parse error state");
            Assert(ReadAnsiBuffer(intTextBuffer) == "2", "Int normalization should canonicalize the pending text");

            r = getFloatState(nsId, modId, floatSettingId, out pendingFloatValue, out floatParseError, floatTextBuffer, 64u);
            ExpectResult(r, EMC_OK, "get_pending_float_state after normalization failed");
            Assert(NearlyEqual(pendingFloatValue, 0.2f, 1e-4f), "Float pending value should remain snapped after normalization");
            Assert(floatParseError == 0, "Float normalize should clear parse error state");
            Assert(ReadAnsiBuffer(floatTextBuffer) == "0.200", "Float normalization should canonicalize the pending text");

            saveOptions();
            ReadSummary(getSummary, out attempted, out succeeded, out failed, out skipped, out skipReason);
            Assert(attempted == 2u && succeeded == 2u && failed == 0u && skipped == 0u, "Normalized text save should commit both numeric settings");
            Assert(CurrentIntValue == 2, "Normalized int text save should commit the snapped int value");
            Assert(NearlyEqual(CurrentFloatValue, 0.2f, 1e-4f), "Normalized float text save should commit the snapped float value");
            Assert(IntSetCount == 1 && FloatSetCount == 1, "Normalized text save should call both numeric set callbacks exactly once");

            IntSetCount = 0;
            FloatSetCount = 0;
            CommitSetOrder.Clear();

            r = adjustIntStep(nsId, modId, intSettingId, 5);
            ExpectResult(r, EMC_OK, "adjust_pending_int_step clamp-high failed");
            r = adjustFloatStep(nsId, modId, floatSettingId, 10);
            ExpectResult(r, EMC_OK, "adjust_pending_float_step clamp-high failed");

            saveOptions();
            ReadSummary(getSummary, out attempted, out succeeded, out failed, out skipped, out skipReason);
            Assert(attempted == 2u && succeeded == 2u && failed == 0u && skipped == 0u, "Expected two numeric commits after step adjustments");
            Assert(skipReason == HUB_COMMIT_SKIP_REASON_NONE, "Expected skip reason NONE after numeric step commit");
            Assert(IntSetCount == 1 && FloatSetCount == 1, "Expected one int and one float set callback after step commit");
            Assert(CommitSetOrder.Count >= 2, "Expected set order to capture int/float commits");
            Assert(CommitSetOrder[0] == "int" && CommitSetOrder[1] == "float", "Numeric commit order must follow registration order");
            Assert(CurrentIntValue == 10, "Int step clamp-high did not resolve to max bound");
            Assert(NearlyEqual(CurrentFloatValue, 1.0f, 1e-4f), "Float step clamp-high did not resolve to max bound");

            IntPtr intTieText = AllocAnsi(allocations, "-8");
            IntPtr floatTieText = AllocAnsi(allocations, "-0.4");
            r = setIntFromText(nsId, modId, intSettingId, intTieText);
            ExpectResult(r, EMC_OK, "set_pending_int_from_text midpoint-tie failed");
            r = setFloatFromText(nsId, modId, floatSettingId, floatTieText);
            ExpectResult(r, EMC_OK, "set_pending_float_from_text midpoint-tie failed");

            saveOptions();
            ReadSummary(getSummary, out attempted, out succeeded, out failed, out skipped, out skipReason);
            Assert(attempted == 2u && succeeded == 2u && failed == 0u && skipped == 0u, "Midpoint-tie save should commit both numeric settings");
            Assert(CurrentIntValue == -6, "Int midpoint tie should snap toward zero");
            Assert(NearlyEqual(CurrentFloatValue, -0.2f, 1e-4f), "Float midpoint tie should snap toward zero");

            IntPtr intInvalidText = AllocAnsi(allocations, "abc");
            IntPtr floatInvalidText = AllocAnsi(allocations, "abc");
            r = setIntFromText(nsId, modId, intSettingId, intInvalidText);
            ExpectResult(r, EMC_OK, "set_pending_int_from_text invalid parse should not hard-fail");
            r = setFloatFromText(nsId, modId, floatSettingId, floatInvalidText);
            ExpectResult(r, EMC_OK, "set_pending_float_from_text invalid parse should not hard-fail");

            r = getIntState(nsId, modId, intSettingId, out pendingIntValue, out intParseError, intTextBuffer, 64u);
            ExpectResult(r, EMC_OK, "get_pending_int_state after invalid parse failed");
            Assert(intParseError == 1, "Invalid int text should set parse-error state");
            Assert(ReadAnsiBuffer(intTextBuffer) == "abc", "Invalid int text should remain visible before normalization");

            r = getFloatState(nsId, modId, floatSettingId, out pendingFloatValue, out floatParseError, floatTextBuffer, 64u);
            ExpectResult(r, EMC_OK, "get_pending_float_state after invalid parse failed");
            Assert(floatParseError == 1, "Invalid float text should set parse-error state");
            Assert(ReadAnsiBuffer(floatTextBuffer) == "abc", "Invalid float text should remain visible before normalization");

            saveOptions();
            ReadSummary(getSummary, out attempted, out succeeded, out failed, out skipped, out skipReason);
            Assert(attempted == 0u && succeeded == 0u && failed == 0u && skipped == 0u, "Invalid numeric text rows should be skipped during commit");
            Assert(IntSetCount == 2 && FloatSetCount == 2, "Invalid parse save should not call numeric set callbacks");

            r = normalizeIntText(nsId, modId, intSettingId);
            ExpectResult(r, EMC_OK, "normalize_pending_int_text after invalid parse failed");
            r = normalizeFloatText(nsId, modId, floatSettingId);
            ExpectResult(r, EMC_OK, "normalize_pending_float_text after invalid parse failed");

            r = getIntState(nsId, modId, intSettingId, out pendingIntValue, out intParseError, intTextBuffer, 64u);
            ExpectResult(r, EMC_OK, "get_pending_int_state after invalid normalization failed");
            Assert(intParseError == 0, "Int normalize should clear invalid parse state");
            Assert(ReadAnsiBuffer(intTextBuffer) == "-6", "Int normalize should restore the canonical snapped value after invalid text");

            r = getFloatState(nsId, modId, floatSettingId, out pendingFloatValue, out floatParseError, floatTextBuffer, 64u);
            ExpectResult(r, EMC_OK, "get_pending_float_state after invalid normalization failed");
            Assert(floatParseError == 0, "Float normalize should clear invalid parse state");
            Assert(ReadAnsiBuffer(floatTextBuffer) == "-0.200", "Float normalize should restore the canonical snapped value after invalid text");

            IntPtr intPositiveTieText = AllocAnsi(allocations, "8");
            IntPtr floatPositiveTieText = AllocAnsi(allocations, "0.4");
            r = setIntFromText(nsId, modId, intSettingId, intPositiveTieText);
            ExpectResult(r, EMC_OK, "set_pending_int_from_text positive midpoint-tie failed");
            r = setFloatFromText(nsId, modId, floatSettingId, floatPositiveTieText);
            ExpectResult(r, EMC_OK, "set_pending_float_from_text positive midpoint-tie failed");

            saveOptions();
            ReadSummary(getSummary, out attempted, out succeeded, out failed, out skipped, out skipReason);
            Assert(attempted == 2u && succeeded == 2u && failed == 0u && skipped == 0u, "Positive midpoint-tie save should commit both numeric settings");
            Assert(CurrentIntValue == 6, "Positive int midpoint tie should snap toward zero");
            Assert(NearlyEqual(CurrentFloatValue, 0.2f, 1e-4f), "Positive float midpoint tie should snap toward zero");

            IntPtr intClampHighText = AllocAnsi(allocations, "999");
            IntPtr floatClampHighText = AllocAnsi(allocations, "999");
            r = setIntFromText(nsId, modId, intSettingId, intClampHighText);
            ExpectResult(r, EMC_OK, "set_pending_int_from_text clamp-high failed");
            r = setFloatFromText(nsId, modId, floatSettingId, floatClampHighText);
            ExpectResult(r, EMC_OK, "set_pending_float_from_text clamp-high failed");

            saveOptions();
            ReadSummary(getSummary, out attempted, out succeeded, out failed, out skipped, out skipReason);
            Assert(attempted == 2u && succeeded == 2u && failed == 0u && skipped == 0u, "Clamp-high text save should commit both numeric settings");
            Assert(CurrentIntValue == 10, "Int text clamp-high did not resolve to max bound");
            Assert(NearlyEqual(CurrentFloatValue, 1.0f, 1e-4f), "Float text clamp-high did not resolve to max bound");

            closeOptions();
            return "PASS: phase5 numeric matrix completed";
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

$result = [HubPhase5NumericHarness]::Run($DllPath, $resolvedKenshiPath)
Write-Host $result
