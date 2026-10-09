param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [string]$KenshiPath = ""
)

$ErrorActionPreference = 'Stop'

$code = @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class HubPhase4SearchCollapseHarness
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetApiRaw(uint requestedVersion, uint callerApiSize, IntPtr outApi, IntPtr outApiSize);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterModRaw(IntPtr desc, IntPtr outHandle);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RegisterBoolRaw(IntPtr mod, IntPtr def);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void TestSetRegistrationLockedRaw(int isLocked);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void TestSetRegistryAttachEnabledRaw(int isEnabled);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void MenuSetHubEnabledRaw(int isEnabled);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void MenuOpenRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void MenuCloseRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiSetNamespaceSearchQueryRaw(IntPtr namespaceId, IntPtr searchQuery);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiSetModCollapsedRaw(IntPtr namespaceId, IntPtr modId, int isCollapsed);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiGetModCollapsedRaw(IntPtr namespaceId, IntPtr modId, IntPtr outIsCollapsed);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UiDoesSettingMatchNamespaceSearchRaw(IntPtr namespaceId, IntPtr modId, IntPtr settingId, IntPtr outMatches);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int GetBoolCb(IntPtr userData, IntPtr outValue);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int SetBoolCb(IntPtr userData, int value, IntPtr errBuf, uint errBufSize);

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

    private static IntPtr AllocUtf8(List<IntPtr> allocations, string value)
    {
        byte[] bytes = Encoding.UTF8.GetBytes((value ?? string.Empty) + "\0");
        IntPtr ptr = Marshal.AllocHGlobal(bytes.Length);
        Marshal.Copy(bytes, 0, ptr, bytes.Length);
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
        Marshal.WriteInt32(outValue, 1);
        return EMC_OK;
    }

    private static int BoolSet(IntPtr userData, int value, IntPtr errBuf, uint errBufSize)
    {
        return EMC_OK;
    }

    private static void AssertSearchMatch(
        UiDoesSettingMatchNamespaceSearchRaw doesMatch,
        IntPtr nsId,
        IntPtr modId,
        IntPtr settingId,
        bool expected,
        string context)
    {
        IntPtr outMatches = Marshal.AllocHGlobal(4);
        try
        {
            Marshal.WriteInt32(outMatches, 0);
            int r = doesMatch(nsId, modId, settingId, outMatches);
            ExpectResult(r, EMC_OK, "does_setting_match failed for " + context);
            int actual = Marshal.ReadInt32(outMatches);
            Assert(actual == (expected ? 1 : 0), "search mismatch for " + context + " (expected=" + (expected ? 1 : 0) + ", actual=" + actual + ")");
        }
        finally
        {
            Marshal.FreeHGlobal(outMatches);
        }
    }

    private static void AssertCollapsed(
        UiGetModCollapsedRaw getCollapsed,
        IntPtr nsId,
        IntPtr modId,
        bool expected,
        string context)
    {
        IntPtr outCollapsed = Marshal.AllocHGlobal(4);
        try
        {
            Marshal.WriteInt32(outCollapsed, 0);
            int r = getCollapsed(nsId, modId, outCollapsed);
            ExpectResult(r, EMC_OK, "get_mod_collapsed failed for " + context);
            int actual = Marshal.ReadInt32(outCollapsed);
            Assert(actual == (expected ? 1 : 0), "collapse mismatch for " + context + " (expected=" + (expected ? 1 : 0) + ", actual=" + actual + ")");
        }
        finally
        {
            Marshal.FreeHGlobal(outCollapsed);
        }
    }

    private static void SetSearch(UiSetNamespaceSearchQueryRaw setSearch, IntPtr nsId, string query)
    {
        byte[] queryBytes = Encoding.UTF8.GetBytes((query ?? string.Empty) + "\0");
        IntPtr queryPtr = Marshal.AllocHGlobal(queryBytes.Length);
        try
        {
            Marshal.Copy(queryBytes, 0, queryPtr, queryBytes.Length);
            int r = setSearch(nsId, queryPtr);
            ExpectResult(r, EMC_OK, "set_namespace_search_query failed for query='" + (query ?? string.Empty) + "'");
        }
        finally
        {
            Marshal.FreeHGlobal(queryPtr);
        }
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
        MenuCloseRaw closeOptions = null;

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
            Assert(getApiProc != IntPtr.Zero, "Missing EMC_ModHub_GetApi export");
            GetApiRaw getApi = (GetApiRaw)Marshal.GetDelegateForFunctionPointer(getApiProc, typeof(GetApiRaw));

            IntPtr setLockedProc = GetProcAddress(module, "EMC_ModHub_Test_SetRegistrationLocked");
            IntPtr setAttachProc = GetProcAddress(module, "EMC_ModHub_Test_SetRegistryAttachEnabled");
            IntPtr setHubEnabledProc = GetProcAddress(module, "EMC_ModHub_Test_Menu_SetHubEnabled");
            IntPtr openOptionsProc = GetProcAddress(module, "EMC_ModHub_Test_Menu_OpenOptionsWindow");
            IntPtr closeOptionsProc = GetProcAddress(module, "EMC_ModHub_Test_Menu_CloseOptionsWindow");
            IntPtr setSearchProc = GetProcAddress(module, "EMC_ModHub_Test_UI_SetNamespaceSearchQuery");
            IntPtr setCollapsedProc = GetProcAddress(module, "EMC_ModHub_Test_UI_SetModCollapsed");
            IntPtr getCollapsedProc = GetProcAddress(module, "EMC_ModHub_Test_UI_GetModCollapsed");
            IntPtr doesMatchProc = GetProcAddress(module, "EMC_ModHub_Test_UI_DoesSettingMatchNamespaceSearch");

            Assert(setLockedProc != IntPtr.Zero, "Missing EMC_ModHub_Test_SetRegistrationLocked export");
            Assert(setAttachProc != IntPtr.Zero, "Missing EMC_ModHub_Test_SetRegistryAttachEnabled export");
            Assert(setHubEnabledProc != IntPtr.Zero, "Missing EMC_ModHub_Test_Menu_SetHubEnabled export");
            Assert(openOptionsProc != IntPtr.Zero, "Missing EMC_ModHub_Test_Menu_OpenOptionsWindow export");
            Assert(closeOptionsProc != IntPtr.Zero, "Missing EMC_ModHub_Test_Menu_CloseOptionsWindow export");
            Assert(setSearchProc != IntPtr.Zero, "Missing EMC_ModHub_Test_UI_SetNamespaceSearchQuery export");
            Assert(setCollapsedProc != IntPtr.Zero, "Missing EMC_ModHub_Test_UI_SetModCollapsed export");
            Assert(getCollapsedProc != IntPtr.Zero, "Missing EMC_ModHub_Test_UI_GetModCollapsed export");
            Assert(doesMatchProc != IntPtr.Zero, "Missing EMC_ModHub_Test_UI_DoesSettingMatchNamespaceSearch export");

            setRegistrationLocked = (TestSetRegistrationLockedRaw)Marshal.GetDelegateForFunctionPointer(setLockedProc, typeof(TestSetRegistrationLockedRaw));
            setRegistryAttachEnabled = (TestSetRegistryAttachEnabledRaw)Marshal.GetDelegateForFunctionPointer(setAttachProc, typeof(TestSetRegistryAttachEnabledRaw));
            setHubEnabled = (MenuSetHubEnabledRaw)Marshal.GetDelegateForFunctionPointer(setHubEnabledProc, typeof(MenuSetHubEnabledRaw));
            openOptions = (MenuOpenRaw)Marshal.GetDelegateForFunctionPointer(openOptionsProc, typeof(MenuOpenRaw));
            closeOptions = (MenuCloseRaw)Marshal.GetDelegateForFunctionPointer(closeOptionsProc, typeof(MenuCloseRaw));
            UiSetNamespaceSearchQueryRaw setSearch = (UiSetNamespaceSearchQueryRaw)Marshal.GetDelegateForFunctionPointer(setSearchProc, typeof(UiSetNamespaceSearchQueryRaw));
            UiSetModCollapsedRaw setCollapsed = (UiSetModCollapsedRaw)Marshal.GetDelegateForFunctionPointer(setCollapsedProc, typeof(UiSetModCollapsedRaw));
            UiGetModCollapsedRaw getCollapsed = (UiGetModCollapsedRaw)Marshal.GetDelegateForFunctionPointer(getCollapsedProc, typeof(UiGetModCollapsedRaw));
            UiDoesSettingMatchNamespaceSearchRaw doesMatch = (UiDoesSettingMatchNamespaceSearchRaw)Marshal.GetDelegateForFunctionPointer(doesMatchProc, typeof(UiDoesSettingMatchNamespaceSearchRaw));

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

            GetBoolCb getBool = BoolGet;
            SetBoolCb setBool = BoolSet;
            CallbackRoots.Add(getBool);
            CallbackRoots.Add(setBool);
            IntPtr getBoolPtr = Marshal.GetFunctionPointerForDelegate(getBool);
            IntPtr setBoolPtr = Marshal.GetFunctionPointerForDelegate(setBool);

            IntPtr nsQolId = AllocUtf8(allocations, "emkej.qol");
            IntPtr nsQolDisplay = AllocUtf8(allocations, "QoL");
            IntPtr modAlphaId = AllocUtf8(allocations, "phase4_alpha");
            IntPtr modAlphaDisplay = AllocUtf8(allocations, "Alpha Mod");
            IntPtr settingAlphaId = AllocUtf8(allocations, "alpha_enabled");
            IntPtr settingAlphaLabel = AllocUtf8(allocations, "Enable Feature");
            IntPtr settingAlphaDesc = AllocUtf8(allocations, "Toggles pathing");

            IntPtr modBetaId = AllocUtf8(allocations, "phase4_beta");
            IntPtr modBetaDisplay = AllocUtf8(allocations, "Second Mod");
            IntPtr settingBetaId = AllocUtf8(allocations, "beta_cafe");
            IntPtr settingBetaLabel = AllocUtf8(allocations, "café toggle");
            IntPtr settingBetaDesc = AllocUtf8(allocations, "Backup option");

            IntPtr nsCombatId = AllocUtf8(allocations, "emkej.combat");
            IntPtr nsCombatDisplay = AllocUtf8(allocations, "Combat");
            IntPtr modGammaId = AllocUtf8(allocations, "phase4_gamma");
            IntPtr modGammaDisplay = AllocUtf8(allocations, "Gamma Mod");
            IntPtr settingGammaId = AllocUtf8(allocations, "gamma_guard");
            IntPtr settingGammaLabel = AllocUtf8(allocations, "Guard stance");
            IntPtr settingGammaDesc = AllocUtf8(allocations, "Gamma setting");

            EMC_ModDescriptorV1 modAlphaDesc = new EMC_ModDescriptorV1
            {
                namespace_id = nsQolId,
                namespace_display_name = nsQolDisplay,
                mod_id = modAlphaId,
                mod_display_name = modAlphaDisplay,
                mod_user_data = IntPtr.Zero
            };
            EMC_ModDescriptorV1 modBetaDesc = new EMC_ModDescriptorV1
            {
                namespace_id = nsQolId,
                namespace_display_name = nsQolDisplay,
                mod_id = modBetaId,
                mod_display_name = modBetaDisplay,
                mod_user_data = IntPtr.Zero
            };
            EMC_ModDescriptorV1 modGammaDesc = new EMC_ModDescriptorV1
            {
                namespace_id = nsCombatId,
                namespace_display_name = nsCombatDisplay,
                mod_id = modGammaId,
                mod_display_name = modGammaDisplay,
                mod_user_data = IntPtr.Zero
            };

            IntPtr modAlphaDescPtr = AllocStruct(allocations, modAlphaDesc);
            IntPtr modBetaDescPtr = AllocStruct(allocations, modBetaDesc);
            IntPtr modGammaDescPtr = AllocStruct(allocations, modGammaDesc);

            EMC_BoolSettingDefV1 alphaBoolDef = new EMC_BoolSettingDefV1
            {
                setting_id = settingAlphaId,
                label = settingAlphaLabel,
                description = settingAlphaDesc,
                user_data = new IntPtr(4101),
                get_value = getBoolPtr,
                set_value = setBoolPtr
            };
            EMC_BoolSettingDefV1 betaBoolDef = new EMC_BoolSettingDefV1
            {
                setting_id = settingBetaId,
                label = settingBetaLabel,
                description = settingBetaDesc,
                user_data = new IntPtr(4201),
                get_value = getBoolPtr,
                set_value = setBoolPtr
            };
            EMC_BoolSettingDefV1 gammaBoolDef = new EMC_BoolSettingDefV1
            {
                setting_id = settingGammaId,
                label = settingGammaLabel,
                description = settingGammaDesc,
                user_data = new IntPtr(4301),
                get_value = getBoolPtr,
                set_value = setBoolPtr
            };

            IntPtr alphaBoolDefPtr = AllocStruct(allocations, alphaBoolDef);
            IntPtr betaBoolDefPtr = AllocStruct(allocations, betaBoolDef);
            IntPtr gammaBoolDefPtr = AllocStruct(allocations, gammaBoolDef);

            IntPtr outHandleA = Marshal.AllocHGlobal(IntPtr.Size);
            IntPtr outHandleB = Marshal.AllocHGlobal(IntPtr.Size);
            IntPtr outHandleC = Marshal.AllocHGlobal(IntPtr.Size);
            allocations.Add(outHandleA);
            allocations.Add(outHandleB);
            allocations.Add(outHandleC);
            Marshal.WriteIntPtr(outHandleA, IntPtr.Zero);
            Marshal.WriteIntPtr(outHandleB, IntPtr.Zero);
            Marshal.WriteIntPtr(outHandleC, IntPtr.Zero);

            r = registerMod(modAlphaDescPtr, outHandleA);
            ExpectResult(r, EMC_OK, "register_mod alpha failed");
            IntPtr handleA = Marshal.ReadIntPtr(outHandleA);
            Assert(handleA != IntPtr.Zero, "alpha mod handle is null");
            r = registerBool(handleA, alphaBoolDefPtr);
            ExpectResult(r, EMC_OK, "register_bool alpha failed");

            r = registerMod(modBetaDescPtr, outHandleB);
            ExpectResult(r, EMC_OK, "register_mod beta failed");
            IntPtr handleB = Marshal.ReadIntPtr(outHandleB);
            Assert(handleB != IntPtr.Zero, "beta mod handle is null");
            r = registerBool(handleB, betaBoolDefPtr);
            ExpectResult(r, EMC_OK, "register_bool beta failed");

            r = registerMod(modGammaDescPtr, outHandleC);
            ExpectResult(r, EMC_OK, "register_mod gamma failed");
            IntPtr handleC = Marshal.ReadIntPtr(outHandleC);
            Assert(handleC != IntPtr.Zero, "gamma mod handle is null");
            r = registerBool(handleC, gammaBoolDefPtr);
            ExpectResult(r, EMC_OK, "register_bool gamma failed");

            openOptions();

            AssertSearchMatch(doesMatch, nsQolId, modAlphaId, settingAlphaId, true, "default empty query / alpha");
            AssertSearchMatch(doesMatch, nsQolId, modBetaId, settingBetaId, true, "default empty query / beta");
            AssertSearchMatch(doesMatch, nsCombatId, modGammaId, settingGammaId, true, "default empty query / gamma");

            SetSearch(setSearch, nsQolId, "alpha");
            AssertSearchMatch(doesMatch, nsQolId, modAlphaId, settingAlphaId, true, "mod name search");
            AssertSearchMatch(doesMatch, nsQolId, modBetaId, settingBetaId, false, "mod name search miss");
            AssertSearchMatch(doesMatch, nsCombatId, modGammaId, settingGammaId, true, "per-tab isolation with empty other tab query");

            SetSearch(setSearch, nsQolId, "ENABLE");
            AssertSearchMatch(doesMatch, nsQolId, modAlphaId, settingAlphaId, true, "label ASCII case-insensitive");

            SetSearch(setSearch, nsQolId, "PATHING");
            AssertSearchMatch(doesMatch, nsQolId, modAlphaId, settingAlphaId, true, "description ASCII case-insensitive");

            SetSearch(setSearch, nsQolId, "café");
            AssertSearchMatch(doesMatch, nsQolId, modBetaId, settingBetaId, true, "non-ASCII literal exact bytes");
            SetSearch(setSearch, nsQolId, "CAFÉ");
            AssertSearchMatch(doesMatch, nsQolId, modBetaId, settingBetaId, false, "non-ASCII must not locale-fold");

            SetSearch(setSearch, nsQolId, "does-not-exist");
            AssertSearchMatch(doesMatch, nsQolId, modAlphaId, settingAlphaId, false, "miss / alpha");
            AssertSearchMatch(doesMatch, nsQolId, modBetaId, settingBetaId, false, "miss / beta");

            r = setCollapsed(nsQolId, modAlphaId, 1);
            ExpectResult(r, EMC_OK, "set_mod_collapsed(true) failed");
            AssertCollapsed(getCollapsed, nsQolId, modAlphaId, true, "before filtering");

            SetSearch(setSearch, nsQolId, "alpha");
            AssertCollapsed(getCollapsed, nsQolId, modAlphaId, true, "while filtered");

            SetSearch(setSearch, nsQolId, "");
            AssertCollapsed(getCollapsed, nsQolId, modAlphaId, true, "after clearing filter");

            r = setCollapsed(nsQolId, modAlphaId, 0);
            ExpectResult(r, EMC_OK, "set_mod_collapsed(false) failed");
            SetSearch(setSearch, nsQolId, "alpha");
            AssertCollapsed(getCollapsed, nsQolId, modAlphaId, false, "expanded while filtered");
            SetSearch(setSearch, nsQolId, "");
            AssertCollapsed(getCollapsed, nsQolId, modAlphaId, false, "expanded after clearing filter");

            SetSearch(setSearch, nsQolId, "alpha");
            SetSearch(setSearch, nsCombatId, "guard");
            AssertSearchMatch(doesMatch, nsQolId, modAlphaId, settingAlphaId, true, "ns1 filter retained");
            AssertSearchMatch(doesMatch, nsQolId, modBetaId, settingBetaId, false, "ns1 filter retained miss");
            AssertSearchMatch(doesMatch, nsCombatId, modGammaId, settingGammaId, true, "ns2 independent filter");

            closeOptions();
            return "PASS: phase4 search/filter/collapse matrix completed";
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

$result = [HubPhase4SearchCollapseHarness]::Run($DllPath, $resolvedKenshiPath)
Write-Host $result
