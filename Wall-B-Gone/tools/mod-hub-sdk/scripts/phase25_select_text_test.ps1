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

public static class HubPhase25SelectTextHarness
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetApiRaw(uint requestedVersion, uint callerApiSize, IntPtr outApi, IntPtr outApiSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterModRaw(IntPtr desc, IntPtr outHandle);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterSelectRaw(IntPtr mod, IntPtr def);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterTextRaw(IntPtr mod, IntPtr def);

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
    public delegate void GetSummaryRaw(out uint attempted, out uint succeeded, out uint failed, out uint skipped, out int skipReason);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetSelectCb(IntPtr userData, IntPtr outValue);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int SetSelectCb(IntPtr userData, int value, IntPtr errBuf, uint errBufSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetTextCb(IntPtr userData, IntPtr outValue, uint outValueSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int SetTextCb(IntPtr userData, IntPtr value, IntPtr errBuf, uint errBufSize);

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
    public struct EMC_SelectOptionV1
    {
        public int value;
        public IntPtr label;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct EMC_SelectSettingDefV1
    {
        public IntPtr setting_id;
        public IntPtr label;
        public IntPtr description;
        public IntPtr user_data;
        public IntPtr options;
        public uint option_count;
        public IntPtr get_value;
        public IntPtr set_value;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct EMC_TextSettingDefV1
    {
        public IntPtr setting_id;
        public IntPtr label;
        public IntPtr description;
        public IntPtr user_data;
        public uint max_length;
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

    private const uint DONT_RESOLVE_DLL_REFERENCES = 0x00000001u;
    private const int EMC_OK = 0;
    private const int EMC_ERR_INVALID_ARGUMENT = 1;
    private const int HUB_COMMIT_SKIP_REASON_NONE = 0;
    private const uint HUB_API_V1 = 1u;
    private const uint HUB_API_V1_SIZE = 104u;
    private const int REGISTER_MOD_OFFSET = 8;
    private const int REGISTER_SELECT_OFFSET = 80;
    private const int REGISTER_TEXT_OFFSET = 88;
    private const uint TEXT_MAX_LENGTH = 24u;

    private static readonly List<Delegate> CallbackRoots = new List<Delegate>();

    private static int CurrentSelectValue = 1;
    private static string CurrentTextValue = "Alpha";
    private static int SelectGetCount = 0;
    private static int SelectSetCount = 0;
    private static int TextGetCount = 0;
    private static int TextSetCount = 0;

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

    private static int SelectGet(IntPtr userData, IntPtr outValue)
    {
        SelectGetCount += 1;
        Marshal.WriteInt32(outValue, CurrentSelectValue);
        return EMC_OK;
    }

    private static int SelectSet(IntPtr userData, int value, IntPtr errBuf, uint errBufSize)
    {
        SelectSetCount += 1;
        CurrentSelectValue = value;
        return EMC_OK;
    }

    private static int TextGet(IntPtr userData, IntPtr outValue, uint outValueSize)
    {
        TextGetCount += 1;
        byte[] bytes = Encoding.ASCII.GetBytes(CurrentTextValue);
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
        string nextValue = Marshal.PtrToStringAnsi(value) ?? string.Empty;
        if ((uint)nextValue.Length > TEXT_MAX_LENGTH)
        {
            return EMC_ERR_INVALID_ARGUMENT;
        }

        TextSetCount += 1;
        CurrentTextValue = nextValue;
        return EMC_OK;
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
            CurrentSelectValue = 1;
            CurrentTextValue = "Alpha";
            SelectGetCount = 0;
            SelectSetCount = 0;
            TextGetCount = 0;
            TextSetCount = 0;

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
                GetProcAddress(module, "EMC_ModHub_GetApi"), typeof(GetApiRaw));
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
            GetSummaryRaw getSummary = (GetSummaryRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_Commit_GetLastSummary"), typeof(GetSummaryRaw));

            Assert(getApi != null, "Missing EMC_ModHub_GetApi export");
            Assert(setPendingSelect != null, "Missing EMC_ModHub_Test_UI_SetPendingSelect export");
            Assert(getPendingSelect != null, "Missing EMC_ModHub_Test_UI_GetPendingSelectState export");
            Assert(setPendingText != null, "Missing EMC_ModHub_Test_UI_SetPendingText export");
            Assert(getPendingText != null, "Missing EMC_ModHub_Test_UI_GetPendingTextState export");

            setRegistrationLocked(0);
            setRegistryAttachEnabled(1);
            setHubEnabled(1);

            IntPtr outApi = Marshal.AllocHGlobal(IntPtr.Size);
            IntPtr outApiSize = Marshal.AllocHGlobal(4);
            allocations.Add(outApi);
            allocations.Add(outApiSize);
            ExpectResult(getApi(HUB_API_V1, HUB_API_V1_SIZE, outApi, outApiSize), EMC_OK, "phase25 get_api failed");
            IntPtr apiPtr = Marshal.ReadIntPtr(outApi);
            int apiSize = Marshal.ReadInt32(outApiSize);
            Assert(apiPtr != IntPtr.Zero, "phase25 get_api returned null api");
            Assert(apiSize >= (int)HUB_API_V1_SIZE, "phase25 api size too small");

            RegisterModRaw registerMod = (RegisterModRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, REGISTER_MOD_OFFSET), typeof(RegisterModRaw));
            RegisterSelectRaw registerSelect = (RegisterSelectRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, REGISTER_SELECT_OFFSET), typeof(RegisterSelectRaw));
            RegisterTextRaw registerText = (RegisterTextRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, REGISTER_TEXT_OFFSET), typeof(RegisterTextRaw));

            GetSelectCb selectGet = new GetSelectCb(SelectGet);
            SetSelectCb selectSet = new SetSelectCb(SelectSet);
            GetTextCb textGet = new GetTextCb(TextGet);
            SetTextCb textSet = new SetTextCb(TextSet);
            CallbackRoots.Add(selectGet);
            CallbackRoots.Add(selectSet);
            CallbackRoots.Add(textGet);
            CallbackRoots.Add(textSet);

            IntPtr nsId = AllocAnsi(allocations, "phase25.select_text");
            IntPtr nsDisplayName = AllocAnsi(allocations, "Phase25 Select/Text");
            IntPtr modId = AllocAnsi(allocations, "phase25_mod");
            IntPtr modDisplayName = AllocAnsi(allocations, "Phase25 Test Mod");
            IntPtr selectSettingId = AllocAnsi(allocations, "palette");
            IntPtr textSettingId = AllocAnsi(allocations, "title");
            IntPtr userData = AllocAnsi(allocations, "phase25_user_data");

            EMC_SelectOptionV1[] options = new EMC_SelectOptionV1[] {
                new EMC_SelectOptionV1 { value = 0, label = AllocAnsi(allocations, "Default") },
                new EMC_SelectOptionV1 { value = 1, label = AllocAnsi(allocations, "Warm") },
                new EMC_SelectOptionV1 { value = 2, label = AllocAnsi(allocations, "Cool") }
            };
            IntPtr optionsPtr = AllocArray(allocations, options);

            EMC_ModDescriptorV1 modDesc = new EMC_ModDescriptorV1
            {
                namespace_id = nsId,
                namespace_display_name = nsDisplayName,
                mod_id = modId,
                mod_display_name = modDisplayName,
                mod_user_data = userData
            };

            EMC_SelectSettingDefV1 selectDef = new EMC_SelectSettingDefV1
            {
                setting_id = selectSettingId,
                label = AllocAnsi(allocations, "Palette"),
                description = AllocAnsi(allocations, "Choose a preset palette"),
                user_data = userData,
                options = optionsPtr,
                option_count = (uint)options.Length,
                get_value = Marshal.GetFunctionPointerForDelegate(selectGet),
                set_value = Marshal.GetFunctionPointerForDelegate(selectSet)
            };

            EMC_TextSettingDefV1 textDef = new EMC_TextSettingDefV1
            {
                setting_id = textSettingId,
                label = AllocAnsi(allocations, "Title"),
                description = AllocAnsi(allocations, "Single-line title"),
                user_data = userData,
                max_length = TEXT_MAX_LENGTH,
                get_value = Marshal.GetFunctionPointerForDelegate(textGet),
                set_value = Marshal.GetFunctionPointerForDelegate(textSet)
            };

            IntPtr modDescPtr = AllocStruct(allocations, modDesc);
            IntPtr selectDefPtr = AllocStruct(allocations, selectDef);
            IntPtr textDefPtr = AllocStruct(allocations, textDef);
            IntPtr outHandle = Marshal.AllocHGlobal(IntPtr.Size);
            allocations.Add(outHandle);

            ExpectResult(registerMod(modDescPtr, outHandle), EMC_OK, "phase25 register_mod failed");
            IntPtr modHandle = Marshal.ReadIntPtr(outHandle);
            Assert(modHandle != IntPtr.Zero, "phase25 register_mod returned null handle");
            ExpectResult(registerSelect(modHandle, selectDefPtr), EMC_OK, "phase25 register_select failed");
            ExpectResult(registerText(modHandle, textDefPtr), EMC_OK, "phase25 register_text failed");

            openOptionsWindow();

            Assert(countSettingsForMod(nsId, modId) == 2, "phase25 settings count mismatch");

            int pendingSelectValue;
            ExpectResult(getPendingSelect(nsId, modId, selectSettingId, out pendingSelectValue), EMC_OK, "phase25 initial select state failed");
            Assert(pendingSelectValue == 1, "phase25 initial select value mismatch");

            IntPtr textBuffer = Marshal.AllocHGlobal((int)TEXT_MAX_LENGTH + 1);
            allocations.Add(textBuffer);
            ExpectResult(getPendingText(nsId, modId, textSettingId, textBuffer, TEXT_MAX_LENGTH + 1u), EMC_OK, "phase25 initial text state failed");
            Assert(ReadAnsiBuffer(textBuffer) == "Alpha", "phase25 initial text value mismatch");

            ExpectResult(setPendingSelect(nsId, modId, selectSettingId, 999), EMC_ERR_INVALID_ARGUMENT, "phase25 invalid select should fail");
            string tooLongValue = new string('x', (int)TEXT_MAX_LENGTH + 1);
            IntPtr tooLongPtr = AllocAnsi(allocations, tooLongValue);
            ExpectResult(setPendingText(nsId, modId, textSettingId, tooLongPtr), EMC_ERR_INVALID_ARGUMENT, "phase25 overlong text should fail");

            ExpectResult(setPendingSelect(nsId, modId, selectSettingId, 2), EMC_OK, "phase25 set_pending_select failed");
            ExpectResult(getPendingSelect(nsId, modId, selectSettingId, out pendingSelectValue), EMC_OK, "phase25 pending select readback failed");
            Assert(pendingSelectValue == 2, "phase25 pending select value mismatch");

            IntPtr updatedText = AllocAnsi(allocations, "Gamma");
            ExpectResult(setPendingText(nsId, modId, textSettingId, updatedText), EMC_OK, "phase25 set_pending_text failed");
            ExpectResult(getPendingText(nsId, modId, textSettingId, textBuffer, TEXT_MAX_LENGTH + 1u), EMC_OK, "phase25 pending text readback failed");
            Assert(ReadAnsiBuffer(textBuffer) == "Gamma", "phase25 pending text mismatch");

            saveOptionsWindow();

            AssertSummary(getSummary, 2u, 2u, 0u, 0u, HUB_COMMIT_SKIP_REASON_NONE, "phase25 commit");
            Assert(CurrentSelectValue == 2, "phase25 committed select value mismatch");
            Assert(CurrentTextValue == "Gamma", "phase25 committed text value mismatch");
            Assert(SelectSetCount == 1, "phase25 select set count mismatch");
            Assert(TextSetCount == 1, "phase25 text set count mismatch");
            Assert(SelectGetCount >= 2, "phase25 select get count too small");
            Assert(TextGetCount >= 2, "phase25 text get count too small");

            ExpectResult(getPendingSelect(nsId, modId, selectSettingId, out pendingSelectValue), EMC_OK, "phase25 final select state failed");
            Assert(pendingSelectValue == 2, "phase25 final select value mismatch");
            ExpectResult(getPendingText(nsId, modId, textSettingId, textBuffer, TEXT_MAX_LENGTH + 1u), EMC_OK, "phase25 final text state failed");
            Assert(ReadAnsiBuffer(textBuffer) == "Gamma", "phase25 final text value mismatch");

            closeOptionsWindow();
            closeOptionsWindow = null;

            return "PASS";
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
$result = [HubPhase25SelectTextHarness]::Run($resolvedDllPath, $resolvedKenshiPath)
Write-Host $result
