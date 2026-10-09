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

public static class HubPhase26ColorHarness
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetApiRaw(uint requestedVersion, uint callerApiSize, IntPtr outApi, IntPtr outApiSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterModRaw(IntPtr desc, IntPtr outHandle);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterColorRaw(IntPtr mod, IntPtr def);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void VoidRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void SetIntRaw(int value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int CountSettingsForModRaw(IntPtr nsId, IntPtr modId);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiSetPendingColorRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, IntPtr value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiSetPendingColorFromTextRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, IntPtr value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiNormalizePendingColorTextRaw(IntPtr nsId, IntPtr modId, IntPtr settingId);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiSetColorHexModeRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, int isHexMode);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiSetColorPaletteExpandedRaw(IntPtr nsId, IntPtr modId, IntPtr settingId, int isExpanded);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiGetPendingColorStateRaw(
        IntPtr nsId,
        IntPtr modId,
        IntPtr settingId,
        IntPtr outText,
        uint outTextSize,
        IntPtr outInputText,
        uint outInputTextSize,
        out int outPreviewKind,
        out int outHexMode,
        out int outParseError,
        out int outPaletteExpanded,
        out uint outPresetCount);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void GetSummaryRaw(out uint attempted, out uint succeeded, out uint failed, out uint skipped, out int skipReason);

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
    public struct EMC_ColorPresetV1
    {
        public IntPtr value_hex;
        public IntPtr label;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct EMC_ColorSettingDefV1
    {
        public IntPtr setting_id;
        public IntPtr label;
        public IntPtr description;
        public IntPtr user_data;
        public uint preview_kind;
        public IntPtr presets;
        public uint preset_count;
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
    private const int REGISTER_COLOR_OFFSET = 96;
    private const int COLOR_PREVIEW_KIND_SWATCH = 0;
    private const int COLOR_PREVIEW_KIND_TEXT = 1;

    private static readonly List<Delegate> CallbackRoots = new List<Delegate>();

    private static string AccentColorValue = "40ff40";
    private static string RelationColorValue = "#123abc";
    private static int AccentGetCount = 0;
    private static int AccentSetCount = 0;
    private static int RelationGetCount = 0;
    private static int RelationSetCount = 0;

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

    private static void AssertColorState(
        UiGetPendingColorStateRaw getPendingColorState,
        IntPtr nsId,
        IntPtr modId,
        IntPtr settingId,
        string expectedText,
        string expectedInputText,
        int expectedPreviewKind,
        int expectedHexMode,
        int expectedParseError,
        int expectedPaletteExpanded,
        uint expectedPresetCount,
        string context)
    {
        IntPtr buffer = Marshal.AllocHGlobal(64);
        IntPtr inputBuffer = Marshal.AllocHGlobal(64);
        try
        {
            Marshal.WriteByte(buffer, 0, 0);
            Marshal.WriteByte(inputBuffer, 0, 0);
            int previewKind;
            int hexMode;
            int parseError;
            int paletteExpanded;
            uint presetCount;
            ExpectResult(
                getPendingColorState(
                    nsId,
                    modId,
                    settingId,
                    buffer,
                    64u,
                    inputBuffer,
                    64u,
                    out previewKind,
                    out hexMode,
                    out parseError,
                    out paletteExpanded,
                    out presetCount),
                EMC_OK,
                context + " get pending color state failed");
            Assert(ReadAnsiBuffer(buffer) == expectedText, context + " pending text mismatch");
            Assert(ReadAnsiBuffer(inputBuffer) == expectedInputText, context + " input text mismatch");
            Assert(previewKind == expectedPreviewKind, context + " preview kind mismatch");
            Assert(hexMode == expectedHexMode, context + " hex mode mismatch");
            Assert(parseError == expectedParseError, context + " parse error mismatch");
            Assert(paletteExpanded == expectedPaletteExpanded, context + " palette expanded mismatch");
            Assert(presetCount == expectedPresetCount, context + " preset count mismatch");
        }
        finally
        {
            Marshal.FreeHGlobal(buffer);
            Marshal.FreeHGlobal(inputBuffer);
        }
    }

    private static int GetColor(IntPtr userData, IntPtr outValue, uint outValueSize)
    {
        string value;
        if (userData == new IntPtr(1))
        {
            AccentGetCount += 1;
            value = AccentColorValue;
        }
        else
        {
            RelationGetCount += 1;
            value = RelationColorValue;
        }

        byte[] bytes = Encoding.ASCII.GetBytes(value);
        if ((uint)bytes.Length + 1u > outValueSize)
        {
            return EMC_ERR_INVALID_ARGUMENT;
        }

        Marshal.Copy(bytes, 0, outValue, bytes.Length);
        Marshal.WriteByte(outValue, bytes.Length, 0);
        return EMC_OK;
    }

    private static int SetColor(IntPtr userData, IntPtr value, IntPtr errBuf, uint errBufSize)
    {
        string incomingValue = Marshal.PtrToStringAnsi(value) ?? string.Empty;
        string storedValue = incomingValue.Trim().TrimStart('#').ToLowerInvariant();
        if (storedValue.Length != 6)
        {
            return EMC_ERR_INVALID_ARGUMENT;
        }

        if (userData == new IntPtr(1))
        {
            AccentSetCount += 1;
            AccentColorValue = storedValue;
        }
        else
        {
            RelationSetCount += 1;
            RelationColorValue = storedValue;
        }

        return EMC_OK;
    }

    public static string Run(string dllPath, string kenshiPath)
    {
        IntPtr module = IntPtr.Zero;
        IntPtr outApi = IntPtr.Zero;
        IntPtr outApiSize = IntPtr.Zero;
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
            Assert(module != IntPtr.Zero, "LoadLibrary failed for " + dllPath);

            GetApiRaw getApi = (GetApiRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_GetApi"), typeof(GetApiRaw));
            SetIntRaw setHubEnabled = (SetIntRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_Menu_SetHubEnabled"), typeof(SetIntRaw));
            VoidRaw openOptions = (VoidRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_Menu_OpenOptionsWindow"), typeof(VoidRaw));
            VoidRaw saveOptions = (VoidRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_Menu_SaveOptionsWindow"), typeof(VoidRaw));
            VoidRaw closeOptions = (VoidRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_Menu_CloseOptionsWindow"), typeof(VoidRaw));
            CountSettingsForModRaw countSettingsForMod = (CountSettingsForModRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_UI_CountSettingsForMod"), typeof(CountSettingsForModRaw));
            UiSetPendingColorRaw setPendingColor = (UiSetPendingColorRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_UI_SetPendingColor"), typeof(UiSetPendingColorRaw));
            UiSetPendingColorFromTextRaw setPendingColorFromText = (UiSetPendingColorFromTextRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_UI_SetPendingColorFromText"), typeof(UiSetPendingColorFromTextRaw));
            UiNormalizePendingColorTextRaw normalizePendingColorText = (UiNormalizePendingColorTextRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_UI_NormalizePendingColorText"), typeof(UiNormalizePendingColorTextRaw));
            UiSetColorHexModeRaw setColorHexMode = (UiSetColorHexModeRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_UI_SetColorHexMode"), typeof(UiSetColorHexModeRaw));
            UiSetColorPaletteExpandedRaw setColorPaletteExpanded = (UiSetColorPaletteExpandedRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_UI_SetColorPaletteExpanded"), typeof(UiSetColorPaletteExpandedRaw));
            UiGetPendingColorStateRaw getPendingColorState = (UiGetPendingColorStateRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_UI_GetPendingColorState"), typeof(UiGetPendingColorStateRaw));
            GetSummaryRaw getSummary = (GetSummaryRaw)Marshal.GetDelegateForFunctionPointer(
                GetProcAddress(module, "EMC_ModHub_Test_Commit_GetLastSummary"), typeof(GetSummaryRaw));

            outApi = Marshal.AllocHGlobal(IntPtr.Size);
            outApiSize = Marshal.AllocHGlobal(sizeof(uint));
            Marshal.WriteIntPtr(outApi, IntPtr.Zero);
            Marshal.WriteInt32(outApiSize, 0);

            ExpectResult(getApi(HUB_API_V1, HUB_API_V1_SIZE, outApi, outApiSize), EMC_OK, "phase26 get_api failed");

            IntPtr apiPtr = Marshal.ReadIntPtr(outApi);
            int apiSize = Marshal.ReadInt32(outApiSize);
            Assert(apiPtr != IntPtr.Zero, "phase26 api pointer should not be null");
            Assert(apiSize >= (int)HUB_API_V1_SIZE, "phase26 api size too small");

            RegisterModRaw registerMod = (RegisterModRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, REGISTER_MOD_OFFSET), typeof(RegisterModRaw));
            RegisterColorRaw registerColor = (RegisterColorRaw)Marshal.GetDelegateForFunctionPointer(
                ReadFnPtr(apiPtr, REGISTER_COLOR_OFFSET), typeof(RegisterColorRaw));

            GetTextCb getColor = new GetTextCb(GetColor);
            SetTextCb setColor = new SetTextCb(SetColor);
            CallbackRoots.Add(getColor);
            CallbackRoots.Add(setColor);

            IntPtr namespaceId = AllocAnsi(allocations, "phase26.colors");
            IntPtr namespaceDisplayName = AllocAnsi(allocations, "Phase26 Colors");
            IntPtr modId = AllocAnsi(allocations, "phase26_color_mod");
            IntPtr modDisplayName = AllocAnsi(allocations, "Phase26 Color Mod");
            IntPtr accentSettingId = AllocAnsi(allocations, "accent_color");
            IntPtr relationSettingId = AllocAnsi(allocations, "relation_color");

            EMC_ModDescriptorV1 modDescriptor = new EMC_ModDescriptorV1
            {
                namespace_id = namespaceId,
                namespace_display_name = namespaceDisplayName,
                mod_id = modId,
                mod_display_name = modDisplayName,
                mod_user_data = new IntPtr(77)
            };

            IntPtr modDescriptorPtr = AllocStruct(allocations, modDescriptor);
            IntPtr modHandlePtr = Marshal.AllocHGlobal(IntPtr.Size);
            allocations.Add(modHandlePtr);
            Marshal.WriteIntPtr(modHandlePtr, IntPtr.Zero);
            ExpectResult(registerMod(modDescriptorPtr, modHandlePtr), EMC_OK, "phase26 register_mod failed");
            IntPtr modHandle = Marshal.ReadIntPtr(modHandlePtr);
            Assert(modHandle != IntPtr.Zero, "phase26 mod handle should not be null");

            EMC_ColorPresetV1[] relationPresets = new EMC_ColorPresetV1[]
            {
                new EMC_ColorPresetV1 { value_hex = AllocAnsi(allocations, "#FF3333"), label = AllocAnsi(allocations, "Enemy") },
                new EMC_ColorPresetV1 { value_hex = AllocAnsi(allocations, "#DEE85A"), label = AllocAnsi(allocations, "Ally") },
                new EMC_ColorPresetV1 { value_hex = AllocAnsi(allocations, "#40FF40"), label = AllocAnsi(allocations, "Squad") }
            };
            IntPtr relationPresetsPtr = AllocArray(allocations, relationPresets);

            EMC_ColorSettingDefV1 accentDef = new EMC_ColorSettingDefV1
            {
                setting_id = accentSettingId,
                label = AllocAnsi(allocations, "Accent color"),
                description = AllocAnsi(allocations, "Default palette swatch preview"),
                user_data = new IntPtr(1),
                preview_kind = (uint)COLOR_PREVIEW_KIND_SWATCH,
                presets = IntPtr.Zero,
                preset_count = 0u,
                get_value = Marshal.GetFunctionPointerForDelegate(getColor),
                set_value = Marshal.GetFunctionPointerForDelegate(setColor)
            };

            EMC_ColorSettingDefV1 relationDef = new EMC_ColorSettingDefV1
            {
                setting_id = relationSettingId,
                label = AllocAnsi(allocations, "Relation color"),
                description = AllocAnsi(allocations, "Override palette text preview"),
                user_data = new IntPtr(2),
                preview_kind = (uint)COLOR_PREVIEW_KIND_TEXT,
                presets = relationPresetsPtr,
                preset_count = (uint)relationPresets.Length,
                get_value = Marshal.GetFunctionPointerForDelegate(getColor),
                set_value = Marshal.GetFunctionPointerForDelegate(setColor)
            };

            EMC_ColorPresetV1[] invalidDuplicatePresets = new EMC_ColorPresetV1[]
            {
                new EMC_ColorPresetV1 { value_hex = AllocAnsi(allocations, "#ff3333"), label = IntPtr.Zero },
                new EMC_ColorPresetV1 { value_hex = AllocAnsi(allocations, "FF3333"), label = IntPtr.Zero }
            };
            IntPtr invalidDuplicatePresetsPtr = AllocArray(allocations, invalidDuplicatePresets);
            EMC_ColorSettingDefV1 invalidDef = new EMC_ColorSettingDefV1
            {
                setting_id = AllocAnsi(allocations, "bad_color"),
                label = AllocAnsi(allocations, "Bad color"),
                description = AllocAnsi(allocations, "Duplicate presets"),
                user_data = new IntPtr(3),
                preview_kind = (uint)COLOR_PREVIEW_KIND_SWATCH,
                presets = invalidDuplicatePresetsPtr,
                preset_count = (uint)invalidDuplicatePresets.Length,
                get_value = Marshal.GetFunctionPointerForDelegate(getColor),
                set_value = Marshal.GetFunctionPointerForDelegate(setColor)
            };

            IntPtr accentDefPtr = AllocStruct(allocations, accentDef);
            IntPtr relationDefPtr = AllocStruct(allocations, relationDef);
            IntPtr invalidDefPtr = AllocStruct(allocations, invalidDef);

            ExpectResult(registerColor(modHandle, accentDefPtr), EMC_OK, "phase26 register accent color failed");
            ExpectResult(registerColor(modHandle, relationDefPtr), EMC_OK, "phase26 register relation color failed");
            ExpectResult(registerColor(modHandle, invalidDefPtr), EMC_ERR_INVALID_ARGUMENT, "phase26 duplicate preset registration should fail");

            Assert(countSettingsForMod(namespaceId, modId) == 2, "phase26 setting count mismatch");

            setHubEnabled(1);
            closeOptions();
            openOptions();

            AssertColorState(
                getPendingColorState,
                namespaceId,
                modId,
                accentSettingId,
                "#40FF40",
                "#40FF40",
                COLOR_PREVIEW_KIND_SWATCH,
                0,
                0,
                0,
                25u,
                "phase26 accent initial");
            AssertColorState(
                getPendingColorState,
                namespaceId,
                modId,
                relationSettingId,
                "#123ABC",
                "#123ABC",
                COLOR_PREVIEW_KIND_TEXT,
                1,
                0,
                0,
                3u,
                "phase26 relation initial");

            IntPtr accentLowercase = AllocAnsi(allocations, "dee85a");
            ExpectResult(setPendingColor(namespaceId, modId, accentSettingId, accentLowercase), EMC_OK, "phase26 set pending color failed");
            AssertColorState(
                getPendingColorState,
                namespaceId,
                modId,
                accentSettingId,
                "#DEE85A",
                "#DEE85A",
                COLOR_PREVIEW_KIND_SWATCH,
                0,
                0,
                0,
                25u,
                "phase26 accent normalized");

            ExpectResult(setColorHexMode(namespaceId, modId, accentSettingId, 1), EMC_OK, "phase26 enable hex mode failed");
            AssertColorState(
                getPendingColorState,
                namespaceId,
                modId,
                accentSettingId,
                "#DEE85A",
                "#DEE85A",
                COLOR_PREVIEW_KIND_SWATCH,
                1,
                0,
                0,
                25u,
                "phase26 accent hex mode");

            IntPtr rawCustomColor = AllocAnsi(allocations, "123abc");
            ExpectResult(setPendingColorFromText(namespaceId, modId, accentSettingId, rawCustomColor), EMC_OK, "phase26 set pending color text failed");
            AssertColorState(
                getPendingColorState,
                namespaceId,
                modId,
                accentSettingId,
                "#123ABC",
                "123abc",
                COLOR_PREVIEW_KIND_SWATCH,
                1,
                0,
                0,
                25u,
                "phase26 accent custom pending");

            IntPtr invalidColor = AllocAnsi(allocations, "#12ZZZZ");
            ExpectResult(setPendingColorFromText(namespaceId, modId, accentSettingId, invalidColor), EMC_OK, "phase26 invalid color text should stay pending");
            AssertColorState(
                getPendingColorState,
                namespaceId,
                modId,
                accentSettingId,
                "#123ABC",
                "#12ZZZZ",
                COLOR_PREVIEW_KIND_SWATCH,
                1,
                1,
                0,
                25u,
                "phase26 accent invalid text");

            ExpectResult(normalizePendingColorText(namespaceId, modId, accentSettingId), EMC_OK, "phase26 normalize pending color text failed");
            AssertColorState(
                getPendingColorState,
                namespaceId,
                modId,
                accentSettingId,
                "#123ABC",
                "#123ABC",
                COLOR_PREVIEW_KIND_SWATCH,
                1,
                0,
                0,
                25u,
                "phase26 accent normalized text");

            ExpectResult(setColorPaletteExpanded(namespaceId, modId, accentSettingId, 1), EMC_OK, "phase26 expand palette failed");
            AssertColorState(
                getPendingColorState,
                namespaceId,
                modId,
                accentSettingId,
                "#123ABC",
                "#123ABC",
                COLOR_PREVIEW_KIND_SWATCH,
                1,
                0,
                1,
                25u,
                "phase26 accent hidden palette in hex mode");

            ExpectResult(setColorHexMode(namespaceId, modId, accentSettingId, 0), EMC_OK, "phase26 disable hex mode failed");
            ExpectResult(setColorPaletteExpanded(namespaceId, modId, accentSettingId, 1), EMC_OK, "phase26 expand palette failed");
            AssertColorState(
                getPendingColorState,
                namespaceId,
                modId,
                accentSettingId,
                "#123ABC",
                "#123ABC",
                COLOR_PREVIEW_KIND_SWATCH,
                0,
                0,
                1,
                25u,
                "phase26 accent expanded");

            saveOptions();
            AssertSummary(getSummary, 1u, 1u, 0u, 0u, HUB_COMMIT_SKIP_REASON_NONE, "phase26 save");
            Assert(AccentSetCount == 1, "phase26 accent set count mismatch");
            Assert(RelationSetCount == 0, "phase26 relation set count mismatch");
            Assert(AccentColorValue == "123abc", "phase26 stored accent value mismatch");
            AssertColorState(
                getPendingColorState,
                namespaceId,
                modId,
                accentSettingId,
                "#123ABC",
                "#123ABC",
                COLOR_PREVIEW_KIND_SWATCH,
                0,
                0,
                1,
                25u,
                "phase26 accent post-save");

            closeOptions();
            return "PASS";
        }
        finally
        {
            for (int index = allocations.Count - 1; index >= 0; --index)
            {
                Marshal.FreeHGlobal(allocations[index]);
            }

            if (outApi != IntPtr.Zero)
            {
                Marshal.FreeHGlobal(outApi);
            }
            if (outApiSize != IntPtr.Zero)
            {
                Marshal.FreeHGlobal(outApiSize);
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
$result = [HubPhase26ColorHarness]::Run($DllPath, $KenshiPath)
Write-Host $result
