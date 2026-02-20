static bool IsExecutableProtectFlags(DWORD protectFlags)
{
    if ((protectFlags & PAGE_GUARD) != 0 || (protectFlags & PAGE_NOACCESS) != 0)
    {
        return false;
    }

    const DWORD baseProtect = protectFlags & 0xFF;
    return baseProtect == PAGE_EXECUTE
        || baseProtect == PAGE_EXECUTE_READ
        || baseProtect == PAGE_EXECUTE_READWRITE
        || baseProtect == PAGE_EXECUTE_WRITECOPY;
}

static bool IsExecutableCodeAddress(uintptr_t address)
{
    if (!address)
    {
        return false;
    }

    MEMORY_BASIC_INFORMATION mbi;
    std::memset(&mbi, 0, sizeof(mbi));
    if (!VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)))
    {
        return false;
    }

    if (mbi.State != MEM_COMMIT)
    {
        return false;
    }

    return IsExecutableProtectFlags(mbi.Protect);
}

static bool TryReadCodeBytes(uintptr_t address, unsigned char* bytesOut, size_t byteCount)
{
    if (!address || !bytesOut || byteCount == 0)
    {
        return false;
    }

    __try
    {
        const unsigned char* ptr = reinterpret_cast<const unsigned char*>(address);
        for (size_t i = 0; i < byteCount; ++i)
        {
            bytesOut[i] = ptr[i];
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static std::string FormatCodeBytes(uintptr_t address, size_t byteCount)
{
    if (byteCount == 0)
    {
        return std::string();
    }

    unsigned char bytes[8] = { 0 };
    if (byteCount > sizeof(bytes))
    {
        byteCount = sizeof(bytes);
    }

    if (!TryReadCodeBytes(address, bytes, byteCount))
    {
        return "unreadable";
    }

    std::stringstream ss;
    for (size_t i = 0; i < byteCount; ++i)
    {
        if (i > 0)
        {
            ss << " ";
        }
        const int value = static_cast<int>(bytes[i]);
        if (value < 16)
        {
            ss << '0';
        }
        ss << std::hex << std::uppercase << value;
    }
    return ss.str();
}

static bool ValidateExpectedRvaForSymbol(
    const char* symbolName,
    uintptr_t baseAddr,
    uintptr_t resolvedAddr,
    uintptr_t expectedRva,
    intptr_t* rvaDeltaOut,
    bool* rvaMatchOut)
{
    if (!symbolName || !baseAddr || !resolvedAddr)
    {
        return false;
    }

    const uintptr_t expectedAddr = baseAddr + expectedRva;
    const bool rvaMatch = (resolvedAddr == expectedAddr);
    const intptr_t rvaDelta = static_cast<intptr_t>(resolvedAddr) - static_cast<intptr_t>(expectedAddr);
    if (rvaDeltaOut)
    {
        *rvaDeltaOut = rvaDelta;
    }
    if (rvaMatchOut)
    {
        *rvaMatchOut = rvaMatch;
    }
    const bool executable = IsExecutableCodeAddress(resolvedAddr);
    unsigned char firstByte = 0;
    const bool readableFirstByte = TryReadCodeBytes(resolvedAddr, &firstByte, 1);
    const bool signatureLooksReasonable = readableFirstByte && firstByte != 0x00 && firstByte != 0xCC;

    std::stringstream detail;
    detail << "Loot-Scoot-Execute DEBUG: compatibility_check symbol=" << symbolName
           << " resolved=0x" << std::hex << resolvedAddr
           << " expected=0x" << expectedAddr
           << " rva_match=" << (rvaMatch ? "true" : "false")
           << " rva_delta=0x" << std::hex << rvaDelta
           << " executable=" << (executable ? "true" : "false")
           << " signature_ok=" << (signatureLooksReasonable ? "true" : "false")
           << " first_bytes=\"" << FormatCodeBytes(resolvedAddr, 6) << "\"";
    DebugLog(detail.str().c_str());

    if (!executable)
    {
        std::stringstream warn;
        warn << "Loot-Scoot-Execute WARN: compatibility_check_failed symbol=" << symbolName
             << " reason=non_executable_address resolved=0x" << std::hex << resolvedAddr;
        ErrorLog(warn.str().c_str());
    }
    else if (!signatureLooksReasonable)
    {
        std::stringstream warn;
        warn << "Loot-Scoot-Execute WARN: compatibility_check_failed symbol=" << symbolName
             << " reason=unexpected_signature resolved=0x" << std::hex << resolvedAddr
             << " first_bytes=\"" << FormatCodeBytes(resolvedAddr, 6) << "\"";
        ErrorLog(warn.str().c_str());
    }

    return executable && signatureLooksReasonable;
}

static uintptr_t NormalizeThunkEntryAddress(const char* symbolName, uintptr_t candidateAddress)
{
    if (!candidateAddress)
    {
        return 0;
    }

    unsigned char bytes[4] = { 0 };
    if (!TryReadCodeBytes(candidateAddress, bytes, sizeof(bytes)))
    {
        return candidateAddress;
    }

    // Some thunks include a 2-byte prefix before an E9 jump. Keep the original
    // entry address, because call sites can target the prefixed entry directly.
    if (bytes[0] != 0xE9 && bytes[2] == 0xE9 && IsExecutableCodeAddress(candidateAddress + 2))
    {
        std::stringstream prefixed;
        prefixed << "Loot-Scoot-Execute INFO: thunk entry has 2-byte prefix symbol="
                 << (symbolName ? symbolName : "unknown")
                 << " entry=0x" << std::hex << candidateAddress
                 << " jump_at=0x" << std::hex << (candidateAddress + 2);
        DebugLog(prefixed.str().c_str());
    }

    return candidateAddress;
}

static bool TryResolveThunkCalleeAddress(
    uintptr_t thunkEntryAddress,
    uintptr_t* jumpInstructionAddressOut,
    uintptr_t* calleeAddressOut)
{
    if (!thunkEntryAddress || !jumpInstructionAddressOut || !calleeAddressOut)
    {
        return false;
    }

    unsigned char bytes[8] = { 0 };
    if (!TryReadCodeBytes(thunkEntryAddress, bytes, sizeof(bytes)))
    {
        return false;
    }

    size_t jumpOffset = static_cast<size_t>(-1);
    if (bytes[0] == 0xE9)
    {
        jumpOffset = 0;
    }
    else if (bytes[2] == 0xE9)
    {
        jumpOffset = 2;
    }
    else
    {
        return false;
    }

    int32_t relativeDelta = 0;
    std::memcpy(&relativeDelta, bytes + jumpOffset + 1, sizeof(relativeDelta));

    const uintptr_t jumpInstructionAddress = thunkEntryAddress + jumpOffset;
    const uintptr_t nextInstructionAddress = jumpInstructionAddress + 5;
    const intptr_t rawTargetAddress = static_cast<intptr_t>(nextInstructionAddress) + static_cast<intptr_t>(relativeDelta);
    if (rawTargetAddress <= 0)
    {
        return false;
    }

    const uintptr_t calleeAddress = static_cast<uintptr_t>(rawTargetAddress);
    if (!IsExecutableCodeAddress(calleeAddress))
    {
        return false;
    }

    *jumpInstructionAddressOut = jumpInstructionAddress;
    *calleeAddressOut = calleeAddress;
    return true;
}

static uintptr_t ResolvePreferredThunkHookAddress(const char* symbolName, uintptr_t thunkEntryAddress)
{
    if (!thunkEntryAddress)
    {
        return 0;
    }

    uintptr_t jumpInstructionAddress = 0;
    uintptr_t calleeAddress = 0;
    if (TryResolveThunkCalleeAddress(thunkEntryAddress, &jumpInstructionAddress, &calleeAddress))
    {
        std::stringstream detail;
        detail << "Loot-Scoot-Execute INFO: thunk_callee_resolved symbol="
               << (symbolName ? symbolName : "unknown")
               << " entry=0x" << std::hex << thunkEntryAddress
               << " jump=0x" << std::hex << jumpInstructionAddress
               << " callee=0x" << std::hex << calleeAddress
               << " entry_first_bytes=\"" << FormatCodeBytes(thunkEntryAddress, 6) << "\""
               << " callee_first_bytes=\"" << FormatCodeBytes(calleeAddress, 6) << "\"";
        DebugLog(detail.str().c_str());
        return calleeAddress;
    }

    std::stringstream fallback;
    fallback << "Loot-Scoot-Execute INFO: thunk_callee_unresolved_using_entry symbol="
             << (symbolName ? symbolName : "unknown")
             << " entry=0x" << std::hex << thunkEntryAddress
             << " entry_first_bytes=\"" << FormatCodeBytes(thunkEntryAddress, 6) << "\"";
    DebugLog(fallback.str().c_str());
    return thunkEntryAddress;
}

static uintptr_t ResolveAlternateThunkHookAddress(uintptr_t thunkEntryAddress)
{
    (void)thunkEntryAddress;
    // Disabled while primary hooks target resolved real callees.
    return 0;
}

static bool AreDualThunkHookTargetsSafe(uintptr_t primaryAddress, uintptr_t alternateAddress)
{
    if (!primaryAddress || !alternateAddress || primaryAddress == alternateAddress)
    {
        return false;
    }

    const uintptr_t low = (primaryAddress < alternateAddress) ? primaryAddress : alternateAddress;
    const uintptr_t high = (primaryAddress < alternateAddress) ? alternateAddress : primaryAddress;
    return (high - low) >= 8;
}

static bool ResolveValidatedThunkAddress(
    const char* symbolName,
    uintptr_t baseAddr,
    uintptr_t expectedRva,
    intptr_t coreRvaDelta,
    uintptr_t* resolvedAddressOut,
    intptr_t* selectedDeltaOut)
{
    if (!symbolName || !baseAddr || !resolvedAddressOut || !selectedDeltaOut)
    {
        return false;
    }

    // Prefer unshifted thunk RVAs first. In 1.0.65 builds we observed that
    // menu call sites often still target original thunk entries even when core
    // menu functions are shifted by a consistent delta.
    const intptr_t candidateDeltas[2] = { 0, coreRvaDelta };
    const size_t candidateCount = (coreRvaDelta == 0) ? 1 : 2;

    for (size_t i = 0; i < candidateCount; ++i)
    {
        const intptr_t candidateDelta = candidateDeltas[i];
        uintptr_t candidateAddress = baseAddr + expectedRva + candidateDelta;
        candidateAddress = NormalizeThunkEntryAddress(symbolName, candidateAddress);
        if (ValidateExpectedRvaForSymbol(
                symbolName,
                baseAddr,
                candidateAddress,
                expectedRva,
                0,
                0))
        {
            *resolvedAddressOut = candidateAddress;
            *selectedDeltaOut = candidateDelta;
            return true;
        }
    }

    return false;
}

static bool ResolveDirectCallTargetFromReturnRva(
    const char* symbolName,
    uintptr_t baseAddr,
    uintptr_t expectedReturnRva,
    intptr_t coreRvaDelta,
    uintptr_t* resolvedReturnAddressOut,
    uintptr_t* resolvedCallTargetAddressOut)
{
    if (!symbolName || !baseAddr || !resolvedReturnAddressOut || !resolvedCallTargetAddressOut)
    {
        return false;
    }

    const intptr_t candidateDeltas[2] = { 0, coreRvaDelta };
    const size_t candidateCount = (coreRvaDelta == 0) ? 1 : 2;

    for (size_t i = 0; i < candidateCount; ++i)
    {
        const intptr_t candidateDelta = candidateDeltas[i];
        const uintptr_t candidateReturnAddress = baseAddr + expectedReturnRva + candidateDelta;
        if (candidateReturnAddress < baseAddr + 5)
        {
            continue;
        }

        unsigned char callBytes[5] = { 0 };
        if (!TryReadCodeBytes(candidateReturnAddress - 5, callBytes, sizeof(callBytes)))
        {
            continue;
        }

        if (callBytes[0] != 0xE8)
        {
            continue;
        }

        int32_t relativeDelta = 0;
        std::memcpy(&relativeDelta, callBytes + 1, sizeof(relativeDelta));
        const intptr_t rawTarget = static_cast<intptr_t>(candidateReturnAddress) + static_cast<intptr_t>(relativeDelta);
        if (rawTarget <= 0)
        {
            continue;
        }

        const uintptr_t candidateTargetAddress = static_cast<uintptr_t>(rawTarget);
        if (!IsExecutableCodeAddress(candidateTargetAddress))
        {
            continue;
        }

        *resolvedReturnAddressOut = candidateReturnAddress;
        *resolvedCallTargetAddressOut = candidateTargetAddress;

        std::stringstream detail;
        detail << "Loot-Scoot-Execute INFO: callsite_target_resolved symbol="
               << symbolName
               << " return=0x" << std::hex << candidateReturnAddress
               << " call=0x" << std::hex << (candidateReturnAddress - 5)
               << " target=0x" << std::hex << candidateTargetAddress
               << " return_rva=0x" << std::hex << expectedReturnRva
               << " delta=0x" << std::hex << candidateDelta
               << " call_bytes=\"" << FormatCodeBytes(candidateReturnAddress - 5, 5) << "\"";
        DebugLog(detail.str().c_str());
        return true;
    }

    std::stringstream warn;
    warn << "Loot-Scoot-Execute WARN: callsite_target_unresolved symbol="
         << symbolName
         << " return_rva=0x" << std::hex << expectedReturnRva
         << " delta=0x" << std::hex << coreRvaDelta;
    ErrorLog(warn.str().c_str());
    return false;
}

static void LogDirectCallTargetsFromAddressWindow(
    const char* sourceTag,
    uintptr_t startAddress,
    size_t scanBytes,
    size_t maxCalls)
{
    if (!sourceTag || !startAddress || scanBytes < 5 || maxCalls == 0)
    {
        return;
    }

    const uintptr_t moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandleA(0));
    const auto toRva = [moduleBase](uintptr_t absoluteAddress) -> uintptr_t
    {
        if (absoluteAddress == 0 || moduleBase == 0 || absoluteAddress < moduleBase)
        {
            return 0;
        }
        return absoluteAddress - moduleBase;
    };

    size_t foundCalls = 0;
    std::stringstream logline;
    logline << "Loot-Scoot-Execute DEBUG: context_menu_callsite_branch_trace"
            << " source=" << sourceTag
            << " start=0x" << std::hex << startAddress
            << " start_rva=0x" << std::hex << toRva(startAddress)
            << " scan_bytes=0x" << std::hex << scanBytes;

    for (size_t offset = 0; offset + 5 <= scanBytes && foundCalls < maxCalls; ++offset)
    {
        const uintptr_t callAddress = startAddress + offset;
        unsigned char callBytes[5] = { 0 };
        if (!TryReadCodeBytes(callAddress, callBytes, sizeof(callBytes)))
        {
            break;
        }

        if (callBytes[0] != 0xE8)
        {
            continue;
        }

        int32_t relativeDelta = 0;
        std::memcpy(&relativeDelta, callBytes + 1, sizeof(relativeDelta));
        const intptr_t rawTarget = static_cast<intptr_t>(callAddress + 5) + static_cast<intptr_t>(relativeDelta);
        if (rawTarget <= 0)
        {
            continue;
        }

        const uintptr_t callTarget = static_cast<uintptr_t>(rawTarget);
        if (!IsExecutableCodeAddress(callTarget))
        {
            continue;
        }

        const uintptr_t returnAddress = callAddress + 5;
        logline << " call" << std::dec << foundCalls << "_site=0x" << std::hex << callAddress
                << " call" << std::dec << foundCalls << "_site_rva=0x" << std::hex << toRva(callAddress)
                << " call" << std::dec << foundCalls << "_return_rva=0x" << std::hex << toRva(returnAddress)
                << " call" << std::dec << foundCalls << "_target=0x" << std::hex << callTarget
                << " call" << std::dec << foundCalls << "_target_rva=0x" << std::hex << toRva(callTarget);
        ++foundCalls;
    }

    if (foundCalls == 0)
    {
        logline << " calls=none";
    }
    else
    {
        logline << " calls_found=" << std::dec << foundCalls;
    }

    DebugLog(logline.str().c_str());
}

static void LogAcceptedBranchCallTraceOnce(
    const char* sourceTag,
    uintptr_t callerAddress,
    bool accepted)
{
    if (!g_config.debugContextMenu || !sourceTag || !callerAddress || !accepted)
    {
        return;
    }

    const uintptr_t moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandleA(0));
    const uintptr_t callerRva = (callerAddress >= moduleBase && moduleBase != 0)
        ? (callerAddress - moduleBase)
        : 0;
    if (callerRva == 0)
    {
        return;
    }

    static bool s_orderFilterTraceLogged = false;
    static bool s_taskProbabilityTraceLogged = false;

    if (callerRva == kExpectedRvaContextMenuOrderFilterCallReturn_1_0_65)
    {
        if (s_orderFilterTraceLogged)
        {
            return;
        }
        s_orderFilterTraceLogged = true;
        LogDirectCallTargetsFromAddressWindow(sourceTag, callerAddress, 0x160, 4);
        return;
    }

    if (callerRva == kExpectedRvaContextMenuTaskProbabilityCallReturn_1_0_65)
    {
        if (s_taskProbabilityTraceLogged)
        {
            return;
        }
        s_taskProbabilityTraceLogged = true;
        LogDirectCallTargetsFromAddressWindow(sourceTag, callerAddress, 0x160, 4);
    }
}

static uintptr_t CaptureCallerAddress()
{
#if defined(_MSC_VER)
    return reinterpret_cast<uintptr_t>(_ReturnAddress());
#else
    return 0;
#endif
}

static uintptr_t CaptureCurrentModuleBaseAddress()
{
    return reinterpret_cast<uintptr_t>(GetModuleHandleA(0));
}

static uintptr_t ComputeRvaFromAbsoluteAddress(uintptr_t absoluteAddress)
{
    const uintptr_t baseAddress = CaptureCurrentModuleBaseAddress();
    if (absoluteAddress == 0 || baseAddress == 0 || absoluteAddress < baseAddress)
    {
        return 0;
    }

    return absoluteAddress - baseAddress;
}

static bool EvaluateContextMenuCompatibilityGate(unsigned int platform, const std::string& version, uintptr_t baseAddr)
{
    g_contextMenuCompatibilityGatePassed = false;
    g_contextMenuGateFailureReason.clear();
    g_resolvedContextMenuShowAddress = 0;
    g_contextMenuLoopScanSeedAddr = 0;
    g_contextMenuLoopScanBaseReg = -1;
    g_resolvedContextMenuBuildRowsAddress = 0;
    g_resolvedContextMenuUpdateAddress = 0;
    g_resolvedPlayerInterfaceUpdateUTAddress = 0;
    g_resolvedPlayerInterfaceAddOrderSelectedCharactersAddress = 0;
    g_resolvedPlayerInterfaceGetPlayerTaskProbabilityAddress = 0;
    g_resolvedPlayerInterfaceContextMenuOrderFilterThunkAddress = 0;
    g_resolvedPlayerInterfaceContextMenuTaskProbabilityThunkAddress = 0;
    g_resolvedPlayerInterfaceIsOrderValidForSelectionAddress = 0;
    g_resolvedContextMenuAppendOrderThunkAddress = 0;
    g_resolvedContextMenuTaskLabelThunkAddress = 0;
    g_hookPlayerInterfaceContextMenuOrderFilterAddress = 0;
    g_hookPlayerInterfaceContextMenuTaskProbabilityAddress = 0;
    g_hookContextMenuAppendOrderAddress = 0;
    g_hookContextMenuTaskLabelAddress = 0;
    g_resolvedContextMenuOrderFilterCallReturnAddress = 0;
    g_resolvedContextMenuOrderFilterCallTargetAddress = 0;
    g_resolvedContextMenuTaskProbabilityCallReturnAddress = 0;
    g_resolvedContextMenuTaskProbabilityCallTargetAddress = 0;
    g_resolvedContextMenuRowInsertCallReturnAddress = 0;
    g_resolvedContextMenuRowInsertCallTargetAddress = 0;
    g_resolvedContextMenuLoopEntryAddress = 0;
    g_hookPlayerInterfaceContextMenuOrderFilterAlternateAddress = 0;
    g_hookPlayerInterfaceContextMenuTaskProbabilityAlternateAddress = 0;
    g_hookContextMenuAppendOrderAlternateAddress = 0;
    g_hookContextMenuTaskLabelAlternateAddress = 0;

    std::stringstream begin;
    begin << "Loot-Scoot-Execute DEBUG: compatibility_gate_begin platform=" << platform
          << " version=" << version
          << " base=0x" << std::hex << baseAddr;
    DebugLog(begin.str().c_str());

    if (platform == KenshiLib::BinaryVersion::UNKNOWN)
    {
        g_contextMenuGateFailureReason = "unknown_platform";
        ErrorLog("Loot-Scoot-Execute WARN: compatibility gate failed: unknown platform");
        return false;
    }

    if (version != "1.0.65")
    {
        g_contextMenuGateFailureReason = "unsupported_version";
        ErrorLog("Loot-Scoot-Execute WARN: compatibility gate failed: version is not 1.0.65");
        return false;
    }

    if (baseAddr == 0)
    {
        g_contextMenuGateFailureReason = "null_module_base";
        ErrorLog("Loot-Scoot-Execute WARN: compatibility gate failed: null module base");
        return false;
    }

    g_resolvedContextMenuShowAddress = static_cast<uintptr_t>(KenshiLib::GetRealAddress(&ContextMenu::showContextMenu));
    g_contextMenuLoopScanSeedAddr = g_resolvedContextMenuShowAddress;
    g_resolvedContextMenuBuildRowsAddress = baseAddr + kExpectedRvaContextMenuBuildRows_1_0_65;
    g_resolvedContextMenuUpdateAddress = static_cast<uintptr_t>(KenshiLib::GetRealAddress(&ContextMenu::update));
    g_resolvedPlayerInterfaceUpdateUTAddress = static_cast<uintptr_t>(KenshiLib::GetRealAddress(&PlayerInterface::updateUT));
    g_resolvedPlayerInterfaceAddOrderSelectedCharactersAddress =
        static_cast<uintptr_t>(KenshiLib::GetRealAddress(&PlayerInterface::addOrderSelectedCharacters));
    g_resolvedPlayerInterfaceGetPlayerTaskProbabilityAddress =
        static_cast<uintptr_t>(KenshiLib::GetRealAddress(&PlayerInterface::getPlayerTaskProbability));
    g_resolvedPlayerInterfaceContextMenuOrderFilterThunkAddress =
        baseAddr + kExpectedRvaPlayerInterfaceContextMenuOrderFilterThunk_1_0_65;
    g_resolvedPlayerInterfaceContextMenuTaskProbabilityThunkAddress =
        baseAddr + kExpectedRvaPlayerInterfaceContextMenuTaskProbabilityThunk_1_0_65;
    g_resolvedPlayerInterfaceIsOrderValidForSelectionAddress =
        static_cast<uintptr_t>(KenshiLib::GetRealAddress(&PlayerInterface::isOrderValidForSelection));
    g_resolvedContextMenuAppendOrderThunkAddress = baseAddr + kExpectedRvaContextMenuAppendOrderThunk_1_0_65;
    g_resolvedContextMenuTaskLabelThunkAddress = baseAddr + kExpectedRvaContextMenuTaskLabelThunk_1_0_65;

    intptr_t showDelta = 0;
    intptr_t updateDelta = 0;
    intptr_t updateUTDelta = 0;
    intptr_t addOrderSelectedCharactersDelta = 0;
    intptr_t getPlayerTaskProbabilityDelta = 0;
    intptr_t isOrderValidForSelectionDelta = 0;
    bool showRvaMatch = false;
    bool updateRvaMatch = false;
    bool updateUTRvaMatch = false;
    bool addOrderSelectedCharactersRvaMatch = false;
    bool getPlayerTaskProbabilityRvaMatch = false;
    bool isOrderValidForSelectionRvaMatch = false;

    const bool showValid = ValidateExpectedRvaForSymbol(
        "ContextMenu::showContextMenu",
        baseAddr,
        g_resolvedContextMenuShowAddress,
        kExpectedRvaContextMenuShow_1_0_65,
        &showDelta,
        &showRvaMatch);
    const bool updateValid = ValidateExpectedRvaForSymbol(
        "ContextMenu::update",
        baseAddr,
        g_resolvedContextMenuUpdateAddress,
        kExpectedRvaContextMenuUpdate_1_0_65,
        &updateDelta,
        &updateRvaMatch);
    const bool updateUTValid = ValidateExpectedRvaForSymbol(
        "PlayerInterface::updateUT",
        baseAddr,
        g_resolvedPlayerInterfaceUpdateUTAddress,
        kExpectedRvaPlayerInterfaceUpdateUT_1_0_65,
        &updateUTDelta,
        &updateUTRvaMatch);
    const bool addOrderSelectedCharactersValid = ValidateExpectedRvaForSymbol(
        "PlayerInterface::addOrderSelectedCharacters",
        baseAddr,
        g_resolvedPlayerInterfaceAddOrderSelectedCharactersAddress,
        kExpectedRvaPlayerInterfaceAddOrderSelectedCharacters_1_0_65,
        &addOrderSelectedCharactersDelta,
        &addOrderSelectedCharactersRvaMatch);
    const bool getPlayerTaskProbabilityValid = ValidateExpectedRvaForSymbol(
        "PlayerInterface::getPlayerTaskProbability",
        baseAddr,
        g_resolvedPlayerInterfaceGetPlayerTaskProbabilityAddress,
        kExpectedRvaPlayerInterfaceGetPlayerTaskProbability_1_0_65,
        &getPlayerTaskProbabilityDelta,
        &getPlayerTaskProbabilityRvaMatch);
    const bool isOrderValidForSelectionValid = ValidateExpectedRvaForSymbol(
        "PlayerInterface::isOrderValidForSelection",
        baseAddr,
        g_resolvedPlayerInterfaceIsOrderValidForSelectionAddress,
        kExpectedRvaPlayerInterfaceIsOrderValidForSelection_1_0_65,
        &isOrderValidForSelectionDelta,
        &isOrderValidForSelectionRvaMatch);

    if (!showValid
        || !updateValid
        || !updateUTValid
        || !addOrderSelectedCharactersValid
        || !getPlayerTaskProbabilityValid
        || !isOrderValidForSelectionValid)
    {
        g_contextMenuCompatibilityGatePassed = false;
        g_contextMenuGateFailureReason = "rva_or_signature_validation_failed";
        ErrorLog("Loot-Scoot-Execute WARN: compatibility gate failed: one or more symbol checks failed");
        return false;
    }

    const bool exactRvaMatch = showRvaMatch
        && updateRvaMatch
        && updateUTRvaMatch
        && addOrderSelectedCharactersRvaMatch
        && getPlayerTaskProbabilityRvaMatch
        && isOrderValidForSelectionRvaMatch;
    const bool consistentDelta = !exactRvaMatch
        && showDelta == updateDelta
        && showDelta == updateUTDelta
        && showDelta == addOrderSelectedCharactersDelta
        && showDelta == getPlayerTaskProbabilityDelta
        && showDelta == isOrderValidForSelectionDelta;

    g_contextMenuCompatibilityGatePassed = exactRvaMatch || consistentDelta;
    if (!g_contextMenuCompatibilityGatePassed)
    {
        g_contextMenuGateFailureReason = "inconsistent_rva_delta";
        std::stringstream warn;
        warn << "Loot-Scoot-Execute WARN: compatibility gate failed: inconsistent RVA deltas"
             << " show=0x" << std::hex << showDelta
             << " update=0x" << updateDelta
             << " updateUT=0x" << updateUTDelta
             << " addOrderSelectedCharacters=0x" << addOrderSelectedCharactersDelta
             << " getPlayerTaskProbability=0x" << getPlayerTaskProbabilityDelta
             << " isOrderValidForSelection=0x" << isOrderValidForSelectionDelta;
        ErrorLog(warn.str().c_str());
        return false;
    }

    const intptr_t coreRvaDelta = exactRvaMatch ? 0 : showDelta;
    if (!IsExecutableCodeAddress(g_resolvedContextMenuShowAddress))
    {
        g_contextMenuCompatibilityGatePassed = false;
        g_contextMenuGateFailureReason = "rva_or_signature_validation_failed";
        ErrorLog("Loot-Scoot-Execute WARN: compatibility gate failed: ContextMenu::showContextMenu check failed");
        return false;
    }

    // Do not require exact function-entry bytes. Scan forward for loop-head
    // structure:
    //   xor reg32, reg32
    //   cmp [base+0x8], reg32 OR cmp reg32, [base+0x8]
    //   mov reg64, [base+0x10]
    const uintptr_t loopEntryScanBase = g_contextMenuLoopScanSeedAddr;
    unsigned char scanBaseBytes[16] = { 0 };
    if (!TryReadCodeBytes(loopEntryScanBase, scanBaseBytes, sizeof(scanBaseBytes)))
    {
        g_contextMenuCompatibilityGatePassed = false;
        g_contextMenuGateFailureReason = "signature_read_failed";
        ErrorLog("Loot-Scoot-Execute WARN: compatibility gate failed: could not read scan base bytes");
        return false;
    }

    if (loopEntryScanBase != g_resolvedContextMenuShowAddress)
    {
        g_contextMenuCompatibilityGatePassed = false;
        g_contextMenuGateFailureReason = "scan_base_mismatch";
        ErrorLog("Loot-Scoot-Execute FATAL: loop scan base mismatch. seed/base not showContextMenu. refusing to continue.");
        return false;
    }

    {
        std::stringstream scanBaseLog;
        scanBaseLog << "Loot-Scoot-Execute DEBUG: loop-head scan base selected"
                    << " seed=0x" << std::hex << g_contextMenuLoopScanSeedAddr
                    << " base=0x" << std::hex << loopEntryScanBase
                    << " bytes=\"" << FormatCodeBytes(loopEntryScanBase, 16) << "\"";
        DebugLog(scanBaseLog.str().c_str());
    }

    static const size_t kLoopHeadScanMax = 0x800;
    unsigned char loopEntryScanBytes[kLoopHeadScanMax] = { 0 };
    if (!TryReadCodeBytes(loopEntryScanBase, loopEntryScanBytes, sizeof(loopEntryScanBytes)))
    {
        g_contextMenuCompatibilityGatePassed = false;
        g_contextMenuGateFailureReason = "signature_read_failed";
        ErrorLog("Loot-Scoot-Execute WARN: compatibility gate failed: loop-entry scan read failed");
        return false;
    }

    auto ExtractMemBaseAndDisp = [](const unsigned char* code,
                                    size_t remaining,
                                    int* baseRegOut,
                                    int32_t* dispOut,
                                    size_t* lenOut,
                                    bool* isDwordAccessOut) -> bool
    {
        if (!code || remaining < 2 || !baseRegOut || !dispOut || !lenOut || !isDwordAccessOut)
        {
            return false;
        }

        const unsigned char modrm = code[0];
        const unsigned char mod = static_cast<unsigned char>(modrm & 0xC0);
        const unsigned char rm = static_cast<unsigned char>(modrm & 0x07);
        if (mod != 0x40 && mod != 0x80)
        {
            return false;
        }

        size_t pos = 1;
        int baseReg = -1;
        if (rm == 0x04)
        {
            if (remaining < pos + 1)
            {
                return false;
            }

            const unsigned char sib = code[pos++];
            const unsigned char scale = static_cast<unsigned char>((sib >> 6) & 0x03);
            const unsigned char index = static_cast<unsigned char>((sib >> 3) & 0x07);
            if (scale != 0 || index != 0x04)
            {
                return false;
            }

            baseReg = static_cast<int>(sib & 0x07);
        }
        else
        {
            baseReg = static_cast<int>(rm);
        }

        int32_t disp = 0;
        if (mod == 0x40)
        {
            if (remaining < pos + 1)
            {
                return false;
            }
            disp = static_cast<int8_t>(code[pos]);
            ++pos;
        }
        else
        {
            if (remaining < pos + 4)
            {
                return false;
            }
            std::memcpy(&disp, code + pos, sizeof(disp));
            pos += 4;
        }

        *baseRegOut = baseReg;
        *dispOut = disp;
        *lenOut = pos;
        *isDwordAccessOut = true;
        return true;
    };

    auto MatchMovMemPlus10At = [&ExtractMemBaseAndDisp](const unsigned char* code,
                                                        size_t remaining,
                                                        int* baseRegOut,
                                                        size_t* lenOut) -> bool
    {
        if (!code || remaining < 3 || !baseRegOut || !lenOut)
        {
            return false;
        }

        size_t pos = 0;
        unsigned char rex = 0;
        if ((code[pos] & 0xF0) == 0x40)
        {
            rex = code[pos];
            ++pos;
        }
        if (remaining < pos + 2)
        {
            return false;
        }

        if (code[pos] != 0x8B)
        {
            return false;
        }
        ++pos;

        int baseRegNoExt = -1;
        int32_t disp = 0;
        size_t memLen = 0;
        bool isDwordAccess = false;
        if (!ExtractMemBaseAndDisp(code + pos, remaining - pos, &baseRegNoExt, &disp, &memLen, &isDwordAccess))
        {
            return false;
        }
        if (!isDwordAccess || disp != 0x10)
        {
            return false;
        }

        const int baseReg = baseRegNoExt + ((rex & 0x01) ? 8 : 0);
        *baseRegOut = baseReg;
        *lenOut = pos + memLen;
        return true;
    };

    auto IsRefBasePlus08At = [&ExtractMemBaseAndDisp](const unsigned char* code,
                                                      size_t remaining,
                                                      int expectedBaseReg,
                                                      size_t* lenOut) -> bool
    {
        if (!code || remaining < 3)
        {
            return false;
        }

        size_t pos = 0;
        unsigned char rex = 0;
        if ((code[pos] & 0xF0) == 0x40)
        {
            rex = code[pos];
            ++pos;
        }
        if (remaining < pos + 2)
        {
            return false;
        }

        if (remaining < pos + 1)
        {
            return false;
        }

        unsigned char opcode = code[pos++];
        bool isMovzxRm8 = false;
        if (opcode == 0x0F)
        {
            if (remaining < pos + 1)
            {
                return false;
            }

            const unsigned char opcode2 = code[pos++];
            if (opcode2 != 0xB6)
            {
                return false;
            }
            isMovzxRm8 = true;
        }
        else if (opcode != 0x8B   // mov r32,r/m32
                 && opcode != 0x39 // cmp r/m32,r32
                 && opcode != 0x3B // cmp r32,r/m32
                 && opcode != 0x81 // cmp r/m32,imm32
                 && opcode != 0x83 // cmp r/m32,imm8
                 && opcode != 0x8D // lea r,mem
                 && opcode != 0x63) // movsxd r64,r/m32
        {
            return false;
        }

        if (remaining < pos + 1)
        {
            return false;
        }

        const unsigned char modrm = code[pos];
        if (!isMovzxRm8 && (opcode == 0x81 || opcode == 0x83) && (((modrm >> 3) & 0x07) != 0x07))
        {
            return false;
        }

        int baseRegNoExt = -1;
        int32_t disp = 0;
        size_t memLen = 0;
        bool isDwordAccess = false;
        if (!ExtractMemBaseAndDisp(code + pos, remaining - pos, &baseRegNoExt, &disp, &memLen, &isDwordAccess))
        {
            return false;
        }

        size_t totalLen = pos + memLen;
        if (!isMovzxRm8 && opcode == 0x81)
        {
            if (remaining < totalLen + 4)
            {
                return false;
            }
            totalLen += 4;
        }
        else if (!isMovzxRm8 && opcode == 0x83)
        {
            if (remaining < totalLen + 1)
            {
                return false;
            }
            totalLen += 1;
        }

        const int baseReg = baseRegNoExt + ((rex & 0x01) ? 8 : 0);
        if (lenOut)
        {
            *lenOut = totalLen;
        }
        // Any recognized memory reference to [base+0x08] counts.
        return isDwordAccess && baseReg == expectedBaseReg && disp == 0x08;
    };

    size_t movCandidatesAny = 0;
    uintptr_t loopHeadAddress = 0;

    static const size_t kMaxMovProbeEntries = 3;
    uintptr_t movProbeAddress[kMaxMovProbeEntries] = { 0 };
    int movProbeBaseReg[kMaxMovProbeEntries] = { -1, -1, -1 };
    bool movProbeBackRefFound[kMaxMovProbeEntries] = { false, false, false };
    uintptr_t movProbeBackRefAddress[kMaxMovProbeEntries] = { 0 };
    size_t movProbeCount = 0;

    for (size_t i = 0; i + 3 < kLoopHeadScanMax; ++i)
    {
        int baseReg = -1;
        size_t movLen = 0;
        if (!MatchMovMemPlus10At(loopEntryScanBytes + i, kLoopHeadScanMax - i, &baseReg, &movLen))
        {
            continue;
        }

        ++movCandidatesAny;
        const uintptr_t movAddress = loopEntryScanBase + i;

        bool foundBackRef = false;
        uintptr_t foundBackRefAddress = 0;
        const size_t backStart = (i > 0x300) ? (i - 0x300) : 0;
        for (size_t b = i;; --b)
        {
            size_t refLen = 0;
            if (IsRefBasePlus08At(loopEntryScanBytes + b, kLoopHeadScanMax - b, baseReg, &refLen))
            {
                foundBackRef = true;
                foundBackRefAddress = loopEntryScanBase + b;
                g_contextMenuLoopScanBaseReg = baseReg;
                loopHeadAddress = foundBackRefAddress;
                break;
            }

            if (b == backStart)
            {
                break;
            }
        }

        if (movProbeCount < kMaxMovProbeEntries)
        {
            movProbeAddress[movProbeCount] = movAddress;
            movProbeBaseReg[movProbeCount] = baseReg;
            movProbeBackRefFound[movProbeCount] = foundBackRef;
            movProbeBackRefAddress[movProbeCount] = foundBackRefAddress;
            ++movProbeCount;
        }

        if (loopHeadAddress != 0)
        {
            break;
        }
    }

    if (loopHeadAddress == 0)
    {
        g_contextMenuCompatibilityGatePassed = false;
        g_contextMenuGateFailureReason = "signature_validation_failed";
        std::stringstream loopSig;
        loopSig << "Loot-Scoot-Execute WARN: compatibility gate failed: ContextMenu loop-head not found"
                << " base_addr=0x" << std::hex << loopEntryScanBase
                << " scan_max=0x" << std::hex << kLoopHeadScanMax
                << " mov_candidates=" << std::dec << movCandidatesAny
                << " bytes=\"" << FormatCodeBytes(loopEntryScanBase, 16) << "\"";
        for (size_t m = 0; m < movProbeCount; ++m)
        {
            loopSig << " mov" << std::dec << m << "_addr=0x" << std::hex << movProbeAddress[m]
                    << " mov" << std::dec << m << "_base_reg=" << std::dec << movProbeBaseReg[m]
                    << " mov" << std::dec << m << "_ref08_found=" << (movProbeBackRefFound[m] ? 1 : 0)
                    << " mov" << std::dec << m << "_ref08_addr=0x" << std::hex << movProbeBackRefAddress[m];

            uintptr_t movPrevAddress = movProbeAddress[m];
            if (movPrevAddress >= 16)
            {
                movPrevAddress -= 16;
            }
            loopSig << " mov" << std::dec << m << "_prev16=\""
                    << FormatCodeBytes(movPrevAddress, 16) << "\""
                    << " mov" << std::dec << m << "_at16=\""
                    << FormatCodeBytes(movProbeAddress[m], 16) << "\"";
        }
        ErrorLog(loopSig.str().c_str());

        for (size_t dumpOffset = 0; dumpOffset < 0x200; dumpOffset += 0x40)
        {
            std::stringstream dumpChunk;
            dumpChunk << "Loot-Scoot-Execute DEBUG: loop-head scan dump offset=0x" << std::hex << dumpOffset
                      << " bytes=\"" << FormatCodeBytes(loopEntryScanBase + dumpOffset, 0x40) << "\"";
            DebugLog(dumpChunk.str().c_str());
        }
        return false;
    }

    g_resolvedContextMenuLoopEntryAddress = loopHeadAddress;
    {
        std::stringstream loopHead;
        loopHead << "Loot-Scoot-Execute INFO: compatibility gate: loop-head found"
                 << " base_addr=0x" << std::hex << loopEntryScanBase
                 << " found_addr=0x" << std::hex << g_resolvedContextMenuLoopEntryAddress
                 << " bytes=\"" << FormatCodeBytes(g_resolvedContextMenuLoopEntryAddress, 8) << "\"";
        DebugLog(loopHead.str().c_str());
    }

    g_resolvedContextMenuBuildRowsAddress = baseAddr + kExpectedRvaContextMenuBuildRows_1_0_65 + coreRvaDelta;
    const bool contextMenuBuildRowsValid = ValidateExpectedRvaForSymbol(
        "ContextMenu::buildRows",
        baseAddr,
        g_resolvedContextMenuBuildRowsAddress,
        kExpectedRvaContextMenuBuildRows_1_0_65,
        0,
        0);
    if (!contextMenuBuildRowsValid)
    {
        g_contextMenuCompatibilityGatePassed = false;
        g_contextMenuGateFailureReason = "rva_or_signature_validation_failed";
        ErrorLog("Loot-Scoot-Execute WARN: compatibility gate failed: ContextMenu::buildRows check failed");
        return false;
    }

    intptr_t orderFilterThunkDelta = 0;
    intptr_t taskProbabilityThunkDelta = 0;
    intptr_t appendOrderThunkDelta = 0;
    intptr_t taskLabelThunkDelta = 0;
    const bool contextMenuOrderFilterThunkValid = ResolveValidatedThunkAddress(
        "PlayerInterface::contextMenuOrderFilterThunk",
        baseAddr,
        kExpectedRvaPlayerInterfaceContextMenuOrderFilterThunk_1_0_65,
        coreRvaDelta,
        &g_resolvedPlayerInterfaceContextMenuOrderFilterThunkAddress,
        &orderFilterThunkDelta);
    const bool contextMenuTaskProbabilityThunkValid = ResolveValidatedThunkAddress(
        "PlayerInterface::contextMenuTaskProbabilityThunk",
        baseAddr,
        kExpectedRvaPlayerInterfaceContextMenuTaskProbabilityThunk_1_0_65,
        coreRvaDelta,
        &g_resolvedPlayerInterfaceContextMenuTaskProbabilityThunkAddress,
        &taskProbabilityThunkDelta);
    const bool contextMenuAppendOrderThunkValid = ResolveValidatedThunkAddress(
        "ContextMenu::appendOrderThunk",
        baseAddr,
        kExpectedRvaContextMenuAppendOrderThunk_1_0_65,
        coreRvaDelta,
        &g_resolvedContextMenuAppendOrderThunkAddress,
        &appendOrderThunkDelta);
    const bool contextMenuTaskLabelThunkValid = ResolveValidatedThunkAddress(
        "ContextMenu::taskLabelThunk",
        baseAddr,
        kExpectedRvaContextMenuTaskLabelThunk_1_0_65,
        coreRvaDelta,
        &g_resolvedContextMenuTaskLabelThunkAddress,
        &taskLabelThunkDelta);
    if (!contextMenuOrderFilterThunkValid
        || !contextMenuTaskProbabilityThunkValid
        || !contextMenuAppendOrderThunkValid
        || !contextMenuTaskLabelThunkValid)
    {
        g_contextMenuCompatibilityGatePassed = false;
        g_contextMenuGateFailureReason = "rva_or_signature_validation_failed";
        ErrorLog("Loot-Scoot-Execute WARN: compatibility gate failed: one or more thunk checks failed");
        return false;
    }

    uintptr_t orderFilterHookSeedAddress = g_resolvedPlayerInterfaceContextMenuOrderFilterThunkAddress;
    if (ResolveDirectCallTargetFromReturnRva(
            "ContextMenu::order_filter_callsite",
            baseAddr,
            kExpectedRvaContextMenuOrderFilterCallReturn_1_0_65,
            coreRvaDelta,
            &g_resolvedContextMenuOrderFilterCallReturnAddress,
            &g_resolvedContextMenuOrderFilterCallTargetAddress))
    {
        orderFilterHookSeedAddress = g_resolvedContextMenuOrderFilterCallTargetAddress;
    }

    uintptr_t taskProbabilityHookSeedAddress = g_resolvedPlayerInterfaceContextMenuTaskProbabilityThunkAddress;
    if (ResolveDirectCallTargetFromReturnRva(
            "ContextMenu::task_probability_callsite",
            baseAddr,
            kExpectedRvaContextMenuTaskProbabilityCallReturn_1_0_65,
            coreRvaDelta,
            &g_resolvedContextMenuTaskProbabilityCallReturnAddress,
            &g_resolvedContextMenuTaskProbabilityCallTargetAddress))
    {
        taskProbabilityHookSeedAddress = g_resolvedContextMenuTaskProbabilityCallTargetAddress;
    }

    (void)ResolveDirectCallTargetFromReturnRva(
        "ContextMenu::row_insert_callsite",
        baseAddr,
        kExpectedRvaContextMenuRowInsertCallReturn_1_0_65,
        coreRvaDelta,
        &g_resolvedContextMenuRowInsertCallReturnAddress,
        &g_resolvedContextMenuRowInsertCallTargetAddress);

    g_hookPlayerInterfaceContextMenuOrderFilterAddress =
        ResolvePreferredThunkHookAddress("PlayerInterface::contextMenuOrderFilterThunk", orderFilterHookSeedAddress);
    g_hookPlayerInterfaceContextMenuTaskProbabilityAddress =
        ResolvePreferredThunkHookAddress("PlayerInterface::contextMenuTaskProbabilityThunk", taskProbabilityHookSeedAddress);
    g_hookContextMenuTaskLabelAddress =
        ResolvePreferredThunkHookAddress("ContextMenu::taskLabelThunk", g_resolvedContextMenuTaskLabelThunkAddress);
    g_hookContextMenuAppendOrderAddress =
        ResolvePreferredThunkHookAddress("ContextMenu::appendOrderThunk", g_resolvedContextMenuAppendOrderThunkAddress);
    g_hookPlayerInterfaceContextMenuOrderFilterAlternateAddress =
        ResolveAlternateThunkHookAddress(g_resolvedPlayerInterfaceContextMenuOrderFilterThunkAddress);
    g_hookPlayerInterfaceContextMenuTaskProbabilityAlternateAddress =
        ResolveAlternateThunkHookAddress(g_resolvedPlayerInterfaceContextMenuTaskProbabilityThunkAddress);
    g_hookContextMenuTaskLabelAlternateAddress =
        ResolveAlternateThunkHookAddress(g_resolvedContextMenuTaskLabelThunkAddress);
    g_hookContextMenuAppendOrderAlternateAddress =
        ResolveAlternateThunkHookAddress(g_resolvedContextMenuAppendOrderThunkAddress);

    std::stringstream hookTargets;
    hookTargets << "Loot-Scoot-Execute INFO: resolved thunk hook targets"
                << " order_filter_entry=0x" << std::hex << g_resolvedPlayerInterfaceContextMenuOrderFilterThunkAddress
                << " order_filter_call_return=0x" << std::hex << g_resolvedContextMenuOrderFilterCallReturnAddress
                << " order_filter_call_target=0x" << std::hex << g_resolvedContextMenuOrderFilterCallTargetAddress
                << " order_filter=0x" << std::hex << g_hookPlayerInterfaceContextMenuOrderFilterAddress
                << " order_filter_alt=0x" << std::hex << g_hookPlayerInterfaceContextMenuOrderFilterAlternateAddress
                << " task_probability_entry=0x" << std::hex << g_resolvedPlayerInterfaceContextMenuTaskProbabilityThunkAddress
                << " task_probability_call_return=0x" << std::hex << g_resolvedContextMenuTaskProbabilityCallReturnAddress
                << " task_probability_call_target=0x" << std::hex << g_resolvedContextMenuTaskProbabilityCallTargetAddress
                << " task_probability=0x" << g_hookPlayerInterfaceContextMenuTaskProbabilityAddress
                << " task_probability_alt=0x" << g_hookPlayerInterfaceContextMenuTaskProbabilityAlternateAddress
                << " append_order_entry=0x" << std::hex << g_resolvedContextMenuAppendOrderThunkAddress
                << " append_order=0x" << g_hookContextMenuAppendOrderAddress
                << " append_order_alt=0x" << g_hookContextMenuAppendOrderAlternateAddress
                << " task_label_entry=0x" << std::hex << g_resolvedContextMenuTaskLabelThunkAddress
                << " task_label=0x" << g_hookContextMenuTaskLabelAddress
                << " task_label_alt=0x" << g_hookContextMenuTaskLabelAlternateAddress
                << " row_insert_call_return=0x" << std::hex << g_resolvedContextMenuRowInsertCallReturnAddress
                << " row_insert_call_target=0x" << std::hex << g_resolvedContextMenuRowInsertCallTargetAddress
                << " loop_entry=0x" << std::hex << g_resolvedContextMenuLoopEntryAddress;
    DebugLog(hookTargets.str().c_str());

    if (orderFilterThunkDelta != 0
        || taskProbabilityThunkDelta != 0
        || appendOrderThunkDelta != 0
        || taskLabelThunkDelta != 0)
    {
        std::stringstream shiftedThunks;
        shiftedThunks << "Loot-Scoot-Execute INFO: compatibility gate remapped thunk addresses"
                      << " order_filter_delta=0x" << std::hex << orderFilterThunkDelta
                      << " task_probability_delta=0x" << taskProbabilityThunkDelta
                      << " append_order_delta=0x" << appendOrderThunkDelta
                      << " task_label_delta=0x" << taskLabelThunkDelta;
        DebugLog(shiftedThunks.str().c_str());
    }

    if (consistentDelta)
    {
        std::stringstream shifted;
        shifted << "Loot-Scoot-Execute INFO: compatibility gate accepted consistent RVA shift"
                << " delta=0x" << std::hex << showDelta;
        DebugLog(shifted.str().c_str());
    }

    DebugLog("Loot-Scoot-Execute INFO: compatibility gate passed for native context-menu work");
    return true;
}

static void RefreshEffectiveContextMenuFeatureFlags(const char* source)
{
    const bool gateAllowsFeatures = g_contextMenuCompatibilityGatePassed && g_contextMenuHookInstallVerified;
    const bool mappingAllowsInjection = g_contextMenuMappingConfidenceGatePassed;
    const bool selectionHookReady = g_nativeExecuteSelectionHookInstallVerified;
    const bool probabilityHookReady = g_nativeExecuteProbabilityHookInstallVerified;
    const bool orderFilterHookReady = g_nativeExecuteOrderFilterHookInstallVerified;
    const bool contextMenuProbabilityHookReady = g_nativeExecuteContextMenuProbabilityHookInstallVerified;
    const bool orderValidityHookReady = g_nativeExecuteOrderValidityHookInstallVerified;
    const bool orderAppendHookReady = g_nativeExecuteOrderAppendHookInstallVerified;
    const bool taskLabelHookReady = g_nativeExecuteTaskLabelHookInstallVerified;
    const bool menuBuildHookReady = g_nativeExecuteMenuBuildHookInstallVerified;
    const bool hookAllowsInjection = selectionHookReady
        && probabilityHookReady
        && orderFilterHookReady
        && contextMenuProbabilityHookReady
        && orderValidityHookReady
        && orderAppendHookReady
        && taskLabelHookReady
        && menuBuildHookReady;
    g_effectiveEnableContextMenuProbe = gateAllowsFeatures && g_config.enableContextMenuProbe;
    g_effectiveEnableContextMenuInjection = gateAllowsFeatures
        && mappingAllowsInjection
        && hookAllowsInjection
        && g_config.enableContextMenuInjection;
    g_effectiveEnableExecuteAction = gateAllowsFeatures && g_config.enableExecuteAction;

    const bool anyConfiguredOn = g_config.enableContextMenuProbe || g_config.enableContextMenuInjection || g_config.enableExecuteAction;
    if (anyConfiguredOn && !gateAllowsFeatures)
    {
        std::stringstream warn;
        warn << "Loot-Scoot-Execute WARN: fail_closed_context_menu_features source="
             << (source ? source : "unknown")
             << " reason="
             << (g_contextMenuCompatibilityGatePassed ? "required_hooks_not_ready" : g_contextMenuGateFailureReason)
             << " probe=false injection=false execute=false";
        ErrorLog(warn.str().c_str());
    }

    if (g_config.enableContextMenuInjection && gateAllowsFeatures && !mappingAllowsInjection)
    {
        std::stringstream warn;
        warn << "Loot-Scoot-Execute WARN: fail_closed_context_menu_injection source="
             << (source ? source : "unknown")
             << " reason=" << g_contextMenuMappingGateFailureReason
             << " injection=false";
        ErrorLog(warn.str().c_str());
    }
    if (g_config.enableContextMenuInjection && gateAllowsFeatures && !selectionHookReady)
    {
        std::stringstream warn;
        warn << "Loot-Scoot-Execute WARN: fail_closed_context_menu_injection source="
             << (source ? source : "unknown")
             << " reason=execute_selection_hook_not_ready"
             << " injection=false";
        ErrorLog(warn.str().c_str());
    }
    if (g_config.enableContextMenuInjection && gateAllowsFeatures && !probabilityHookReady)
    {
        std::stringstream warn;
        warn << "Loot-Scoot-Execute WARN: fail_closed_context_menu_injection source="
             << (source ? source : "unknown")
             << " reason=execute_probability_hook_not_ready"
             << " injection=false";
        ErrorLog(warn.str().c_str());
    }
    if (g_config.enableContextMenuInjection && gateAllowsFeatures && !orderFilterHookReady)
    {
        std::stringstream warn;
        warn << "Loot-Scoot-Execute WARN: fail_closed_context_menu_injection source="
             << (source ? source : "unknown")
             << " reason=execute_order_filter_hook_not_ready"
             << " injection=false";
        ErrorLog(warn.str().c_str());
    }
    if (g_config.enableContextMenuInjection && gateAllowsFeatures && !contextMenuProbabilityHookReady)
    {
        std::stringstream warn;
        warn << "Loot-Scoot-Execute WARN: fail_closed_context_menu_injection source="
             << (source ? source : "unknown")
             << " reason=execute_context_menu_probability_hook_not_ready"
             << " injection=false";
        ErrorLog(warn.str().c_str());
    }
    if (g_config.enableContextMenuInjection && gateAllowsFeatures && !orderValidityHookReady)
    {
        std::stringstream warn;
        warn << "Loot-Scoot-Execute WARN: fail_closed_context_menu_injection source="
             << (source ? source : "unknown")
             << " reason=execute_order_validity_hook_not_ready"
             << " injection=false";
        ErrorLog(warn.str().c_str());
    }
    if (g_config.enableContextMenuInjection && gateAllowsFeatures && !orderAppendHookReady)
    {
        std::stringstream warn;
        warn << "Loot-Scoot-Execute WARN: fail_closed_context_menu_injection source="
             << (source ? source : "unknown")
             << " reason=execute_order_append_hook_not_ready"
             << " injection=false";
        ErrorLog(warn.str().c_str());
    }
    if (g_config.enableContextMenuInjection && gateAllowsFeatures && !taskLabelHookReady)
    {
        std::stringstream warn;
        warn << "Loot-Scoot-Execute WARN: fail_closed_context_menu_injection source="
             << (source ? source : "unknown")
             << " reason=execute_task_label_hook_not_ready"
             << " injection=false";
        ErrorLog(warn.str().c_str());
    }
    if (g_config.enableContextMenuInjection && gateAllowsFeatures && !menuBuildHookReady)
    {
        std::stringstream warn;
        warn << "Loot-Scoot-Execute WARN: fail_closed_context_menu_injection source="
             << (source ? source : "unknown")
             << " reason=execute_menu_build_hook_not_ready"
             << " injection=false";
        ErrorLog(warn.str().c_str());
    }
    std::stringstream detail;
    detail << "Loot-Scoot-Execute DEBUG: context_menu_feature_flags source=" << (source ? source : "unknown")
           << " gate_passed=" << (g_contextMenuCompatibilityGatePassed ? "true" : "false")
           << " hooks_verified=" << (g_contextMenuHookInstallVerified ? "true" : "false")
           << " execute_selection_hook_verified=" << (g_nativeExecuteSelectionHookInstallVerified ? "true" : "false")
           << " execute_probability_hook_verified=" << (g_nativeExecuteProbabilityHookInstallVerified ? "true" : "false")
           << " execute_order_filter_hook_verified=" << (g_nativeExecuteOrderFilterHookInstallVerified ? "true" : "false")
           << " execute_order_filter_alt_hook_verified=" << (g_nativeExecuteOrderFilterAlternateHookInstallVerified ? "true" : "false")
           << " execute_context_menu_probability_hook_verified=" << (g_nativeExecuteContextMenuProbabilityHookInstallVerified ? "true" : "false")
           << " execute_context_menu_probability_alt_hook_verified=" << (g_nativeExecuteContextMenuProbabilityAlternateHookInstallVerified ? "true" : "false")
           << " execute_order_validity_hook_verified=" << (g_nativeExecuteOrderValidityHookInstallVerified ? "true" : "false")
           << " execute_order_append_hook_verified=" << (g_nativeExecuteOrderAppendHookInstallVerified ? "true" : "false")
           << " execute_order_append_alt_hook_verified=" << (g_nativeExecuteOrderAppendAlternateHookInstallVerified ? "true" : "false")
           << " execute_task_label_hook_verified=" << (g_nativeExecuteTaskLabelHookInstallVerified ? "true" : "false")
           << " execute_task_label_alt_hook_verified=" << (g_nativeExecuteTaskLabelAlternateHookInstallVerified ? "true" : "false")
           << " execute_menu_build_hook_verified=" << (g_nativeExecuteMenuBuildHookInstallVerified ? "true" : "false")
           << " execute_loop_entry_hook_verified=" << (g_nativeExecuteLoopEntryHookInstallVerified ? "true" : "false")
           << " execute_row_insert_hook_verified=" << (g_nativeExecuteRowInsertHookInstallVerified ? "true" : "false")
           << " mapping_gate_passed=" << (g_contextMenuMappingConfidenceGatePassed ? "true" : "false")
           << " mapping_gate_reason=" << (g_contextMenuMappingConfidenceGatePassed ? "none" : g_contextMenuMappingGateFailureReason)
           << " cfg_probe=" << (g_config.enableContextMenuProbe ? "true" : "false")
           << " cfg_injection=" << (g_config.enableContextMenuInjection ? "true" : "false")
           << " cfg_execute=" << (g_config.enableExecuteAction ? "true" : "false")
           << " effective_probe=" << (g_effectiveEnableContextMenuProbe ? "true" : "false")
           << " effective_injection=" << (g_effectiveEnableContextMenuInjection ? "true" : "false")
           << " effective_execute=" << (g_effectiveEnableExecuteAction ? "true" : "false");
    DebugLog(detail.str().c_str());
}

static bool TryResolveRootObjectType(RootObject* object, int* dataTypeOut)
{
    if (!object || !dataTypeOut)
    {
        return false;
    }

    __try
    {
        *dataTypeOut = static_cast<int>(object->getDataType());
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryReadContextMenuProbeSnapshot(
    ContextMenu* menu,
    bool* visibleOut,
    std::string* menuNameOut,
    uint32_t* ordersCountOut,
    int* orderSampleOut,
    size_t* orderSampleCountOut)
{
    if (!menu || !visibleOut || !menuNameOut || !ordersCountOut || !orderSampleOut || !orderSampleCountOut)
    {
        return false;
    }

    __try
    {
        *visibleOut = menu->isVisible();
        *menuNameOut = menu->contextMenuName;

        const uint32_t orderCount = menu->orders.size();
        *ordersCountOut = orderCount;

        size_t sampleCount = static_cast<size_t>(orderCount);
        if (sampleCount > kContextMenuProbeOrderSampleCount)
        {
            sampleCount = kContextMenuProbeOrderSampleCount;
        }

        for (size_t i = 0; i < sampleCount; ++i)
        {
            orderSampleOut[i] = menu->orders[static_cast<uint32_t>(i)];
        }

        *orderSampleCountOut = sampleCount;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static std::string SanitizeMenuLabelForLog(const std::string& label)
{
    std::string safe;
    safe.reserve(label.size());
    for (size_t i = 0; i < label.size(); ++i)
    {
        const char c = label[i];
        if (c == '\"')
        {
            safe.push_back('\'');
            continue;
        }
        if (c == '\r' || c == '\n' || c == '\t')
        {
            safe.push_back(' ');
            continue;
        }
        safe.push_back(c);
    }
    return safe;
}

static bool TryResolveTaskLabelForMaterializationProbe(
    ContextMenu* menu,
    int taskValue,
    std::string* labelOut)
{
    if (!menu || !labelOut || !ContextMenu_taskLabelThunk_orig)
    {
        return false;
    }

    labelOut->clear();
    void* guiContext = 0;
    __try
    {
        if (menu->menuGUI)
        {
            guiContext = menu->menuGUI;
        }
        else if (menu->menuGUI2)
        {
            guiContext = menu->menuGUI2;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    __try
    {
        ContextMenu_taskLabelThunk_orig(labelOut, taskValue, guiContext);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        labelOut->clear();
        return false;
    }
}

static bool TryReadContextMenuVisibilityAndOrderCount(
    ContextMenu* menu,
    bool* visibleOut,
    uint32_t* ordersCountOut)
{
    if (!menu || !visibleOut || !ordersCountOut)
    {
        return false;
    }

    __try
    {
        *visibleOut = menu->isVisible();
        *ordersCountOut = menu->orders.size();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryReadContextMenuOrderAt(ContextMenu* menu, uint32_t index, int* taskIdOut)
{
    if (!menu || !taskIdOut)
    {
        return false;
    }

    __try
    {
        const uint32_t count = menu->orders.size();
        if (index >= count)
        {
            return false;
        }
        *taskIdOut = menu->orders[index];
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryReadOrderListCount(lektor<int>* orders, uint32_t* countOut)
{
    if (!orders || !countOut)
    {
        return false;
    }

    __try
    {
        *countOut = orders->size();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryOrderListContains(lektor<int>* orders, int orderId, bool* containsOut)
{
    if (!orders || !containsOut)
    {
        return false;
    }

    *containsOut = false;
    uint32_t count = 0;
    if (!TryReadOrderListCount(orders, &count))
    {
        return false;
    }

    __try
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if ((*orders)[i] == orderId)
            {
                *containsOut = true;
                break;
            }
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static size_t CaptureOrderSampleForLog(
    lektor<int>* orders,
    int* sampleOut,
    size_t sampleCapacity,
    uint32_t* countOut)
{
    if (countOut)
    {
        *countOut = 0;
    }

    if (!orders || !sampleOut || sampleCapacity == 0)
    {
        return 0;
    }

    uint32_t count = 0;
    if (!TryReadOrderListCount(orders, &count))
    {
        return 0;
    }
    if (countOut)
    {
        *countOut = count;
    }

    size_t sampleCount = static_cast<size_t>(count);
    if (sampleCount > sampleCapacity)
    {
        sampleCount = sampleCapacity;
    }

    __try
    {
        for (size_t i = 0; i < sampleCount; ++i)
        {
            sampleOut[i] = (*orders)[static_cast<uint32_t>(i)];
        }
        return sampleCount;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

static void AppendOrderSampleForLog(
    std::stringstream& logline,
    const int* sample,
    size_t sampleCount,
    uint32_t totalCount)
{
    logline << "[";
    for (size_t i = 0; i < sampleCount; ++i)
    {
        if (i > 0)
        {
            logline << ",";
        }
        logline << std::dec << sample[i];
    }
    if (static_cast<size_t>(totalCount) > sampleCount)
    {
        if (sampleCount > 0)
        {
            logline << ",";
        }
        logline << "...";
    }
    logline << "]";
}

static bool TryReadInt32At(const void* address, int* valueOut)
{
    if (!address || !valueOut)
    {
        return false;
    }

    __try
    {
        *valueOut = *reinterpret_cast<const int*>(address);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryWriteInt32At(void* address, int value)
{
    if (!address)
    {
        return false;
    }

    __try
    {
        *reinterpret_cast<int*>(address) = value;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryWriteUintptrAt(void* address, uintptr_t value)
{
    if (!address)
    {
        return false;
    }

    __try
    {
        *reinterpret_cast<uintptr_t*>(address) = value;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryReadUintptrAt(const void* address, uintptr_t* valueOut)
{
    if (!address || !valueOut)
    {
        return false;
    }

    __try
    {
        *valueOut = *reinterpret_cast<const uintptr_t*>(address);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool WriteCodeBytes(uintptr_t address, const unsigned char* bytes, size_t byteCount)
{
    if (address == 0 || !bytes || byteCount == 0)
    {
        return false;
    }

    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(address), byteCount, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        return false;
    }

    std::memcpy(reinterpret_cast<void*>(address), bytes, byteCount);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), byteCount);

    DWORD restoredProtect = 0;
    (void)VirtualProtect(reinterpret_cast<void*>(address), byteCount, oldProtect, &restoredProtect);
    return true;
}

static uintptr_t AlignDownToGranularity(uintptr_t value, uintptr_t granularity)
{
    if (granularity == 0)
    {
        return value;
    }
    return value & ~(granularity - 1);
}

static void* AllocateExecutableStubNear(uintptr_t patchAddress, size_t requestedSize)
{
    if (patchAddress == 0 || requestedSize == 0)
    {
        return 0;
    }

    SYSTEM_INFO systemInfo;
    std::memset(&systemInfo, 0, sizeof(systemInfo));
    GetSystemInfo(&systemInfo);

    const uintptr_t granularity = systemInfo.dwAllocationGranularity != 0
        ? static_cast<uintptr_t>(systemInfo.dwAllocationGranularity)
        : static_cast<uintptr_t>(0x10000);
    const uintptr_t pageSize = systemInfo.dwPageSize != 0
        ? static_cast<uintptr_t>(systemInfo.dwPageSize)
        : static_cast<uintptr_t>(0x1000);
    const size_t allocSize = static_cast<size_t>(
        (static_cast<uintptr_t>(requestedSize) + pageSize - 1) & ~(pageSize - 1));

    const uintptr_t alignedPatch = AlignDownToGranularity(patchAddress, granularity);
    const uintptr_t maxDistance = 0x7FFF0000ull;

    for (uintptr_t delta = 0; delta <= maxDistance; delta += granularity)
    {
        if (alignedPatch >= delta)
        {
            const uintptr_t lowCandidate = alignedPatch - delta;
            void* lowAlloc = VirtualAlloc(
                reinterpret_cast<void*>(lowCandidate),
                allocSize,
                MEM_COMMIT | MEM_RESERVE,
                PAGE_EXECUTE_READWRITE);
            if (lowAlloc)
            {
                return lowAlloc;
            }
        }

        const uintptr_t maxAddress = static_cast<uintptr_t>(~static_cast<uintptr_t>(0));
        if (delta != 0 && alignedPatch <= (maxAddress - delta))
        {
            const uintptr_t highCandidate = alignedPatch + delta;
            void* highAlloc = VirtualAlloc(
                reinterpret_cast<void*>(highCandidate),
                allocSize,
                MEM_COMMIT | MEM_RESERVE,
                PAGE_EXECUTE_READWRITE);
            if (highAlloc)
            {
                return highAlloc;
            }
        }
    }

    return 0;
}

static void ArmNativeMenuExecuteDispatchContext(uintptr_t targetPtr, DWORD nowMs);
static bool TryAppendExecuteOrderToOrdersList(lektor<int>* orders);

static bool TryValidateOrderListPointerCandidate(uintptr_t candidatePtr, uint32_t* sizeOut, uintptr_t* dataOut)
{
    if (candidatePtr == 0 || !sizeOut || !dataOut)
    {
        return false;
    }

    lektor<int>* orders = reinterpret_cast<lektor<int>*>(candidatePtr);
    uint32_t size = 0;
    if (!TryReadOrderListCount(orders, &size))
    {
        return false;
    }

    uintptr_t dataPtr = 0;
    if (!TryReadUintptrAt(reinterpret_cast<const void*>(candidatePtr + 0x10), &dataPtr))
    {
        return false;
    }

    if (size < 1 || size > 16 || dataPtr == 0)
    {
        return false;
    }

    int firstOrderValue = 0;
    if (!TryReadInt32At(reinterpret_cast<const void*>(dataPtr), &firstOrderValue))
    {
        return false;
    }

    *sizeOut = size;
    *dataOut = dataPtr;
    return true;
}

static void ContextMenu_loopEntryInjectHelper(
    uintptr_t ordersCandidateR14,
    uintptr_t ordersCandidateRsi,
    uintptr_t ordersCandidateRdi,
    uintptr_t ordersCandidateRcx)
{
    if (kEnableBuildRowsPreloopInjection)
    {
        return;
    }

    if (g_nativeMenuLoopEntryInjectionInProgress)
    {
        return;
    }

    const DWORD nowMs = GetTickCount();
    const bool remapWindowFresh = g_nativeMenuOrderRemapArmed
        && g_nativeMenuOrderRemapTargetPtr != 0
        && g_nativeMenuOrderRemapArmMs != 0
        && !DebounceWindowElapsed(nowMs, g_nativeMenuOrderRemapArmMs, kNativeMenuExecuteArmMaxAgeMs);
    RootObject* remapTarget = remapWindowFresh
        ? reinterpret_cast<RootObject*>(g_nativeMenuOrderRemapTargetPtr)
        : 0;
    Character* remapActor = ResolveExecuteActorForPredicate();
    CanExecuteDiagnostics remapDiagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
    const bool remapDownedEnemyContext = remapTarget
        && CanExecuteFromNativeMenuSelection(remapActor, remapTarget, &remapDiagnostics, false);

    if (!(g_nativeMenuOrderRemapArmed
        && remapWindowFresh
        && remapDownedEnemyContext
        && g_effectiveEnableContextMenuInjection
        && g_effectiveEnableExecuteAction))
    {
        return;
    }

    static const char* kCandidateNames[4] = { "r14", "rsi", "rdi", "rcx" };
    const uintptr_t candidateValues[4] = {
        ordersCandidateR14,
        ordersCandidateRsi,
        ordersCandidateRdi,
        ordersCandidateRcx
    };

    uintptr_t selectedOrdersPtr = 0;
    uint32_t selectedOrdersCount = 0;
    uintptr_t selectedOrdersDataPtr = 0;

    for (size_t i = 0; i < 4; ++i)
    {
        uint32_t candidateCount = 0;
        uintptr_t candidateDataPtr = 0;
        const bool candidateValid = TryValidateOrderListPointerCandidate(
            candidateValues[i],
            &candidateCount,
            &candidateDataPtr);

        if (g_config.debugContextMenu)
        {
            std::stringstream candidateLog;
            candidateLog << "Loot-Scoot-Execute DEBUG: loop_entry_orders_candidate"
                         << " reg=" << kCandidateNames[i]
                         << " ptr=0x" << std::hex << candidateValues[i]
                         << " valid=" << (candidateValid ? 1 : 0)
                         << " size=" << std::dec << (candidateValid ? candidateCount : 0)
                         << " data=0x" << std::hex << (candidateValid ? candidateDataPtr : 0);
            DebugLog(candidateLog.str().c_str());
        }

        if (selectedOrdersPtr == 0 && candidateValid)
        {
            selectedOrdersPtr = candidateValues[i];
            selectedOrdersCount = candidateCount;
            selectedOrdersDataPtr = candidateDataPtr;
        }
    }

    const uintptr_t ordersPtr = selectedOrdersPtr;
    if (ordersPtr == 0)
    {
        return;
    }

    if (g_config.debugContextMenu)
    {
        std::stringstream selected;
        selected << "Loot-Scoot-Execute DEBUG: loop_entry_orders_selected"
                 << " ptr=0x" << std::hex << ordersPtr
                 << " size=" << std::dec << selectedOrdersCount
                 << " data=0x" << std::hex << selectedOrdersDataPtr;
        DebugLog(selected.str().c_str());
    }

    lektor<int>* orders = reinterpret_cast<lektor<int>*>(ordersPtr);

    if (g_nativeMenuLoopEntryInjectedOrdersPtr == ordersPtr
        && g_nativeMenuLoopEntryInjectedArmMs == g_nativeMenuOrderRemapArmMs)
    {
        return;
    }

    bool hasCarryOrder = false;
    bool hasExecuteOrder = false;
    const bool hasCarryResolved = TryOrderListContains(
        orders,
        kContextMenuOrderIdLiftPersonPlayerOrder,
        &hasCarryOrder);
    const bool hasExecuteResolved = TryOrderListContains(
        orders,
        kContextMenuOrderIdExecuteProxy,
        &hasExecuteOrder);

    int preSample[kContextMenuProbeOrderSampleCount] = { 0 };
    uint32_t preCount = 0;
    const size_t preSampleCount = CaptureOrderSampleForLog(
        orders,
        preSample,
        kContextMenuProbeOrderSampleCount,
        &preCount);
    {
        std::stringstream preLog;
        preLog << "Loot-Scoot-Execute INFO: loop_entry_inject_pre"
               << " orders=0x" << std::hex << ordersPtr
               << " count=" << std::dec << preCount
               << " first_orders=";
        AppendOrderSampleForLog(preLog, preSample, preSampleCount, preCount);
        preLog << " has225=" << (hasCarryResolved && hasCarryOrder ? 1 : 0)
               << " has134=" << (hasExecuteResolved && hasExecuteOrder ? 1 : 0);
        DebugLog(preLog.str().c_str());
    }

    if (!(hasCarryResolved && hasExecuteResolved && hasCarryOrder && !hasExecuteOrder))
    {
        return;
    }

    g_nativeMenuLoopEntryInjectionInProgress = true;
    g_nativeMenuAppendInjectionInProgress = true;
    const bool appended = TryAppendExecuteOrderToOrdersList(orders);
    g_nativeMenuAppendInjectionInProgress = false;
    g_nativeMenuLoopEntryInjectionInProgress = false;
    if (!appended)
    {
        return;
    }

    g_nativeMenuLoopEntryInjectedOrdersPtr = ordersPtr;
    g_nativeMenuLoopEntryInjectedArmMs = g_nativeMenuOrderRemapArmMs;
    g_nativeMenuExecuteRowInjectedOrdersPtr = ordersPtr;
    g_nativeMenuExecuteRowInjectedArmMs = g_nativeMenuOrderRemapArmMs;
    if (g_nativeMenuOrderRemapTargetPtr != 0)
    {
        ArmNativeMenuExecuteDispatchContext(g_nativeMenuOrderRemapTargetPtr, nowMs);
    }

    int postSample[kContextMenuProbeOrderSampleCount] = { 0 };
    uint32_t postCount = 0;
    const size_t postSampleCount = CaptureOrderSampleForLog(
        orders,
        postSample,
        kContextMenuProbeOrderSampleCount,
        &postCount);
    {
        std::stringstream postLog;
        postLog << "Loot-Scoot-Execute INFO: loop_entry_inject_post"
                << " orders=0x" << std::hex << ordersPtr
                << " count=" << std::dec << postCount
                << " first_orders=";
        AppendOrderSampleForLog(postLog, postSample, postSampleCount, postCount);
        postLog << " appended=134";
        DebugLog(postLog.str().c_str());
    }
}

static void ContextMenu_callDetourInjectHelper(uintptr_t ordersCandidateRsi)
{
    // buildRows pre-loop injection is the single active mutation path.
    if (kEnableBuildRowsPreloopInjection || kEnableRowInsertLateInjection)
    {
        return;
    }

    static uint32_t s_loopHeadCallDetourHitCount = 0;
    if (s_loopHeadCallDetourHitCount < 240)
    {
        std::stringstream hit;
        hit << "Loot-Scoot-Execute INFO: LOOPHEAD_CALL_DETOUR hit rsi=0x"
            << std::hex << ordersCandidateRsi;
        DebugLog(hit.str().c_str());
        ++s_loopHeadCallDetourHitCount;
    }

    if (g_nativeMenuAppendInjectionInProgress
        || !g_effectiveEnableContextMenuInjection
        || !g_effectiveEnableExecuteAction)
    {
        return;
    }

    uint32_t orderCount = 0;
    uintptr_t orderDataPtr = 0;
    if (!TryValidateOrderListPointerCandidate(ordersCandidateRsi, &orderCount, &orderDataPtr))
    {
        return;
    }

    lektor<int>* orders = reinterpret_cast<lektor<int>*>(ordersCandidateRsi);
    bool hasExecuteOrder = false;
    if (!TryOrderListContains(orders, kContextMenuOrderIdExecuteProxy, &hasExecuteOrder))
    {
        return;
    }

    if (hasExecuteOrder)
    {
        return;
    }

    if (g_nativeMenuLoopEntryInjectedOrdersPtr == ordersCandidateRsi)
    {
        return;
    }

    uint32_t beforeCount = 0;
    int beforeSample[3] = { 0 };
    const bool beforeCountResolved = TryReadOrderListCount(orders, &beforeCount);
    const bool beforeSampleResolved = TryReadInt32At(reinterpret_cast<const void*>(orderDataPtr + 0x0), &beforeSample[0])
        && TryReadInt32At(reinterpret_cast<const void*>(orderDataPtr + 0x4), &beforeSample[1])
        && TryReadInt32At(reinterpret_cast<const void*>(orderDataPtr + 0x8), &beforeSample[2]);
    const bool baselineValid = beforeCountResolved
        && beforeSampleResolved
        && beforeCount == 3
        && beforeSample[0] == kContextMenuOrderIdLoot
        && beforeSample[1] == kContextMenuOrderIdLiftPersonPlayerOrder
        && beforeSample[2] == 25;
    if (!baselineValid)
    {
        return;
    }

    static uint32_t s_callDetourSeen = 0;
    if (g_config.debugContextMenu && s_callDetourSeen < 200)
    {
        std::stringstream seen;
        seen << "Loot-Scoot-Execute DEBUG: call_detour_seen"
             << " call_index=" << std::dec << s_callDetourSeen
             << " patch_site=0x" << std::hex << g_contextMenuLoopEntryInlineTargetAddress
             << " orders_ptr=0x" << std::hex << ordersCandidateRsi
             << " count=";
        if (beforeCountResolved)
        {
            seen << std::dec << beforeCount;
        }
        else
        {
            seen << "unresolved";
        }
        seen << " first_before=";
        if (beforeSampleResolved)
        {
            seen << std::dec << beforeSample[0] << "," << beforeSample[1] << "," << beforeSample[2];
        }
        else
        {
            seen << "unresolved";
        }
        DebugLog(seen.str().c_str());
        ++s_callDetourSeen;
    }

    int capacityValue = 0;
    const bool capacityResolved = TryReadInt32At(
        reinterpret_cast<const void*>(ordersCandidateRsi + 0xC),
        &capacityValue);
    if (capacityResolved
        && capacityValue <= static_cast<int>(beforeCount))
    {
        return;
    }

    const uintptr_t appendSlot = orderDataPtr + static_cast<uintptr_t>(beforeCount) * sizeof(int);
    g_nativeMenuAppendInjectionInProgress = true;
    const bool wroteTask = TryWriteInt32At(
        reinterpret_cast<void*>(appendSlot),
        kContextMenuOrderIdExecuteProxy);
    const bool wroteSize = wroteTask
        && TryWriteInt32At(
            reinterpret_cast<void*>(ordersCandidateRsi + 0x8),
            static_cast<int>(beforeCount + 1));
    g_nativeMenuAppendInjectionInProgress = false;
    if (!wroteSize)
    {
        return;
    }

    const DWORD nowMs = GetTickCount();
    g_nativeMenuLoopEntryInjectedOrdersPtr = ordersCandidateRsi;
    g_nativeMenuLoopEntryInjectedArmMs = nowMs;
    g_nativeMenuExecuteRowInjectedOrdersPtr = ordersCandidateRsi;
    g_nativeMenuExecuteRowInjectedArmMs = nowMs;
    g_nativeMenuOrderRemapOrdersPtr = ordersCandidateRsi;

    uint32_t afterCount = 0;
    uintptr_t afterDataPtr = 0;
    int afterSample[4] = { 0 };
    const bool afterCountResolved = TryReadOrderListCount(orders, &afterCount);
    const bool afterDataResolved = TryReadUintptrAt(reinterpret_cast<const void*>(ordersCandidateRsi + 0x10), &afterDataPtr);
    const bool afterSampleResolved = afterDataResolved
        && afterDataPtr != 0
        && TryReadInt32At(reinterpret_cast<const void*>(afterDataPtr + 0x0), &afterSample[0])
        && TryReadInt32At(reinterpret_cast<const void*>(afterDataPtr + 0x4), &afterSample[1])
        && TryReadInt32At(reinterpret_cast<const void*>(afterDataPtr + 0x8), &afterSample[2])
        && TryReadInt32At(reinterpret_cast<const void*>(afterDataPtr + 0xC), &afterSample[3]);

    if (g_config.debugContextMenu)
    {
        std::stringstream inject;
        inject << "Loot-Scoot-Execute INFO: LOOPHEAD_APPEND"
               << " orders=0x" << std::hex << ordersCandidateRsi
               << " size 3->";
        if (afterCountResolved)
        {
            inject << std::dec << afterCount;
        }
        else
        {
            inject << "unresolved";
        }
        inject << " first=[";
        if (afterSampleResolved)
        {
            inject << std::dec << afterSample[0] << "," << afterSample[1] << "," << afterSample[2] << "," << afterSample[3];
        }
        else
        {
            inject << "unresolved";
        }
        inject << "]";
        DebugLog(inject.str().c_str());
    }

    if (afterCountResolved
        && afterCount == 4
        && afterSampleResolved
        && afterSample[0] == kContextMenuOrderIdLoot
        && afterSample[1] == kContextMenuOrderIdLiftPersonPlayerOrder
        && afterSample[2] == 25
        && afterSample[3] == kContextMenuOrderIdExecuteProxy)
    {
        DebugLog("Loot-Scoot-Execute INFO: LOOPHEAD_APPEND OK size 3->4 first=[26,225,25,134]");
    }
}

static bool InstallContextMenuLoopEntryInlineHook(uintptr_t targetAddress)
{
    static const size_t kLoopHeadCallOffset = 3;
    static const size_t kPatchSize = 5;
    if (g_contextMenuLoopEntryInlineHookInstalled)
    {
        return true;
    }

    if (targetAddress == 0 || !IsExecutableCodeAddress(targetAddress))
    {
        std::stringstream invalid;
        invalid << "Loot-Scoot-Execute WARN: loop-entry call detour skipped reason=invalid_target"
                << " loop_head=0x" << std::hex << targetAddress;
        ErrorLog(invalid.str().c_str());
        return false;
    }

    const uintptr_t patchAddress = targetAddress + kLoopHeadCallOffset;
    if (!IsExecutableCodeAddress(patchAddress))
    {
        std::stringstream invalidPatch;
        invalidPatch << "Loot-Scoot-Execute WARN: loop-entry call detour skipped reason=invalid_patch_site"
                     << " loop_head=0x" << std::hex << targetAddress
                     << " patch_site=0x" << std::hex << patchAddress;
        ErrorLog(invalidPatch.str().c_str());
        return false;
    }

    unsigned char original[kPatchSize] = { 0 };
    if (!TryReadCodeBytes(patchAddress, original, kPatchSize))
    {
        std::stringstream unreadable;
        unreadable << "Loot-Scoot-Execute WARN: loop-entry call detour skipped reason=read_failed"
                   << " patch_site=0x" << std::hex << patchAddress;
        ErrorLog(unreadable.str().c_str());
        return false;
    }
    if (original[0] != 0xE8)
    {
        std::stringstream mismatch;
        mismatch << "Loot-Scoot-Execute WARN: loop-entry call detour skipped reason=expected_call_opcode"
                 << " loop_head=0x" << std::hex << targetAddress
                 << " patch_site=0x" << std::hex << patchAddress
                 << " bytes=\"" << FormatCodeBytes(patchAddress, 8) << "\"";
        ErrorLog(mismatch.str().c_str());
        return false;
    }

    int32_t originalRel = 0;
    std::memcpy(&originalRel, original + 1, sizeof(originalRel));
    const uintptr_t originalCallNext = patchAddress + kPatchSize;
    const intptr_t originalCallTargetRaw = static_cast<intptr_t>(originalCallNext) + static_cast<intptr_t>(originalRel);
    if (originalCallTargetRaw <= 0)
    {
        std::stringstream invalidCallTarget;
        invalidCallTarget << "Loot-Scoot-Execute WARN: loop-entry call detour skipped reason=invalid_call_target"
                          << " patch_site=0x" << std::hex << patchAddress;
        ErrorLog(invalidCallTarget.str().c_str());
        return false;
    }
    const uintptr_t originalCallTarget = static_cast<uintptr_t>(originalCallTargetRaw);
    if (!IsExecutableCodeAddress(originalCallTarget))
    {
        std::stringstream invalidCallTarget;
        invalidCallTarget << "Loot-Scoot-Execute WARN: loop-entry call detour skipped reason=call_target_not_executable"
                          << " patch_site=0x" << std::hex << patchAddress
                          << " call_target=0x" << std::hex << originalCallTarget;
        ErrorLog(invalidCallTarget.str().c_str());
        return false;
    }

    unsigned char* stub = reinterpret_cast<unsigned char*>(AllocateExecutableStubNear(patchAddress, 512));
    if (!stub)
    {
        std::stringstream allocFail;
        allocFail << "Loot-Scoot-Execute WARN: loop-entry call detour skipped reason=stub_alloc_failed"
                  << " patch_site=0x" << std::hex << patchAddress;
        ErrorLog(allocFail.str().c_str());
        return false;
    }

    std::vector<unsigned char> stubCode;
    stubCode.reserve(256);

    auto emitByte = [&stubCode](unsigned char b)
    {
        stubCode.push_back(b);
    };
    auto emitBytes = [&stubCode](const unsigned char* bytes, size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            stubCode.push_back(bytes[i]);
        }
    };
    auto emitU64 = [&stubCode](uint64_t value)
    {
        for (size_t i = 0; i < sizeof(value); ++i)
        {
            stubCode.push_back(static_cast<unsigned char>((value >> (8 * i)) & 0xFF));
        }
    };

    emitByte(0x9C);                                // pushfq
    emitByte(0x50);                                // push rax
    emitByte(0x51);                                // push rcx
    emitByte(0x52);                                // push rdx
    emitBytes(reinterpret_cast<const unsigned char*>("\x41\x50"), 2); // push r8
    emitBytes(reinterpret_cast<const unsigned char*>("\x41\x51"), 2); // push r9
    emitBytes(reinterpret_cast<const unsigned char*>("\x41\x52"), 2); // push r10
    emitBytes(reinterpret_cast<const unsigned char*>("\x41\x53"), 2); // push r11
    emitBytes(reinterpret_cast<const unsigned char*>("\x49\x89\xE3"), 3); // mov r11,rsp
    emitBytes(reinterpret_cast<const unsigned char*>("\x48\x83\xE4\xF0"), 4); // and rsp,-16
    emitBytes(reinterpret_cast<const unsigned char*>("\x48\x83\xEC\x20"), 4); // sub rsp,0x20
    emitBytes(reinterpret_cast<const unsigned char*>("\x48\x89\xF1"), 3); // mov rcx,rsi
    emitBytes(reinterpret_cast<const unsigned char*>("\x48\xB8"), 2); // mov rax,imm64
    emitU64(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&ContextMenu_callDetourInjectHelper)));
    emitBytes(reinterpret_cast<const unsigned char*>("\xFF\xD0"), 2); // call rax
    emitBytes(reinterpret_cast<const unsigned char*>("\x4C\x89\xDC"), 3); // mov rsp,r11
    emitBytes(reinterpret_cast<const unsigned char*>("\x41\x5B"), 2); // pop r11
    emitBytes(reinterpret_cast<const unsigned char*>("\x41\x5A"), 2); // pop r10
    emitBytes(reinterpret_cast<const unsigned char*>("\x41\x59"), 2); // pop r9
    emitBytes(reinterpret_cast<const unsigned char*>("\x41\x58"), 2); // pop r8
    emitByte(0x5A);                                // pop rdx
    emitByte(0x59);                                // pop rcx
    emitByte(0x58);                                // pop rax
    emitByte(0x9D);                                // popfq

    // Execute the original call that was replaced at patch_site.
    emitBytes(reinterpret_cast<const unsigned char*>("\x48\xB8"), 2); // mov rax,imm64
    emitU64(static_cast<uint64_t>(originalCallTarget));
    emitBytes(reinterpret_cast<const unsigned char*>("\xFF\xD0"), 2); // call rax

    // Continue execution after patch_site without clobbering registers.
    emitBytes(reinterpret_cast<const unsigned char*>("\xFF\x25\x00\x00\x00\x00"), 6); // jmp [rip]
    emitU64(static_cast<uint64_t>(patchAddress + kPatchSize));

    if (!WriteCodeBytes(reinterpret_cast<uintptr_t>(stub), stubCode.data(), stubCode.size()))
    {
        std::stringstream stubWriteFail;
        stubWriteFail << "Loot-Scoot-Execute WARN: loop-entry call detour skipped reason=stub_write_failed"
                      << " patch_site=0x" << std::hex << patchAddress
                      << " stub=0x" << std::hex << reinterpret_cast<uintptr_t>(stub);
        ErrorLog(stubWriteFail.str().c_str());
        VirtualFree(stub, 0, MEM_RELEASE);
        return false;
    }

    const intptr_t relWide = static_cast<intptr_t>(reinterpret_cast<uintptr_t>(stub))
        - static_cast<intptr_t>(patchAddress + kPatchSize);
    if (relWide < static_cast<intptr_t>(-0x80000000LL)
        || relWide > static_cast<intptr_t>(0x7FFFFFFFLL))
    {
        std::stringstream relFail;
        relFail << "Loot-Scoot-Execute WARN: loop-entry call detour skipped reason=rel32_out_of_range"
                << " patch_site=0x" << std::hex << patchAddress
                << " stub=0x" << std::hex << reinterpret_cast<uintptr_t>(stub)
                << " delta=0x" << std::hex << static_cast<uintptr_t>(relWide >= 0 ? relWide : -relWide);
        ErrorLog(relFail.str().c_str());
        VirtualFree(stub, 0, MEM_RELEASE);
        return false;
    }

    unsigned char patch[kPatchSize] = { 0 };
    patch[0] = 0xE9;
    const int32_t rel32 = static_cast<int32_t>(relWide);
    std::memcpy(patch + 1, &rel32, sizeof(rel32));
    if (!WriteCodeBytes(patchAddress, patch, sizeof(patch)))
    {
        std::stringstream patchWriteFail;
        patchWriteFail << "Loot-Scoot-Execute WARN: loop-entry call detour skipped reason=patch_write_failed"
                       << " patch_site=0x" << std::hex << patchAddress;
        ErrorLog(patchWriteFail.str().c_str());
        VirtualFree(stub, 0, MEM_RELEASE);
        return false;
    }

    std::memset(g_contextMenuLoopEntryInlineOriginalBytes, 0, sizeof(g_contextMenuLoopEntryInlineOriginalBytes));
    std::memcpy(g_contextMenuLoopEntryInlineOriginalBytes, original, kPatchSize);
    g_contextMenuLoopEntryInlineTargetAddress = patchAddress;
    g_contextMenuLoopEntryInlineReturnAddress = patchAddress + kPatchSize;
    g_contextMenuLoopEntryInlineStubAddress = stub;
    g_contextMenuLoopEntryInlineHookInstalled = true;

    std::stringstream installed;
    installed << "Loot-Scoot-Execute INFO: loop-entry call detour installed"
              << " loop_head=0x" << std::hex << targetAddress
              << " patch_site=0x" << std::hex << patchAddress
              << " original_call_target=0x" << std::hex << originalCallTarget
              << " patch_bytes=\"" << FormatCodeBytes(patchAddress, 5) << "\"";
    DebugLog(installed.str().c_str());

    return true;
}

static void LogContextMenuRowMaterializationSnapshot(
    ContextMenu* menu,
    const char* sourceTag,
    uint64_t showSeq)
{
    if (!menu || !g_config.debugContextMenu)
    {
        return;
    }

    static uint32_t s_rowSnapshotHit = 0;
    if (s_rowSnapshotHit >= 200)
    {
        return;
    }

    const uintptr_t callerAddress = CaptureCallerAddress();
    const uintptr_t callerRva = ComputeRvaFromAbsoluteAddress(callerAddress);

    bool visible = false;
    uint32_t ordersCount = 0;
    if (!TryReadContextMenuVisibilityAndOrderCount(menu, &visible, &ordersCount))
    {
        ErrorLog("Loot-Scoot-Execute WARN: row_materialization_snapshot_failed");
        return;
    }

    const uint32_t cappedRows = ordersCount > static_cast<uint32_t>(kContextMenuRowMaterializationMaxRows)
        ? static_cast<uint32_t>(kContextMenuRowMaterializationMaxRows)
        : ordersCount;

    std::stringstream logline;
    logline << "Loot-Scoot-Execute DEBUG: context_menu_row_materialization_snapshot"
            << " hit_index=" << std::dec << s_rowSnapshotHit
            << " source=" << (sourceTag ? sourceTag : "unknown")
            << " show_seq=" << std::dec << showSeq
            << " visible=" << (visible ? "true" : "false")
            << " row_count=" << std::dec << ordersCount
            << " caller=0x" << std::hex << callerAddress;
    if (callerRva != 0)
    {
        logline << " caller_rva=0x" << std::hex << callerRva;
    }
    logline << " rows=[";

    for (uint32_t i = 0; i < cappedRows; ++i)
    {
        int taskId = 0;
        if (!TryReadContextMenuOrderAt(menu, i, &taskId))
        {
            taskId = 0;
        }

        std::string label;
        const bool labelResolved = TryResolveTaskLabelForMaterializationProbe(menu, taskId, &label);
        if (i > 0)
        {
            logline << ",";
        }
        logline << "{task=" << std::dec << taskId << ",label=\""
                << (labelResolved ? SanitizeMenuLabelForLog(label) : "<unresolved>")
                << "\"}";
    }

    if (ordersCount > cappedRows)
    {
        if (cappedRows > 0)
        {
            logline << ",";
        }
        logline << "...";
    }
    logline << "]";

    DebugLog(logline.str().c_str());
    ++s_rowSnapshotHit;
}

static bool TryReadMouseRightTargetForContextMenu(
    ContextMenu* menu,
    bool* targetSetOut,
    RootObject** targetOut,
    bool* ownerMatchedOut)
{
    if (!targetSetOut || !targetOut || !ownerMatchedOut)
    {
        return false;
    }

    *targetSetOut = false;
    *targetOut = 0;
    *ownerMatchedOut = false;

    if (!ou)
    {
        return false;
    }

    PlayerInterface* player = 0;
    __try
    {
        player = ou->player;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    if (!player)
    {
        return false;
    }

    __try
    {
        *ownerMatchedOut = (&player->contextMenu == menu);
        *targetSetOut = player->mouseRightTargetSet;
        if (*targetSetOut)
        {
            *targetOut = player->mouseRightTarget;
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool ContextMenuOrderSampleChanged(const int* orderSample, size_t orderSampleCount)
{
    if (!orderSample && orderSampleCount > 0)
    {
        return false;
    }

    if (!g_hasContextMenuProbeSnapshot)
    {
        return true;
    }

    if (g_lastContextMenuProbeSampleCount != orderSampleCount)
    {
        return true;
    }

    for (size_t i = 0; i < orderSampleCount; ++i)
    {
        if (g_lastContextMenuProbeOrderSample[i] != orderSample[i])
        {
            return true;
        }
    }

    return false;
}

static bool OrderSamplesEqual(const int* lhs, size_t lhsCount, const int* rhs, size_t rhsCount)
{
    if (lhsCount != rhsCount)
    {
        return false;
    }

    if ((!lhs && lhsCount > 0) || (!rhs && rhsCount > 0))
    {
        return false;
    }

    for (size_t i = 0; i < lhsCount; ++i)
    {
        if (lhs[i] != rhs[i])
        {
            return false;
        }
    }

    return true;
}

static void UpdateContextMenuProbeSnapshot(
    bool on,
    bool visible,
    uintptr_t whatPtr,
    uintptr_t mouseRightTargetPtr,
    uint32_t ordersCount,
    const std::string& menuName,
    const int* orderSample,
    size_t orderSampleCount,
    DWORD nowMs)
{
    g_hasContextMenuProbeSnapshot = true;
    g_lastContextMenuProbeLogMs = nowMs;
    g_lastContextMenuProbeOn = on;
    g_lastContextMenuProbeVisible = visible;
    g_lastContextMenuProbeWhatPtr = whatPtr;
    g_lastContextMenuProbeMouseRightTargetPtr = mouseRightTargetPtr;
    g_lastContextMenuProbeOrdersCount = ordersCount;
    g_lastContextMenuProbeName = menuName;
    g_lastContextMenuProbeSampleCount = orderSampleCount;

    for (size_t i = 0; i < orderSampleCount; ++i)
    {
        g_lastContextMenuProbeOrderSample[i] = orderSample[i];
    }
}

static void UpdateContextMenuShowProbeEvent(
    bool on,
    bool visible,
    uintptr_t whatPtr,
    uintptr_t mouseRightTargetPtr,
    uint32_t ordersCount,
    const int* orderSample,
    size_t orderSampleCount,
    DWORD nowMs)
{
    ++g_contextMenuShowProbeEventSeq;
    g_contextMenuShowProbeEventMs = nowMs;
    g_contextMenuShowProbeEventOn = on;
    g_contextMenuShowProbeEventVisible = visible;
    g_contextMenuShowProbeEventWhatPtr = whatPtr;
    g_contextMenuShowProbeEventMouseRightTargetPtr = mouseRightTargetPtr;
    g_contextMenuShowProbeEventOrdersCount = ordersCount;
    g_contextMenuShowProbeEventSampleCount = orderSampleCount;

    for (size_t i = 0; i < orderSampleCount; ++i)
    {
        g_contextMenuShowProbeEventOrderSample[i] = orderSample[i];
    }
}

static void DisarmNativeMenuExecuteDispatchContext()
{
    g_nativeMenuExecuteDispatchArmed = false;
    g_nativeMenuExecuteDispatchTargetPtr = 0;
    g_nativeMenuExecuteDispatchArmMs = 0;
}

static void ResetNativeMenuExecuteRowInsertState()
{
    g_menuLatchActive = false;
    g_menuLatchTimestampMs = 0;
    g_nativeMenuExecuteRowInjectedOrdersPtr = 0;
    g_nativeMenuExecuteRowInjectedArmMs = 0;
    g_nativeMenuBuildRowsInjectedOrdersPtr = 0;
    g_nativeMenuBuildRowsInjectedShowSeq = 0;
    g_nativeMenuRowDescriptorTemplate = 0;
    g_nativeMenuRowDescriptorTemplateOrdersPtr = 0;
    g_nativeMenuRowDescriptorTemplateArmMs = 0;
    g_nativeMenuLoopEntryInjectedOrdersPtr = 0;
    g_nativeMenuLoopEntryInjectedArmMs = 0;
}

static bool IsNativeMenuExecuteRowAlreadyInjectedForActiveMenu()
{
    if (!g_nativeMenuOrderRemapArmed
        || g_nativeMenuOrderRemapOrdersPtr == 0
        || g_nativeMenuOrderRemapArmMs == 0)
    {
        return false;
    }

    return g_nativeMenuExecuteRowInjectedOrdersPtr == g_nativeMenuOrderRemapOrdersPtr
        && g_nativeMenuExecuteRowInjectedArmMs == g_nativeMenuOrderRemapArmMs;
}

static void MarkNativeMenuExecuteRowInjectedForActiveMenu()
{
    g_nativeMenuExecuteRowInjectedOrdersPtr = g_nativeMenuOrderRemapOrdersPtr;
    g_nativeMenuExecuteRowInjectedArmMs = g_nativeMenuOrderRemapArmMs;
}

static void ArmNativeMenuExecuteDispatchContext(uintptr_t targetPtr, DWORD nowMs)
{
    g_nativeMenuExecuteDispatchArmed = true;
    g_nativeMenuExecuteDispatchTargetPtr = targetPtr;
    g_nativeMenuExecuteDispatchArmMs = nowMs;
}

static void DisarmNativeMenuOrderRemapContext()
{
    g_nativeMenuOrderRemapArmed = false;
    g_nativeMenuOrderRemapOrdersPtr = 0;
    g_nativeMenuOrderRemapTargetPtr = 0;
    g_nativeMenuOrderRemapArmMs = 0;
    ResetNativeMenuExecuteRowInsertState();
}

static void ArmNativeMenuOrderRemapContext(ContextMenu* menu, RootObject* target, DWORD nowMs)
{
    if (!menu || !target)
    {
        DisarmNativeMenuOrderRemapContext();
        return;
    }

    const uintptr_t nextOrdersPtr = reinterpret_cast<uintptr_t>(&menu->orders);
    const uintptr_t nextTargetPtr = reinterpret_cast<uintptr_t>(target);
    const bool sameContext = g_nativeMenuOrderRemapArmed
        && g_nativeMenuOrderRemapOrdersPtr == nextOrdersPtr
        && g_nativeMenuOrderRemapTargetPtr == nextTargetPtr;

    if (!sameContext)
    {
        ResetNativeMenuExecuteRowInsertState();
    }

    g_nativeMenuOrderRemapArmed = true;
    g_nativeMenuOrderRemapOrdersPtr = nextOrdersPtr;
    g_nativeMenuOrderRemapTargetPtr = nextTargetPtr;
    if (!sameContext || g_nativeMenuOrderRemapArmMs == 0)
    {
        g_nativeMenuOrderRemapArmMs = nowMs;
    }
}

static bool TryRemapDownedEnemyOrderToExecute(
    ContextMenu* menu,
    bool* executeOrderAlreadyPresentOut,
    bool* remappedCarryOrderOut,
    bool* appendedExecuteOrderOut)
{
    if (!menu || !executeOrderAlreadyPresentOut || !remappedCarryOrderOut || !appendedExecuteOrderOut)
    {
        return false;
    }

    *executeOrderAlreadyPresentOut = false;
    *remappedCarryOrderOut = false;
    *appendedExecuteOrderOut = false;

    __try
    {
        if (g_nativeMenuAppendInjectionInProgress)
        {
            return true;
        }

        const uintptr_t ordersPtr = reinterpret_cast<uintptr_t>(&menu->orders);
        if (ordersPtr != 0 && g_nativeMenuExecuteRowInjectedOrdersPtr == ordersPtr)
        {
            *executeOrderAlreadyPresentOut = true;
            return true;
        }

        const uint32_t orderCount = menu->orders.size();
        bool carryOrderPresent = false;
        for (uint32_t i = 0; i < orderCount; ++i)
        {
            const int orderId = menu->orders[i];
            if (orderId == kContextMenuOrderIdExecuteProxy)
            {
                *executeOrderAlreadyPresentOut = true;
                return true;
            }
            if (orderId == kContextMenuOrderIdLiftPersonPlayerOrder)
            {
                carryOrderPresent = true;
            }
        }

        if (!carryOrderPresent)
        {
            return true;
        }

        const uint32_t beforeCount = menu->orders.size();
        g_nativeMenuAppendInjectionInProgress = true;
        // Preserve the native carry row and append a real execute row.
        const bool appended = TryAppendExecuteOrderToOrdersList(&menu->orders);
        g_nativeMenuAppendInjectionInProgress = false;
        if (!appended)
        {
            return false;
        }

        g_nativeMenuExecuteRowInjectedOrdersPtr = ordersPtr;
        g_nativeMenuExecuteRowInjectedArmMs = g_nativeMenuOrderRemapArmMs;
        (void)beforeCount;
        *appendedExecuteOrderOut = true;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_nativeMenuAppendInjectionInProgress = false;
        return false;
    }
}

static bool TryResolveBuildRowsOrdersPointer(
    ContextMenu* menu,
    lektor<int>** ordersOut,
    uintptr_t* ordersPtrOut)
{
    if (!menu || !ordersOut || !ordersPtrOut)
    {
        return false;
    }

    *ordersOut = 0;
    *ordersPtrOut = 0;

    const uintptr_t menuPtr = reinterpret_cast<uintptr_t>(menu);
    const uintptr_t ordersPtr = menuPtr + 0x8;

    uint32_t candidateCount = 0;
    uintptr_t candidateDataPtr = 0;
    const bool valid = TryValidateOrderListPointerCandidate(
        ordersPtr,
        &candidateCount,
        &candidateDataPtr);

    if (g_config.debugContextMenu)
    {
        std::stringstream probe;
        probe << "Loot-Scoot-Execute DEBUG: buildRows_orders_direct_candidate"
              << " menu_ptr=0x" << std::hex << menuPtr
              << " orders_ptr=0x" << std::hex << ordersPtr
              << " valid=" << (valid ? 1 : 0)
              << " size=" << std::dec << (valid ? candidateCount : 0)
              << " data=0x" << std::hex << (valid ? candidateDataPtr : 0);
        DebugLog(probe.str().c_str());
    }

    if (!valid)
    {
        return false;
    }

    *ordersOut = reinterpret_cast<lektor<int>*>(ordersPtr);
    *ordersPtrOut = ordersPtr;
    return true;
}

static bool TryAppendExecuteOrderToOrdersList(lektor<int>* orders)
{
    if (!orders)
    {
        return false;
    }

    __try
    {
        uint32_t beforeCount = 0;
        if (!TryReadOrderListCount(orders, &beforeCount))
        {
            return false;
        }

        const uintptr_t ordersPtr = reinterpret_cast<uintptr_t>(orders);
        uintptr_t dataPtr = 0;
        if (!TryReadUintptrAt(reinterpret_cast<const void*>(ordersPtr + 0x10), &dataPtr)
            || dataPtr == 0)
        {
            return false;
        }

        int capacity = 0;
        const bool capacityResolved = TryReadInt32At(
            reinterpret_cast<const void*>(ordersPtr + 0xC),
            &capacity);
        if (capacityResolved
            && (capacity <= 0
                || static_cast<uint32_t>(capacity) <= beforeCount))
        {
            return false;
        }

        if (!TryWriteInt32At(
            reinterpret_cast<void*>(dataPtr + static_cast<uintptr_t>(beforeCount) * sizeof(int)),
            kContextMenuOrderIdExecuteProxy))
        {
            return false;
        }

        if (!TryWriteInt32At(
            reinterpret_cast<void*>(ordersPtr + 0x8),
            static_cast<int>(beforeCount + 1)))
        {
            return false;
        }

        uint32_t afterCount = 0;
        if (!TryReadOrderListCount(orders, &afterCount)
            || afterCount != (beforeCount + 1))
        {
            return false;
        }

        int appendedValue = 0;
        if (!TryReadInt32At(
            reinterpret_cast<const void*>(dataPtr + static_cast<uintptr_t>(beforeCount) * sizeof(int)),
            &appendedValue)
            || appendedValue != kContextMenuOrderIdExecuteProxy)
        {
            return false;
        }

        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryInjectExecuteOrderInShowContextMenuPre(ContextMenu* menu, RootObject* target, DWORD nowMs)
{
    if (!menu
        || !target
        || !g_effectiveEnableContextMenuInjection
        || !g_effectiveEnableExecuteAction
        || g_nativeMenuAppendInjectionInProgress)
    {
        return false;
    }

    const uintptr_t menuPtr = reinterpret_cast<uintptr_t>(menu);
    const uintptr_t ordersPtr = menuPtr + 0x8;
    lektor<int>* orders = reinterpret_cast<lektor<int>*>(ordersPtr);

    uint32_t beforeCount = 0;
    const bool beforeCountResolved = TryReadOrderListCount(orders, &beforeCount);
    uintptr_t beforeDataPtr = 0;
    const bool beforeDataResolved = TryReadUintptrAt(reinterpret_cast<const void*>(ordersPtr + 0x10), &beforeDataPtr);

    int before0 = 0;
    int before1 = 0;
    int before2 = 0;
    const bool beforeBaselineResolved = beforeDataResolved
        && beforeDataPtr != 0
        && TryReadInt32At(reinterpret_cast<const void*>(beforeDataPtr + 0x0), &before0)
        && TryReadInt32At(reinterpret_cast<const void*>(beforeDataPtr + 0x4), &before1)
        && TryReadInt32At(reinterpret_cast<const void*>(beforeDataPtr + 0x8), &before2);

    const bool baselineValid = beforeCountResolved
        && beforeCount == 3
        && beforeBaselineResolved
        && before0 == kContextMenuOrderIdLoot
        && before1 == kContextMenuOrderIdLiftPersonPlayerOrder
        && before2 == 25;
    if (!baselineValid)
    {
        if (g_config.debugContextMenu)
        {
            std::stringstream skip;
            skip << "Loot-Scoot-Execute WARN: show_pre_inject_skip invalid_orders"
                 << " menu=0x" << std::hex << menuPtr
                 << " orders=0x" << std::hex << ordersPtr
                 << " size=";
            if (beforeCountResolved)
            {
                skip << std::dec << beforeCount;
            }
            else
            {
                skip << "unresolved";
            }
            skip << " first=[";
            if (beforeBaselineResolved)
            {
                skip << std::dec << before0 << "," << before1 << "," << before2;
            }
            else
            {
                skip << "unresolved";
            }
            skip << "]";
            DebugLog(skip.str().c_str());
        }
        return false;
    }

    bool hasExecuteOrder = false;
    const bool hasExecuteOrderResolved = TryOrderListContains(
        orders,
        kContextMenuOrderIdExecuteProxy,
        &hasExecuteOrder);
    if (hasExecuteOrderResolved && hasExecuteOrder)
    {
        g_nativeMenuOrderRemapOrdersPtr = ordersPtr;
        g_nativeMenuExecuteRowInjectedOrdersPtr = ordersPtr;
        g_nativeMenuExecuteRowInjectedArmMs = (g_nativeMenuOrderRemapArmMs != 0) ? g_nativeMenuOrderRemapArmMs : nowMs;
        return true;
    }

    g_nativeMenuAppendInjectionInProgress = true;
    const bool appended = TryAppendExecuteOrderToOrdersList(orders);
    g_nativeMenuAppendInjectionInProgress = false;

    uint32_t afterCount = 0;
    const bool afterCountResolved = TryReadOrderListCount(orders, &afterCount);
    uintptr_t afterDataPtr = 0;
    const bool afterDataResolved = TryReadUintptrAt(reinterpret_cast<const void*>(ordersPtr + 0x10), &afterDataPtr);
    int after0 = 0;
    int after1 = 0;
    int after2 = 0;
    int after3 = 0;
    const bool afterOrdersResolved = afterDataResolved
        && afterDataPtr != 0
        && TryReadInt32At(reinterpret_cast<const void*>(afterDataPtr + 0x0), &after0)
        && TryReadInt32At(reinterpret_cast<const void*>(afterDataPtr + 0x4), &after1)
        && TryReadInt32At(reinterpret_cast<const void*>(afterDataPtr + 0x8), &after2)
        && TryReadInt32At(reinterpret_cast<const void*>(afterDataPtr + 0xC), &after3);

    if (g_config.debugContextMenu)
    {
        std::stringstream info;
        info << "Loot-Scoot-Execute INFO: show_pre_inject"
             << " menu=0x" << std::hex << menuPtr
             << " orders=0x" << std::hex << ordersPtr
             << " size_before=";
        if (beforeCountResolved)
        {
            info << std::dec << beforeCount;
        }
        else
        {
            info << "unresolved";
        }
        info << " size_after=";
        if (afterCountResolved)
        {
            info << std::dec << afterCount;
        }
        else
        {
            info << "unresolved";
        }
        info << " first_after=[";
        if (afterOrdersResolved)
        {
            info << std::dec << after0 << "," << after1 << "," << after2 << "," << after3;
        }
        else
        {
            info << "unresolved";
        }
        info << "] applied=" << (appended ? 1 : 0);
        DebugLog(info.str().c_str());
    }

    if (!appended)
    {
        return false;
    }

    g_nativeMenuOrderRemapOrdersPtr = ordersPtr;
    g_nativeMenuExecuteRowInjectedOrdersPtr = ordersPtr;
    g_nativeMenuExecuteRowInjectedArmMs = (g_nativeMenuOrderRemapArmMs != 0) ? g_nativeMenuOrderRemapArmMs : nowMs;
    g_nativeMenuBuildRowsInjectedOrdersPtr = ordersPtr;
    g_nativeMenuBuildRowsInjectedShowSeq = g_contextMenuShowProbeEventSeq;
    ArmNativeMenuExecuteDispatchContext(reinterpret_cast<uintptr_t>(target), nowMs);
    return true;
}

static void TryInjectExecuteOrderInBuildRows(ContextMenu* menu)
{
    if (!kEnableBuildRowsPreloopInjection
        || !menu
        || g_nativeMenuAppendInjectionInProgress)
    {
        return;
    }

    const uintptr_t menuPtr = reinterpret_cast<uintptr_t>(menu);
    const uintptr_t expectedOrdersPtr = menuPtr + 0x8;
    const DWORD nowMs = GetTickCount();

    const bool remapWindowFresh = g_nativeMenuOrderRemapArmed
        && g_nativeMenuOrderRemapTargetPtr != 0
        && g_nativeMenuOrderRemapArmMs != 0
        && !DebounceWindowElapsed(nowMs, g_nativeMenuOrderRemapArmMs, kNativeMenuExecuteArmMaxAgeMs);
    RootObject* remapTarget = remapWindowFresh
        ? reinterpret_cast<RootObject*>(g_nativeMenuOrderRemapTargetPtr)
        : 0;
    Character* remapActor = ResolveExecuteActorForPredicate();
    CanExecuteDiagnostics remapDiagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
    const bool remapDownedEnemyContext = remapTarget
        && CanExecuteFromNativeMenuSelection(remapActor, remapTarget, &remapDiagnostics, false);

    if (!(g_nativeMenuOrderRemapArmed && remapWindowFresh && remapDownedEnemyContext))
    {
        return;
    }

    lektor<int>* orders = 0;
    uintptr_t ordersPtr = 0;
    const bool ordersResolved = TryResolveBuildRowsOrdersPointer(menu, &orders, &ordersPtr);
    if (!ordersResolved)
    {
        if (g_config.debugContextMenu)
        {
            std::stringstream miss;
            miss << "Loot-Scoot-Execute WARN: buildRows_inject orders_ptr_not_found"
                 << " menu_ptr=0x" << std::hex << menuPtr
                 << " expected_orders_ptr=0x" << std::hex << expectedOrdersPtr
                 << " pointer_window=[";
            for (size_t off = 0; off <= 0x50; off += sizeof(uintptr_t))
            {
                if (off > 0)
                {
                    miss << ",";
                }
                uintptr_t value = 0;
                const bool valueResolved = TryReadUintptrAt(reinterpret_cast<const void*>(menuPtr + off), &value);
                miss << "+0x" << std::hex << off << "=";
                if (valueResolved)
                {
                    miss << "0x" << std::hex << value;
                }
                else
                {
                    miss << "unreadable";
                }
            }
            miss << "]";
            ErrorLog(miss.str().c_str());
        }
        return;
    }

    uint32_t beforeCount = 0;
    const bool beforeCountResolved = TryReadOrderListCount(orders, &beforeCount);
    uintptr_t beforeData = 0;
    const bool beforeDataResolved = TryReadUintptrAt(reinterpret_cast<const void*>(ordersPtr + 0x10), &beforeData);

    int before0 = 0;
    int before1 = 0;
    int before2 = 0;
    const bool beforeOrdersResolved = beforeDataResolved
        && beforeData != 0
        && TryReadInt32At(reinterpret_cast<const void*>(beforeData + 0x0), &before0)
        && TryReadInt32At(reinterpret_cast<const void*>(beforeData + 0x4), &before1)
        && TryReadInt32At(reinterpret_cast<const void*>(beforeData + 0x8), &before2);

    {
        std::stringstream preloop;
        preloop << "Loot-Scoot-Execute INFO: buildRows_preloop"
                << " orders=0x" << std::hex << ordersPtr
                << " size=";
        if (beforeCountResolved)
        {
            preloop << std::dec << beforeCount;
        }
        else
        {
            preloop << "unresolved";
        }
        preloop << " first=[";
        if (beforeOrdersResolved)
        {
            preloop << std::dec << before0 << "," << before1 << "," << before2;
        }
        else
        {
            preloop << "unresolved";
        }
        preloop << "]";
        DebugLog(preloop.str().c_str());
    }

    const bool baselineValid = beforeCountResolved
        && beforeCount == 3
        && beforeOrdersResolved
        && before0 == kContextMenuOrderIdLoot
        && before1 == kContextMenuOrderIdLiftPersonPlayerOrder
        && before2 == 25;

    if (!baselineValid)
    {
        if (g_config.debugContextMenu)
        {
            std::stringstream skip;
            skip << "Loot-Scoot-Execute WARN: buildRows_inject_skip invalid_orders"
                 << " menu=0x" << std::hex << menuPtr
                 << " orders=0x" << std::hex << ordersPtr
                 << " size=";
            if (beforeCountResolved)
            {
                skip << std::dec << beforeCount;
            }
            else
            {
                skip << "unresolved";
            }
            skip << " first=[" << std::dec
                 << (beforeOrdersResolved ? before0 : 0) << ","
                 << (beforeOrdersResolved ? before1 : 0) << ","
                 << (beforeOrdersResolved ? before2 : 0) << "]";
            ErrorLog(skip.str().c_str());
        }
        return;
    }

    bool hasCarry = false;
    bool hasExecuteBeforeResolved = false;
    bool hasExecuteBefore = false;
    const bool hasCarryResolved = TryOrderListContains(
        orders,
        kContextMenuOrderIdLiftPersonPlayerOrder,
        &hasCarry);
    hasExecuteBeforeResolved = TryOrderListContains(
        orders,
        kContextMenuOrderIdExecuteProxy,
        &hasExecuteBefore);

    const bool alreadyInjectedForOrders = g_nativeMenuBuildRowsInjectedOrdersPtr == ordersPtr;
    const bool shouldAppend = hasCarryResolved
        && hasCarry
        && hasExecuteBeforeResolved
        && !hasExecuteBefore
        && !alreadyInjectedForOrders;

    bool appended = false;
    if (shouldAppend)
    {
        if (ContextMenu_appendOrderThunk_orig)
        {
            g_nativeMenuAppendInjectionInProgress = true;
            ContextMenu_appendOrderThunk_orig(orders, kContextMenuOrderIdExecuteProxy);
            g_nativeMenuAppendInjectionInProgress = false;

            bool hasExecuteAfter = false;
            const bool hasExecuteAfterResolved = TryOrderListContains(
                orders,
                kContextMenuOrderIdExecuteProxy,
                &hasExecuteAfter);
            appended = hasExecuteAfterResolved && hasExecuteAfter;
        }

        if (appended)
        {
            g_nativeMenuBuildRowsInjectedOrdersPtr = ordersPtr;
            g_nativeMenuBuildRowsInjectedShowSeq = g_contextMenuShowProbeEventSeq;
            g_nativeMenuExecuteRowInjectedOrdersPtr = ordersPtr;
            g_nativeMenuExecuteRowInjectedArmMs = nowMs;
            g_nativeMenuOrderRemapOrdersPtr = ordersPtr;
        }
    }

    uint32_t afterCount = 0;
    const bool afterCountResolved = TryReadOrderListCount(orders, &afterCount);
    uintptr_t afterData = 0;
    const bool afterDataResolved = TryReadUintptrAt(reinterpret_cast<const void*>(ordersPtr + 0x10), &afterData);

    int after0 = 0;
    int after1 = 0;
    int after2 = 0;
    int after3 = 0;
    const bool afterOrdersResolved = afterDataResolved
        && afterData != 0
        && TryReadInt32At(reinterpret_cast<const void*>(afterData + 0x0), &after0)
        && TryReadInt32At(reinterpret_cast<const void*>(afterData + 0x4), &after1)
        && TryReadInt32At(reinterpret_cast<const void*>(afterData + 0x8), &after2)
        && TryReadInt32At(reinterpret_cast<const void*>(afterData + 0xC), &after3);

    if (g_config.debugContextMenu)
    {
        std::stringstream logline;
        logline << "Loot-Scoot-Execute INFO: buildRows_inject"
                << " menu=0x" << std::hex << reinterpret_cast<uintptr_t>(menu)
                << " orders=0x" << std::hex << ordersPtr
                << " size_before=";
        if (beforeCountResolved)
        {
            logline << std::dec << beforeCount;
        }
        else
        {
            logline << "unresolved";
        }
        logline << " size_after=";
        if (afterCountResolved)
        {
            logline << std::dec << afterCount;
        }
        else
        {
            logline << "unresolved";
        }
        logline << " first_after=";
        if (afterOrdersResolved)
        {
            logline << std::dec << after0 << "," << after1 << "," << after2 << "," << after3;
        }
        else
        {
            logline << "unresolved";
        }
        logline << " injection_applied=" << (appended ? "true" : "false");
        DebugLog(logline.str().c_str());
    }

    if (appended
        && afterCountResolved
        && afterCount == 4
        && afterOrdersResolved
        && after0 == kContextMenuOrderIdLoot
        && after1 == kContextMenuOrderIdLiftPersonPlayerOrder
        && after2 == 25
        && after3 == kContextMenuOrderIdExecuteProxy)
    {
        std::stringstream ok;
        ok << "Loot-Scoot-Execute INFO: BUILDROWS_INJECT_OK orders=0x" << std::hex << ordersPtr
           << " size 3->4";
        DebugLog(ok.str().c_str());
    }
}

static void ContextMenu_appendOrderThunk_hook(lektor<int>* orders, int orderId)
{
    if (orderId == kContextMenuOrderIdExecuteProxy)
    {
        std::stringstream seen134;
        seen134 << "Loot-Scoot-Execute INFO: appendOrder_seen_134"
                << " orders_ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(orders);
        DebugLog(seen134.str().c_str());
    }

    if (!ContextMenu_appendOrderThunk_orig)
    {
        return;
    }

    if (g_nativeMenuAppendInjectionInProgress)
    {
        ContextMenu_appendOrderThunk_orig(orders, orderId);
        return;
    }

    const DWORD nowMs = GetTickCount();
    const bool remapWindowFresh = g_nativeMenuOrderRemapArmed
        && g_nativeMenuOrderRemapTargetPtr != 0
        && g_nativeMenuOrderRemapArmMs != 0
        && !DebounceWindowElapsed(nowMs, g_nativeMenuOrderRemapArmMs, kNativeMenuExecuteArmMaxAgeMs);
    RootObject* remapTarget = remapWindowFresh
        ? reinterpret_cast<RootObject*>(g_nativeMenuOrderRemapTargetPtr)
        : 0;
    Character* remapActor = ResolveExecuteActorForPredicate();
    CanExecuteDiagnostics remapDiagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
    const bool remapDownedEnemyContext = remapTarget
        && CanExecuteFromNativeMenuSelection(remapActor, remapTarget, &remapDiagnostics, false);

    uint32_t beforeCount = 0;
    const bool beforeCountResolved = TryReadOrderListCount(orders, &beforeCount);

    if (g_config.debugContextMenu)
    {
        static uint32_t s_appendOrderHookSeen = 0;
        if (s_appendOrderHookSeen < 120)
        {
            const uintptr_t callerAddress = CaptureCallerAddress();
            const uintptr_t callerRva = ComputeRvaFromAbsoluteAddress(callerAddress);
            std::stringstream seen;
            seen << "Loot-Scoot-Execute DEBUG: context_menu_append_order_hook_seen"
                 << " call_index=" << std::dec << s_appendOrderHookSeen
                 << " hook_site=primary"
                 << " hook_target=0x" << std::hex << g_hookContextMenuAppendOrderAddress
                 << " alternate_target=0x" << std::hex << g_hookContextMenuAppendOrderAlternateAddress
                 << " order_id=" << orderId
                 << " orders_ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(orders)
                 << " remap_armed=" << (g_nativeMenuOrderRemapArmed ? "true" : "false")
                 << " remap_window_fresh=" << (remapWindowFresh ? "true" : "false")
                 << " remap_context_downed_enemy=" << (remapDownedEnemyContext ? "true" : "false")
                 << " before_count=";
            if (beforeCountResolved)
            {
                seen << std::dec << beforeCount;
            }
            else
            {
                seen << "unresolved";
            }
            seen << " caller=0x" << std::hex << callerAddress;
            if (callerRva != 0)
            {
                seen << " caller_rva=0x" << std::hex << callerRva;
            }
            seen << " caller_first_bytes=\"" << FormatCodeBytes(callerAddress, 6) << "\"";
            DebugLog(seen.str().c_str());
            ++s_appendOrderHookSeen;
        }
    }

    const int effectiveOrderId = orderId;
    ContextMenu_appendOrderThunk_orig(orders, effectiveOrderId);

    uint32_t afterPrimaryCount = 0;
    const bool afterPrimaryCountResolved = TryReadOrderListCount(orders, &afterPrimaryCount);

    bool hasExecuteOrder = false;
    const bool hasExecuteOrderResolved = TryOrderListContains(
        orders,
        kContextMenuOrderIdExecuteProxy,
        &hasExecuteOrder);
    const bool shouldInjectExecuteOrder = false;
    const bool injectedExecuteOrder = false;

    uint32_t afterCount = 0;
    const bool afterCountResolved = TryReadOrderListCount(orders, &afterCount);

    if (g_config.debugContextMenu)
    {
        static uint32_t s_appendOrderHookResultSeen = 0;
        if (s_appendOrderHookResultSeen < 120)
        {
            std::stringstream result;
            result << "Loot-Scoot-Execute DEBUG: context_menu_append_order_hook_result"
                   << " call_index=" << std::dec << s_appendOrderHookResultSeen
                   << " input_order=" << orderId
                   << " effective_order=" << effectiveOrderId
                   << " carry_order_detected=" << (orderId == kContextMenuOrderIdLiftPersonPlayerOrder ? "true" : "false")
                   << " has_execute_order=" << (hasExecuteOrder ? "true" : "false")
                   << " has_execute_order_resolved=" << (hasExecuteOrderResolved ? "true" : "false")
                   << " should_inject_execute_order=" << (shouldInjectExecuteOrder ? "true" : "false")
                   << " injected_execute_order=" << (injectedExecuteOrder ? "true" : "false")
                   << " injection_mode=buildRows_pre"
                   << " after_primary_count=";
            if (afterPrimaryCountResolved)
            {
                result << std::dec << afterPrimaryCount;
            }
            else
            {
                result << "unresolved";
            }
            result << " after_final_count=";
            if (afterCountResolved)
            {
                result << std::dec << afterCount;
            }
            else
            {
                result << "unresolved";
            }
            DebugLog(result.str().c_str());
            ++s_appendOrderHookResultSeen;
        }
    }
}

static void ContextMenu_taskLabelThunk_hook(std::string* outLabel, int taskValue, void* contextMenuGui)
{
    if (g_config.debugContextMenu)
    {
        static uint32_t s_taskLabelCalledCatchAll = 0;
        if (s_taskLabelCalledCatchAll < 10)
        {
            std::stringstream catchAll;
            catchAll << "Loot-Scoot-Execute DEBUG: task_label_called task=" << std::dec << taskValue;
            DebugLog(catchAll.str().c_str());
            ++s_taskLabelCalledCatchAll;
        }
    }

    if (g_config.debugContextMenu)
    {
        static uint32_t s_taskLabelHookSeen = 0;
        if (s_taskLabelHookSeen < 120)
        {
            const uintptr_t callerAddress = CaptureCallerAddress();
            const uintptr_t callerRva = ComputeRvaFromAbsoluteAddress(callerAddress);
            std::stringstream seen;
            seen << "Loot-Scoot-Execute DEBUG: context_menu_task_label_hook_seen"
                 << " call_index=" << std::dec << s_taskLabelHookSeen
                 << " hook_site=primary"
                 << " hook_target=0x" << std::hex << g_hookContextMenuTaskLabelAddress
                 << " alternate_target=0x" << std::hex << g_hookContextMenuTaskLabelAlternateAddress
                 << " task=" << taskValue
                 << " remap_armed=" << (g_nativeMenuOrderRemapArmed ? "true" : "false")
                 << " gui_ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(contextMenuGui)
                 << " caller=0x" << std::hex << callerAddress;
            if (callerRva != 0)
            {
                seen << " caller_rva=0x" << std::hex << callerRva;
            }
            seen << " caller_first_bytes=\"" << FormatCodeBytes(callerAddress, 6) << "\"";
            DebugLog(seen.str().c_str());
            ++s_taskLabelHookSeen;
        }
    }

    const int effectiveTaskValue = taskValue;
    const bool forceExecuteLabel = taskValue == kContextMenuOrderIdExecuteProxy;

    if (ContextMenu_taskLabelThunk_orig)
    {
        ContextMenu_taskLabelThunk_orig(outLabel, effectiveTaskValue, contextMenuGui);
    }

    if (forceExecuteLabel && outLabel)
    {
        *outLabel = "[[[ EXECUTE !!! ]]]";
    }

    if (g_config.debugContextMenu && outLabel)
    {
        static uint32_t s_taskLabelHookResultSeen = 0;
        if (s_taskLabelHookResultSeen < 120)
        {
            std::stringstream result;
            result << "Loot-Scoot-Execute DEBUG: context_menu_task_label_hook_result"
                   << " call_index=" << std::dec << s_taskLabelHookResultSeen
                   << " task=" << taskValue
                   << " effective_task=" << effectiveTaskValue
                   << " label=\"" << SanitizeMenuLabelForLog(*outLabel) << "\""
                   << " force_execute_label=" << (forceExecuteLabel ? "true" : "false");
            DebugLog(result.str().c_str());
            ++s_taskLabelHookResultSeen;
        }
    }

    if (g_config.debugContextMenu && forceExecuteLabel)
    {
        std::stringstream called;
        called << "Loot-Scoot-Execute DEBUG: task_label_called task=" << std::dec << taskValue << " returning='"
               << SanitizeMenuLabelForLog(*outLabel) << "'";
        DebugLog(called.str().c_str());

        std::stringstream logline;
        logline << "Loot-Scoot-Execute DEBUG: context_menu_task_label_override"
                << " from_task=" << taskValue
                << " via_task=" << effectiveTaskValue
                << " label=\"[[[ EXECUTE !!! ]]]\"";
        DebugLog(logline.str().c_str());
    }
}

static void ContextMenu_appendOrderThunk_probe_hook(lektor<int>* orders, int orderId)
{
    if (g_config.debugContextMenu)
    {
        static uint32_t s_appendOrderAltHookSeen = 0;
        if (s_appendOrderAltHookSeen < 120)
        {
            const uintptr_t callerAddress = CaptureCallerAddress();
            const uintptr_t callerRva = ComputeRvaFromAbsoluteAddress(callerAddress);
            std::stringstream seen;
            seen << "Loot-Scoot-Execute DEBUG: context_menu_append_order_hook_seen"
                 << " call_index=" << std::dec << s_appendOrderAltHookSeen
                 << " hook_site=alternate"
                 << " hook_target=0x" << std::hex << g_hookContextMenuAppendOrderAlternateAddress
                 << " primary_target=0x" << std::hex << g_hookContextMenuAppendOrderAddress
                 << " order_id=" << std::dec << orderId
                 << " orders_ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(orders)
                 << " caller=0x" << std::hex << callerAddress;
            if (callerRva != 0)
            {
                seen << " caller_rva=0x" << std::hex << callerRva;
            }
            seen << " caller_first_bytes=\"" << FormatCodeBytes(callerAddress, 6) << "\"";
            DebugLog(seen.str().c_str());
            ++s_appendOrderAltHookSeen;
        }
    }

    if (ContextMenu_appendOrderThunk_probe_orig)
    {
        ContextMenu_appendOrderThunk_probe_orig(orders, orderId);
    }
}

static void ContextMenu_taskLabelThunk_probe_hook(std::string* outLabel, int taskValue, void* contextMenuGui)
{
    if (g_config.debugContextMenu)
    {
        static uint32_t s_taskLabelAltHookSeen = 0;
        if (s_taskLabelAltHookSeen < 120)
        {
            const uintptr_t callerAddress = CaptureCallerAddress();
            const uintptr_t callerRva = ComputeRvaFromAbsoluteAddress(callerAddress);
            std::stringstream seen;
            seen << "Loot-Scoot-Execute DEBUG: context_menu_task_label_hook_seen"
                 << " call_index=" << std::dec << s_taskLabelAltHookSeen
                 << " hook_site=alternate"
                 << " hook_target=0x" << std::hex << g_hookContextMenuTaskLabelAlternateAddress
                 << " primary_target=0x" << std::hex << g_hookContextMenuTaskLabelAddress
                 << " task=" << std::dec << taskValue
                 << " gui_ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(contextMenuGui)
                 << " caller=0x" << std::hex << callerAddress;
            if (callerRva != 0)
            {
                seen << " caller_rva=0x" << std::hex << callerRva;
            }
            seen << " caller_first_bytes=\"" << FormatCodeBytes(callerAddress, 6) << "\"";
            DebugLog(seen.str().c_str());
            ++s_taskLabelAltHookSeen;
        }
    }

    if (ContextMenu_taskLabelThunk_probe_orig)
    {
        ContextMenu_taskLabelThunk_probe_orig(outLabel, taskValue, contextMenuGui);
    }
}

static void* ContextMenu_rowInsertCall_hook(
    void* rcx,
    void* rdx,
    unsigned char insertToRight,
    void* r9,
    void* stackArg5)
{
    const DWORD nowMs = GetTickCount();
    const bool remapWindowFresh = g_nativeMenuOrderRemapArmed
        && g_nativeMenuOrderRemapTargetPtr != 0
        && g_nativeMenuOrderRemapArmMs != 0
        && !DebounceWindowElapsed(nowMs, g_nativeMenuOrderRemapArmMs, kNativeMenuExecuteArmMaxAgeMs);

    const uintptr_t returnSlotAddress = reinterpret_cast<uintptr_t>(_AddressOfReturnAddress());
    uintptr_t stack20Raw = 0;
    uintptr_t stack28Raw = 0;
    uintptr_t stack30Raw = 0;
    uintptr_t stack38Raw = 0;
    const bool stack20Resolved = returnSlotAddress != 0
        && TryReadUintptrAt(reinterpret_cast<const void*>(returnSlotAddress + 0x20), &stack20Raw);
    const bool stack28Resolved = returnSlotAddress != 0
        && TryReadUintptrAt(reinterpret_cast<const void*>(returnSlotAddress + 0x28), &stack28Raw);
    const bool stack30Resolved = returnSlotAddress != 0
        && TryReadUintptrAt(reinterpret_cast<const void*>(returnSlotAddress + 0x30), &stack30Raw);
    const bool stack38Resolved = returnSlotAddress != 0
        && TryReadUintptrAt(reinterpret_cast<const void*>(returnSlotAddress + 0x38), &stack38Raw);

    int taskFromStack20Slot = 0;
    int taskFromStack28Slot = 0;
    int taskFromStack30Slot = 0;
    int taskFromStack38Slot = 0;
    const bool taskFromStack20SlotResolved = returnSlotAddress != 0
        && TryReadInt32At(reinterpret_cast<const void*>(returnSlotAddress + 0x20), &taskFromStack20Slot);
    const bool taskFromStack28SlotResolved = returnSlotAddress != 0
        && TryReadInt32At(reinterpret_cast<const void*>(returnSlotAddress + 0x28), &taskFromStack28Slot);
    const bool taskFromStack30SlotResolved = returnSlotAddress != 0
        && TryReadInt32At(reinterpret_cast<const void*>(returnSlotAddress + 0x30), &taskFromStack30Slot);
    const bool taskFromStack38SlotResolved = returnSlotAddress != 0
        && TryReadInt32At(reinterpret_cast<const void*>(returnSlotAddress + 0x38), &taskFromStack38Slot);

    int taskFromStackWindow = 0;
    const bool taskFromStackWindowResolved = rcx
        && TryReadInt32At(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(rcx) - 0x18), &taskFromStackWindow);
    int taskFromArg5NodeKey = 0;
    const bool taskFromArg5NodeKeyResolved = stackArg5
        && TryReadInt32At(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(stackArg5) + 0x18), &taskFromArg5NodeKey);

    Character* remapActor = ResolveExecuteActorForPredicate();
    RootObject* remapTarget = remapWindowFresh
        ? reinterpret_cast<RootObject*>(g_nativeMenuOrderRemapTargetPtr)
        : 0;
    CanExecuteDiagnostics remapDiagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
    const bool remapDownedEnemyContext = remapTarget
        && CanExecuteFromNativeMenuSelection(remapActor, remapTarget, &remapDiagnostics, false);
    const bool remapArmedForInjection = g_nativeMenuOrderRemapArmed;
    const bool remapWindowFreshForInjection = remapWindowFresh;
    const bool remapDownedEnemyContextForInjection = remapDownedEnemyContext;
    uintptr_t remapOrdersPtrForInjection = reinterpret_cast<uintptr_t>(rdx);
    if (remapOrdersPtrForInjection == 0)
    {
        remapOrdersPtrForInjection = g_nativeMenuOrderRemapOrdersPtr;
    }
    const bool carryRowDetected = (taskFromStackWindowResolved
        && taskFromStackWindow == kContextMenuOrderIdLiftPersonPlayerOrder)
        || (taskFromArg5NodeKeyResolved
            && taskFromArg5NodeKey == kContextMenuOrderIdLiftPersonPlayerOrder);
    const int taskForRowInsertInjection = taskFromStackWindowResolved
        ? taskFromStackWindow
        : (taskFromArg5NodeKeyResolved ? taskFromArg5NodeKey : 0);
    const bool taskForRowInsertInjectionResolved = taskFromStackWindowResolved
        || taskFromArg5NodeKeyResolved;
    const bool firstRowLootDetected = taskForRowInsertInjectionResolved
        && taskForRowInsertInjection == kContextMenuOrderIdLoot;
    const bool executeRowAlreadyInjected = remapOrdersPtrForInjection != 0
        && g_nativeMenuExecuteRowInjectedOrdersPtr == remapOrdersPtrForInjection;
    const bool latchFresh = g_menuLatchActive
        && g_menuLatchTimestampMs != 0
        && !DebounceWindowElapsed(nowMs, g_menuLatchTimestampMs, 500);
    const bool rowInsertReadonly = !latchFresh;
    const bool rowInsertMutationEnabled = !rowInsertReadonly;

    if (g_config.debugContextMenu)
    {
        std::stringstream mode;
        mode << "Loot-Scoot-Execute DEBUG: row_insert_mode"
             << " readonly=" << (rowInsertReadonly ? 1 : 0)
             << " remap_armed=" << (remapArmedForInjection ? 1 : 0)
             << " fresh=" << (remapWindowFreshForInjection ? 1 : 0)
             << " downed=" << (remapDownedEnemyContextForInjection ? 1 : 0)
             << " orders=0x" << std::hex << remapOrdersPtrForInjection;
        DebugLog(mode.str().c_str());

        std::stringstream observe;
        observe << "Loot-Scoot-Execute DEBUG: row_insert_observe"
                << " readonly=" << (rowInsertReadonly ? "true" : "false")
                << " carry=" << (carryRowDetected ? 1 : 0)
                << " task=";
        if (taskForRowInsertInjectionResolved)
        {
            observe << std::dec << taskForRowInsertInjection;
        }
        else
        {
            observe << "unresolved";
        }
        observe << " first_row_loot=" << (firstRowLootDetected ? 1 : 0)
                << " already_injected=" << (executeRowAlreadyInjected ? 1 : 0);
        DebugLog(observe.str().c_str());
    }

    void* rowInsertResult = 0;
    bool injectedExecuteRow = false;
    const char* injectionSource = "none";
    const unsigned char injectedInsertToRight = 1;
    bool rowInsertInjectionAttempted = false;
    bool rowInsertInjectionApplied = false;
    bool rowInsertInjectionOrdersValidated = false;
    bool executeRowDescriptorTemplateCaptured = false;
    bool executeRowDescriptorPatched = false;
    bool executeRowDescriptorBeforeResolved = false;
    bool executeRowDescriptorAfterResolved = false;
    uintptr_t executeRowDescriptorTemplateUsed = 0;
    uintptr_t executeRowDescriptorBefore = 0;
    uintptr_t executeRowDescriptorAfter = 0;

    const bool shouldAttemptRowInsertInjection = kEnableRowInsertLateInjection
        && rowInsertMutationEnabled
        && taskForRowInsertInjectionResolved
        && taskForRowInsertInjection == 25
        && remapOrdersPtrForInjection != 0
        && !executeRowAlreadyInjected
        && !g_nativeMenuAppendInjectionInProgress;

    const bool canRewriteStackSlot20 = taskFromStack20SlotResolved && returnSlotAddress != 0;
    const bool canRewriteStackWindow = taskFromStackWindowResolved && rcx != 0;
    const bool canRewriteArg5Node = taskFromArg5NodeKeyResolved && stackArg5 != 0;
    const int originalStackSlot20Task = taskFromStack20Slot;
    const int originalStackWindowTask = taskFromStackWindow;
    const int originalArg5NodeTask = taskFromArg5NodeKey;
    bool rewroteStackSlot20 = false;
    bool rewroteStackWindow = false;
    bool rewroteArg5Node = false;

    if (shouldAttemptRowInsertInjection)
    {
        rowInsertInjectionAttempted = true;
        rowInsertInjectionOrdersValidated = true;
        if (canRewriteStackSlot20)
        {
            rewroteStackSlot20 = TryWriteInt32At(
                reinterpret_cast<void*>(returnSlotAddress + 0x20),
                kContextMenuOrderIdExecuteProxy);
        }
        if (canRewriteStackWindow)
        {
            rewroteStackWindow = TryWriteInt32At(
                reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(rcx) - 0x18),
                kContextMenuOrderIdExecuteProxy);
        }
        if (canRewriteArg5Node)
        {
            rewroteArg5Node = TryWriteInt32At(
                reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(stackArg5) + 0x18),
                kContextMenuOrderIdExecuteProxy);
        }

        rowInsertInjectionApplied = rewroteStackSlot20 || rewroteStackWindow || rewroteArg5Node;
        if (rowInsertInjectionApplied)
        {
            injectedExecuteRow = true;
            injectionSource = "row_insert_substitute_25_to_134";
            g_nativeMenuExecuteRowInjectedOrdersPtr = remapOrdersPtrForInjection;
            g_nativeMenuExecuteRowInjectedArmMs = g_nativeMenuOrderRemapArmMs;
            std::stringstream injectOk;
            injectOk << "Loot-Scoot-Execute INFO: INJECT_OK substitute task=25->134"
                     << " orders=0x" << std::hex << remapOrdersPtrForInjection
                     << " rewrote_stack20=" << (rewroteStackSlot20 ? 1 : 0)
                     << " rewrote_stack_window=" << (rewroteStackWindow ? 1 : 0)
                     << " rewrote_arg5_node=" << (rewroteArg5Node ? 1 : 0);
            DebugLog(injectOk.str().c_str());
        }
    }

    if (ContextMenu_rowInsertCall_orig)
    {
        rowInsertResult = ContextMenu_rowInsertCall_orig(rcx, rdx, insertToRight, r9, stackArg5);
    }

    const bool shouldHandleExecuteRowDescriptor = !kEnableRowInsertLateInjection
        && rowInsertMutationEnabled
        && taskForRowInsertInjectionResolved
        && remapDownedEnemyContextForInjection
        && stackArg5 != 0
        && g_effectiveEnableContextMenuInjection
        && g_effectiveEnableExecuteAction;
    if (shouldHandleExecuteRowDescriptor)
    {
        const uintptr_t executeRowNodePtr = reinterpret_cast<uintptr_t>(stackArg5);
        uintptr_t descriptorValue = 0;
        const bool descriptorResolved = TryReadUintptrAt(
            reinterpret_cast<const void*>(executeRowNodePtr + 0x20),
            &descriptorValue);

        const bool isNativeTaskRow = taskForRowInsertInjection != kContextMenuOrderIdExecuteProxy;
        if (isNativeTaskRow && descriptorResolved && descriptorValue != 0)
        {
            g_nativeMenuRowDescriptorTemplate = descriptorValue;
            g_nativeMenuRowDescriptorTemplateOrdersPtr = remapOrdersPtrForInjection;
            g_nativeMenuRowDescriptorTemplateArmMs = g_nativeMenuOrderRemapArmMs;
            executeRowDescriptorTemplateCaptured = true;
        }
        else if (!isNativeTaskRow)
        {
            executeRowDescriptorBeforeResolved = descriptorResolved;
            if (descriptorResolved)
            {
                executeRowDescriptorBefore = descriptorValue;
            }

            const bool needsPatch = !descriptorResolved || descriptorValue == 0;
            const bool templateFreshForMenu = g_nativeMenuRowDescriptorTemplate != 0
                && g_nativeMenuRowDescriptorTemplateOrdersPtr == remapOrdersPtrForInjection
                && g_nativeMenuRowDescriptorTemplateArmMs == g_nativeMenuOrderRemapArmMs;
            if (needsPatch && templateFreshForMenu)
            {
                executeRowDescriptorTemplateUsed = g_nativeMenuRowDescriptorTemplate;
                executeRowDescriptorPatched = TryWriteUintptrAt(
                    reinterpret_cast<void*>(executeRowNodePtr + 0x20),
                    g_nativeMenuRowDescriptorTemplate);
                if (executeRowDescriptorPatched)
                {
                    executeRowDescriptorAfterResolved = TryReadUintptrAt(
                        reinterpret_cast<const void*>(executeRowNodePtr + 0x20),
                        &executeRowDescriptorAfter);
                }
            }

            if (g_config.debugContextMenu)
            {
                std::stringstream descriptorLog;
                descriptorLog << "Loot-Scoot-Execute INFO: execute_row_descriptor_fix"
                              << " row=0x" << std::hex << executeRowNodePtr
                              << " before=0x";
                if (executeRowDescriptorBeforeResolved)
                {
                    descriptorLog << std::hex << executeRowDescriptorBefore;
                }
                else
                {
                    descriptorLog << "unresolved";
                }
                descriptorLog << " template=0x" << std::hex << executeRowDescriptorTemplateUsed
                              << " patched=" << std::dec << (executeRowDescriptorPatched ? 1 : 0)
                              << " after=0x";
                if (executeRowDescriptorAfterResolved)
                {
                    descriptorLog << std::hex << executeRowDescriptorAfter;
                }
                else
                {
                    descriptorLog << "unresolved";
                }
                DebugLog(descriptorLog.str().c_str());
            }
        }
    }

    if (rewroteStackSlot20)
    {
        (void)TryWriteInt32At(
            reinterpret_cast<void*>(returnSlotAddress + 0x20),
            originalStackSlot20Task);
    }
    if (rewroteStackWindow)
    {
        (void)TryWriteInt32At(
            reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(rcx) - 0x18),
            originalStackWindowTask);
    }
    if (rewroteArg5Node)
    {
        (void)TryWriteInt32At(
            reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(stackArg5) + 0x18),
            originalArg5NodeTask);
    }

    if (g_config.debugContextMenu)
    {
        static uint32_t s_rowInsertHookSeen = 0;
        if (s_rowInsertHookSeen < 120)
        {
            const uintptr_t callerAddress = CaptureCallerAddress();
            const uintptr_t callerRva = ComputeRvaFromAbsoluteAddress(callerAddress);
            std::stringstream seen;
            seen << "Loot-Scoot-Execute DEBUG: context_menu_row_insert_hook_seen"
                 << " call_index=" << std::dec << s_rowInsertHookSeen
                 << " hook_target=0x" << std::hex << g_resolvedContextMenuRowInsertCallTargetAddress
                 << " rcx=0x" << std::hex << reinterpret_cast<uintptr_t>(rcx)
                 << " rdx=0x" << std::hex << reinterpret_cast<uintptr_t>(rdx)
                 << " r8_raw_u8=" << std::dec << static_cast<unsigned int>(insertToRight)
                 << " r9=0x" << std::hex << reinterpret_cast<uintptr_t>(r9)
                 << " arg5=0x" << std::hex << reinterpret_cast<uintptr_t>(stackArg5)
                 << " stack_slot_20=0x";
            if (stack20Resolved)
            {
                seen << std::hex << stack20Raw;
            }
            else
            {
                seen << "unresolved";
            }
            seen << " stack_slot_28=0x";
            if (stack28Resolved)
            {
                seen << std::hex << stack28Raw;
            }
            else
            {
                seen << "unresolved";
            }
            seen << " stack_slot_30=0x";
            if (stack30Resolved)
            {
                seen << std::hex << stack30Raw;
            }
            else
            {
                seen << "unresolved";
            }
            seen << " stack_slot_38=0x";
            if (stack38Resolved)
            {
                seen << std::hex << stack38Raw;
            }
            else
            {
                seen << "unresolved";
            }
            seen << " stack_slot_20_i32=";
            if (taskFromStack20SlotResolved)
            {
                seen << std::dec << taskFromStack20Slot;
            }
            else
            {
                seen << "unresolved";
            }
            seen << " stack_slot_28_i32=";
            if (taskFromStack28SlotResolved)
            {
                seen << std::dec << taskFromStack28Slot;
            }
            else
            {
                seen << "unresolved";
            }
            seen << " stack_slot_30_i32=";
            if (taskFromStack30SlotResolved)
            {
                seen << std::dec << taskFromStack30Slot;
            }
            else
            {
                seen << "unresolved";
            }
            seen << " stack_slot_38_i32=";
            if (taskFromStack38SlotResolved)
            {
                seen << std::dec << taskFromStack38Slot;
            }
            else
            {
                seen << "unresolved";
            }
            seen << " insert_to_right=" << (insertToRight != 0 ? "true" : "false")
                 << " injected_insert_to_right=" << (injectedInsertToRight != 0 ? "true" : "false")
                 << " task_stack_window=";
            if (taskFromStackWindowResolved)
            {
                seen << std::dec << taskFromStackWindow;
            }
            else
            {
                seen << "unresolved";
            }
            seen << " task_arg5_node_key=";
            if (taskFromArg5NodeKeyResolved)
            {
                seen << std::dec << taskFromArg5NodeKey;
            }
            else
            {
                seen << "unresolved";
            }
            seen << " carry_row_detected=" << (carryRowDetected ? "true" : "false")
                 << " execute_row_already_injected=" << (executeRowAlreadyInjected ? "true" : "false")
                 << " injected_execute_row=" << (injectedExecuteRow ? "true" : "false")
                 << " injection_source=" << injectionSource
                 << " row_insert_injection_attempted=" << (rowInsertInjectionAttempted ? "true" : "false")
                 << " row_insert_injection_applied=" << (rowInsertInjectionApplied ? "true" : "false")
                 << " row_insert_injection_orders_validated=" << (rowInsertInjectionOrdersValidated ? "true" : "false")
                 << " execute_row_descriptor_template_captured=" << (executeRowDescriptorTemplateCaptured ? "true" : "false")
                 << " execute_row_descriptor_patched=" << (executeRowDescriptorPatched ? "true" : "false")
                 << " remap_armed=" << (remapArmedForInjection ? "true" : "false")
                 << " remap_window_fresh=" << (remapWindowFreshForInjection ? "true" : "false")
                 << " remap_context_downed_enemy=" << (remapDownedEnemyContextForInjection ? "true" : "false")
                 << " remap_orders_ptr=0x" << std::hex << remapOrdersPtrForInjection
                 << " injected_orders_ptr=0x" << std::hex << g_nativeMenuExecuteRowInjectedOrdersPtr
                 << " remap_arm_ms=" << std::dec << g_nativeMenuOrderRemapArmMs
                 << " injected_arm_ms=" << std::dec << g_nativeMenuExecuteRowInjectedArmMs
                 << " caller=0x" << std::hex << callerAddress;
            if (callerRva != 0)
            {
                seen << " caller_rva=0x" << std::hex << callerRva;
            }
            seen << " caller_first_bytes=\"" << FormatCodeBytes(callerAddress, 6) << "\"";
            DebugLog(seen.str().c_str());
            ++s_rowInsertHookSeen;
        }
    }

    return rowInsertResult;
}

static void ContextMenu_showContextMenu_hook(ContextMenu* thisptr, bool on, RootObject* what)
{
    if (g_config.debugContextMenu)
    {
        static uint32_t s_showHookSeen = 0;
        if (s_showHookSeen < 40)
        {
            const uintptr_t callerAddress = CaptureCallerAddress();
            const uintptr_t callerRva = ComputeRvaFromAbsoluteAddress(callerAddress);
            std::stringstream seen;
            seen << "Loot-Scoot-Execute DEBUG: context_menu_show_hook_seen"
                 << " call_index=" << std::dec << s_showHookSeen
                 << " on=" << (on ? "true" : "false")
                 << " menu=0x" << std::hex << reinterpret_cast<uintptr_t>(thisptr)
                 << " what=0x" << std::hex << reinterpret_cast<uintptr_t>(what)
                 << " caller=0x" << std::hex << callerAddress;
            if (callerRva != 0)
            {
                seen << " caller_rva=0x" << std::hex << callerRva;
            }
            seen << " caller_first_bytes=\"" << FormatCodeBytes(callerAddress, 6) << "\"";
            DebugLog(seen.str().c_str());
            ++s_showHookSeen;
        }
    }

    const DWORD showHookNowMs = GetTickCount();
    if (!on && thisptr)
    {
        const uint64_t showSeq = g_currentShowSeq;
        const DWORD showAgeMs = g_lastShowTimeMs != 0 ? (showHookNowMs - g_lastShowTimeMs) : 0;
        const bool shouldBlockClose = g_enableBlockCloseForDebug
            && showSeq != 0
            && showSeq == g_lastShowSeq
            && showAgeMs < kContextMenuCloseBlockWindowMs
            && reinterpret_cast<uintptr_t>(thisptr) == g_lastMenuPtr
            && g_lastWasDownedEnemy
            && g_lastShowTargetIsEnemy
            && g_lastShowTargetIsIncapacitated
            && !g_lastShowTargetIsDead;

        if (shouldBlockClose)
        {
            std::stringstream block;
            block << "Loot-Scoot-Execute INFO: BLOCK_CLOSE"
                  << " show_seq=" << std::dec << showSeq
                  << " age_ms=" << showAgeMs;
            DebugLog(block.str().c_str());
            return;
        }
    }

    bool showPreInjectionAttempted = false;
    bool showPreInjectionApplied = false;

    if (thisptr
        && g_effectiveEnableContextMenuInjection
        && g_effectiveEnableExecuteAction
        && on
        && what)
    {
        const DWORD preNowMs = GetTickCount();
        ArmNativeMenuOrderRemapContext(thisptr, what, preNowMs);
        if (g_enableShowContextMenuPreInjection)
        {
            showPreInjectionAttempted = true;
            showPreInjectionApplied = TryInjectExecuteOrderInShowContextMenuPre(thisptr, what, preNowMs);
        }
    }
    else
    {
        DisarmNativeMenuOrderRemapContext();
    }

    if (ContextMenu_showContextMenu_orig)
    {
        ContextMenu_showContextMenu_orig(thisptr, on, what);
    }

    if (!on)
    {
        g_currentShowSeq = 0;
        g_lastShowSeq = 0;
        g_lastShowTimeMs = 0;
        g_lastMenuPtr = 0;
        g_lastWasDownedEnemy = false;
        g_lastShowTargetIsEnemy = false;
        g_lastShowTargetIsIncapacitated = false;
        g_lastShowTargetIsDead = false;
    }

    if (!thisptr)
    {
        DisarmNativeMenuExecuteDispatchContext();
        DisarmNativeMenuOrderRemapContext();
        return;
    }

    const bool shouldProbe = g_effectiveEnableContextMenuProbe;
    const bool shouldInject = g_effectiveEnableContextMenuInjection && g_effectiveEnableExecuteAction;
    if (!shouldProbe && !shouldInject)
    {
        if (!on)
        {
            DisarmNativeMenuExecuteDispatchContext();
            DisarmNativeMenuOrderRemapContext();
        }
        return;
    }

    bool visible = false;
    std::string contextMenuName;
    uint32_t ordersCount = 0;
    int orderSample[kContextMenuProbeOrderSampleCount] = { 0 };
    size_t orderSampleCount = 0;
    if (!TryReadContextMenuProbeSnapshot(
        thisptr,
        &visible,
        &contextMenuName,
        &ordersCount,
        orderSample,
        &orderSampleCount))
    {
        if (g_config.debugContextMenu)
        {
            ErrorLog("Loot-Scoot-Execute WARN: context_menu_probe_snapshot_failed");
        }
        DisarmNativeMenuExecuteDispatchContext();
        return;
    }

    bool mouseRightTargetSet = false;
    bool contextMenuOwnerMatched = false;
    RootObject* mouseRightTarget = 0;
    const bool mouseRightTargetResolved = TryReadMouseRightTargetForContextMenu(
        thisptr,
        &mouseRightTargetSet,
        &mouseRightTarget,
        &contextMenuOwnerMatched);

    int whatType = 0;
    const bool whatTypeResolved = TryResolveRootObjectType(what, &whatType);
    Character* executeActor = ResolveExecuteActorForPredicate();
    CanExecuteDiagnostics canExecuteDiagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
    const bool canExecuteTarget = CanExecuteFromNativeMenuSelection(
        executeActor,
        what,
        &canExecuteDiagnostics,
        g_config.debugContextMenu);
    if (g_config.debugContextMenu)
    {
        (void)CanExecuteFromDebugTrigger(executeActor, what, 0, false);
        (void)CanExecuteFromFallbackPopup(executeActor, what, 0, false);
    }

    int mouseRightTargetType = 0;
    const bool mouseRightTargetTypeResolved = mouseRightTargetResolved
        && mouseRightTargetSet
        && mouseRightTarget
        && TryResolveRootObjectType(mouseRightTarget, &mouseRightTargetType);

    // Preserve a pre-injection orders snapshot for confidence-gate sampling.
    uint32_t mappingOrdersCount = ordersCount;
    size_t mappingOrderSampleCount = orderSampleCount;
    int mappingOrderSample[kContextMenuProbeOrderSampleCount] = { 0 };
    for (size_t i = 0; i < mappingOrderSampleCount; ++i)
    {
        mappingOrderSample[i] = orderSample[i];
    }

    const uintptr_t whatPtr = reinterpret_cast<uintptr_t>(what);
    const uintptr_t mouseRightTargetPtr = reinterpret_cast<uintptr_t>(mouseRightTarget);
    const DWORD nowMs = GetTickCount();
    if (on)
    {
        ++g_currentShowSeq;
        g_lastShowSeq = g_currentShowSeq;
        g_lastShowTimeMs = nowMs;
        g_lastMenuPtr = reinterpret_cast<uintptr_t>(thisptr);
        g_lastWasDownedEnemy = canExecuteTarget;
        g_lastShowTargetIsEnemy = canExecuteDiagnostics.targetIsEnemy;
        g_lastShowTargetIsIncapacitated = canExecuteDiagnostics.targetIsIncapacitated;
        g_lastShowTargetIsDead = canExecuteDiagnostics.targetIsDead;
    }

    bool injectionAttempted = showPreInjectionAttempted;
    bool injectionOrderAlreadyPresent = false;
    bool injectionOrderRemapped = false;
    bool injectionOrderAppended = showPreInjectionApplied;
    bool injectionApplied = showPreInjectionApplied;
    bool injectionMutationFailed = false;

    if (shouldInject && on && visible && canExecuteTarget && whatPtr != 0)
    {
        injectionAttempted = true;
        bool executeOrderPresentInSample = false;
        for (size_t i = 0; i < orderSampleCount; ++i)
        {
            if (orderSample[i] == kContextMenuOrderIdExecuteProxy)
            {
                executeOrderPresentInSample = true;
                break;
            }
        }
        injectionOrderAlreadyPresent = executeOrderPresentInSample;
        if (!showPreInjectionApplied)
        {
            injectionOrderAppended = executeOrderPresentInSample;
        }
        injectionApplied = showPreInjectionApplied || executeOrderPresentInSample;
        if (injectionApplied)
        {
            ArmNativeMenuExecuteDispatchContext(whatPtr, nowMs);
        }
    }
    else if (!on || !visible || !canExecuteTarget)
    {
        DisarmNativeMenuExecuteDispatchContext();
    }

    if (on && visible && canExecuteTarget && whatPtr != 0)
    {
        ArmNativeMenuOrderRemapContext(thisptr, what, nowMs);
    }
    else if (!on || !visible || !canExecuteTarget)
    {
        DisarmNativeMenuOrderRemapContext();
    }

    if (on && visible && canExecuteTarget && whatPtr != 0)
    {
        g_lastDebugExecuteContextTargetPtr = whatPtr;
        g_lastDebugExecuteContextTargetCaptureMs = nowMs;
    }
    else if (!on || !visible || !canExecuteTarget)
    {
        g_lastDebugExecuteContextTargetPtr = 0;
        g_lastDebugExecuteContextTargetCaptureMs = 0;
    }
    if (shouldProbe && on)
    {
        UpdateContextMenuShowProbeEvent(
            on,
            visible,
            whatPtr,
            mouseRightTargetPtr,
            mappingOrdersCount,
            mappingOrderSample,
            mappingOrderSampleCount,
            nowMs);

        if (visible)
        {
            const ContextTypeKey contextType = InferContextTypeKeyFromProbe(
                whatTypeResolved,
                whatType,
                contextMenuName,
                ordersCount,
                orderSample,
                orderSampleCount);
            const ContextTypeKey mappingContextType = canExecuteTarget
                ? ContextTypeKey_DOWNED_ENEMY
                : contextType;

            RecordContextMenuMappingSample(
                "show_context_menu_open",
                mappingContextType,
                mappingOrdersCount,
                mappingOrderSample,
                mappingOrderSampleCount);

            if (ReevaluateContextMenuMappingConfidenceGate("show_context_menu_open", false))
            {
                RefreshEffectiveContextMenuFeatureFlags("context_menu_mapping_gate_changed");
            }
        }
    }

    if (!shouldProbe)
    {
        if (g_config.debugContextMenu && injectionAttempted)
        {
            std::stringstream logline;
            logline << "Loot-Scoot-Execute DEBUG: context_menu_injection"
                    << " attempted=true"
                    << " applied=" << (injectionApplied ? "true" : "false")
                    << " execute_order_already_present=" << (injectionOrderAlreadyPresent ? "true" : "false")
                    << " execute_order_appended=" << (injectionOrderAppended ? "true" : "false")
                    << " carry_order_remapped=" << (injectionOrderRemapped ? "true" : "false")
                    << " mutation_failed=" << (injectionMutationFailed ? "true" : "false")
                    << " target=0x" << std::hex << whatPtr;
            DebugLog(logline.str().c_str());
        }
        return;
    }

    const uint64_t showSeq = g_contextMenuShowProbeEventSeq;

    const bool transitionChanged = !g_hasContextMenuProbeSnapshot
        || g_lastContextMenuProbeOn != on
        || g_lastContextMenuProbeVisible != visible
        || g_lastContextMenuProbeWhatPtr != whatPtr
        || g_lastContextMenuProbeMouseRightTargetPtr != mouseRightTargetPtr
        || g_lastContextMenuProbeOrdersCount != ordersCount
        || g_lastContextMenuProbeName != contextMenuName
        || ContextMenuOrderSampleChanged(orderSample, orderSampleCount);

    const bool importantTransition = !g_hasContextMenuProbeSnapshot
        || g_lastContextMenuProbeOn != on
        || g_lastContextMenuProbeVisible != visible;

    const bool periodicSnapshot = !g_hasContextMenuProbeSnapshot
        || DebounceWindowElapsed(nowMs, g_lastContextMenuProbeLogMs, kContextMenuProbePeriodicMs);

    if (!transitionChanged && !periodicSnapshot)
    {
        return;
    }

    if (transitionChanged
        && !importantTransition
        && g_hasContextMenuProbeSnapshot
        && !DebounceWindowElapsed(nowMs, g_lastContextMenuProbeLogMs, kContextMenuProbeMinIntervalMs))
    {
        return;
    }

    std::stringstream logline;
    logline << "Loot-Scoot-Execute DEBUG: context_menu_show_probe"
            << " show_seq=" << std::dec << showSeq
            << " on=" << (on ? "true" : "false")
            << " visible=" << (visible ? "true" : "false")
            << " context_menu_name=\"" << contextMenuName << "\""
            << " orders_count=" << std::dec << ordersCount
            << " first_orders=[";

    for (size_t i = 0; i < orderSampleCount; ++i)
    {
        if (i > 0)
        {
            logline << ",";
        }
        logline << orderSample[i];
    }

    if (static_cast<size_t>(ordersCount) > orderSampleCount)
    {
        if (orderSampleCount > 0)
        {
            logline << ",";
        }
        logline << "...";
    }

    logline << "]"
            << " what=0x" << std::hex << whatPtr
            << " what_type=";
    if (whatTypeResolved)
    {
        logline << std::dec << whatType;
    }
    else
    {
        logline << "unresolved";
    }

    logline << " mouse_right_target_set=";
    if (mouseRightTargetResolved)
    {
        logline << (mouseRightTargetSet ? "true" : "false");
    }
    else
    {
        logline << "unresolved";
    }

    logline << " mouse_right_target=0x" << std::hex << mouseRightTargetPtr
            << " mouse_right_target_type=";
    if (mouseRightTargetTypeResolved)
    {
        logline << std::dec << mouseRightTargetType;
    }
    else
    {
        logline << "unresolved";
    }

    logline << " menu_owner_match=";
    if (mouseRightTargetResolved)
    {
        logline << (contextMenuOwnerMatched ? "true" : "false");
    }
    else
    {
            logline << "unresolved";
    }

    logline << " can_execute_target=" << (canExecuteTarget ? "true" : "false")
            << " execute_actor_resolved=" << (canExecuteDiagnostics.actorResolved ? "true" : "false")
            << " execute_target_is_enemy=" << (canExecuteDiagnostics.targetIsEnemy ? "true" : "false")
            << " execute_target_is_incapacitated=" << (canExecuteDiagnostics.targetIsIncapacitated ? "true" : "false")
            << " execute_target_is_dead=" << (canExecuteDiagnostics.targetIsDead ? "true" : "false")
            << " injection_attempted=" << (injectionAttempted ? "true" : "false")
            << " injection_applied=" << (injectionApplied ? "true" : "false")
            << " injection_execute_order_already_present=" << (injectionOrderAlreadyPresent ? "true" : "false")
            << " injection_execute_order_appended=" << (injectionOrderAppended ? "true" : "false")
            << " injection_carry_order_remapped=" << (injectionOrderRemapped ? "true" : "false")
            << " injection_mutation_failed=" << (injectionMutationFailed ? "true" : "false")
            << " native_execute_armed=" << (g_nativeMenuExecuteDispatchArmed ? "true" : "false");

    DebugLog(logline.str().c_str());

    UpdateContextMenuProbeSnapshot(
        on,
        visible,
        whatPtr,
        mouseRightTargetPtr,
        ordersCount,
        contextMenuName,
        orderSample,
        orderSampleCount,
        nowMs);
}

static RootObject* ResolveRecentContextMenuTargetForUpdateInjection(DWORD nowMs)
{
    if (g_contextMenuShowProbeEventSeq == 0
        || !g_contextMenuShowProbeEventOn
        || !g_contextMenuShowProbeEventVisible
        || g_contextMenuShowProbeEventWhatPtr == 0)
    {
        return 0;
    }

    if (DebounceWindowElapsed(nowMs, g_contextMenuShowProbeEventMs, kContextMenuObserverCorrelationWindowMs))
    {
        return 0;
    }

    return reinterpret_cast<RootObject*>(g_contextMenuShowProbeEventWhatPtr);
}

static RootObject* ResolveExecuteTargetForNativeMenuValidation(RootObject* explicitTarget, DWORD nowMs)
{
    if (explicitTarget)
    {
        return explicitTarget;
    }

    const bool armFresh = g_nativeMenuExecuteDispatchArmed
        && g_nativeMenuExecuteDispatchTargetPtr != 0
        && g_nativeMenuExecuteDispatchArmMs != 0
        && !DebounceWindowElapsed(nowMs, g_nativeMenuExecuteDispatchArmMs, kNativeMenuExecuteArmMaxAgeMs);
    if (armFresh)
    {
        return reinterpret_cast<RootObject*>(g_nativeMenuExecuteDispatchTargetPtr);
    }

    const bool remapTargetFresh = g_nativeMenuOrderRemapArmed
        && g_nativeMenuOrderRemapTargetPtr != 0
        && g_nativeMenuOrderRemapArmMs != 0
        && !DebounceWindowElapsed(nowMs, g_nativeMenuOrderRemapArmMs, kNativeMenuExecuteArmMaxAgeMs);
    if (remapTargetFresh)
    {
        return reinterpret_cast<RootObject*>(g_nativeMenuOrderRemapTargetPtr);
    }

    RootObject* recentTarget = ResolveRecentContextMenuTargetForUpdateInjection(nowMs);
    if (recentTarget)
    {
        return recentTarget;
    }

    const bool debugTargetFresh = g_lastDebugExecuteContextTargetPtr != 0
        && g_lastDebugExecuteContextTargetCaptureMs != 0
        && !DebounceWindowElapsed(nowMs, g_lastDebugExecuteContextTargetCaptureMs, kDebugExecuteContextTargetMaxAgeMs);
    if (debugTargetFresh)
    {
        return reinterpret_cast<RootObject*>(g_lastDebugExecuteContextTargetPtr);
    }

    return 0;
}

static void ContextMenu_buildRows_hook(ContextMenu* thisptr, void* argRdx, void* argR8, void* argR9)
{
    if (g_config.debugContextMenu)
    {
        std::stringstream entry;
        entry << "Loot-Scoot-Execute DEBUG: buildRows_hook_seen"
              << " menu=0x" << std::hex << reinterpret_cast<uintptr_t>(thisptr)
              << " rcx=0x" << std::hex << reinterpret_cast<uintptr_t>(thisptr)
              << " rdx=0x" << std::hex << reinterpret_cast<uintptr_t>(argRdx)
              << " r8=0x" << std::hex << reinterpret_cast<uintptr_t>(argR8)
              << " r9=0x" << std::hex << reinterpret_cast<uintptr_t>(argR9);
        DebugLog(entry.str().c_str());
    }

    if (thisptr)
    {
        TryInjectExecuteOrderInBuildRows(thisptr);
    }

    if (g_config.debugContextMenu)
    {
        static uint32_t s_buildRowsHookSeen = 0;
        if (s_buildRowsHookSeen < 40)
        {
            const uint32_t ordersCount = thisptr ? thisptr->orders.size() : 0;

            std::stringstream seen;
            seen << "Loot-Scoot-Execute DEBUG: context_menu_buildrows_hook_seen"
                 << " call_index=" << std::dec << s_buildRowsHookSeen
                 << " menu_ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(thisptr)
                 << " orders_count=" << std::dec << ordersCount
                 << " remap_armed=" << (g_nativeMenuOrderRemapArmed ? "true" : "false");
            DebugLog(seen.str().c_str());
            ++s_buildRowsHookSeen;
        }
    }

    if (g_config.debugContextMenu && thisptr)
    {
        LogContextMenuRowMaterializationSnapshot(thisptr, "build_rows_pre", g_contextMenuShowProbeEventSeq);
    }

    if (ContextMenu_buildRows_orig)
    {
        ContextMenu_buildRows_orig(thisptr, argRdx, argR8, argR9);
    }

    if (g_config.debugContextMenu && thisptr)
    {
        LogContextMenuRowMaterializationSnapshot(thisptr, "build_rows_post", g_contextMenuShowProbeEventSeq);
    }
}

static void ContextMenu_update_hook(ContextMenu* thisptr)
{
    if (g_config.debugContextMenu && thisptr)
    {
        bool visibleForSnapshot = false;
        __try
        {
            visibleForSnapshot = thisptr->isVisible();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            visibleForSnapshot = false;
        }

        if (visibleForSnapshot)
        {
            const DWORD nowMs = GetTickCount();
            const uint64_t showSeq = g_contextMenuShowProbeEventSeq;
            const bool newShowSeq = showSeq != 0 && showSeq != g_lastContextMenuRowSnapshotShowSeq;
            const bool periodicSnapshot = g_lastContextMenuRowSnapshotMs == 0
                || DebounceWindowElapsed(nowMs, g_lastContextMenuRowSnapshotMs, kContextMenuObserverPeriodicMs);
            if (newShowSeq || periodicSnapshot)
            {
                LogContextMenuRowMaterializationSnapshot(thisptr, "update_pre_draw", showSeq);
                g_lastContextMenuRowSnapshotShowSeq = showSeq;
                g_lastContextMenuRowSnapshotMs = nowMs;
            }
        }
    }

    if (thisptr
        && kDebugEnableShowContextMenuOrderMutationFallback
        && g_effectiveEnableContextMenuInjection
        && g_effectiveEnableExecuteAction)
    {
        bool visible = false;
        __try
        {
            visible = thisptr->isVisible();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            visible = false;
        }

        if (visible)
        {
            const DWORD nowMs = GetTickCount();
            RootObject* target = ResolveExecuteTargetForNativeMenuValidation(0, nowMs);
            const uintptr_t targetPtr = reinterpret_cast<uintptr_t>(target);
            if (targetPtr != 0)
            {
                Character* actor = ResolveExecuteActorForPredicate();
                CanExecuteDiagnostics diagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
                const bool canExecuteTarget = CanExecuteFromNativeMenuSelection(actor, target, &diagnostics, false);
                if (canExecuteTarget)
                {
                    bool executeOrderAlreadyPresent = false;
                    bool remappedCarryOrder = false;
                    bool appendedExecuteOrder = false;
                    if (TryRemapDownedEnemyOrderToExecute(
                        thisptr,
                        &executeOrderAlreadyPresent,
                        &remappedCarryOrder,
                        &appendedExecuteOrder))
                    {
                        if (executeOrderAlreadyPresent || remappedCarryOrder || appendedExecuteOrder)
                        {
                            ArmNativeMenuExecuteDispatchContext(targetPtr, nowMs);
                        }
                    }
                }
            }
        }
        else
        {
            DisarmNativeMenuExecuteDispatchContext();
        }
    }

    if (ContextMenu_update_orig)
    {
        ContextMenu_update_orig(thisptr);
    }
}

static bool TryReadContextMenuObserverSnapshot(
    PlayerInterface* player,
    bool* visibleOut,
    uintptr_t* mouseRightTargetPtrOut,
    uint32_t* ordersCountOut,
    int* orderSampleOut,
    size_t* orderSampleCountOut)
{
    if (!player || !visibleOut || !mouseRightTargetPtrOut || !ordersCountOut || !orderSampleOut || !orderSampleCountOut)
    {
        return false;
    }

    __try
    {
        ContextMenu* menu = &player->contextMenu;
        *visibleOut = menu->isVisible();

        const uint32_t orderCount = menu->orders.size();
        *ordersCountOut = orderCount;

        size_t sampleCount = static_cast<size_t>(orderCount);
        if (sampleCount > kContextMenuProbeOrderSampleCount)
        {
            sampleCount = kContextMenuProbeOrderSampleCount;
        }

        for (size_t i = 0; i < sampleCount; ++i)
        {
            orderSampleOut[i] = menu->orders[static_cast<uint32_t>(i)];
        }
        *orderSampleCountOut = sampleCount;

        RootObject* mouseRightTarget = 0;
        if (player->mouseRightTargetSet)
        {
            mouseRightTarget = player->mouseRightTarget;
        }
        *mouseRightTargetPtrOut = reinterpret_cast<uintptr_t>(mouseRightTarget);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void UpdateContextMenuObserverSnapshot(
    bool visible,
    uintptr_t mouseRightTargetPtr,
    uint32_t ordersCount,
    const int* orderSample,
    size_t orderSampleCount,
    DWORD nowMs)
{
    g_hasContextMenuObserverSnapshot = true;
    g_lastContextMenuObserverSampleMs = nowMs;
    g_lastContextMenuObserverVisible = visible;
    g_lastContextMenuObserverMouseRightTargetPtr = mouseRightTargetPtr;
    g_lastContextMenuObserverOrdersCount = ordersCount;
    g_lastContextMenuObserverSampleCount = orderSampleCount;

    for (size_t i = 0; i < orderSampleCount; ++i)
    {
        g_lastContextMenuObserverOrderSample[i] = orderSample[i];
    }
}

static void ObserveContextMenuInUpdateUT(PlayerInterface* thisptr)
{
    if (!g_effectiveEnableContextMenuProbe || !thisptr)
    {
        return;
    }

    const DWORD nowMs = GetTickCount();
    if (g_lastContextMenuObserverSampleMs != 0
        && !DebounceWindowElapsed(nowMs, g_lastContextMenuObserverSampleMs, kContextMenuObserverSampleMinIntervalMs))
    {
        return;
    }

    bool visible = false;
    uintptr_t mouseRightTargetPtr = 0;
    uint32_t ordersCount = 0;
    int orderSample[kContextMenuProbeOrderSampleCount] = { 0 };
    size_t orderSampleCount = 0;
    if (!TryReadContextMenuObserverSnapshot(
        thisptr,
        &visible,
        &mouseRightTargetPtr,
        &ordersCount,
        orderSample,
        &orderSampleCount))
    {
        g_lastContextMenuObserverSampleMs = nowMs;
        if (g_config.debugContextMenu
            && (g_lastContextMenuObserverLogMs == 0
                || DebounceWindowElapsed(nowMs, g_lastContextMenuObserverLogMs, kContextMenuObserverPeriodicMs)))
        {
            ErrorLog("Loot-Scoot-Execute WARN: context_menu_updateut_probe_snapshot_failed");
            g_lastContextMenuObserverLogMs = nowMs;
        }
        return;
    }

    const bool hasShowEvent = g_contextMenuShowProbeEventSeq != 0;
    const bool newShowEvent = hasShowEvent
        && g_contextMenuShowProbeEventSeq != g_lastContextMenuObserverShowEventSeq;
    const DWORD showAgeMs = hasShowEvent ? (nowMs - g_contextMenuShowProbeEventMs) : 0;
    const bool showEventRecent = hasShowEvent
        && showAgeMs <= kContextMenuObserverCorrelationWindowMs;
    const bool sameTargetAsShow = hasShowEvent
        && mouseRightTargetPtr == g_contextMenuShowProbeEventMouseRightTargetPtr;
    const bool ordersChangedSinceShow = hasShowEvent
        && (ordersCount != g_contextMenuShowProbeEventOrdersCount
            || !OrderSamplesEqual(
                orderSample,
                orderSampleCount,
                g_contextMenuShowProbeEventOrderSample,
                g_contextMenuShowProbeEventSampleCount));

    const bool sampleChangedSinceLast = !g_hasContextMenuObserverSnapshot
        || g_lastContextMenuObserverVisible != visible
        || g_lastContextMenuObserverMouseRightTargetPtr != mouseRightTargetPtr
        || g_lastContextMenuObserverOrdersCount != ordersCount
        || !OrderSamplesEqual(
            orderSample,
            orderSampleCount,
            g_lastContextMenuObserverOrderSample,
            g_lastContextMenuObserverSampleCount);

    const bool periodicSnapshot = !g_hasContextMenuObserverSnapshot
        || g_lastContextMenuObserverLogMs == 0
        || DebounceWindowElapsed(nowMs, g_lastContextMenuObserverLogMs, kContextMenuObserverPeriodicMs);

    const bool correlatedOrderChange = showEventRecent && sameTargetAsShow && ordersChangedSinceShow;
    const bool shouldLog = newShowEvent || correlatedOrderChange || periodicSnapshot;
    if (shouldLog)
    {
        std::stringstream logline;
        logline << "Loot-Scoot-Execute DEBUG: context_menu_updateut_probe"
                << " show_seq=" << std::dec << g_contextMenuShowProbeEventSeq
                << " show_age_ms=" << showAgeMs
                << " show_recent=" << (showEventRecent ? "true" : "false")
                << " show_on=" << (g_contextMenuShowProbeEventOn ? "true" : "false")
                << " show_visible=" << (g_contextMenuShowProbeEventVisible ? "true" : "false")
                << " visible=" << (visible ? "true" : "false")
                << " same_target_as_show=" << (sameTargetAsShow ? "true" : "false")
                << " sample_changed_since_last=" << (sampleChangedSinceLast ? "true" : "false")
                << " orders_changed_since_show=" << (ordersChangedSinceShow ? "true" : "false")
                << " show_orders_count=" << g_contextMenuShowProbeEventOrdersCount
                << " orders_count=" << ordersCount
                << " show_what=0x" << std::hex << g_contextMenuShowProbeEventWhatPtr
                << " show_target=0x" << g_contextMenuShowProbeEventMouseRightTargetPtr
                << " target=0x" << mouseRightTargetPtr
                << " show_first_orders=[";

        for (size_t i = 0; i < g_contextMenuShowProbeEventSampleCount; ++i)
        {
            if (i > 0)
            {
                logline << ",";
            }
            logline << std::dec << g_contextMenuShowProbeEventOrderSample[i];
        }
        if (static_cast<size_t>(g_contextMenuShowProbeEventOrdersCount) > g_contextMenuShowProbeEventSampleCount)
        {
            if (g_contextMenuShowProbeEventSampleCount > 0)
            {
                logline << ",";
            }
            logline << "...";
        }

        logline << "] current_first_orders=[";
        for (size_t i = 0; i < orderSampleCount; ++i)
        {
            if (i > 0)
            {
                logline << ",";
            }
            logline << std::dec << orderSample[i];
        }
        if (static_cast<size_t>(ordersCount) > orderSampleCount)
        {
            if (orderSampleCount > 0)
            {
                logline << ",";
            }
            logline << "...";
        }
        logline << "]";

        DebugLog(logline.str().c_str());
        g_lastContextMenuObserverLogMs = nowMs;
    }

    if (newShowEvent)
    {
        g_lastContextMenuObserverShowEventSeq = g_contextMenuShowProbeEventSeq;
    }

    UpdateContextMenuObserverSnapshot(
        visible,
        mouseRightTargetPtr,
        ordersCount,
        orderSample,
        orderSampleCount,
        nowMs);
}

static void TickPauseOnLoad()
{
    if (!g_config.enabled)
    {
        return;
    }

    const DWORD nowMs = GetTickCount();

    if (g_config.debugLogTransitions)
    {
        if (g_state.lastTickAliveLogMs == 0 || DebounceWindowElapsed(nowMs, g_state.lastTickAliveLogMs, kTickAliveIntervalMs))
        {
            DebugLog("Loot-Scoot-Execute DEBUG: tick alive");
            g_state.lastTickAliveLogMs = nowMs;
        }
    }

    if (!g_hasSaveLoadHook || !g_state.pauseArmed)
    {
        return;
    }

    if (g_state.armTimestampMs != 0 && DebounceWindowElapsed(nowMs, g_state.armTimestampMs, kArmedTimeoutMs))
    {
        ErrorLog("Loot-Scoot-Execute WARN: armed pause timed out before load completion");
        DisarmPauseAfterLoad();
        return;
    }

    bool isLoadingSave = false;
    if (!QuerySaveLoadSignal(&isLoadingSave))
    {
        return;
    }

    if (isLoadingSave)
    {
        if (!g_state.loadInProgress && g_config.debugLogTransitions)
        {
            DebugLog("Loot-Scoot-Execute DEBUG: load started");
        }
        g_state.loadInProgress = true;
        g_state.loadSignalSeenAfterArm = true;
        return;
    }

    if (g_state.loadInProgress)
    {
        if (g_config.debugLogTransitions)
        {
            DebugLog("Loot-Scoot-Execute DEBUG: load finished");
        }
        TryPauseAndDisarm(nowMs, "load_transition");
        return;
    }

    // Disarm if no load signal arrives shortly after arming. This avoids
    // false-positive pauses when a load call fails or is cancelled early.
    if (!g_state.loadSignalSeenAfterArm
        && g_state.armTimestampMs != 0
        && DebounceWindowElapsed(nowMs, g_state.armTimestampMs, kNoSignalDisarmMs))
    {
        if (g_config.debugLogTransitions)
        {
            DebugLog("Loot-Scoot-Execute DEBUG: no load signal observed; disarming");
        }
        DisarmPauseAfterLoad();
    }
}

static Character* ResolveExecuteActorForNativeMenuDispatch(PlayerInterface* player)
{
    if (!player)
    {
        return 0;
    }

    __try
    {
        return player->getAnyPlayerCharacter();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

static bool TryOverrideExecuteTaskProbability(
    PlayerInterface* thisptr,
    TaskType task,
    RootObject* target,
    float& probability,
    const char* sourceTag)
{
    if (!g_effectiveEnableContextMenuInjection || !g_effectiveEnableExecuteAction)
    {
        return false;
    }

    const DWORD nowMs = GetTickCount();
    RootObject* resolvedTarget = ResolveExecuteTargetForNativeMenuValidation(target, nowMs);
    if (!resolvedTarget)
    {
        return false;
    }

    Character* actor = ResolveExecuteActorForNativeMenuDispatch(thisptr);
    CanExecuteDiagnostics diagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
    const bool canExecuteTarget = CanExecuteFromNativeMenuSelection(actor, resolvedTarget, &diagnostics, false);
    if (!canExecuteTarget)
    {
        return false;
    }

    const bool remapWindowFresh = g_nativeMenuOrderRemapArmed
        && g_nativeMenuOrderRemapTargetPtr != 0
        && g_nativeMenuOrderRemapArmMs != 0
        && !DebounceWindowElapsed(nowMs, g_nativeMenuOrderRemapArmMs, kNativeMenuExecuteArmMaxAgeMs);
    RootObject* remapTarget = remapWindowFresh
        ? reinterpret_cast<RootObject*>(g_nativeMenuOrderRemapTargetPtr)
        : 0;
    CanExecuteDiagnostics remapDiagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
    const bool remapDownedEnemyContext = remapTarget
        && CanExecuteFromNativeMenuSelection(actor, remapTarget, &remapDiagnostics, false);

    const int taskValue = static_cast<int>(task);
    const bool executeTaskInInjectedRow = taskValue == kContextMenuOrderIdExecuteProxy
        && remapWindowFresh
        && remapDownedEnemyContext;
    if (!executeTaskInInjectedRow)
    {
        return false;
    }

    probability = 100.0f;

    if (g_config.debugContextMenu)
    {
        std::stringstream logline;
        logline << "Loot-Scoot-Execute DEBUG: execute_probability_override"
                << " source=" << (sourceTag ? sourceTag : "unknown")
                << " task=" << taskValue
                << " remap_armed=" << (g_nativeMenuOrderRemapArmed ? "true" : "false")
                << " remap_window_fresh=" << (remapWindowFresh ? "true" : "false")
                << " remap_context_downed_enemy=" << (remapDownedEnemyContext ? "true" : "false")
                << " target=0x" << std::hex << reinterpret_cast<uintptr_t>(resolvedTarget)
                << " probability=" << std::dec << probability;
        DebugLog(logline.str().c_str());
    }

    return true;
}

static bool TryForceExecuteProxyProbabilityOne(
    PlayerInterface* thisptr,
    TaskType task,
    RootObject* target,
    float& probability,
    const char* sourceTag)
{
    if (!g_effectiveEnableContextMenuInjection || !g_effectiveEnableExecuteAction)
    {
        return false;
    }

    const int taskValue = static_cast<int>(task);
    if (taskValue != kContextMenuOrderIdExecuteProxy)
    {
        return false;
    }

    const DWORD nowMs = GetTickCount();
    RootObject* resolvedTarget = ResolveExecuteTargetForNativeMenuValidation(target, nowMs);
    if (!resolvedTarget)
    {
        return false;
    }

    Character* actor = ResolveExecuteActorForNativeMenuDispatch(thisptr);
    CanExecuteDiagnostics diagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
    const bool downedEnemyContext = CanExecuteFromNativeMenuSelection(actor, resolvedTarget, &diagnostics, false);
    if (!downedEnemyContext
        || !diagnostics.targetIsEnemy
        || !diagnostics.targetIsIncapacitated
        || diagnostics.targetIsDead)
    {
        return false;
    }

    const float originalProbability = probability;
    if (probability < 1.0f)
    {
        probability = 1.0f;
    }

    if (g_config.debugContextMenu)
    {
        std::stringstream forced;
        forced << "Loot-Scoot-Execute DEBUG: prob_forced task=134 -> 1"
               << " source=" << (sourceTag ? sourceTag : "unknown")
               << " original_prob=" << originalProbability
               << " final_prob=" << probability
               << " target=0x" << std::hex << reinterpret_cast<uintptr_t>(resolvedTarget);
        DebugLog(forced.str().c_str());
    }

    return true;
}

static bool PlayerInterface_getPlayerTaskProbability_hook(
    PlayerInterface* thisptr,
    TaskType task,
    RootObject* target,
    float& probability)
{
    if (g_config.debugContextMenu)
    {
        static uint32_t s_playerProbHookSeen = 0;
        if (s_playerProbHookSeen < 40)
        {
            const uintptr_t callerAddress = CaptureCallerAddress();
            const uintptr_t callerRva = ComputeRvaFromAbsoluteAddress(callerAddress);
            std::stringstream seen;
            seen << "Loot-Scoot-Execute DEBUG: player_task_probability_hook_seen"
                 << " call_index=" << std::dec << s_playerProbHookSeen
                 << " task=" << static_cast<int>(task)
                 << " target=0x" << std::hex << reinterpret_cast<uintptr_t>(target)
                 << " caller=0x" << std::hex << callerAddress;
            if (callerRva != 0)
            {
                seen << " caller_rva=0x" << std::hex << callerRva;
            }
            seen << " caller_first_bytes=\"" << FormatCodeBytes(callerAddress, 6) << "\"";
            DebugLog(seen.str().c_str());
            ++s_playerProbHookSeen;
        }
    }

    bool originalResult = false;
    float originalProbability = probability;
    if (PlayerInterface_getPlayerTaskProbability_orig)
    {
        originalResult = PlayerInterface_getPlayerTaskProbability_orig(thisptr, task, target, probability);
        originalProbability = probability;
    }

    const int taskValue = static_cast<int>(task);
    const DWORD nowMs = GetTickCount();
    RootObject* resolvedTarget = ResolveExecuteTargetForNativeMenuValidation(target, nowMs);
    Character* actor = ResolveExecuteActorForNativeMenuDispatch(thisptr);
    CanExecuteDiagnostics diagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
    const bool downedEnemyContext = CanExecuteFromNativeMenuSelection(actor, resolvedTarget, &diagnostics, false);
    if (TryForceExecuteProxyProbabilityOne(
        thisptr,
        task,
        target,
        probability,
        "player_interface_task_probability"))
    {
        return true;
    }

    if (g_forceMenuPersistence
        && downedEnemyContext
        && diagnostics.targetIsEnemy
        && diagnostics.targetIsIncapacitated
        && !diagnostics.targetIsDead
        && taskValue == kContextMenuOrderIdLiftPersonPlayerOrder)
    {
        const float finalProbability = (probability < 1.0f) ? 1.0f : probability;
        if (finalProbability != probability)
        {
            probability = finalProbability;
        }

        if (g_config.debugContextMenu)
        {
            std::stringstream persistenceLog;
            persistenceLog << "Loot-Scoot-Execute DEBUG: force_menu_persistence applied"
                           << " task=" << std::dec << taskValue
                           << " original_prob=" << originalProbability
                           << " final_prob=" << probability;
            DebugLog(persistenceLog.str().c_str());
        }

        return true;
    }

    if (TryOverrideExecuteTaskProbability(
        thisptr,
        task,
        target,
        probability,
        "player_interface_task_probability"))
    {
        return true;
    }

    if (originalResult)
    {
        return true;
    }
    return false;
}

static bool PlayerInterface_contextMenuOrderFilterThunk_hook(
    PlayerInterface* thisptr,
    TaskType task)
{
    const uintptr_t callerAddress = CaptureCallerAddress();
    const uintptr_t callerRva = ComputeRvaFromAbsoluteAddress(callerAddress);

    if (g_config.debugContextMenu)
    {
        static uint32_t s_orderFilterHookSeen = 0;
        if (s_orderFilterHookSeen < 40)
        {
            std::stringstream seen;
            seen << "Loot-Scoot-Execute DEBUG: context_menu_order_filter_hook_seen"
                 << " call_index=" << std::dec << s_orderFilterHookSeen
                 << " hook_site=primary"
                 << " hook_target=0x" << std::hex << g_hookPlayerInterfaceContextMenuOrderFilterAddress
                 << " alternate_target=0x" << std::hex << g_hookPlayerInterfaceContextMenuOrderFilterAlternateAddress
                 << " task=" << static_cast<int>(task)
                 << " remap_armed=" << (g_nativeMenuOrderRemapArmed ? "true" : "false")
                 << " caller=0x" << std::hex << callerAddress;
            if (callerRva != 0)
            {
                seen << " caller_rva=0x" << std::hex << callerRva;
            }
            seen << " caller_first_bytes=\"" << FormatCodeBytes(callerAddress, 6) << "\"";
            DebugLog(seen.str().c_str());
            ++s_orderFilterHookSeen;
        }
    }

    bool originalResult = false;
    if (PlayerInterface_contextMenuOrderFilterThunk_orig)
    {
        originalResult = PlayerInterface_contextMenuOrderFilterThunk_orig(thisptr, task);
    }

    const int taskValue = static_cast<int>(task);
    const bool taskEligibleForExecute = taskValue == kContextMenuOrderIdExecuteProxy;
    const bool taskIsCarrySanity = taskValue == kContextMenuOrderIdLiftPersonPlayerOrder;
    const bool taskEligibleForSanityProbe = kDebugForceAcceptCarryTaskForFilterSanity && taskIsCarrySanity;

    const DWORD nowMs = GetTickCount();
    RootObject* resolvedTarget = ResolveExecuteTargetForNativeMenuValidation(0, nowMs);
    const uintptr_t resolvedTargetPtr = reinterpret_cast<uintptr_t>(resolvedTarget);

    int resolvedTargetType = 0;
    const bool resolvedTargetTypeKnown = resolvedTarget
        && TryResolveRootObjectType(resolvedTarget, &resolvedTargetType);

    Character* actor = ResolveExecuteActorForNativeMenuDispatch(thisptr);
    CanExecuteDiagnostics diagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
    const bool downedEnemyContext = CanExecuteFromNativeMenuSelection(actor, resolvedTarget, &diagnostics, false);

    if (taskValue == kContextMenuOrderIdLoot)
    {
        g_menuLatchActive = downedEnemyContext;
        g_menuLatchTimestampMs = nowMs;
        std::stringstream latch;
        latch << "Loot-Scoot-Execute INFO: LATCH set=" << (g_menuLatchActive ? 1 : 0)
              << " downed=" << (downedEnemyContext ? 1 : 0)
              << " task=26";
        DebugLog(latch.str().c_str());
    }

    if (taskValue == kContextMenuOrderIdLoot
        && downedEnemyContext
        && g_effectiveEnableContextMenuInjection
        && g_effectiveEnableExecuteAction
        && thisptr
        && !g_nativeMenuAppendInjectionInProgress
        && !IsNativeMenuExecuteRowAlreadyInjectedForActiveMenu())
    {
        ContextMenu* menu = &thisptr->contextMenu;
        lektor<int>* orders = &menu->orders;
        const uintptr_t ordersPtr = reinterpret_cast<uintptr_t>(orders);

        bool hasCarryOrder = false;
        bool hasExecuteOrder = false;
        const bool hasCarryResolved = TryOrderListContains(
            orders,
            kContextMenuOrderIdLiftPersonPlayerOrder,
            &hasCarryOrder);
        const bool hasExecuteResolved = TryOrderListContains(
            orders,
            kContextMenuOrderIdExecuteProxy,
            &hasExecuteOrder);

        if (hasCarryResolved && hasExecuteResolved && hasCarryOrder && !hasExecuteOrder)
        {
            int beforeSample[kContextMenuProbeOrderSampleCount] = { 0 };
            uint32_t beforeCount = 0;
            const size_t beforeSampleCount = CaptureOrderSampleForLog(
                orders,
                beforeSample,
                kContextMenuProbeOrderSampleCount,
                &beforeCount);

            g_nativeMenuAppendInjectionInProgress = true;
            const bool appendedRaw = TryAppendExecuteOrderToOrdersList(orders);
            g_nativeMenuAppendInjectionInProgress = false;

            bool hasExecuteAfter = false;
            const bool hasExecuteAfterResolved = TryOrderListContains(
                orders,
                kContextMenuOrderIdExecuteProxy,
                &hasExecuteAfter);
            const bool appended = appendedRaw && hasExecuteAfterResolved && hasExecuteAfter;

            int afterSample[kContextMenuProbeOrderSampleCount] = { 0 };
            uint32_t afterCount = 0;
            const size_t afterSampleCount = CaptureOrderSampleForLog(
                orders,
                afterSample,
                kContextMenuProbeOrderSampleCount,
                &afterCount);

            if (appended)
            {
                g_nativeMenuOrderRemapOrdersPtr = ordersPtr;
                MarkNativeMenuExecuteRowInjectedForActiveMenu();
                if (resolvedTargetPtr != 0)
                {
                    ArmNativeMenuExecuteDispatchContext(resolvedTargetPtr, nowMs);
                }
            }

            if (g_config.debugContextMenu)
            {
                std::stringstream inject;
                inject << "Loot-Scoot-Execute INFO: order_filter_preappend"
                       << " orders=0x" << std::hex << ordersPtr
                       << " appended=" << (appended ? 1 : 0)
                       << " before_count=" << std::dec << beforeCount
                       << " before_first=";
                AppendOrderSampleForLog(inject, beforeSample, beforeSampleCount, beforeCount);
                inject << " after_count=" << std::dec << afterCount
                       << " after_first=";
                AppendOrderSampleForLog(inject, afterSample, afterSampleCount, afterCount);
                DebugLog(inject.str().c_str());
            }
        }
    }

    const bool remapWindowFresh = g_nativeMenuOrderRemapArmed
        && g_nativeMenuOrderRemapTargetPtr != 0
        && g_nativeMenuOrderRemapArmMs != 0
        && !DebounceWindowElapsed(nowMs, g_nativeMenuOrderRemapArmMs, kNativeMenuExecuteArmMaxAgeMs);

    bool forcedAccept = false;
    const char* decisionReason = "rejected";
    if (originalResult)
    {
        decisionReason = "accepted_by_original";
    }
    else if (!g_effectiveEnableContextMenuInjection || !g_effectiveEnableExecuteAction)
    {
        decisionReason = "features_disabled";
    }
    else if (!taskEligibleForExecute && !taskEligibleForSanityProbe)
    {
        decisionReason = "task_not_execute";
    }
    else if (!remapWindowFresh)
    {
        decisionReason = "remap_window_stale";
    }
    else if (resolvedTargetPtr == 0)
    {
        decisionReason = "target_unresolved";
    }
    else if (!downedEnemyContext)
    {
        decisionReason = "context_not_downed_enemy";
    }
    else if (kDebugForceAcceptCarryTaskForFilterSanity && taskIsCarrySanity)
    {
        forcedAccept = true;
        decisionReason = "sanity_force_accept_task_225";
    }
    else
    {
        forcedAccept = true;
        decisionReason = "forced_accept_downed_enemy";
    }

    const bool finalAccepted = originalResult || forcedAccept;
    if (g_config.debugContextMenu)
    {
        std::stringstream logline;
        logline << "Loot-Scoot-Execute DEBUG: context_menu_order_filter_decision"
                << " task=" << taskValue
                << " accepted=" << (finalAccepted ? "true" : "false")
                << " original_accepted=" << (originalResult ? "true" : "false")
                << " forced_accept=" << (forcedAccept ? "true" : "false")
                << " reason=" << decisionReason
                << " target=0x" << std::hex << resolvedTargetPtr
                << " target_type=";
        if (resolvedTargetTypeKnown)
        {
            logline << std::dec << resolvedTargetType;
        }
        else
        {
            logline << "unresolved";
        }
        logline << " context_downed_enemy=" << (downedEnemyContext ? "true" : "false")
                << " target_is_enemy=" << (diagnostics.targetIsEnemy ? "true" : "false")
                << " target_is_incapacitated=" << (diagnostics.targetIsIncapacitated ? "true" : "false")
                << " target_is_dead=" << (diagnostics.targetIsDead ? "true" : "false");
        DebugLog(logline.str().c_str());
    }

    LogAcceptedBranchCallTraceOnce("order_filter", callerAddress, finalAccepted);
    return finalAccepted;
}

static bool PlayerInterface_getContextMenuTaskProbabilityThunk_hook(
    PlayerInterface* thisptr,
    TaskType task,
    RootObject* target,
    float& probability)
{
    const uintptr_t callerAddress = CaptureCallerAddress();
    const uintptr_t callerRva = ComputeRvaFromAbsoluteAddress(callerAddress);

    if (g_config.debugContextMenu)
    {
        static uint32_t s_contextProbHookSeen = 0;
        if (s_contextProbHookSeen < 40)
        {
            std::stringstream seen;
            seen << "Loot-Scoot-Execute DEBUG: context_menu_task_probability_hook_seen"
                 << " call_index=" << std::dec << s_contextProbHookSeen
                 << " hook_site=primary"
                 << " hook_target=0x" << std::hex << g_hookPlayerInterfaceContextMenuTaskProbabilityAddress
                 << " alternate_target=0x" << std::hex << g_hookPlayerInterfaceContextMenuTaskProbabilityAlternateAddress
                 << " task=" << static_cast<int>(task)
                 << " target=0x" << std::hex << reinterpret_cast<uintptr_t>(target)
                 << " caller=0x" << std::hex << callerAddress;
            if (callerRva != 0)
            {
                seen << " caller_rva=0x" << std::hex << callerRva;
            }
            seen << " caller_first_bytes=\"" << FormatCodeBytes(callerAddress, 6) << "\"";
            DebugLog(seen.str().c_str());
            ++s_contextProbHookSeen;
        }
    }

    bool originalResult = false;
    float originalProbability = probability;
    if (PlayerInterface_getContextMenuTaskProbabilityThunk_orig)
    {
        originalResult = PlayerInterface_getContextMenuTaskProbabilityThunk_orig(thisptr, task, target, probability);
        originalProbability = probability;
    }

    const bool forcedProxyProbability = TryForceExecuteProxyProbabilityOne(
        thisptr,
        task,
        target,
        probability,
        "context_menu_task_probability");

    const bool forcedOverride = TryOverrideExecuteTaskProbability(
        thisptr,
        task,
        target,
        probability,
        "context_menu_task_probability");
    if (forcedOverride)
    {
        // fallthrough to shared logging and return path
    }

    const int taskValue = static_cast<int>(task);
    const DWORD nowMs = GetTickCount();
    RootObject* resolvedTarget = ResolveExecuteTargetForNativeMenuValidation(target, nowMs);
    const uintptr_t resolvedTargetPtr = reinterpret_cast<uintptr_t>(resolvedTarget);

    int resolvedTargetType = 0;
    const bool resolvedTargetTypeKnown = resolvedTarget
        && TryResolveRootObjectType(resolvedTarget, &resolvedTargetType);

    Character* actor = ResolveExecuteActorForNativeMenuDispatch(thisptr);
    CanExecuteDiagnostics diagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
    const bool downedEnemyContext = CanExecuteFromNativeMenuSelection(actor, resolvedTarget, &diagnostics, false);

    const bool finalAccepted = forcedProxyProbability || forcedOverride || originalResult;
    const char* decisionReason = "rejected";
    if (forcedProxyProbability)
    {
        decisionReason = "forced_probability_one_task_134";
    }
    else if (forcedOverride)
    {
        decisionReason = "forced_accept_downed_enemy";
    }
    else if (originalResult)
    {
        decisionReason = "accepted_by_original";
    }

    if (g_config.debugContextMenu)
    {
        std::stringstream logline;
        logline << "Loot-Scoot-Execute DEBUG: context_menu_task_probability_decision"
                << " task=" << std::dec << taskValue
                << " accepted=" << (finalAccepted ? "true" : "false")
                << " original_accepted=" << (originalResult ? "true" : "false")
                << " forced_accept=" << (forcedOverride ? "true" : "false")
                << " forced_probability=" << (forcedProxyProbability ? "true" : "false")
                << " reason=" << decisionReason
                << " target=0x" << std::hex << resolvedTargetPtr
                << " target_type=";
        if (resolvedTargetTypeKnown)
        {
            logline << std::dec << resolvedTargetType;
        }
        else
        {
            logline << "unresolved";
        }
        logline << " context_downed_enemy=" << (downedEnemyContext ? "true" : "false")
                << " target_is_enemy=" << (diagnostics.targetIsEnemy ? "true" : "false")
                << " target_is_incapacitated=" << (diagnostics.targetIsIncapacitated ? "true" : "false")
                << " target_is_dead=" << (diagnostics.targetIsDead ? "true" : "false")
                << " original_probability=" << std::dec << originalProbability
                << " final_probability=" << probability;
        DebugLog(logline.str().c_str());
    }

    if (forcedProxyProbability || forcedOverride)
    {
        LogAcceptedBranchCallTraceOnce("task_probability", callerAddress, true);
        return true;
    }
    if (originalResult)
    {
        LogAcceptedBranchCallTraceOnce("task_probability", callerAddress, true);
        return true;
    }
    LogAcceptedBranchCallTraceOnce("task_probability", callerAddress, false);
    return false;
}

static bool PlayerInterface_contextMenuOrderFilterThunk_probe_hook(
    PlayerInterface* thisptr,
    TaskType task)
{
    if (g_config.debugContextMenu)
    {
        static uint32_t s_orderFilterAltHookSeen = 0;
        if (s_orderFilterAltHookSeen < 120)
        {
            const uintptr_t callerAddress = CaptureCallerAddress();
            const uintptr_t callerRva = ComputeRvaFromAbsoluteAddress(callerAddress);
            std::stringstream seen;
            seen << "Loot-Scoot-Execute DEBUG: context_menu_order_filter_hook_seen"
                 << " call_index=" << std::dec << s_orderFilterAltHookSeen
                 << " hook_site=alternate"
                 << " hook_target=0x" << std::hex << g_hookPlayerInterfaceContextMenuOrderFilterAlternateAddress
                 << " primary_target=0x" << std::hex << g_hookPlayerInterfaceContextMenuOrderFilterAddress
                 << " task=" << std::dec << static_cast<int>(task)
                 << " caller=0x" << std::hex << callerAddress;
            if (callerRva != 0)
            {
                seen << " caller_rva=0x" << std::hex << callerRva;
            }
            seen << " caller_first_bytes=\"" << FormatCodeBytes(callerAddress, 6) << "\"";
            DebugLog(seen.str().c_str());
            ++s_orderFilterAltHookSeen;
        }
    }

    if (PlayerInterface_contextMenuOrderFilterThunk_probe_orig)
    {
        return PlayerInterface_contextMenuOrderFilterThunk_probe_orig(thisptr, task);
    }
    return false;
}

static bool PlayerInterface_getContextMenuTaskProbabilityThunk_probe_hook(
    PlayerInterface* thisptr,
    TaskType task,
    RootObject* target,
    float& probability)
{
    if (g_config.debugContextMenu)
    {
        static uint32_t s_contextProbAltHookSeen = 0;
        if (s_contextProbAltHookSeen < 120)
        {
            const uintptr_t callerAddress = CaptureCallerAddress();
            const uintptr_t callerRva = ComputeRvaFromAbsoluteAddress(callerAddress);
            std::stringstream seen;
            seen << "Loot-Scoot-Execute DEBUG: context_menu_task_probability_hook_seen"
                 << " call_index=" << std::dec << s_contextProbAltHookSeen
                 << " hook_site=alternate"
                 << " hook_target=0x" << std::hex << g_hookPlayerInterfaceContextMenuTaskProbabilityAlternateAddress
                 << " primary_target=0x" << std::hex << g_hookPlayerInterfaceContextMenuTaskProbabilityAddress
                 << " task=" << std::dec << static_cast<int>(task)
                 << " target=0x" << std::hex << reinterpret_cast<uintptr_t>(target)
                 << " caller=0x" << std::hex << callerAddress;
            if (callerRva != 0)
            {
                seen << " caller_rva=0x" << std::hex << callerRva;
            }
            seen << " caller_first_bytes=\"" << FormatCodeBytes(callerAddress, 6) << "\"";
            DebugLog(seen.str().c_str());
            ++s_contextProbAltHookSeen;
        }
    }

    if (PlayerInterface_getContextMenuTaskProbabilityThunk_probe_orig)
    {
        return PlayerInterface_getContextMenuTaskProbabilityThunk_probe_orig(thisptr, task, target, probability);
    }
    return false;
}

static bool PlayerInterface_isOrderValidForSelection_hook(
    PlayerInterface* thisptr,
    TaskType task)
{
    if (g_config.debugContextMenu)
    {
        static uint32_t s_orderValidHookSeen = 0;
        if (s_orderValidHookSeen < 40)
        {
            const uintptr_t callerAddress = CaptureCallerAddress();
            const uintptr_t callerRva = ComputeRvaFromAbsoluteAddress(callerAddress);
            std::stringstream seen;
            seen << "Loot-Scoot-Execute DEBUG: order_valid_for_selection_hook_seen"
                 << " call_index=" << std::dec << s_orderValidHookSeen
                 << " task=" << static_cast<int>(task)
                 << " caller=0x" << std::hex << callerAddress;
            if (callerRva != 0)
            {
                seen << " caller_rva=0x" << std::hex << callerRva;
            }
            seen << " caller_first_bytes=\"" << FormatCodeBytes(callerAddress, 6) << "\"";
            DebugLog(seen.str().c_str());
            ++s_orderValidHookSeen;
        }
    }

    bool originalResult = false;
    if (PlayerInterface_isOrderValidForSelection_orig)
    {
        originalResult = PlayerInterface_isOrderValidForSelection_orig(thisptr, task);
    }

    if (originalResult)
    {
        return true;
    }

    if (!g_effectiveEnableContextMenuInjection || !g_effectiveEnableExecuteAction)
    {
        return false;
    }

    const int taskValue = static_cast<int>(task);
    if (taskValue != kContextMenuOrderIdExecuteProxy)
    {
        return false;
    }

    const DWORD nowMs = GetTickCount();
    RootObject* target = ResolveExecuteTargetForNativeMenuValidation(0, nowMs);
    if (!target)
    {
        return false;
    }

    Character* actor = ResolveExecuteActorForNativeMenuDispatch(thisptr);
    CanExecuteDiagnostics diagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
    const bool canExecuteTarget = CanExecuteFromNativeMenuSelection(actor, target, &diagnostics, false);
    return canExecuteTarget;
}

static void PlayerInterface_addOrderSelectedCharacters_hook(
    PlayerInterface* thisptr,
    Building* destinationIndoors,
    TaskType task,
    RootObject* subject,
    bool shift,
    bool addDontClear,
    const Ogre::Vector3& location)
{
    const DWORD nowMs = GetTickCount();
    const bool armFresh = g_nativeMenuExecuteDispatchArmed
        && g_nativeMenuExecuteDispatchArmMs != 0
        && !DebounceWindowElapsed(nowMs, g_nativeMenuExecuteDispatchArmMs, kNativeMenuExecuteArmMaxAgeMs);
    const uintptr_t subjectPtr = reinterpret_cast<uintptr_t>(subject);
    const bool subjectMatches = subjectPtr != 0 && subjectPtr == g_nativeMenuExecuteDispatchTargetPtr;
    const uintptr_t intendedShowTargetPtr = g_contextMenuShowProbeEventWhatPtr;
    const uintptr_t intendedRemapTargetPtr = g_nativeMenuOrderRemapTargetPtr;
    const bool subjectMatchesIntendedShowTarget = subjectPtr != 0 && subjectPtr == intendedShowTargetPtr;
    const bool subjectMatchesIntendedRemapTarget = subjectPtr != 0 && subjectPtr == intendedRemapTargetPtr;
    const bool intendedShowMatchesArmedTarget = intendedShowTargetPtr != 0 && intendedShowTargetPtr == g_nativeMenuExecuteDispatchTargetPtr;
    const bool intendedRemapMatchesArmedTarget = intendedRemapTargetPtr != 0 && intendedRemapTargetPtr == g_nativeMenuExecuteDispatchTargetPtr;
    const int taskValue = static_cast<int>(task);
    const bool taskIsExecuteProxy = taskValue == kContextMenuOrderIdExecuteProxy;
    const bool taskIsStealthKill = taskValue == kContextMenuOrderIdStealthKill;
    const bool taskIsLiftPerson = taskValue == kContextMenuOrderIdLiftPersonPlayerOrder;
    const bool remapWindowFresh = g_nativeMenuOrderRemapArmed
        && g_nativeMenuOrderRemapTargetPtr != 0
        && g_nativeMenuOrderRemapArmMs != 0
        && !DebounceWindowElapsed(nowMs, g_nativeMenuOrderRemapArmMs, kNativeMenuExecuteArmMaxAgeMs);
    RootObject* remapTarget = remapWindowFresh
        ? reinterpret_cast<RootObject*>(g_nativeMenuOrderRemapTargetPtr)
        : 0;
    Character* remapActor = ResolveExecuteActorForNativeMenuDispatch(thisptr);
    CanExecuteDiagnostics remapDiagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
    const bool remapDownedEnemyContext = remapTarget
        && CanExecuteFromNativeMenuSelection(remapActor, remapTarget, &remapDiagnostics, false);
    const bool rewriteLiftToExecuteProxy = false;
    const TaskType effectiveTask = task;
    const int effectiveTaskValue = static_cast<int>(effectiveTask);
    const bool effectiveTaskIsExecuteProxy = effectiveTaskValue == kContextMenuOrderIdExecuteProxy;
    const bool executeTaskSelected = effectiveTaskIsExecuteProxy;
    const bool subjectFallbackToArmedTarget = subjectPtr == 0 && g_nativeMenuExecuteDispatchTargetPtr != 0;
    RootObject* dispatchSubject = subject;
    if (!dispatchSubject && subjectFallbackToArmedTarget)
    {
        dispatchSubject = reinterpret_cast<RootObject*>(g_nativeMenuExecuteDispatchTargetPtr);
    }
    const bool shouldIntercept = g_effectiveEnableContextMenuInjection
        && executeTaskSelected
        && armFresh
        && (subjectMatches || subjectFallbackToArmedTarget);

    if (g_config.debugContextMenu)
    {
        static uint32_t s_addOrderHookSeen = 0;
        if (s_addOrderHookSeen < 80)
        {
            const uintptr_t callerAddress = CaptureCallerAddress();
            const uintptr_t callerRva = ComputeRvaFromAbsoluteAddress(callerAddress);
            std::stringstream seen;
            seen << "Loot-Scoot-Execute DEBUG: add_order_selected_characters_hook_seen"
                 << " call_index=" << std::dec << s_addOrderHookSeen
                << " task=" << taskValue
                << " task_is_execute_proxy=" << (taskIsExecuteProxy ? "true" : "false")
                << " task_is_stealth_kill=" << (taskIsStealthKill ? "true" : "false")
                << " task_is_lift_person=" << (taskIsLiftPerson ? "true" : "false")
                << " effective_task=" << std::dec << effectiveTaskValue
                 << " rewrite_lift_to_execute=" << (rewriteLiftToExecuteProxy ? "true" : "false")
                 << " subject=0x" << std::hex << subjectPtr
                 << " dispatch_subject=0x" << std::hex << reinterpret_cast<uintptr_t>(dispatchSubject)
                 << " intended_show_target=0x" << std::hex << intendedShowTargetPtr
                 << " intended_remap_target=0x" << std::hex << intendedRemapTargetPtr
                 << " execute_task_selected=" << (executeTaskSelected ? "true" : "false")
                 << " arm_fresh=" << (armFresh ? "true" : "false")
                 << " subject_matches=" << (subjectMatches ? "true" : "false")
                 << " subject_fallback_to_armed_target=" << (subjectFallbackToArmedTarget ? "true" : "false")
                 << " subject_matches_intended_show_target=" << (subjectMatchesIntendedShowTarget ? "true" : "false")
                 << " subject_matches_intended_remap_target=" << (subjectMatchesIntendedRemapTarget ? "true" : "false")
                 << " intended_show_matches_armed_target=" << (intendedShowMatchesArmedTarget ? "true" : "false")
                 << " intended_remap_matches_armed_target=" << (intendedRemapMatchesArmedTarget ? "true" : "false")
                 << " remap_armed=" << (g_nativeMenuOrderRemapArmed ? "true" : "false")
                 << " remap_window_fresh=" << (remapWindowFresh ? "true" : "false")
                 << " remap_context_downed_enemy=" << (remapDownedEnemyContext ? "true" : "false")
                 << " should_intercept=" << (shouldIntercept ? "true" : "false")
                 << " armed_target=0x" << std::hex << g_nativeMenuExecuteDispatchTargetPtr
                 << " show_seq=" << std::dec << g_contextMenuShowProbeEventSeq
                 << " show_orders_count=" << std::dec << g_contextMenuShowProbeEventOrdersCount
                 << " show_first_orders=[";
            for (size_t i = 0; i < g_contextMenuShowProbeEventSampleCount; ++i)
            {
                if (i > 0)
                {
                    seen << ",";
                }
                seen << std::dec << g_contextMenuShowProbeEventOrderSample[i];
            }
            if (static_cast<size_t>(g_contextMenuShowProbeEventOrdersCount) > g_contextMenuShowProbeEventSampleCount)
            {
                if (g_contextMenuShowProbeEventSampleCount > 0)
                {
                    seen << ",";
                }
                seen << "...";
            }
            seen << "]"
                 << " caller=0x" << std::hex << callerAddress;
            if (callerRva != 0)
            {
                seen << " caller_rva=0x" << std::hex << callerRva;
            }
            seen << " caller_first_bytes=\"" << FormatCodeBytes(callerAddress, 6) << "\"";
            DebugLog(seen.str().c_str());
            ++s_addOrderHookSeen;
        }

        if (armFresh && (subjectMatches || subjectFallbackToArmedTarget))
        {
            std::stringstream dispatchProbe;
            dispatchProbe << "Loot-Scoot-Execute DEBUG: context_menu_dispatch_probe"
                          << " task=" << std::dec << taskValue
                          << " task_is_execute_proxy=" << (taskIsExecuteProxy ? "true" : "false")
                          << " task_is_lift_person=" << (taskIsLiftPerson ? "true" : "false")
                          << " effective_task=" << std::dec << effectiveTaskValue
                          << " rewrite_lift_to_execute=" << (rewriteLiftToExecuteProxy ? "true" : "false")
                          << " task_is_stealth_kill=" << (taskIsStealthKill ? "true" : "false")
                          << " dispatch_subject=0x" << std::hex << reinterpret_cast<uintptr_t>(dispatchSubject)
                          << " show_seq=" << std::dec << g_contextMenuShowProbeEventSeq
                          << " show_orders_count=" << std::dec << g_contextMenuShowProbeEventOrdersCount;
            DebugLog(dispatchProbe.str().c_str());
        }
    }

    if (shouldIntercept)
    {
        Character* actor = ResolveExecuteActorForNativeMenuDispatch(thisptr);
        const bool dispatched = DispatchExecuteFromNativeMenuSelection(actor, dispatchSubject, true);
        std::stringstream logline;
        logline << "Loot-Scoot-Execute DEBUG: native_execute_menu_select"
                << " intercepted=true"
                << " dispatched=" << (dispatched ? "true" : "false")
                << " task=" << taskValue
                << " effective_task=" << effectiveTaskValue
                << " rewrite_lift_to_execute=" << (rewriteLiftToExecuteProxy ? "true" : "false")
                << " task_is_execute_proxy=" << (taskIsExecuteProxy ? "true" : "false")
                << " task_is_stealth_kill=" << (taskIsStealthKill ? "true" : "false")
                << " task_is_lift_person=" << (taskIsLiftPerson ? "true" : "false")
                << " subject=0x" << std::hex << subjectPtr
                << " dispatch_subject=0x" << std::hex << reinterpret_cast<uintptr_t>(dispatchSubject);
        DebugLog(logline.str().c_str());
        DisarmNativeMenuExecuteDispatchContext();
        return;
    }

    if (!armFresh)
    {
        DisarmNativeMenuExecuteDispatchContext();
    }

    if (PlayerInterface_addOrderSelectedCharacters_orig)
    {
        PlayerInterface_addOrderSelectedCharacters_orig(
            thisptr,
            destinationIndoors,
            task,
            subject,
            shift,
            addDontClear,
            location);
    }
}

static void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    PlayerInterface_updateUT_orig(thisptr);
    ObserveContextMenuInUpdateUT(thisptr);
    TickDebugExecuteHotkey(thisptr);
    TickPauseOnLoad();
}

static void SaveManager_loadByInfo_hook(SaveManager* thisptr, const SaveInfo& saveInfo, bool resetPos)
{
    ArmPauseAfterLoad("SaveManager::load(saveInfo,resetPos)");
    if (SaveManager_loadByInfo_orig)
    {
        SaveManager_loadByInfo_orig(thisptr, saveInfo, resetPos);
    }
}

static void SaveManager_loadByName_hook(SaveManager* thisptr, const std::string& saveName)
{
    ArmPauseAfterLoad("SaveManager::load(name)");
    if (SaveManager_loadByName_orig)
    {
        SaveManager_loadByName_orig(thisptr, saveName);
    }
}

__declspec(dllexport) void startPlugin()
{
    DebugLog("Loot-Scoot-Execute: startPlugin()");

    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    {
        std::stringstream detected;
        detected << "Loot-Scoot-Execute INFO: detected platform=" << platform
                 << " version=" << version;
        DebugLog(detected.str().c_str());
    }

    if (platform == KenshiLib::BinaryVersion::UNKNOWN || version != "1.0.65")
    {
        ErrorLog("Loot-Scoot-Execute: unsupported Kenshi version/platform (requires 1.0.65)");
        return;
    }

    g_runtimeGameVersion = version;
    g_runtimeLocaleTag = DetectRuntimeLocaleTag();
    SeedContextMenuMappingTable();
    ReevaluateContextMenuMappingConfidenceGate("startup", true);

    {
        std::stringstream runtimeKey;
        runtimeKey << "Loot-Scoot-Execute INFO: runtime_mapping_key version=" << g_runtimeGameVersion
                   << " locale=" << g_runtimeLocaleTag;
        DebugLog(runtimeKey.str().c_str());
    }

    LoadConfigState();
    if (g_configNeedsWriteBack)
    {
        if (!SaveConfigState())
        {
            ErrorLog("Loot-Scoot-Execute WARN: failed to persist normalized mod-config.json");
        }
    }

    const uintptr_t baseAddr = reinterpret_cast<uintptr_t>(GetModuleHandleA(0));
    g_contextMenuHookInstallVerified = false;
    g_nativeExecuteSelectionHookInstallVerified = false;
    g_nativeExecuteProbabilityHookInstallVerified = false;
    g_nativeExecuteOrderFilterHookInstallVerified = false;
    g_nativeExecuteContextMenuProbabilityHookInstallVerified = false;
    g_nativeExecuteOrderValidityHookInstallVerified = false;
    g_nativeExecuteOrderAppendHookInstallVerified = false;
    g_nativeExecuteTaskLabelHookInstallVerified = false;
    g_nativeExecuteMenuBuildHookInstallVerified = false;
    g_nativeExecuteRowInsertHookInstallVerified = false;
    g_nativeExecuteLoopEntryHookInstallVerified = false;
    g_nativeExecuteOrderFilterAlternateHookInstallVerified = false;
    g_nativeExecuteContextMenuProbabilityAlternateHookInstallVerified = false;
    g_nativeExecuteOrderAppendAlternateHookInstallVerified = false;
    g_nativeExecuteTaskLabelAlternateHookInstallVerified = false;
    g_lastContextMenuRowSnapshotShowSeq = 0;
    g_lastContextMenuRowSnapshotMs = 0;
    DisarmNativeMenuExecuteDispatchContext();
    DisarmNativeMenuOrderRemapContext();
    EvaluateContextMenuCompatibilityGate(platform, version, baseAddr);
    RefreshEffectiveContextMenuFeatureFlags("post_compatibility_gate");

    if (!RunInternalSelfChecks())
    {
        ErrorLog("Loot-Scoot-Execute ERROR: internal self-check failed");
        return;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        &PlayerInterface_updateUT_orig))
    {
        ErrorLog("Loot-Scoot-Execute: Could not hook PlayerInterface::updateUT");
        return;
    }
    g_contextMenuHookInstallVerified = false;
    RefreshEffectiveContextMenuFeatureFlags("post_updateUT_hook");

    bool addOrderSelectedCharactersHookInstalled = false;
    if (g_contextMenuCompatibilityGatePassed)
    {
        if (KenshiLib::SUCCESS == KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&PlayerInterface::addOrderSelectedCharacters),
            PlayerInterface_addOrderSelectedCharacters_hook,
            &PlayerInterface_addOrderSelectedCharacters_orig))
        {
            addOrderSelectedCharactersHookInstalled = true;
            DebugLog("Loot-Scoot-Execute INFO: PlayerInterface::addOrderSelectedCharacters hook verification passed");
        }
        else
        {
            ErrorLog("Loot-Scoot-Execute WARN: could not hook PlayerInterface::addOrderSelectedCharacters; context-menu injection fail-closed");
        }
    }
    else
    {
        ErrorLog("Loot-Scoot-Execute WARN: skipping PlayerInterface::addOrderSelectedCharacters hook because compatibility gate did not pass");
    }
    g_nativeExecuteSelectionHookInstallVerified = addOrderSelectedCharactersHookInstalled;

    bool getPlayerTaskProbabilityHookInstalled = false;
    if (g_contextMenuCompatibilityGatePassed)
    {
        if (KenshiLib::SUCCESS == KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&PlayerInterface::getPlayerTaskProbability),
            PlayerInterface_getPlayerTaskProbability_hook,
            &PlayerInterface_getPlayerTaskProbability_orig))
        {
            getPlayerTaskProbabilityHookInstalled = true;
            DebugLog("Loot-Scoot-Execute INFO: PlayerInterface::getPlayerTaskProbability hook verification passed");
        }
        else
        {
            ErrorLog("Loot-Scoot-Execute WARN: could not hook PlayerInterface::getPlayerTaskProbability; context-menu injection fail-closed");
        }
    }
    else
    {
        ErrorLog("Loot-Scoot-Execute WARN: skipping PlayerInterface::getPlayerTaskProbability hook because compatibility gate did not pass");
    }
    g_nativeExecuteProbabilityHookInstallVerified = getPlayerTaskProbabilityHookInstalled;

    bool contextMenuOrderFilterHookInstalled = false;
    if (g_contextMenuCompatibilityGatePassed)
    {
        if (KenshiLib::SUCCESS == KenshiLib::AddHook(
            reinterpret_cast<void*>(g_hookPlayerInterfaceContextMenuOrderFilterAddress),
            PlayerInterface_contextMenuOrderFilterThunk_hook,
            &PlayerInterface_contextMenuOrderFilterThunk_orig))
        {
            contextMenuOrderFilterHookInstalled = true;
            std::stringstream info;
            info << "Loot-Scoot-Execute INFO: PlayerInterface context-menu order filter hook verification passed"
                 << " entry=0x" << std::hex << g_resolvedPlayerInterfaceContextMenuOrderFilterThunkAddress
                 << " target=0x" << std::hex << g_hookPlayerInterfaceContextMenuOrderFilterAddress;
            DebugLog(info.str().c_str());
        }
        else
        {
            ErrorLog("Loot-Scoot-Execute WARN: could not hook PlayerInterface context-menu order filter; context-menu injection fail-closed");
        }
    }
    else
    {
        ErrorLog("Loot-Scoot-Execute WARN: skipping PlayerInterface context-menu order filter hook because compatibility gate did not pass");
    }
    g_nativeExecuteOrderFilterHookInstallVerified = contextMenuOrderFilterHookInstalled;

    bool contextMenuOrderFilterAlternateHookInstalled = false;
    if (g_contextMenuCompatibilityGatePassed && g_hookPlayerInterfaceContextMenuOrderFilterAlternateAddress != 0)
    {
        if (!AreDualThunkHookTargetsSafe(
            g_hookPlayerInterfaceContextMenuOrderFilterAddress,
            g_hookPlayerInterfaceContextMenuOrderFilterAlternateAddress))
        {
            std::stringstream skip;
            skip << "Loot-Scoot-Execute INFO: skipped alternate order-filter thunk hook"
                 << " reason=overlapping_targets"
                 << " primary=0x" << std::hex << g_hookPlayerInterfaceContextMenuOrderFilterAddress
                 << " alternate=0x" << std::hex << g_hookPlayerInterfaceContextMenuOrderFilterAlternateAddress;
            DebugLog(skip.str().c_str());
        }
        else if (KenshiLib::SUCCESS == KenshiLib::AddHook(
            reinterpret_cast<void*>(g_hookPlayerInterfaceContextMenuOrderFilterAlternateAddress),
            PlayerInterface_contextMenuOrderFilterThunk_probe_hook,
            &PlayerInterface_contextMenuOrderFilterThunk_probe_orig))
        {
            contextMenuOrderFilterAlternateHookInstalled = true;
            std::stringstream info;
            info << "Loot-Scoot-Execute INFO: PlayerInterface context-menu order filter alternate thunk hook verification passed"
                 << " target=0x" << std::hex << g_hookPlayerInterfaceContextMenuOrderFilterAlternateAddress;
            DebugLog(info.str().c_str());
        }
        else
        {
            ErrorLog("Loot-Scoot-Execute WARN: could not hook PlayerInterface context-menu order filter alternate thunk");
        }
    }
    g_nativeExecuteOrderFilterAlternateHookInstallVerified = contextMenuOrderFilterAlternateHookInstalled;

    bool contextMenuTaskProbabilityHookInstalled = false;
    if (g_contextMenuCompatibilityGatePassed)
    {
        if (KenshiLib::SUCCESS == KenshiLib::AddHook(
            reinterpret_cast<void*>(g_hookPlayerInterfaceContextMenuTaskProbabilityAddress),
            PlayerInterface_getContextMenuTaskProbabilityThunk_hook,
            &PlayerInterface_getContextMenuTaskProbabilityThunk_orig))
        {
            contextMenuTaskProbabilityHookInstalled = true;
            std::stringstream info;
            info << "Loot-Scoot-Execute INFO: PlayerInterface context-menu task probability hook verification passed"
                 << " entry=0x" << std::hex << g_resolvedPlayerInterfaceContextMenuTaskProbabilityThunkAddress
                 << " target=0x" << std::hex << g_hookPlayerInterfaceContextMenuTaskProbabilityAddress;
            DebugLog(info.str().c_str());
        }
        else
        {
            ErrorLog("Loot-Scoot-Execute WARN: could not hook PlayerInterface context-menu task probability; context-menu injection fail-closed");
        }
    }
    else
    {
        ErrorLog("Loot-Scoot-Execute WARN: skipping PlayerInterface context-menu task probability hook because compatibility gate did not pass");
    }
    g_nativeExecuteContextMenuProbabilityHookInstallVerified = contextMenuTaskProbabilityHookInstalled;

    bool contextMenuTaskProbabilityAlternateHookInstalled = false;
    if (g_contextMenuCompatibilityGatePassed && g_hookPlayerInterfaceContextMenuTaskProbabilityAlternateAddress != 0)
    {
        if (!AreDualThunkHookTargetsSafe(
            g_hookPlayerInterfaceContextMenuTaskProbabilityAddress,
            g_hookPlayerInterfaceContextMenuTaskProbabilityAlternateAddress))
        {
            std::stringstream skip;
            skip << "Loot-Scoot-Execute INFO: skipped alternate task-probability thunk hook"
                 << " reason=overlapping_targets"
                 << " primary=0x" << std::hex << g_hookPlayerInterfaceContextMenuTaskProbabilityAddress
                 << " alternate=0x" << std::hex << g_hookPlayerInterfaceContextMenuTaskProbabilityAlternateAddress;
            DebugLog(skip.str().c_str());
        }
        else if (KenshiLib::SUCCESS == KenshiLib::AddHook(
            reinterpret_cast<void*>(g_hookPlayerInterfaceContextMenuTaskProbabilityAlternateAddress),
            PlayerInterface_getContextMenuTaskProbabilityThunk_probe_hook,
            &PlayerInterface_getContextMenuTaskProbabilityThunk_probe_orig))
        {
            contextMenuTaskProbabilityAlternateHookInstalled = true;
            std::stringstream info;
            info << "Loot-Scoot-Execute INFO: PlayerInterface context-menu task probability alternate thunk hook verification passed"
                 << " target=0x" << std::hex << g_hookPlayerInterfaceContextMenuTaskProbabilityAlternateAddress;
            DebugLog(info.str().c_str());
        }
        else
        {
            ErrorLog("Loot-Scoot-Execute WARN: could not hook PlayerInterface context-menu task probability alternate thunk");
        }
    }
    g_nativeExecuteContextMenuProbabilityAlternateHookInstallVerified = contextMenuTaskProbabilityAlternateHookInstalled;

    bool isOrderValidForSelectionHookInstalled = false;
    if (g_contextMenuCompatibilityGatePassed)
    {
        if (KenshiLib::SUCCESS == KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&PlayerInterface::isOrderValidForSelection),
            PlayerInterface_isOrderValidForSelection_hook,
            &PlayerInterface_isOrderValidForSelection_orig))
        {
            isOrderValidForSelectionHookInstalled = true;
            DebugLog("Loot-Scoot-Execute INFO: PlayerInterface::isOrderValidForSelection hook verification passed");
        }
        else
        {
            ErrorLog("Loot-Scoot-Execute WARN: could not hook PlayerInterface::isOrderValidForSelection; context-menu injection fail-closed");
        }
    }
    else
    {
        ErrorLog("Loot-Scoot-Execute WARN: skipping PlayerInterface::isOrderValidForSelection hook because compatibility gate did not pass");
    }
    g_nativeExecuteOrderValidityHookInstallVerified = isOrderValidForSelectionHookInstalled;

    bool contextMenuAppendOrderHookInstalled = false;
    if (g_contextMenuCompatibilityGatePassed)
    {
        if (KenshiLib::SUCCESS == KenshiLib::AddHook(
            reinterpret_cast<void*>(g_hookContextMenuAppendOrderAddress),
            ContextMenu_appendOrderThunk_hook,
            &ContextMenu_appendOrderThunk_orig))
        {
            contextMenuAppendOrderHookInstalled = true;
            std::stringstream info;
            info << "Loot-Scoot-Execute INFO: ContextMenu append-order thunk hook verification passed"
                 << " entry=0x" << std::hex << g_resolvedContextMenuAppendOrderThunkAddress
                 << " target=0x" << std::hex << g_hookContextMenuAppendOrderAddress;
            DebugLog(info.str().c_str());
        }
        else
        {
            ErrorLog("Loot-Scoot-Execute WARN: could not hook ContextMenu append-order thunk; context-menu injection fail-closed");
        }
    }
    else
    {
        ErrorLog("Loot-Scoot-Execute WARN: skipping ContextMenu append-order thunk hook because compatibility gate did not pass");
    }
    g_nativeExecuteOrderAppendHookInstallVerified = contextMenuAppendOrderHookInstalled;

    bool contextMenuAppendOrderAlternateHookInstalled = false;
    if (g_contextMenuCompatibilityGatePassed && g_hookContextMenuAppendOrderAlternateAddress != 0)
    {
        if (!AreDualThunkHookTargetsSafe(
            g_hookContextMenuAppendOrderAddress,
            g_hookContextMenuAppendOrderAlternateAddress))
        {
            std::stringstream skip;
            skip << "Loot-Scoot-Execute INFO: skipped alternate append-order thunk hook"
                 << " reason=overlapping_targets"
                 << " primary=0x" << std::hex << g_hookContextMenuAppendOrderAddress
                 << " alternate=0x" << std::hex << g_hookContextMenuAppendOrderAlternateAddress;
            DebugLog(skip.str().c_str());
        }
        else if (KenshiLib::SUCCESS == KenshiLib::AddHook(
            reinterpret_cast<void*>(g_hookContextMenuAppendOrderAlternateAddress),
            ContextMenu_appendOrderThunk_probe_hook,
            &ContextMenu_appendOrderThunk_probe_orig))
        {
            contextMenuAppendOrderAlternateHookInstalled = true;
            std::stringstream info;
            info << "Loot-Scoot-Execute INFO: ContextMenu append-order alternate thunk hook verification passed"
                 << " target=0x" << std::hex << g_hookContextMenuAppendOrderAlternateAddress;
            DebugLog(info.str().c_str());
        }
        else
        {
            ErrorLog("Loot-Scoot-Execute WARN: could not hook ContextMenu append-order alternate thunk");
        }
    }
    g_nativeExecuteOrderAppendAlternateHookInstallVerified = contextMenuAppendOrderAlternateHookInstalled;

    bool contextMenuTaskLabelHookInstalled = false;
    if (g_contextMenuCompatibilityGatePassed)
    {
        const uintptr_t preferredTaskLabelHookTarget = g_hookContextMenuTaskLabelAddress;
        if (KenshiLib::SUCCESS == KenshiLib::AddHook(
            reinterpret_cast<void*>(preferredTaskLabelHookTarget),
            ContextMenu_taskLabelThunk_hook,
            &ContextMenu_taskLabelThunk_orig))
        {
            contextMenuTaskLabelHookInstalled = true;
            std::stringstream info;
            info << "Loot-Scoot-Execute INFO: ContextMenu task-label thunk hook verification passed"
                 << " entry=0x" << std::hex << g_resolvedContextMenuTaskLabelThunkAddress
                 << " target=0x" << std::hex << preferredTaskLabelHookTarget;
            DebugLog(info.str().c_str());
        }

        // Fallback: some runtimes keep task-label call sites at unshifted thunk RVAs.
        if (!contextMenuTaskLabelHookInstalled)
        {
            const uintptr_t unshiftedTaskLabelThunkAddress = NormalizeThunkEntryAddress(
                "ContextMenu::taskLabelThunk",
                baseAddr + kExpectedRvaContextMenuTaskLabelThunk_1_0_65);
            const bool fallbackCandidateValid = unshiftedTaskLabelThunkAddress != 0
                && unshiftedTaskLabelThunkAddress != preferredTaskLabelHookTarget
                && ValidateExpectedRvaForSymbol(
                    "ContextMenu::taskLabelThunk",
                    baseAddr,
                    unshiftedTaskLabelThunkAddress,
                    kExpectedRvaContextMenuTaskLabelThunk_1_0_65,
                    0,
                    0);
            if (fallbackCandidateValid
                && KenshiLib::SUCCESS == KenshiLib::AddHook(
                    reinterpret_cast<void*>(unshiftedTaskLabelThunkAddress),
                    ContextMenu_taskLabelThunk_hook,
                    &ContextMenu_taskLabelThunk_orig))
            {
                contextMenuTaskLabelHookInstalled = true;
                g_hookContextMenuTaskLabelAddress = unshiftedTaskLabelThunkAddress;

                std::stringstream info;
                info << "Loot-Scoot-Execute INFO: ContextMenu task-label thunk hook fallback verification passed"
                     << " preferred_target=0x" << std::hex << preferredTaskLabelHookTarget
                     << " fallback_target=0x" << std::hex << unshiftedTaskLabelThunkAddress;
                DebugLog(info.str().c_str());
            }
        }

        if (!contextMenuTaskLabelHookInstalled)
        {
            ErrorLog("Loot-Scoot-Execute WARN: could not hook ContextMenu task-label thunk; context-menu injection fail-closed");
        }
    }
    else
    {
        ErrorLog("Loot-Scoot-Execute WARN: skipping ContextMenu task-label thunk hook because compatibility gate did not pass");
    }
    g_nativeExecuteTaskLabelHookInstallVerified = contextMenuTaskLabelHookInstalled;

    bool contextMenuRowInsertHookInstalled = false;
    if (g_contextMenuCompatibilityGatePassed)
    {
        if (g_resolvedContextMenuRowInsertCallTargetAddress != 0
            && KenshiLib::SUCCESS == KenshiLib::AddHook(
                reinterpret_cast<void*>(g_resolvedContextMenuRowInsertCallTargetAddress),
                ContextMenu_rowInsertCall_hook,
                &ContextMenu_rowInsertCall_orig))
        {
            contextMenuRowInsertHookInstalled = true;
            std::stringstream info;
            info << "Loot-Scoot-Execute INFO: ContextMenu row-insert call hook verification passed"
                 << " return=0x" << std::hex << g_resolvedContextMenuRowInsertCallReturnAddress
                 << " target=0x" << std::hex << g_resolvedContextMenuRowInsertCallTargetAddress;
            DebugLog(info.str().c_str());
        }
        else
        {
            ErrorLog("Loot-Scoot-Execute WARN: could not hook ContextMenu row-insert call target");
        }
    }
    else
    {
        ErrorLog("Loot-Scoot-Execute WARN: skipping ContextMenu row-insert call hook because compatibility gate did not pass");
    }
    g_nativeExecuteRowInsertHookInstallVerified = contextMenuRowInsertHookInstalled;

    bool contextMenuTaskLabelAlternateHookInstalled = false;
    if (g_contextMenuCompatibilityGatePassed && g_hookContextMenuTaskLabelAlternateAddress != 0)
    {
        if (!AreDualThunkHookTargetsSafe(
            g_hookContextMenuTaskLabelAddress,
            g_hookContextMenuTaskLabelAlternateAddress))
        {
            std::stringstream skip;
            skip << "Loot-Scoot-Execute INFO: skipped alternate task-label thunk hook"
                 << " reason=overlapping_targets"
                 << " primary=0x" << std::hex << g_hookContextMenuTaskLabelAddress
                 << " alternate=0x" << std::hex << g_hookContextMenuTaskLabelAlternateAddress;
            DebugLog(skip.str().c_str());
        }
        else if (KenshiLib::SUCCESS == KenshiLib::AddHook(
            reinterpret_cast<void*>(g_hookContextMenuTaskLabelAlternateAddress),
            ContextMenu_taskLabelThunk_probe_hook,
            &ContextMenu_taskLabelThunk_probe_orig))
        {
            contextMenuTaskLabelAlternateHookInstalled = true;
            std::stringstream info;
            info << "Loot-Scoot-Execute INFO: ContextMenu task-label alternate thunk hook verification passed"
                 << " target=0x" << std::hex << g_hookContextMenuTaskLabelAlternateAddress;
            DebugLog(info.str().c_str());
        }
        else
        {
            ErrorLog("Loot-Scoot-Execute WARN: could not hook ContextMenu task-label alternate thunk");
        }
    }
    g_nativeExecuteTaskLabelAlternateHookInstallVerified = contextMenuTaskLabelAlternateHookInstalled;

    bool contextMenuBuildRowsHookInstalled = false;
    if (g_contextMenuCompatibilityGatePassed)
    {
        if (KenshiLib::SUCCESS == KenshiLib::AddHook(
            reinterpret_cast<void*>(g_resolvedContextMenuBuildRowsAddress),
            ContextMenu_buildRows_hook,
            &ContextMenu_buildRows_orig))
        {
            contextMenuBuildRowsHookInstalled = true;
            DebugLog("Loot-Scoot-Execute INFO: ContextMenu::buildRows hook verification passed");
        }
        else
        {
            ErrorLog("Loot-Scoot-Execute WARN: could not hook ContextMenu::buildRows; context-menu injection fail-closed");
        }
    }
    else
    {
        ErrorLog("Loot-Scoot-Execute WARN: skipping ContextMenu::buildRows hook because compatibility gate did not pass");
    }
    g_nativeExecuteMenuBuildHookInstallVerified = contextMenuBuildRowsHookInstalled;

    bool contextMenuLoopEntryHookInstalled = false;
    if (g_contextMenuCompatibilityGatePassed)
    {
        if (InstallContextMenuLoopEntryInlineHook(g_resolvedContextMenuLoopEntryAddress))
        {
            contextMenuLoopEntryHookInstalled = true;
            std::stringstream info;
            info << "Loot-Scoot-Execute INFO: ContextMenu loop-entry inline hook verification passed"
                 << " target=0x" << std::hex << g_resolvedContextMenuLoopEntryAddress
                 << " return=0x" << std::hex << g_contextMenuLoopEntryInlineReturnAddress
                 << " stub=0x" << std::hex << reinterpret_cast<uintptr_t>(g_contextMenuLoopEntryInlineStubAddress);
            DebugLog(info.str().c_str());
        }
        else
        {
            ErrorLog("Loot-Scoot-Execute WARN: could not install ContextMenu loop-entry inline hook; continuing with buildRows preloop injection");
        }
    }
    else
    {
        ErrorLog("Loot-Scoot-Execute WARN: skipping ContextMenu loop-entry inline hook because compatibility gate did not pass");
    }
    g_nativeExecuteLoopEntryHookInstallVerified = contextMenuLoopEntryHookInstalled;

    RefreshEffectiveContextMenuFeatureFlags("post_playerinterface_hooks");

    bool contextMenuShowHookInstalled = false;
    bool contextMenuUpdateHookInstalled = false;
    if (g_contextMenuCompatibilityGatePassed)
    {
        if (KenshiLib::SUCCESS == KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&ContextMenu::showContextMenu),
            ContextMenu_showContextMenu_hook,
            &ContextMenu_showContextMenu_orig))
        {
            contextMenuShowHookInstalled = true;
            DebugLog("Loot-Scoot-Execute INFO: ContextMenu::showContextMenu hook verification passed");
        }
        else
        {
            ErrorLog("Loot-Scoot-Execute WARN: could not hook ContextMenu::showContextMenu; context-menu features fail-closed");
        }

        if (KenshiLib::SUCCESS == KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&ContextMenu::update),
            ContextMenu_update_hook,
            &ContextMenu_update_orig))
        {
            contextMenuUpdateHookInstalled = true;
            DebugLog("Loot-Scoot-Execute INFO: ContextMenu::update hook verification passed");
        }
        else
        {
            ErrorLog("Loot-Scoot-Execute WARN: could not hook ContextMenu::update; context-menu features fail-closed");
        }
    }
    else
    {
        ErrorLog("Loot-Scoot-Execute WARN: skipping ContextMenu::showContextMenu/ContextMenu::update hooks because compatibility gate did not pass");
    }

    g_contextMenuHookInstallVerified = contextMenuShowHookInstalled
        && contextMenuUpdateHookInstalled
        && contextMenuBuildRowsHookInstalled;
    RefreshEffectiveContextMenuFeatureFlags("post_showContextMenu_hook");

    g_hasSaveLoadHook = false;
    if (KenshiLib::SUCCESS == KenshiLib::AddHook(
        KenshiLib::GetRealAddress(static_cast<void (SaveManager::*)(const SaveInfo&, bool)>(&SaveManager::load)),
        SaveManager_loadByInfo_hook,
        &SaveManager_loadByInfo_orig))
    {
        g_hasSaveLoadHook = true;
    }
    else
    {
        ErrorLog("Loot-Scoot-Execute: Could not hook SaveManager::load(SaveInfo,bool)");
    }

    if (KenshiLib::SUCCESS == KenshiLib::AddHook(
        KenshiLib::GetRealAddress(static_cast<void (SaveManager::*)(const std::string&)>(&SaveManager::load)),
        SaveManager_loadByName_hook,
        &SaveManager_loadByName_orig))
    {
        g_hasSaveLoadHook = true;
    }
    else
    {
        ErrorLog("Loot-Scoot-Execute: Could not hook SaveManager::load(std::string)");
    }

    if (!g_hasSaveLoadHook)
    {
        ErrorLog("Loot-Scoot-Execute: no SaveManager load hooks active; feature disabled");
    }
    else
    {
        DebugLog("Loot-Scoot-Execute INFO: SaveManager load hook verification passed");
    }

    std::stringstream info;
    info << "Loot-Scoot-Execute INFO: initialized (enabled=" << (g_config.enabled ? "true" : "false")
         << ", pause_debounce_ms=" << g_config.pauseDebounceMs
         << ", runtime_mapping_key=" << g_runtimeGameVersion << "|" << g_runtimeLocaleTag << "|downed_enemy"
         << ", enable_context_menu_probe=" << (g_config.enableContextMenuProbe ? "true" : "false")
         << ", enable_context_menu_injection=" << (g_config.enableContextMenuInjection ? "true" : "false")
         << ", enable_execute_action=" << (g_config.enableExecuteAction ? "true" : "false")
         << ", debug_context_menu=" << (g_config.debugContextMenu ? "true" : "false")
         << ", enable_debug_direct_damage_fallback=" << (g_config.enableDebugDirectDamageFallback ? "true" : "false")
         << ", debug_execute_hotkey_vk=" << kDebugExecuteHotkeyVirtualKey
         << ", effective_context_menu_probe=" << (g_effectiveEnableContextMenuProbe ? "true" : "false")
         << ", effective_context_menu_injection=" << (g_effectiveEnableContextMenuInjection ? "true" : "false")
         << ", effective_execute_action=" << (g_effectiveEnableExecuteAction ? "true" : "false")
         << ", mapping_gate=" << (g_contextMenuMappingConfidenceGatePassed ? "passed" : "failed")
         << ", mapping_reason=" << (g_contextMenuMappingConfidenceGatePassed ? "none" : g_contextMenuMappingGateFailureReason)
         << ", compatibility_gate=" << (g_contextMenuCompatibilityGatePassed ? "passed" : "failed")
         << ", hook_verification=" << (g_contextMenuHookInstallVerified ? "passed" : "failed")
         << ", execute_selection_hook=" << (g_nativeExecuteSelectionHookInstallVerified ? "passed" : "failed")
         << ", execute_probability_hook=" << (g_nativeExecuteProbabilityHookInstallVerified ? "passed" : "failed")
         << ", execute_order_filter_hook=" << (g_nativeExecuteOrderFilterHookInstallVerified ? "passed" : "failed")
         << ", execute_order_filter_alt_hook=" << (g_nativeExecuteOrderFilterAlternateHookInstallVerified ? "passed" : "skipped_or_failed")
         << ", execute_context_menu_probability_hook=" << (g_nativeExecuteContextMenuProbabilityHookInstallVerified ? "passed" : "failed")
         << ", execute_context_menu_probability_alt_hook=" << (g_nativeExecuteContextMenuProbabilityAlternateHookInstallVerified ? "passed" : "skipped_or_failed")
         << ", execute_order_validity_hook=" << (g_nativeExecuteOrderValidityHookInstallVerified ? "passed" : "failed")
         << ", execute_order_append_hook=" << (g_nativeExecuteOrderAppendHookInstallVerified ? "passed" : "failed")
         << ", execute_order_append_alt_hook=" << (g_nativeExecuteOrderAppendAlternateHookInstallVerified ? "passed" : "skipped_or_failed")
         << ", execute_task_label_hook=" << (g_nativeExecuteTaskLabelHookInstallVerified ? "passed" : "failed")
         << ", execute_task_label_alt_hook=" << (g_nativeExecuteTaskLabelAlternateHookInstallVerified ? "passed" : "skipped_or_failed")
         << ", execute_menu_build_hook=" << (g_nativeExecuteMenuBuildHookInstallVerified ? "passed" : "failed")
         << ", execute_loop_entry_hook=" << (g_nativeExecuteLoopEntryHookInstallVerified ? "passed" : "failed")
         << ", execute_row_insert_hook=" << (g_nativeExecuteRowInsertHookInstallVerified ? "passed" : "failed")
         << ", save_load_hooks=" << (g_hasSaveLoadHook ? "true" : "false") << ")";
    DebugLog(info.str().c_str());
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        char dllPath[_MAX_PATH] = { 0 };
        if (GetModuleFileNameA(hModule, dllPath, _MAX_PATH) > 0)
        {
            std::string fullPath = TrimAscii(std::string(dllPath));
            size_t sep = fullPath.find_last_of("\\/");
            if (sep != std::string::npos)
            {
                const std::string myDirectory = fullPath.substr(0, sep);
                g_settingsPath = myDirectory + "\\" + kConfigFileName;
            }
        }
    }
    return TRUE;
}
