param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [string]$CoreDllPath = "",
    [string]$KenshiPath = ""
)

$ErrorActionPreference = 'Stop'

if (-not $CoreDllPath) {
    $candidateCore = Join-Path (Split-Path -Parent $DllPath) "Emkejs-Mod-Core.dll"
    if (Test-Path -LiteralPath $candidateCore) {
        $CoreDllPath = $candidateCore
    }
}

$code = @"
using System;
using System.Runtime.InteropServices;

public static class WallBGonePhase6FallbackHarness
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void SetAttachFailureModeRaw(int mode);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void SetRegisterModeRaw(int mode);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ResetClientStateRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void RunStartupAttachRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void OnOptionsWindowInitRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int UseHubUiRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int IsRetryPendingRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int HasRetriedRaw();

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

    private const int ATTACH_FAILURE_NONE = 0;
    private const int ATTACH_FAILURE_STARTUP_ONLY = 1;
    private const int ATTACH_FAILURE_ALWAYS = 2;

    private const int REGISTER_MODE_NORMAL = 0;
    private const int REGISTER_MODE_FAIL = 1;

    private static void Assert(bool condition, string message)
    {
        if (!condition)
        {
            throw new Exception(message);
        }
    }

    private static T Bind<T>(IntPtr module, string exportName)
    {
        IntPtr proc = GetProcAddress(module, exportName);
        Assert(proc != IntPtr.Zero, "Missing export: " + exportName);
        return (T)(object)Marshal.GetDelegateForFunctionPointer(proc, typeof(T));
    }

    private static void AssertState(
        UseHubUiRaw useHubUi,
        IsRetryPendingRaw isRetryPending,
        HasRetriedRaw hasRetried,
        int expectedUseHubUi,
        int expectedRetryPending,
        int expectedRetried,
        string context)
    {
        int useHubUiValue = useHubUi();
        int retryPendingValue = isRetryPending();
        int retriedValue = hasRetried();

        Assert(useHubUiValue == expectedUseHubUi,
            context + " expected use_hub_ui=" + expectedUseHubUi + ", actual=" + useHubUiValue);
        Assert(retryPendingValue == expectedRetryPending,
            context + " expected retry_pending=" + expectedRetryPending + ", actual=" + retryPendingValue);
        Assert(retriedValue == expectedRetried,
            context + " expected retry_attempted=" + expectedRetried + ", actual=" + retriedValue);
    }

    private static IntPtr SafeLoad(string path)
    {
        if (string.IsNullOrEmpty(path))
        {
            return IntPtr.Zero;
        }

        IntPtr module = LoadLibrary(path);
        if (module == IntPtr.Zero)
        {
            module = LoadLibraryEx(path, IntPtr.Zero, DONT_RESOLVE_DLL_REFERENCES);
        }

        return module;
    }

    public static string Run(string wallDllPath, string coreDllPath, string kenshiPath)
    {
        IntPtr coreModule = IntPtr.Zero;
        IntPtr wallModule = IntPtr.Zero;

        try
        {
            if (!string.IsNullOrEmpty(kenshiPath))
            {
                SetDllDirectory(kenshiPath);
            }

            coreModule = SafeLoad(coreDllPath);
            if (!string.IsNullOrEmpty(coreDllPath))
            {
                Assert(coreModule != IntPtr.Zero, "LoadLibrary failed for core dll: " + coreDllPath);
            }

            wallModule = SafeLoad(wallDllPath);
            Assert(wallModule != IntPtr.Zero, "LoadLibrary failed for wall dll: " + wallDllPath);

            SetAttachFailureModeRaw setAttachFailureMode = Bind<SetAttachFailureModeRaw>(wallModule, "WallBGone_Test_ModHub_SetAttachFailureMode");
            SetRegisterModeRaw setRegisterMode = Bind<SetRegisterModeRaw>(wallModule, "WallBGone_Test_ModHub_SetRegisterMode");
            ResetClientStateRaw resetClientState = Bind<ResetClientStateRaw>(wallModule, "WallBGone_Test_ModHub_ResetClientState");
            RunStartupAttachRaw runStartupAttach = Bind<RunStartupAttachRaw>(wallModule, "WallBGone_Test_ModHub_RunStartupAttach");
            OnOptionsWindowInitRaw onOptionsWindowInit = Bind<OnOptionsWindowInitRaw>(wallModule, "WallBGone_Test_ModHub_OnOptionsWindowInit");
            UseHubUiRaw useHubUi = Bind<UseHubUiRaw>(wallModule, "WallBGone_Test_ModHub_UseHubUi");
            IsRetryPendingRaw isRetryPending = Bind<IsRetryPendingRaw>(wallModule, "WallBGone_Test_ModHub_IsAttachRetryPending");
            HasRetriedRaw hasRetried = Bind<HasRetriedRaw>(wallModule, "WallBGone_Test_ModHub_HasAttachRetryAttempted");

            // Case 1: Startup attach success -> hub UI enabled, no retry.
            resetClientState();
            setAttachFailureMode(ATTACH_FAILURE_NONE);
            setRegisterMode(REGISTER_MODE_NORMAL);
            runStartupAttach();
            AssertState(useHubUi, isRetryPending, hasRetried, 1, 0, 0, "startup_success");

            // Case 2: Startup attach fails once -> retry pending; retry succeeds on options init.
            resetClientState();
            setAttachFailureMode(ATTACH_FAILURE_STARTUP_ONLY);
            setRegisterMode(REGISTER_MODE_NORMAL);
            runStartupAttach();
            AssertState(useHubUi, isRetryPending, hasRetried, 0, 1, 0, "startup_fail_once");

            onOptionsWindowInit();
            AssertState(useHubUi, isRetryPending, hasRetried, 1, 0, 1, "retry_success");

            onOptionsWindowInit();
            AssertState(useHubUi, isRetryPending, hasRetried, 1, 0, 1, "retry_idempotent");

            // Case 3: Startup attach fail and retry fail -> deterministic fallback.
            resetClientState();
            setAttachFailureMode(ATTACH_FAILURE_ALWAYS);
            setRegisterMode(REGISTER_MODE_NORMAL);
            runStartupAttach();
            AssertState(useHubUi, isRetryPending, hasRetried, 0, 1, 0, "startup_fail_always");

            onOptionsWindowInit();
            AssertState(useHubUi, isRetryPending, hasRetried, 0, 0, 1, "retry_fail_final");

            onOptionsWindowInit();
            AssertState(useHubUi, isRetryPending, hasRetried, 0, 0, 1, "retry_fail_idempotent");

            // Case 4: Attach succeeds but registration path fails -> fallback without retry.
            resetClientState();
            setAttachFailureMode(ATTACH_FAILURE_NONE);
            setRegisterMode(REGISTER_MODE_FAIL);
            runStartupAttach();
            AssertState(useHubUi, isRetryPending, hasRetried, 0, 0, 0, "register_fail_fallback");

            onOptionsWindowInit();
            AssertState(useHubUi, isRetryPending, hasRetried, 0, 0, 0, "register_fail_no_retry");

            setAttachFailureMode(ATTACH_FAILURE_NONE);
            setRegisterMode(REGISTER_MODE_NORMAL);
            resetClientState();

            return "PASS";
        }
        finally
        {
            if (wallModule != IntPtr.Zero)
            {
                FreeLibrary(wallModule);
            }

            if (coreModule != IntPtr.Zero)
            {
                FreeLibrary(coreModule);
            }
        }
    }
}
"@

Add-Type -TypeDefinition $code -Language CSharp
$result = [WallBGonePhase6FallbackHarness]::Run($DllPath, $CoreDllPath, $KenshiPath)
Write-Host $result
