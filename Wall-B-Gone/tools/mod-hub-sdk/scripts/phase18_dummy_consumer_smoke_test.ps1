param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [string]$KenshiPath = ""
)

$ErrorActionPreference = "Stop"

$code = @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public static class Phase18DummyConsumerSmokeHarness
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetApiRaw(uint requestedVersion, uint callerApiSize, IntPtr outApi, IntPtr outApiSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterModRaw(IntPtr desc, IntPtr outHandle);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterBoolRaw(IntPtr mod, IntPtr def);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterIntRaw(IntPtr mod, IntPtr def);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int SetPendingBoolRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, int value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int SetPendingIntTextRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, IntPtr text);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int CountSettingsForModRaw(IntPtr nsId, IntPtr modId);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void SetIntRaw(int value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void VoidRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void GetSummaryRaw(out uint attempted, out uint succeeded, out uint failed, out uint skipped, out int skipReason);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetBoolCb(IntPtr userData, IntPtr outValue);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int SetBoolCb(IntPtr userData, int value, IntPtr errBuf, uint errBufSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetIntCb(IntPtr userData, IntPtr outValue);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int SetIntCb(IntPtr userData, int value, IntPtr errBuf, uint errBufSize);

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
    private const int EMC_HUB_API_VERSION_1 = 1;
    private const int HUB_COMMIT_SKIP_REASON_NONE = 0;

    private static readonly List<Delegate> CallbackRoots = new List<Delegate>();

    private static int CurrentBoolValue = 1;
    private static int CurrentIntValue = 10;
    private static int BoolSetCount = 0;
    private static int IntSetCount = 0;

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

    private static IntPtr ReadFnPtr(IntPtr apiPtr, int offset)
    {
        return Marshal.ReadIntPtr(apiPtr, offset);
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

    private static int BoolGet(IntPtr userData, IntPtr outValue)
    {
        Marshal.WriteInt32(outValue, CurrentBoolValue);
        return EMC_OK;
    }

    private static int BoolSet(IntPtr userData, int value, IntPtr errBuf, uint errBufSize)
    {
        BoolSetCount += 1;
        CurrentBoolValue = value != 0 ? 1 : 0;
        return EMC_OK;
    }

    private static int IntGet(IntPtr userData, IntPtr outValue)
    {
        Marshal.WriteInt32(outValue, CurrentIntValue);
        return EMC_OK;
    }

    private static int IntSet(IntPtr userData, int value, IntPtr errBuf, uint errBufSize)
    {
        IntSetCount += 1;
        CurrentIntValue = value;
        return EMC_OK;
    }

    private static void AssertSummary(
        GetSummaryRaw getSummary,
        uint expectedAttempted,
        uint expectedSucceeded,
        uint expectedFailed,
        uint expectedSkipped,
        int expectedSkipReason,
        string context)
    {
        uint attempted;
        uint succeeded;
        uint failed;
        uint skipped;
        int skipReason;
        getSummary(out attempted, out succeeded, out failed, out skipped, out skipReason);

        Assert(attempted == expectedAttempted, context + " attempted mismatch");
        Assert(succeeded == expectedSucceeded, context + " succeeded mismatch");
        Assert(failed == expectedFailed, context + " failed mismatch");
        Assert(skipped == expectedSkipped, context + " skipped mismatch");
        Assert(skipReason == expectedSkipReason, context + " skip reason mismatch");
    }

    public static string Run(string dllPath, string kenshiPath)
    {
        IntPtr module = IntPtr.Zero;
        List<IntPtr> allocations = new List<IntPtr>();

        SetIntRaw setRegistrationLocked = null;
        SetIntRaw setRegistryAttachEnabled = null;
        SetIntRaw setHubEnabled = null;
        VoidRaw closeOptionsWindow = null;

        try
        {
            CallbackRoots.Clear();
            CurrentBoolValue = 1;
            CurrentIntValue = 10;
            BoolSetCount = 0;
            IntSetCount = 0;

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

            setRegistrationLocked = (SetIntRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_SetRegistrationLocked"), typeof(SetIntRaw));
            setRegistryAttachEnabled = (SetIntRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_SetRegistryAttachEnabled"), typeof(SetIntRaw));
            setHubEnabled = (SetIntRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_Menu_SetHubEnabled"), typeof(SetIntRaw));
            VoidRaw openOptionsWindow = (VoidRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_Menu_OpenOptionsWindow"), typeof(VoidRaw));
            closeOptionsWindow = (VoidRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_Menu_CloseOptionsWindow"), typeof(VoidRaw));
            VoidRaw saveOptionsWindow = (VoidRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_Menu_SaveOptionsWindow"), typeof(VoidRaw));
            SetPendingBoolRaw setPendingBool = (SetPendingBoolRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_UI_SetPendingBool"), typeof(SetPendingBoolRaw));
            SetPendingIntTextRaw setPendingIntText = (SetPendingIntTextRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_UI_SetPendingIntFromText"), typeof(SetPendingIntTextRaw));
            CountSettingsForModRaw countSettingsForMod = (CountSettingsForModRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_UI_CountSettingsForMod"), typeof(CountSettingsForModRaw));
            GetSummaryRaw getSummary = (GetSummaryRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_Commit_GetLastSummary"), typeof(GetSummaryRaw));

            Assert(setRegistrationLocked != null, "Missing EMC_ModHub_Test_SetRegistrationLocked export");
            Assert(setRegistryAttachEnabled != null, "Missing EMC_ModHub_Test_SetRegistryAttachEnabled export");
            Assert(setHubEnabled != null, "Missing EMC_ModHub_Test_Menu_SetHubEnabled export");
            Assert(openOptionsWindow != null, "Missing EMC_ModHub_Test_Menu_OpenOptionsWindow export");
            Assert(closeOptionsWindow != null, "Missing EMC_ModHub_Test_Menu_CloseOptionsWindow export");
            Assert(saveOptionsWindow != null, "Missing EMC_ModHub_Test_Menu_SaveOptionsWindow export");
            Assert(setPendingBool != null, "Missing EMC_ModHub_Test_UI_SetPendingBool export");
            Assert(setPendingIntText != null, "Missing EMC_ModHub_Test_UI_SetPendingIntFromText export");
            Assert(countSettingsForMod != null, "Missing EMC_ModHub_Test_UI_CountSettingsForMod export");
            Assert(getSummary != null, "Missing EMC_ModHub_Test_Commit_GetLastSummary export");

            setRegistrationLocked(0);
            setRegistryAttachEnabled(-1);
            setHubEnabled(1);

            IntPtr outApi = Marshal.AllocHGlobal(IntPtr.Size);
            IntPtr outApiSize = Marshal.AllocHGlobal(4);
            allocations.Add(outApi);
            allocations.Add(outApiSize);
            Marshal.WriteIntPtr(outApi, IntPtr.Zero);
            Marshal.WriteInt32(outApiSize, 0);

            ExpectResult(
                getApi((uint)EMC_HUB_API_VERSION_1, 56u, outApi, outApiSize),
                EMC_OK,
                "GetApi failed");

            IntPtr apiPtr = Marshal.ReadIntPtr(outApi);
            Assert(apiPtr != IntPtr.Zero, "GetApi returned null api pointer");

            RegisterModRaw registerMod = (RegisterModRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, 8), typeof(RegisterModRaw));
            RegisterBoolRaw registerBool = (RegisterBoolRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, 16), typeof(RegisterBoolRaw));
            RegisterIntRaw registerInt = (RegisterIntRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, 32), typeof(RegisterIntRaw));

            GetBoolCb getBool = BoolGet;
            SetBoolCb setBool = BoolSet;
            GetIntCb getInt = IntGet;
            SetIntCb setInt = IntSet;
            CallbackRoots.Add(getBool);
            CallbackRoots.Add(setBool);
            CallbackRoots.Add(getInt);
            CallbackRoots.Add(setInt);

            IntPtr getBoolPtr = Marshal.GetFunctionPointerForDelegate(getBool);
            IntPtr setBoolPtr = Marshal.GetFunctionPointerForDelegate(setBool);
            IntPtr getIntPtr = Marshal.GetFunctionPointerForDelegate(getInt);
            IntPtr setIntPtr = Marshal.GetFunctionPointerForDelegate(setInt);

            IntPtr nsId = AllocAnsi(allocations, "phase18.smoke");
            IntPtr nsDisplay = AllocAnsi(allocations, "Phase18 Smoke");
            IntPtr modId = AllocAnsi(allocations, "phase18_mod");
            IntPtr modDisplay = AllocAnsi(allocations, "Phase18 Mod");
            IntPtr boolSettingId = AllocAnsi(allocations, "enabled");
            IntPtr boolLabel = AllocAnsi(allocations, "Enabled");
            IntPtr boolDesc = AllocAnsi(allocations, "Enable or disable smoke setting");
            IntPtr intSettingId = AllocAnsi(allocations, "count");
            IntPtr intLabel = AllocAnsi(allocations, "Count");
            IntPtr intDesc = AllocAnsi(allocations, "Integer smoke setting");

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
                user_data = IntPtr.Zero,
                get_value = getBoolPtr,
                set_value = setBoolPtr
            };
            IntPtr boolDefPtr = AllocStruct(allocations, boolDef);

            EMC_IntSettingDefV1 intDef = new EMC_IntSettingDefV1
            {
                setting_id = intSettingId,
                label = intLabel,
                description = intDesc,
                user_data = IntPtr.Zero,
                min_value = 0,
                max_value = 100,
                step = 5,
                get_value = getIntPtr,
                set_value = setIntPtr
            };
            IntPtr intDefPtr = AllocStruct(allocations, intDef);

            IntPtr outHandle = Marshal.AllocHGlobal(IntPtr.Size);
            allocations.Add(outHandle);
            Marshal.WriteIntPtr(outHandle, IntPtr.Zero);

            ExpectResult(registerMod(modDescPtr, outHandle), EMC_OK, "register_mod failed");
            IntPtr modHandle = Marshal.ReadIntPtr(outHandle);
            Assert(modHandle != IntPtr.Zero, "register_mod returned null handle");
            ExpectResult(registerBool(modHandle, boolDefPtr), EMC_OK, "register_bool failed");
            ExpectResult(registerInt(modHandle, intDefPtr), EMC_OK, "register_int failed");

            Assert(countSettingsForMod(nsId, modId) == 2, "Expected two registered settings for phase18 smoke mod");

            openOptionsWindow();
            AssertSummary(getSummary, 0u, 0u, 0u, 0u, HUB_COMMIT_SKIP_REASON_NONE, "initial summary");

            ExpectResult(setPendingBool(nsId, modId, boolSettingId, 0), EMC_OK, "set_pending_bool valid value should succeed");
            saveOptionsWindow();
            AssertSummary(getSummary, 1u, 1u, 0u, 0u, HUB_COMMIT_SKIP_REASON_NONE, "bool commit summary");
            Assert(CurrentBoolValue == 0, "Bool callback did not receive committed value");
            Assert(BoolSetCount == 1, "Bool callback should be invoked exactly once");

            IntPtr intValidText = AllocAnsi(allocations, "17");
            ExpectResult(setPendingIntText(nsId, modId, intSettingId, intValidText), EMC_OK, "set_pending_int_from_text valid value should succeed");
            saveOptionsWindow();
            AssertSummary(getSummary, 1u, 1u, 0u, 0u, HUB_COMMIT_SKIP_REASON_NONE, "int commit summary");
            Assert(CurrentIntValue == 15, "Int callback should receive snapped value");
            Assert(IntSetCount == 1, "Int callback should be invoked exactly once");

            ExpectResult(setPendingBool(nsId, modId, boolSettingId, 2), EMC_ERR_INVALID_ARGUMENT, "set_pending_bool invalid value should be rejected");
            saveOptionsWindow();
            AssertSummary(getSummary, 0u, 0u, 0u, 0u, HUB_COMMIT_SKIP_REASON_NONE, "invalid bool summary");

            IntPtr intInvalidText = AllocAnsi(allocations, "abc");
            ExpectResult(setPendingIntText(nsId, modId, intSettingId, intInvalidText), EMC_OK, "set_pending_int_from_text invalid parse should not hard-fail");
            saveOptionsWindow();
            AssertSummary(getSummary, 0u, 0u, 0u, 0u, HUB_COMMIT_SKIP_REASON_NONE, "invalid int text summary");
            Assert(IntSetCount == 1, "Invalid int text should not invoke int callback");

            closeOptionsWindow();
            return "PASS";
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
            if (closeOptionsWindow != null)
            {
                closeOptionsWindow();
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
$result = [Phase18DummyConsumerSmokeHarness]::Run($DllPath, $KenshiPath)
Write-Host $result
