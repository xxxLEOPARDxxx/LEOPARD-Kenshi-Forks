static bool TryGetViewportSize(int* widthOut, int* heightOut)
{
    if (!widthOut || !heightOut)
    {
        return false;
    }

    MyGUI::RenderManager* renderManager = MyGUI::RenderManager::getInstancePtr();
    if (!renderManager)
    {
        return false;
    }

    const MyGUI::IntSize view = renderManager->getViewSize();
    if (view.width <= 0 || view.height <= 0)
    {
        return false;
    }

    *widthOut = view.width;
    *heightOut = view.height;
    return true;
}

static const int kJobRowsTop = 114;
static const int kJobRowStride = 38;
static const int kJobRowsBottomPadding = 30;
static const int kJobRowsMinVisibleWhenExpanded = 1;
static const int kJobRowsScrollButtonWidth = 24;
static const int kJobRowsScrollButtonHeight = 24;

static int GetJobRowTop(int rowIndex)
{
    return kJobRowsTop + (rowIndex * kJobRowStride);
}

static const int kScopeButtonMeWidth = 52;
static const int kScopeButtonSelectedWidth = 114;
static const int kScopeButtonSquadWidth = 102;
static const int kScopeButtonAllSquadsWidth = 102;
static const int kScopeButtonGap = 6;
static const int kScopeButtonHeight = 30;
static const char* kScopeCaptionMe = "Me";
static const char* kScopeCaptionSelected = "Selected";
static const char* kScopeCaptionSquadWarning = "Squad";
static const char* kScopeCaptionAllSquadsWarning = "All";

static int ComputeVisibleJobRowCapacityForHeight(int expandedHeight)
{
    const int availableHeight = expandedHeight - kPanelExpandedMinHeight;
    if (availableHeight <= 0)
    {
        return 0;
    }

    int capacity = kJobRowsMinVisibleWhenExpanded + (availableHeight / kJobRowStride);
    capacity = ClampIntValue(capacity, 0, kMaxVisibleJobRows);
    return capacity;
}

static int ComputeAdaptiveExpandedPanelHeight(bool showJobRows, bool showEmptyState, int jobCount, bool confirmationOverlayVisible)
{
    if (confirmationOverlayVisible)
    {
        return ClampIntValue(kPanelExpandedConfirmHeight, kPanelExpandedMinHeight, kPanelExpandedMaxHeight);
    }

    if (!showJobRows && !showEmptyState)
    {
        return kPanelExpandedMinHeight;
    }

    if (showEmptyState)
    {
        return kPanelExpandedMinHeight;
    }

    int desiredRows = jobCount;
    if (desiredRows < 0)
    {
        desiredRows = 0;
    }

    if (desiredRows < kJobRowsMinVisibleWhenExpanded)
    {
        desiredRows = kJobRowsMinVisibleWhenExpanded;
    }

    if (desiredRows > kMaxVisibleJobRows)
    {
        desiredRows = kMaxVisibleJobRows;
    }

    int expandedHeight = kPanelExpandedMinHeight + ((desiredRows - kJobRowsMinVisibleWhenExpanded) * kJobRowStride);
    expandedHeight = ClampIntValue(expandedHeight, kPanelExpandedMinHeight, kPanelExpandedMaxHeight);
    return expandedHeight;
}

static int GetExpandedPanelHeight()
{
    return ClampIntValue(g_panelExpandedHeight, kPanelExpandedMinHeight, kPanelExpandedMaxHeight);
}

static void OnJobRowsScrollUpButtonClicked(MyGUI::Widget*)
{
    if (g_jobRowScrollOffset <= 0)
    {
        return;
    }

    --g_jobRowScrollOffset;
}

static void OnJobRowsScrollDownButtonClicked(MyGUI::Widget*)
{
    const int visibleRows = ComputeVisibleJobRowCapacityForHeight(GetExpandedPanelHeight());
    const int totalRows = static_cast<int>(g_selectedMemberJobRows.size());
    const int maxOffset = ClampIntValue(totalRows - visibleRows, 0, totalRows);
    if (g_jobRowScrollOffset >= maxOffset)
    {
        return;
    }

    ++g_jobRowScrollOffset;
}

static int NormalizeMouseWheelNotches(int rel)
{
    if (rel == 0)
    {
        return 0;
    }

    const int absRel = (rel < 0) ? -rel : rel;
    int notches = absRel / 120;
    if (notches <= 0)
    {
        notches = 1;
    }
    return (rel > 0) ? notches : -notches;
}

static void ScrollJobRowsByNotches(int notches)
{
    if (notches == 0)
    {
        return;
    }

    if (g_jobBGonePanelCollapsed || g_jobBGoneConfirmVisible)
    {
        return;
    }

    const int visibleRows = ComputeVisibleJobRowCapacityForHeight(GetExpandedPanelHeight());
    const int totalRows = static_cast<int>(g_selectedMemberJobRows.size());
    const int maxOffset = ClampIntValue(totalRows - visibleRows, 0, totalRows);
    if (maxOffset <= 0)
    {
        g_jobRowScrollOffset = 0;
        return;
    }

    const int delta = (notches > 0) ? -notches : (-notches);
    g_jobRowScrollOffset = ClampIntValue(g_jobRowScrollOffset + delta, 0, maxOffset);
}

static void OnJobBGonePanelMouseWheel(MyGUI::Widget*, int rel)
{
    ScrollJobRowsByNotches(NormalizeMouseWheelNotches(rel));
}

static void SetJobButtonPayload(MyGUI::Button* button, const JobRowModel& row)
{
    if (!button)
    {
        return;
    }

    std::stringstream taskTypeValue;
    taskTypeValue << static_cast<int>(row.taskType);
    button->setUserString("JobKey", row.jobKey);
    button->setUserString("JobTaskType", taskTypeValue.str());
    button->setUserString("JobTaskName", row.taskName);
}

static bool StartsWithAsciiNoCase(const std::string& value, const char* prefix)
{
    if (!prefix)
    {
        return false;
    }

    size_t i = 0;
    for (; prefix[i] != '\0'; ++i)
    {
        if (i >= value.size())
        {
            return false;
        }
        const unsigned char valueChar = static_cast<unsigned char>(value[i]);
        const unsigned char prefixChar = static_cast<unsigned char>(prefix[i]);
        if (std::tolower(valueChar) != std::tolower(prefixChar))
        {
            return false;
        }
    }
    return true;
}

static bool ContainsAsciiNoCase(const std::string& value, const char* needle)
{
    if (!needle || needle[0] == '\0')
    {
        return false;
    }

    const size_t needleLen = std::strlen(needle);
    if (needleLen > value.size())
    {
        return false;
    }

    for (size_t start = 0; start + needleLen <= value.size(); ++start)
    {
        bool match = true;
        for (size_t i = 0; i < needleLen; ++i)
        {
            const unsigned char valueChar = static_cast<unsigned char>(value[start + i]);
            const unsigned char needleChar = static_cast<unsigned char>(needle[i]);
            if (std::tolower(valueChar) != std::tolower(needleChar))
            {
                match = false;
                break;
            }
        }
        if (match)
        {
            return true;
        }
    }
    return false;
}

static std::string NormalizeTaskNameForDisplay(const std::string& taskName)
{
    std::string normalized;
    normalized.reserve(taskName.size());

    bool pendingSpace = false;
    const size_t len = taskName.size();
    for (size_t i = 0; i < len; ++i)
    {
        const unsigned char ch = static_cast<unsigned char>(taskName[i]);
        if (std::isspace(ch) != 0 || ch < 32 || ch == 127)
        {
            pendingSpace = true;
            continue;
        }

        if (pendingSpace && !normalized.empty())
        {
            normalized.push_back(' ');
        }
        pendingSpace = false;
        normalized.push_back(static_cast<char>(ch));
    }

    return normalized;
}

enum TaskTargetInferenceKind
{
    TaskTargetInference_None = 0,
    TaskTargetInference_OperatingMachine = 1,
    TaskTargetInference_Follow = 2,
    TaskTargetInference_Bodyguard = 3
};

static bool IsPrefixOnlyTaskName(const std::string& taskName, const char* prefix)
{
    if (!prefix || !StartsWithAsciiNoCase(taskName, prefix))
    {
        return false;
    }

    size_t pos = std::strlen(prefix);
    while (pos < taskName.size())
    {
        const unsigned char ch = static_cast<unsigned char>(taskName[pos]);
        if (std::isspace(ch) != 0 || ch == ':' || ch == '-' || ch == '>')
        {
            ++pos;
            continue;
        }
        return false;
    }
    return true;
}

static bool IsGenericOperatingMachineName(const std::string& taskName)
{
    return IsPrefixOnlyTaskName(taskName, "Operating machine");
}

static bool IsGenericFollowName(const std::string& taskName)
{
    return IsPrefixOnlyTaskName(taskName, "Follow")
        || IsPrefixOnlyTaskName(taskName, "Following")
        || IsPrefixOnlyTaskName(taskName, "Staying close");
}

static bool IsGenericBodyguardName(const std::string& taskName)
{
    return IsPrefixOnlyTaskName(taskName, "Bodyguard")
        || IsPrefixOnlyTaskName(taskName, "Bodyguarding");
}

static TaskTargetInferenceKind DetectTaskTargetInferenceKind(const std::string& normalizedTaskName)
{
    if (StartsWithAsciiNoCase(normalizedTaskName, "Operating machine"))
    {
        return TaskTargetInference_OperatingMachine;
    }
    if (StartsWithAsciiNoCase(normalizedTaskName, "Follow")
        || StartsWithAsciiNoCase(normalizedTaskName, "Following")
        || StartsWithAsciiNoCase(normalizedTaskName, "Staying close"))
    {
        return TaskTargetInference_Follow;
    }
    if (StartsWithAsciiNoCase(normalizedTaskName, "Bodyguard")
        || StartsWithAsciiNoCase(normalizedTaskName, "Bodyguarding"))
    {
        return TaskTargetInference_Bodyguard;
    }
    return TaskTargetInference_None;
}

static TaskTargetInferenceKind DetectTaskTargetInferenceKindByTaskType(TaskType taskType)
{
    switch (taskType)
    {
    case OPERATE_MACHINERY:
    case OPERATE_AUTOMATIC_MACHINERY:
    case PRETEND_TO_OPERATE_MACHINERY:
        return TaskTargetInference_OperatingMachine;
    case FOLLOW_PLAYER_ORDER:
    case FOLLOW_SQUADLEADER:
    case FOLLOW_WHILE_TALKING:
    case FOLLOW_SLAVEMASTER:
    case FOLLOW_URGENT_ESCAPE:
    case STAY_CLOSE_TO_TARGET:
    case STAY_CLOSE_TO_TARGET_ANIMAL:
    case SWITCH_FOLLOW_ME_MODE_ON:
        return TaskTargetInference_Follow;
    case BODYGUARD:
        return TaskTargetInference_Bodyguard;
    default:
        break;
    }
    return TaskTargetInference_None;
}

static bool TaskNameHasExplicitTargetDelimiter(const std::string& normalizedTaskName)
{
    return normalizedTaskName.find(':') != std::string::npos
        || normalizedTaskName.find("->") != std::string::npos;
}

static bool IsTaskNameMissingTarget(TaskTargetInferenceKind kind, const std::string& normalizedTaskName)
{
    switch (kind)
    {
    case TaskTargetInference_OperatingMachine:
        return IsGenericOperatingMachineName(normalizedTaskName);
    case TaskTargetInference_Follow:
        return IsGenericFollowName(normalizedTaskName);
    case TaskTargetInference_Bodyguard:
        return IsGenericBodyguardName(normalizedTaskName);
    default:
        break;
    }
    return false;
}

static const char* GetTaskTargetInferenceKindLogName(TaskTargetInferenceKind kind)
{
    switch (kind)
    {
    case TaskTargetInference_OperatingMachine:
        return "operating_machine";
    case TaskTargetInference_Follow:
        return "follow";
    case TaskTargetInference_Bodyguard:
        return "bodyguard";
    default:
        break;
    }
    return "unknown";
}

static std::string BuildExpandedTaskNameWithTarget(TaskTargetInferenceKind kind, const std::string& targetName)
{
    switch (kind)
    {
    case TaskTargetInference_OperatingMachine:
        return "Operating machine: " + targetName;
    case TaskTargetInference_Follow:
        return "Follow: " + targetName;
    case TaskTargetInference_Bodyguard:
        return "Bodyguard: " + targetName;
    default:
        break;
    }
    return targetName;
}

static bool IsLikelyGenericTargetCandidate(TaskTargetInferenceKind kind, const std::string& candidateName)
{
    if (candidateName.empty())
    {
        return true;
    }

    switch (kind)
    {
    case TaskTargetInference_OperatingMachine:
        return IsGenericOperatingMachineName(candidateName);
    case TaskTargetInference_Follow:
        return IsGenericFollowName(candidateName);
    case TaskTargetInference_Bodyguard:
        return IsGenericBodyguardName(candidateName);
    default:
        break;
    }
    return false;
}

static bool IsPlausibleHandCandidateNoexcept(const hand* candidate)
{
    if (!candidate)
    {
        return false;
    }

    __try
    {
        const int typeValue = static_cast<int>(candidate->type);
        const bool typeInRange = typeValue >= static_cast<int>(BUILDING) && typeValue < static_cast<int>(OBJECT_TYPE_MAX);
        const bool notNullHandle = candidate->index != 0 || candidate->serial != 0;
        return typeInRange && notNullHandle;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryResolveHandleTargetNoexcept(const hand* handle, RootObjectBase** targetOut)
{
    if (!handle || !targetOut)
    {
        return false;
    }

    __try
    {
        if (!(*handle) || !handle->isValid())
        {
            return false;
        }

        RootObjectBase* target = handle->getRootObjectBase();
        if (!target || !target->isValid())
        {
            return false;
        }

        *targetOut = target;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryResolveNameFromPotentialHandle(const hand* handle, std::string* nameOut)
{
    if (!handle || !nameOut)
    {
        return false;
    }

    RootObjectBase* target = 0;
    if (!TryResolveHandleTargetNoexcept(handle, &target) || !target)
    {
        return false;
    }

    const std::string normalizedName = NormalizeTaskNameForDisplay(target->getName());
    if (normalizedName.empty())
    {
        return false;
    }

    *nameOut = normalizedName;
    return true;
}

static bool TryInferTaskTargetName(uintptr_t taskDataPtr, TaskTargetInferenceKind kind, std::string* targetNameOut)
{
    if (!targetNameOut || taskDataPtr == 0 || kind == TaskTargetInference_None)
    {
        return false;
    }

    struct CachedTaskTargetName
    {
        uintptr_t taskDataPtr;
        TaskTargetInferenceKind kind;
        bool hasValue;
        std::string value;
    };
    static std::vector<CachedTaskTargetName> cache;

    const size_t cacheSize = cache.size();
    for (size_t i = 0; i < cacheSize; ++i)
    {
        if (cache[i].taskDataPtr != taskDataPtr || cache[i].kind != kind)
        {
            continue;
        }

        if (!cache[i].hasValue)
        {
            return false;
        }

        *targetNameOut = cache[i].value;
        return true;
    }

    std::string firstName;
    std::string preferredName;

    // Tasker is opaque in KenshiLib. Probe first bytes for embedded handle fields.
    static const int kScanBytes = 0x180;
    for (int offset = 0; offset <= kScanBytes; offset += 4)
    {
        const hand* candidate = reinterpret_cast<const hand*>(taskDataPtr + static_cast<uintptr_t>(offset));
        if (!IsPlausibleHandCandidateNoexcept(candidate))
        {
            continue;
        }

        std::string resolvedName;
        if (!TryResolveNameFromPotentialHandle(candidate, &resolvedName))
        {
            continue;
        }

        if (IsLikelyGenericTargetCandidate(kind, resolvedName))
        {
            continue;
        }

        if (firstName.empty())
        {
            firstName = resolvedName;
        }

        if (kind == TaskTargetInference_OperatingMachine
            && (ContainsAsciiNoCase(resolvedName, "resource")
                || ContainsAsciiNoCase(resolvedName, "copper")
                || ContainsAsciiNoCase(resolvedName, "iron")
                || ContainsAsciiNoCase(resolvedName, "machine")))
        {
            preferredName = resolvedName;
            break;
        }

        if (kind == TaskTargetInference_Follow || kind == TaskTargetInference_Bodyguard)
        {
            // Character names are typically not machine/resource labels.
            if (!ContainsAsciiNoCase(resolvedName, "machine")
                && !ContainsAsciiNoCase(resolvedName, "resource")
                && !ContainsAsciiNoCase(resolvedName, "ore"))
            {
                preferredName = resolvedName;
                break;
            }
        }
    }

    CachedTaskTargetName cached = { 0, TaskTargetInference_None, false, std::string() };
    cached.taskDataPtr = taskDataPtr;
    cached.kind = kind;
    if (!preferredName.empty())
    {
        cached.hasValue = true;
        cached.value = preferredName;
    }
    else if (!firstName.empty())
    {
        cached.hasValue = true;
        cached.value = firstName;
    }

    if (g_config.debugLogTransitions)
    {
        std::stringstream info;
        info << "Job-B-Gone DEBUG: task_target_infer"
             << " kind=" << GetTaskTargetInferenceKindLogName(kind)
             << " task_data_ptr=0x" << std::hex << taskDataPtr << std::dec
             << " resolved=" << (cached.hasValue ? "true" : "false");
        if (cached.hasValue)
        {
            info << " target=\"" << cached.value << "\"";
        }
        else
        {
            info << " first_candidate=\"" << firstName << "\"";
        }
        DebugLog(info.str().c_str());
    }

    cache.push_back(cached);

    if (!cached.hasValue)
    {
        return false;
    }

    *targetNameOut = cached.value;
    return true;
}

static std::string BuildCompactTaskNameForDisplay(const std::string& taskName)
{
    const std::string normalizedTaskName = NormalizeTaskNameForDisplay(taskName);
    if (normalizedTaskName.empty())
    {
        return taskName;
    }

    static const char* kOperatingMachinePrefix = "Operating machine";
    if (!StartsWithAsciiNoCase(normalizedTaskName, kOperatingMachinePrefix))
    {
        return normalizedTaskName;
    }

    const size_t prefixLen = std::strlen(kOperatingMachinePrefix);
    size_t suffixPos = prefixLen;

    const size_t colonPos = normalizedTaskName.find(':', prefixLen);
    if (colonPos != std::string::npos)
    {
        suffixPos = colonPos + 1;
    }
    else
    {
        const size_t arrowPos = normalizedTaskName.find("->", prefixLen);
        if (arrowPos != std::string::npos)
        {
            suffixPos = arrowPos + 2;
        }
        else
        {
            const size_t dashPos = normalizedTaskName.find(" - ", prefixLen);
            if (dashPos != std::string::npos)
            {
                suffixPos = dashPos + 3;
            }
            else
            {
                const size_t lineBreakPos = normalizedTaskName.find_first_of("\r\n", prefixLen);
                if (lineBreakPos != std::string::npos)
                {
                    suffixPos = lineBreakPos + 1;
                }
            }
        }
    }

    while (suffixPos < normalizedTaskName.size())
    {
        const unsigned char ch = static_cast<unsigned char>(normalizedTaskName[suffixPos]);
        if (std::isspace(ch) != 0 || ch == ':' || ch == '-' || ch == '>')
        {
            ++suffixPos;
            continue;
        }
        break;
    }

    if (suffixPos >= normalizedTaskName.size())
    {
        // Keep compact label even when no machine target can be extracted.
        return "Op. m:";
    }

    std::stringstream compact;
    compact << "Op. m: " << normalizedTaskName.substr(suffixPos);
    return compact.str();
}

static void OnActionButtonMouseSetFocus(MyGUI::Widget* sender, MyGUI::Widget*)
{
    if (!g_jobBGoneHoverHintText || !sender)
    {
        return;
    }

    g_jobBGoneHoverHintText->setCaption(sender->getUserString("JobBGoneHoverHint"));
}

static void OnActionButtonMouseLostFocus(MyGUI::Widget*, MyGUI::Widget*)
{
    if (!g_jobBGoneHoverHintText)
    {
        return;
    }

    g_jobBGoneHoverHintText->setCaption("");
}

static void BindWidgetHoverHintHandlers(MyGUI::Widget* widget)
{
    if (!widget)
    {
        return;
    }

    widget->setNeedMouseFocus(true);
    widget->eventMouseSetFocus += MyGUI::newDelegate(&OnActionButtonMouseSetFocus);
    widget->eventMouseLostFocus += MyGUI::newDelegate(&OnActionButtonMouseLostFocus);
    widget->eventMouseWheel += MyGUI::newDelegate(&OnJobBGonePanelMouseWheel);
}

static void SetWidgetTooltipAndHoverHint(MyGUI::Widget* widget, const char* tooltipText)
{
    if (!widget || !tooltipText)
    {
        return;
    }

    BindWidgetHoverHintHandlers(widget);
    widget->setNeedToolTip(true);
    widget->setUserString("ToolTip", tooltipText);
    widget->setUserString("JobBGoneHoverHint", tooltipText);
}

static void SetWidgetHoverHint(MyGUI::Widget* widget, const std::string& hoverHint)
{
    if (!widget)
    {
        return;
    }

    widget->setUserString("JobBGoneHoverHint", hoverHint);
}

static void AddUniqueCharacterForSelection(std::vector<Character*>* membersOut, Character* member)
{
    if (!membersOut || !member)
    {
        return;
    }

    const size_t count = membersOut->size();
    for (size_t i = 0; i < count; ++i)
    {
        if ((*membersOut)[i] == member)
        {
            return;
        }
    }
    membersOut->push_back(member);
}

static bool HasJobSignature(const std::vector<JobRowModel>& rows, TaskType taskType, const std::string& taskName)
{
    const size_t count = rows.size();
    for (size_t i = 0; i < count; ++i)
    {
        if (rows[i].taskType == taskType && rows[i].taskName == taskName)
        {
            return true;
        }
    }
    return false;
}

static bool TryCollectSelectedCharactersForDisplay(std::vector<Character*>* membersOut)
{
    if (!membersOut || !g_lastPlayerInterface)
    {
        return false;
    }

    __try
    {
        for (ogre_unordered_set<hand>::type::const_iterator it = g_lastPlayerInterface->selectedCharacters.begin();
             it != g_lastPlayerInterface->selectedCharacters.end();
             ++it)
        {
            const hand& selectedHandle = *it;
            if (!selectedHandle || !selectedHandle.isValid())
            {
                continue;
            }

            Character* member = selectedHandle.getCharacter();
            AddUniqueCharacterForSelection(membersOut, member);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return true;
}

enum PanelUiSuppressionReason
{
    PanelUiSuppressionReason_None = 0,
    PanelUiSuppressionReason_CharacterCreation = 1 << 0,
    PanelUiSuppressionReason_InventoryOpen = 1 << 1,
    PanelUiSuppressionReason_CharacterInteraction = 1 << 2,
    PanelUiSuppressionReason_MenuOpen = 1 << 3,
    PanelUiSuppressionReason_HudHidden = 1 << 4
};

static bool g_hudHiddenByToggleEvent = false;
static bool g_hudHiddenByToggleEventKnown = false;
static std::string BuildPanelUiSuppressionReasonSummary(int reasonMask);

struct MemberUiSuppressionProbeCacheEntry
{
    bool inUse;
    uintptr_t memberPtr;
    int reasonMask;
    bool memberEngaged;
    bool memberDialogueActive;
    bool hasConversationTarget;
    bool targetEngaged;
    bool targetDialogueActive;
    bool memberInventoryVisible;
    bool targetInventoryVisible;
    DWORD lastLoggedAtMs;
};

static MemberUiSuppressionProbeCacheEntry g_memberUiSuppressionProbeCache[8] = {};

static MemberUiSuppressionProbeCacheEntry* AcquireMemberUiSuppressionProbeCacheEntry(uintptr_t memberPtr)
{
    MemberUiSuppressionProbeCacheEntry* firstFree = 0;
    MemberUiSuppressionProbeCacheEntry* oldest = 0;
    for (size_t i = 0; i < (sizeof(g_memberUiSuppressionProbeCache) / sizeof(g_memberUiSuppressionProbeCache[0])); ++i)
    {
        MemberUiSuppressionProbeCacheEntry* entry = &g_memberUiSuppressionProbeCache[i];
        if (entry->inUse && entry->memberPtr == memberPtr)
        {
            return entry;
        }
        if (!entry->inUse && !firstFree)
        {
            firstFree = entry;
        }
        if (!oldest || entry->lastLoggedAtMs < oldest->lastLoggedAtMs)
        {
            oldest = entry;
        }
    }

    if (firstFree)
    {
        return firstFree;
    }
    return oldest;
}

static void MaybeLogMemberUiSuppressionProbe(
    Character* member,
    int reasonMask,
    bool memberEngaged,
    bool memberDialogueActive,
    bool hasConversationTarget,
    bool targetEngaged,
    bool targetDialogueActive,
    bool memberInventoryVisible,
    bool targetInventoryVisible)
{
    if (!g_config.debugLogTransitions || !member)
    {
        return;
    }

    MemberUiSuppressionProbeCacheEntry* entry = AcquireMemberUiSuppressionProbeCacheEntry(reinterpret_cast<uintptr_t>(member));
    if (!entry)
    {
        return;
    }

    const bool sameMember = entry->inUse && entry->memberPtr == reinterpret_cast<uintptr_t>(member);
    const bool changed =
        (!sameMember)
        || entry->reasonMask != reasonMask
        || entry->memberEngaged != memberEngaged
        || entry->memberDialogueActive != memberDialogueActive
        || entry->hasConversationTarget != hasConversationTarget
        || entry->targetEngaged != targetEngaged
        || entry->targetDialogueActive != targetDialogueActive
        || entry->memberInventoryVisible != memberInventoryVisible
        || entry->targetInventoryVisible != targetInventoryVisible;

    const DWORD nowMs = GetTickCount();
    const bool changedWithActiveSuppression = changed && reasonMask != PanelUiSuppressionReason_None;
    const bool heartbeat =
        sameMember
        && !changed
        && reasonMask != PanelUiSuppressionReason_None
        && entry->lastLoggedAtMs != 0
        && DebounceWindowElapsed(nowMs, entry->lastLoggedAtMs, 5000);
    const bool transitionedClear =
        sameMember
        && entry->reasonMask != PanelUiSuppressionReason_None
        && reasonMask == PanelUiSuppressionReason_None;

    if (changedWithActiveSuppression || heartbeat || transitionedClear)
    {
        std::stringstream info;
        info << "Job-B-Gone DEBUG: member_ui_suppression_probe"
             << " member_ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(member) << std::dec
             << " reasons=" << BuildPanelUiSuppressionReasonSummary(reasonMask)
             << " member_engaged=" << (memberEngaged ? "true" : "false")
             << " member_dialogue_active=" << (memberDialogueActive ? "true" : "false")
             << " has_conversation_target=" << (hasConversationTarget ? "true" : "false")
             << " target_engaged=" << (targetEngaged ? "true" : "false")
             << " target_dialogue_active=" << (targetDialogueActive ? "true" : "false")
             << " member_inventory_visible=" << (memberInventoryVisible ? "true" : "false")
             << " target_inventory_visible=" << (targetInventoryVisible ? "true" : "false");
        DebugLog(info.str().c_str());
        entry->lastLoggedAtMs = nowMs;
    }

    entry->inUse = true;
    entry->memberPtr = reinterpret_cast<uintptr_t>(member);
    entry->reasonMask = reasonMask;
    entry->memberEngaged = memberEngaged;
    entry->memberDialogueActive = memberDialogueActive;
    entry->hasConversationTarget = hasConversationTarget;
    entry->targetEngaged = targetEngaged;
    entry->targetDialogueActive = targetDialogueActive;
    entry->memberInventoryVisible = memberInventoryVisible;
    entry->targetInventoryVisible = targetInventoryVisible;
}

static void LogPanelUiSuppressionSources(
    bool dialogueWindowOpen,
    bool menuOpen,
    bool hudHidden,
    bool characterCreationModeActive,
    int selectedMemberCount,
    int desiredSuppressionMask,
    int reasonMask)
{
    if (!g_config.debugLogTransitions)
    {
        return;
    }

    static bool s_initialized = false;
    static bool s_lastDialogueWindowOpen = false;
    static bool s_lastMenuOpen = false;
    static bool s_lastHudHidden = false;
    static bool s_lastCharacterCreationModeActive = false;
    static int s_lastSelectedMemberCount = -1;
    static int s_lastDesiredSuppressionMask = PanelUiSuppressionReason_None;
    static int s_lastReasonMask = PanelUiSuppressionReason_None;
    static DWORD s_lastLoggedAtMs = 0;

    const bool changed =
        !s_initialized
        || s_lastDialogueWindowOpen != dialogueWindowOpen
        || s_lastMenuOpen != menuOpen
        || s_lastHudHidden != hudHidden
        || s_lastCharacterCreationModeActive != characterCreationModeActive
        || s_lastSelectedMemberCount != selectedMemberCount
        || s_lastDesiredSuppressionMask != desiredSuppressionMask
        || s_lastReasonMask != reasonMask;
    const DWORD nowMs = GetTickCount();
    const bool heartbeat =
        s_initialized
        && !changed
        && reasonMask != PanelUiSuppressionReason_None
        && s_lastLoggedAtMs != 0
        && DebounceWindowElapsed(nowMs, s_lastLoggedAtMs, 5000);
    if (!(changed || heartbeat))
    {
        return;
    }

    std::stringstream info;
    info << "Job-B-Gone DEBUG: panel_ui_suppression_sources"
         << " dialogue_window_open=" << (dialogueWindowOpen ? "true" : "false")
         << " menu_open=" << (menuOpen ? "true" : "false")
         << " hud_hidden=" << (hudHidden ? "true" : "false")
         << " character_creation_mode=" << (characterCreationModeActive ? "true" : "false")
         << " selected_members=" << selectedMemberCount
         << " desired_member_reasons=" << BuildPanelUiSuppressionReasonSummary(desiredSuppressionMask)
         << " final_reasons=" << BuildPanelUiSuppressionReasonSummary(reasonMask);
    DebugLog(info.str().c_str());

    s_initialized = true;
    s_lastDialogueWindowOpen = dialogueWindowOpen;
    s_lastMenuOpen = menuOpen;
    s_lastHudHidden = hudHidden;
    s_lastCharacterCreationModeActive = characterCreationModeActive;
    s_lastSelectedMemberCount = selectedMemberCount;
    s_lastDesiredSuppressionMask = desiredSuppressionMask;
    s_lastReasonMask = reasonMask;
    s_lastLoggedAtMs = nowMs;
}

static bool TryGetCharacterCreationModeActive(bool* activeOut)
{
    if (!activeOut || !g_lastPlayerInterface)
    {
        return false;
    }

    __try
    {
        *activeOut = g_lastPlayerInterface->getCharacterEditMode() || g_lastPlayerInterface->characterEditorMode;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryGetSelectedMemberInfoPanelNoexcept(DatapanelGUI** panelOut)
{
    if (!panelOut)
    {
        return false;
    }

    *panelOut = 0;
    if (g_selectedMemberInfoWindowAddress == 0)
    {
        return true;
    }

    __try
    {
        DatapanelGUI** infoWindowSlot = reinterpret_cast<DatapanelGUI**>(g_selectedMemberInfoWindowAddress);
        if (!infoWindowSlot)
        {
            return true;
        }

        *panelOut = *infoWindowSlot;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryGetHudHiddenStateFromWidgetHierarchy(bool* hiddenOut)
{
    if (!hiddenOut)
    {
        return false;
    }

    *hiddenOut = false;

    DatapanelGUI* selectedMemberInfoPanel = 0;
    if (!TryGetSelectedMemberInfoPanelNoexcept(&selectedMemberInfoPanel))
    {
        return false;
    }

    if (!selectedMemberInfoPanel)
    {
        return true;
    }

    __try
    {
        MyGUI::Widget* panelWidget = selectedMemberInfoPanel->getWidget();
        if (!panelWidget)
        {
            *hiddenOut = !selectedMemberInfoPanel->isVisible();
            return true;
        }

        const bool hasSelectedMember = (ResolveSelectedMember() != 0);
        const bool panelVisible = panelWidget->getInheritedVisible();

        // Parent container captures global HUD hidden states.
        MyGUI::Widget* hudSignalWidget = panelWidget->getParent();
        if (!hudSignalWidget)
        {
            *hiddenOut = !panelVisible;
            return true;
        }

        const bool hudContainerVisible = hudSignalWidget->getInheritedVisible();

        // If the container is hidden, HUD is hidden.
        // If a member is selected and the selected-member panel itself is hidden,
        // treat that as HUD hidden as well.
        *hiddenOut = (!hudContainerVisible) || (hasSelectedMember && !panelVisible);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void CaptureHudToggleEventSignal()
{
    bool toggleEventSeen = false;
    bool hiddenNow = false;
    bool knownNow = false;
    __try
    {
        if (!key)
        {
            return;
        }

        InputHandler::Command* toggleBarCommand = key->getCommand(kToggleBarCommandName);
        if (!toggleBarCommand)
        {
            return;
        }

        if (key->events.count(toggleBarCommand) == 0)
        {
            return;
        }

        if (!g_hudHiddenByToggleEventKnown)
        {
            g_hudHiddenByToggleEventKnown = true;
        }
        g_hudHiddenByToggleEvent = !g_hudHiddenByToggleEvent;
        toggleEventSeen = true;
        hiddenNow = g_hudHiddenByToggleEvent;
        knownNow = g_hudHiddenByToggleEventKnown;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return;
    }

    if (!toggleEventSeen)
    {
        return;
    }

    if (!g_config.debugLogTransitions)
    {
        return;
    }

    if (hiddenNow)
    {
        DebugLog(knownNow
            ? "Job-B-Gone DEBUG: hud_toggle_event_seen hidden=true known=true"
            : "Job-B-Gone DEBUG: hud_toggle_event_seen hidden=true known=false");
        return;
    }

    DebugLog(knownNow
        ? "Job-B-Gone DEBUG: hud_toggle_event_seen hidden=false known=true"
        : "Job-B-Gone DEBUG: hud_toggle_event_seen hidden=false known=false");
}

static void LogHudSignalProbe(
    bool keyPointerSeen,
    bool toggleBarCommandFound,
    bool toggleBarBooleanFound,
    bool hasCommandSignal,
    bool commandState,
    bool commandHidden,
    bool commandPolarityKnown,
    bool commandTrueMeansHidden,
    bool toggleEventKnown,
    bool toggleEventHidden,
    bool hasWidgetSignal,
    bool widgetHidden,
    int sourceCode,
    bool finalHidden)
{
    if (!g_config.debugLogTransitions)
    {
        return;
    }

    static bool s_initialized = false;
    static bool s_lastKeyPointerSeen = false;
    static bool s_lastToggleBarCommandFound = false;
    static bool s_lastToggleBarBooleanFound = false;
    static bool s_lastHasCommandSignal = false;
    static bool s_lastCommandState = false;
    static bool s_lastCommandHidden = false;
    static bool s_lastCommandPolarityKnown = false;
    static bool s_lastCommandTrueMeansHidden = false;
    static bool s_lastToggleEventKnown = false;
    static bool s_lastToggleEventHidden = false;
    static bool s_lastHasWidgetSignal = false;
    static bool s_lastWidgetHidden = false;
    static int s_lastSourceCode = -1;
    static bool s_lastFinalHidden = false;

    const bool changed =
        (!s_initialized)
        || s_lastKeyPointerSeen != keyPointerSeen
        || s_lastToggleBarCommandFound != toggleBarCommandFound
        || s_lastToggleBarBooleanFound != toggleBarBooleanFound
        || s_lastHasCommandSignal != hasCommandSignal
        || s_lastCommandState != commandState
        || s_lastCommandHidden != commandHidden
        || s_lastCommandPolarityKnown != commandPolarityKnown
        || s_lastCommandTrueMeansHidden != commandTrueMeansHidden
        || s_lastToggleEventKnown != toggleEventKnown
        || s_lastToggleEventHidden != toggleEventHidden
        || s_lastHasWidgetSignal != hasWidgetSignal
        || s_lastWidgetHidden != widgetHidden
        || s_lastSourceCode != sourceCode
        || s_lastFinalHidden != finalHidden;
    if (!changed)
    {
        return;
    }

    const char* sourceName = "none";
    if (sourceCode == 1)
    {
        sourceName = "command_toggle_bar";
    }
    else if (sourceCode == 2)
    {
        sourceName = "widget_fallback";
    }
    else if (sourceCode == 3)
    {
        sourceName = "toggle_event_latched";
    }

    std::stringstream info;
    info << "Job-B-Gone DEBUG: hud_signal_probe"
         << " source=" << sourceName
         << " hidden=" << (finalHidden ? "true" : "false")
         << " key_ptr=" << (keyPointerSeen ? "true" : "false")
         << " command_found=" << (toggleBarCommandFound ? "true" : "false")
         << " command_bool_ptr=" << (toggleBarBooleanFound ? "true" : "false")
         << " command_signal=" << (hasCommandSignal ? "true" : "false")
         << " command_state=" << (commandState ? "true" : "false")
         << " command_hidden=" << (commandHidden ? "true" : "false")
         << " polarity_known=" << (commandPolarityKnown ? "true" : "false")
         << " true_means_hidden=" << (commandTrueMeansHidden ? "true" : "false")
         << " toggle_event_known=" << (toggleEventKnown ? "true" : "false")
         << " toggle_event_hidden=" << (toggleEventHidden ? "true" : "false")
         << " widget_signal=" << (hasWidgetSignal ? "true" : "false")
         << " widget_hidden=" << (widgetHidden ? "true" : "false");
    DebugLog(info.str().c_str());

    s_initialized = true;
    s_lastKeyPointerSeen = keyPointerSeen;
    s_lastToggleBarCommandFound = toggleBarCommandFound;
    s_lastToggleBarBooleanFound = toggleBarBooleanFound;
    s_lastHasCommandSignal = hasCommandSignal;
    s_lastCommandState = commandState;
    s_lastCommandHidden = commandHidden;
    s_lastCommandPolarityKnown = commandPolarityKnown;
    s_lastCommandTrueMeansHidden = commandTrueMeansHidden;
    s_lastToggleEventKnown = toggleEventKnown;
    s_lastToggleEventHidden = toggleEventHidden;
    s_lastHasWidgetSignal = hasWidgetSignal;
    s_lastWidgetHidden = widgetHidden;
    s_lastSourceCode = sourceCode;
    s_lastFinalHidden = finalHidden;
}

static bool TryGetHudHiddenState(bool* hiddenOut)
{
    if (!hiddenOut)
    {
        return false;
    }

    *hiddenOut = false;

    bool widgetHidden = false;
    const bool hasWidgetSignal = TryGetHudHiddenStateFromWidgetHierarchy(&widgetHidden);
    if (!g_hudHiddenByToggleEventKnown && hasWidgetSignal)
    {
        g_hudHiddenByToggleEvent = widgetHidden;
        g_hudHiddenByToggleEventKnown = true;
    }

    bool commandHidden = false;
    bool hasCommandSignal = false;
    bool commandState = false;
    bool commandPolarityKnown = false;
    bool commandTrueMeansHidden = false;
    bool keyPointerSeen = false;
    bool toggleBarCommandFound = false;
    bool toggleBarBooleanFound = false;
    __try
    {
        if (key)
        {
            keyPointerSeen = true;
            InputHandler::Command* toggleBarCommand = key->getCommand(kToggleBarCommandName);
            if (toggleBarCommand)
            {
                toggleBarCommandFound = true;
                if (toggleBarCommand->boolean)
                {
                    toggleBarBooleanFound = true;
                    commandState = *(toggleBarCommand->boolean);
                    static bool s_commandPolarityKnown = false;
                    static bool s_commandTrueMeansHidden = false;
                    if (!s_commandPolarityKnown)
                    {
                        if (hasWidgetSignal)
                        {
                            s_commandTrueMeansHidden = (commandState == widgetHidden);
                        }
                        else
                        {
                            // Default assumption: toggle_bar=true means bars shown.
                            s_commandTrueMeansHidden = false;
                        }
                        s_commandPolarityKnown = true;
                    }

                    commandPolarityKnown = s_commandPolarityKnown;
                    commandTrueMeansHidden = s_commandTrueMeansHidden;
                    commandHidden = s_commandTrueMeansHidden ? commandState : !commandState;
                    hasCommandSignal = true;
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    int sourceCode = 0;
    bool finalHidden = false;

    if (g_hudHiddenByToggleEventKnown)
    {
        sourceCode = 3;
        finalHidden = g_hudHiddenByToggleEvent;
    }
    else if (hasCommandSignal)
    {
        sourceCode = 1;
        finalHidden = commandHidden;
    }
    else if (hasWidgetSignal)
    {
        sourceCode = 2;
        finalHidden = widgetHidden;
    }

    LogHudSignalProbe(
        keyPointerSeen,
        toggleBarCommandFound,
        toggleBarBooleanFound,
        hasCommandSignal,
        commandState,
        commandHidden,
        commandPolarityKnown,
        commandTrueMeansHidden,
        g_hudHiddenByToggleEventKnown,
        g_hudHiddenByToggleEvent,
        hasWidgetSignal,
        widgetHidden,
        sourceCode,
        finalHidden);

    *hiddenOut = finalHidden;
    return true;
}

enum EscMenuCaptionMask
{
    EscMenuCaptionMask_None = 0,
    EscMenuCaptionMask_Resume = 1 << 0,
    EscMenuCaptionMask_SaveGame = 1 << 1,
    EscMenuCaptionMask_LoadGame = 1 << 2,
    EscMenuCaptionMask_Options = 1 << 3,
    EscMenuCaptionMask_Exit = 1 << 4,
    EscMenuCaptionMask_NewGame = 1 << 5,
    EscMenuCaptionMask_MainMenu = 1 << 6,
    EscMenuCaptionMask_All =
        EscMenuCaptionMask_Resume
        | EscMenuCaptionMask_SaveGame
        | EscMenuCaptionMask_LoadGame
        | EscMenuCaptionMask_Options
        | EscMenuCaptionMask_Exit
        | EscMenuCaptionMask_NewGame
        | EscMenuCaptionMask_MainMenu
};

static int CountSetBits32(unsigned int value)
{
    int count = 0;
    while (value != 0)
    {
        count += (value & 1u) ? 1 : 0;
        value >>= 1;
    }
    return count;
}

enum DialogueUiSignalMask
{
    DialogueUiSignalMask_None = 0,
    DialogueUiSignalMask_PlayerPanel = 1 << 0,
    DialogueUiSignalMask_NpcPanel = 1 << 1,
    DialogueUiSignalMask_PlayerDialog = 1 << 2,
    DialogueUiSignalMask_NpcDialog = 1 << 3,
    DialogueUiSignalMask_SpeechText = 1 << 4,
    DialogueUiSignalMask_ConversationRoot = 1 << 5
};

static std::string NormalizeAsciiAlphaNumericUpper(const std::string& input)
{
    std::string normalized;
    normalized.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i)
    {
        const unsigned char ch = static_cast<unsigned char>(input[i]);
        if (std::isalnum(ch) == 0)
        {
            continue;
        }
        normalized.push_back(static_cast<char>(std::toupper(ch)));
    }
    return normalized;
}

static unsigned int BuildDialogueUiSignalMaskFromWidgetName(const std::string& widgetName)
{
    if (widgetName.empty())
    {
        return DialogueUiSignalMask_None;
    }

    const std::string normalized = NormalizeAsciiAlphaNumericUpper(widgetName);

    unsigned int mask = DialogueUiSignalMask_None;
    if (normalized.find("PLAYERPANEL") != std::string::npos)
    {
        mask |= DialogueUiSignalMask_PlayerPanel;
    }
    if (normalized.find("NPCPANEL") != std::string::npos)
    {
        mask |= DialogueUiSignalMask_NpcPanel;
    }
    if (normalized.find("PLAYERDIALOG") != std::string::npos)
    {
        mask |= DialogueUiSignalMask_PlayerDialog;
    }
    if (normalized.find("NPCDIALOG") != std::string::npos)
    {
        mask |= DialogueUiSignalMask_NpcDialog;
    }
    if (normalized.find("SPEECHTEXT") != std::string::npos)
    {
        mask |= DialogueUiSignalMask_SpeechText;
    }
    if (normalized.find("CONVERSATIONPANEL") != std::string::npos)
    {
        mask |= DialogueUiSignalMask_ConversationRoot;
    }

    return mask;
}

static void AccumulateDialogueUiSignalMaskFromWidgetTree(MyGUI::Widget* widget, unsigned int* maskOut)
{
    if (!widget || !maskOut)
    {
        return;
    }

    if (!widget->getInheritedVisible())
    {
        return;
    }

    *maskOut |= BuildDialogueUiSignalMaskFromWidgetName(widget->getName());

    const size_t childCount = widget->getChildCount();
    for (size_t i = 0; i < childCount; ++i)
    {
        AccumulateDialogueUiSignalMaskFromWidgetTree(widget->getChildAt(i), maskOut);
    }
}

static std::string BuildDialogueUiSignalMaskSummary(unsigned int mask)
{
    if (mask == DialogueUiSignalMask_None)
    {
        return "none";
    }

    std::stringstream summary;
    bool wroteAny = false;
    if ((mask & DialogueUiSignalMask_PlayerPanel) != 0)
    {
        summary << "player_panel";
        wroteAny = true;
    }
    if ((mask & DialogueUiSignalMask_NpcPanel) != 0)
    {
        if (wroteAny)
        {
            summary << ",";
        }
        summary << "npc_panel";
        wroteAny = true;
    }
    if ((mask & DialogueUiSignalMask_PlayerDialog) != 0)
    {
        if (wroteAny)
        {
            summary << ",";
        }
        summary << "player_dialog";
        wroteAny = true;
    }
    if ((mask & DialogueUiSignalMask_NpcDialog) != 0)
    {
        if (wroteAny)
        {
            summary << ",";
        }
        summary << "npc_dialog";
        wroteAny = true;
    }
    if ((mask & DialogueUiSignalMask_SpeechText) != 0)
    {
        if (wroteAny)
        {
            summary << ",";
        }
        summary << "speech_text";
        wroteAny = true;
    }
    if ((mask & DialogueUiSignalMask_ConversationRoot) != 0)
    {
        if (wroteAny)
        {
            summary << ",";
        }
        summary << "conversation_root";
    }

    return summary.str();
}

static bool TryDetectDialogueWindowOpenState(bool* openOut, unsigned int* signalMaskOut)
{
    if (!openOut)
    {
        return false;
    }

    *openOut = false;
    if (signalMaskOut)
    {
        *signalMaskOut = DialogueUiSignalMask_None;
    }

    const DWORD nowMs = GetTickCount();
    static DWORD s_lastScanMs = 0;
    static bool s_hasLastScanResult = false;
    static bool s_lastScanDialogueVisible = false;
    static unsigned int s_lastScanSignalMask = DialogueUiSignalMask_None;
    // Keep dialogue probe latency near frame-time to avoid brief panel overlap
    // while the conversation window is appearing.
    if (s_hasLastScanResult && s_lastScanMs != 0 && !DebounceWindowElapsed(nowMs, s_lastScanMs, 16))
    {
        *openOut = s_lastScanDialogueVisible;
        if (signalMaskOut)
        {
            *signalMaskOut = s_lastScanSignalMask;
        }
        return true;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (!gui)
    {
        s_lastScanMs = nowMs;
        s_hasLastScanResult = true;
        s_lastScanDialogueVisible = false;
        s_lastScanSignalMask = DialogueUiSignalMask_None;
        return true;
    }

    unsigned int signalMask = DialogueUiSignalMask_None;
    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        AccumulateDialogueUiSignalMaskFromWidgetTree(roots.current(), &signalMask);
        const unsigned int conversationBits =
            DialogueUiSignalMask_PlayerPanel
            | DialogueUiSignalMask_NpcPanel
            | DialogueUiSignalMask_PlayerDialog
            | DialogueUiSignalMask_NpcDialog;
        if ((signalMask & conversationBits) == conversationBits)
        {
            break;
        }
    }

    const unsigned int conversationMask =
        signalMask
        & (DialogueUiSignalMask_PlayerPanel
            | DialogueUiSignalMask_NpcPanel
            | DialogueUiSignalMask_PlayerDialog
            | DialogueUiSignalMask_NpcDialog);
    const int conversationBitCount = CountSetBits32(conversationMask);
    const bool hasConversationRoot = (signalMask & DialogueUiSignalMask_ConversationRoot) != 0;
    const bool dialogueVisible =
        conversationBitCount >= 2
        || (signalMask & DialogueUiSignalMask_PlayerDialog) != 0
        || (signalMask & DialogueUiSignalMask_NpcDialog) != 0
        || (hasConversationRoot && ((signalMask & DialogueUiSignalMask_SpeechText) != 0 || conversationBitCount > 0));

    s_lastScanMs = nowMs;
    s_hasLastScanResult = true;
    s_lastScanDialogueVisible = dialogueVisible;
    s_lastScanSignalMask = signalMask;

    static bool s_hasLoggedState = false;
    static bool s_lastLoggedVisible = false;
    static unsigned int s_lastLoggedMask = DialogueUiSignalMask_None;
    if (g_config.debugLogTransitions
        && (!s_hasLoggedState || s_lastLoggedVisible != dialogueVisible || s_lastLoggedMask != signalMask))
    {
        std::stringstream info;
        info << "Job-B-Gone DEBUG: dialogue_window_probe"
             << " open=" << (dialogueVisible ? "true" : "false")
             << " signals=" << BuildDialogueUiSignalMaskSummary(signalMask);
        DebugLog(info.str().c_str());
        s_hasLoggedState = true;
        s_lastLoggedVisible = dialogueVisible;
        s_lastLoggedMask = signalMask;
    }

    *openOut = dialogueVisible;
    if (signalMaskOut)
    {
        *signalMaskOut = signalMask;
    }
    return true;
}

static unsigned int BuildEscMenuCaptionMaskFromCaption(const std::string& caption)
{
    if (caption.empty())
    {
        return EscMenuCaptionMask_None;
    }

    std::string normalized;
    normalized.reserve(caption.size());
    for (size_t i = 0; i < caption.size(); ++i)
    {
        const unsigned char ch = static_cast<unsigned char>(caption[i]);
        if (std::isalnum(ch) == 0)
        {
            continue;
        }
        normalized.push_back(static_cast<char>(std::toupper(ch)));
    }

    if (normalized == "RESUME")
    {
        return EscMenuCaptionMask_Resume;
    }
    if (normalized == "SAVEGAME")
    {
        return EscMenuCaptionMask_SaveGame;
    }
    if (normalized == "LOADGAME")
    {
        return EscMenuCaptionMask_LoadGame;
    }
    if (normalized == "OPTIONS")
    {
        return EscMenuCaptionMask_Options;
    }
    if (normalized == "EXIT")
    {
        return EscMenuCaptionMask_Exit;
    }
    if (normalized == "NEWGAME")
    {
        return EscMenuCaptionMask_NewGame;
    }
    if (normalized == "MAINMENU")
    {
        return EscMenuCaptionMask_MainMenu;
    }

    return EscMenuCaptionMask_None;
}

static void AccumulateEscMenuCaptionMaskFromWidgetTree(MyGUI::Widget* widget, unsigned int* maskOut)
{
    if (!widget || !maskOut)
    {
        return;
    }

    if ((widget->getVisible() == false) || ((*maskOut & EscMenuCaptionMask_All) == EscMenuCaptionMask_All))
    {
        return;
    }

    MyGUI::Button* button = widget->castType<MyGUI::Button>(false);
    if (button && button->getInheritedVisible())
    {
        const std::string caption = TrimAscii(button->getCaption().asUTF8());
        *maskOut |= BuildEscMenuCaptionMaskFromCaption(caption);
    }

    const size_t childCount = widget->getChildCount();
    for (size_t i = 0; i < childCount; ++i)
    {
        AccumulateEscMenuCaptionMaskFromWidgetTree(widget->getChildAt(i), maskOut);
    }
}

static bool TryDetectEscMenuOpenState(bool* openOut)
{
    if (!openOut)
    {
        return false;
    }

    *openOut = false;
    const DWORD nowMs = GetTickCount();
    static DWORD s_lastScanMs = 0;
    static bool s_hasLastScanResult = false;
    static bool s_lastScanMenuVisible = false;
    if (s_hasLastScanResult && s_lastScanMs != 0 && !DebounceWindowElapsed(nowMs, s_lastScanMs, 80))
    {
        *openOut = s_lastScanMenuVisible;
        return true;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (!gui)
    {
        s_lastScanMs = nowMs;
        s_hasLastScanResult = true;
        s_lastScanMenuVisible = false;
        return true;
    }

    unsigned int captionMask = EscMenuCaptionMask_None;
    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        AccumulateEscMenuCaptionMaskFromWidgetTree(roots.current(), &captionMask);
        if ((captionMask & EscMenuCaptionMask_All) == EscMenuCaptionMask_All)
        {
            break;
        }
    }

    // Require several canonical pause-menu captions to avoid normal gameplay false positives.
    const bool menuVisible = CountSetBits32(captionMask) >= 3;
    s_lastScanMs = nowMs;
    s_hasLastScanResult = true;
    s_lastScanMenuVisible = menuVisible;
    *openOut = menuVisible;
    return true;
}

static bool TryGetMenuOpenState(bool* openOut)
{
    if (!openOut)
    {
        return false;
    }

    *openOut = false;
    __try
    {
        SaveManager* saveManager = SaveManager::getSingleton();
        if (saveManager && saveManager->isVisible() != 0)
        {
            *openOut = true;
            return true;
        }

        bool escMenuOpen = false;
        TryDetectEscMenuOpenState(&escMenuOpen);

        TitleScreen* titleScreen = TitleScreen::getSingleton();
        if (titleScreen && titleScreen->isVisible())
        {
            *openOut = true;
            return true;
        }

        *openOut = escMenuOpen;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryGetDialogueConversationTargetNoexcept(Character* member, Character** targetOut)
{
    if (!member || !targetOut)
    {
        return false;
    }

    *targetOut = 0;
    __try
    {
        Dialogue* dialogue = member->dialogue;
        if (!dialogue)
        {
            return false;
        }

        const hand conversationTargetHandle = dialogue->getConversationTarget();
        if (!conversationTargetHandle || !conversationTargetHandle.isValid())
        {
            return false;
        }

        Character* target = conversationTargetHandle.getCharacter();
        if (!target)
        {
            return false;
        }

        *targetOut = target;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryGetDialogueActiveNoexcept(Character* member, bool* activeOut)
{
    if (!member || !activeOut)
    {
        return false;
    }

    *activeOut = false;
    __try
    {
        Dialogue* dialogue = member->dialogue;
        if (!dialogue)
        {
            return true;
        }

        *activeOut = !dialogue->conversationHasEndedPrettyMuch();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryResolveCharacterInventoryVisible(Character* character, bool* visibleOut)
{
    if (!visibleOut)
    {
        return false;
    }

    *visibleOut = false;
    if (!character)
    {
        return true;
    }

    __try
    {
        if (character->inventory && character->inventory->isVisible())
        {
            *visibleOut = true;
            return true;
        }

        *visibleOut = character->isInventoryVisible();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void CollectUiSuppressionCharacters(Character* selectedMember, std::vector<Character*>* membersOut)
{
    if (!membersOut)
    {
        return;
    }

    TryCollectSelectedCharactersForDisplay(membersOut);
    AddUniqueCharacterForSelection(membersOut, selectedMember);

    if (!g_lastPlayerInterface)
    {
        return;
    }

    __try
    {
        AddUniqueCharacterForSelection(membersOut, g_lastPlayerInterface->selectedObject.getCharacter());
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }

    const size_t initialCount = membersOut->size();
    for (size_t i = 0; i < initialCount; ++i)
    {
        Character* conversationTarget = 0;
        if (TryGetDialogueConversationTargetNoexcept((*membersOut)[i], &conversationTarget))
        {
            AddUniqueCharacterForSelection(membersOut, conversationTarget);
        }
    }
}

static int DetectMemberUiSuppressionReasonMask(Character* member)
{
    if (!member)
    {
        return PanelUiSuppressionReason_None;
    }

    int reasonMask = PanelUiSuppressionReason_None;
    __try
    {
        bool memberInventoryVisible = false;
        TryResolveCharacterInventoryVisible(member, &memberInventoryVisible);
        bool memberDialogueActive = false;
        TryGetDialogueActiveNoexcept(member, &memberDialogueActive);

        Character* conversationTarget = 0;
        const bool hasConversationTarget = TryGetDialogueConversationTargetNoexcept(member, &conversationTarget);
        bool targetInventoryVisible = false;
        bool targetEngaged = false;
        bool targetDialogueActive = false;
        if (conversationTarget)
        {
            TryResolveCharacterInventoryVisible(conversationTarget, &targetInventoryVisible);
            targetEngaged = conversationTarget->_isEngagedWithAPlayer;
            TryGetDialogueActiveNoexcept(conversationTarget, &targetDialogueActive);
        }

        const bool memberEngaged = member->_isEngagedWithAPlayer;
        const bool hasEngagedSignal = memberEngaged || targetEngaged;
        const bool hasDialogueSignal = memberDialogueActive || targetDialogueActive;
        const bool dialogueOrInteractionActive =
            hasDialogueSignal
            || (hasConversationTarget && hasEngagedSignal);
        const bool tradeLikeInteractionActive =
            hasEngagedSignal
            && (memberInventoryVisible || targetInventoryVisible);

        if (g_config.hidePanelDuringInventoryOpen && (memberInventoryVisible || targetInventoryVisible))
        {
            reasonMask |= PanelUiSuppressionReason_InventoryOpen;
        }
        if (g_config.hidePanelDuringCharacterInteraction && (dialogueOrInteractionActive || tradeLikeInteractionActive))
        {
            reasonMask |= PanelUiSuppressionReason_CharacterInteraction;
        }

        MaybeLogMemberUiSuppressionProbe(
            member,
            reasonMask,
            memberEngaged,
            memberDialogueActive,
            hasConversationTarget,
            targetEngaged,
            targetDialogueActive,
            memberInventoryVisible,
            targetInventoryVisible);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return PanelUiSuppressionReason_None;
    }

    return reasonMask;
}

static int DetectPanelUiSuppressionReasonMask(Character* selectedMember)
{
    int reasonMask = PanelUiSuppressionReason_None;
    int selectedMemberCount = 0;

    bool dialogueWindowOpen = false;
    if (g_config.hidePanelDuringCharacterInteraction
        && TryDetectDialogueWindowOpenState(&dialogueWindowOpen, 0)
        && dialogueWindowOpen)
    {
        reasonMask |= PanelUiSuppressionReason_CharacterInteraction;
    }

    bool menuOpen = false;
    if (TryGetMenuOpenState(&menuOpen) && menuOpen)
    {
        reasonMask |= PanelUiSuppressionReason_MenuOpen;
    }

    bool hudHidden = false;
    if (TryGetHudHiddenState(&hudHidden) && hudHidden)
    {
        reasonMask |= PanelUiSuppressionReason_HudHidden;
    }

    bool characterCreationModeActive = false;
    if (g_config.hidePanelDuringCharacterCreation
        && TryGetCharacterCreationModeActive(&characterCreationModeActive)
        && characterCreationModeActive)
    {
        reasonMask |= PanelUiSuppressionReason_CharacterCreation;
    }

    std::vector<Character*> selectedMembers;
    CollectUiSuppressionCharacters(selectedMember, &selectedMembers);
    selectedMemberCount = static_cast<int>(selectedMembers.size());
    if (selectedMembers.empty())
    {
        LogPanelUiSuppressionSources(
            dialogueWindowOpen,
            menuOpen,
            hudHidden,
            characterCreationModeActive,
            selectedMemberCount,
            PanelUiSuppressionReason_None,
            reasonMask);
        return reasonMask;
    }

    int desiredSuppressionMask = PanelUiSuppressionReason_None;
    if (g_config.hidePanelDuringInventoryOpen)
    {
        desiredSuppressionMask |= PanelUiSuppressionReason_InventoryOpen;
    }
    if (g_config.hidePanelDuringCharacterInteraction)
    {
        desiredSuppressionMask |= PanelUiSuppressionReason_CharacterInteraction;
    }

    for (size_t i = 0; i < selectedMembers.size(); ++i)
    {
        Character* member = selectedMembers[i];
        if (!member)
        {
            continue;
        }

        reasonMask |= DetectMemberUiSuppressionReasonMask(member);

        if (desiredSuppressionMask != PanelUiSuppressionReason_None
            && (reasonMask & desiredSuppressionMask) == desiredSuppressionMask)
        {
            break;
        }
    }

    LogPanelUiSuppressionSources(
        dialogueWindowOpen,
        menuOpen,
        hudHidden,
        characterCreationModeActive,
        selectedMemberCount,
        desiredSuppressionMask,
        reasonMask);
    return reasonMask;
}

static std::string BuildPanelUiSuppressionReasonSummary(int reasonMask)
{
    if (reasonMask == PanelUiSuppressionReason_None)
    {
        return "none";
    }

    std::stringstream summary;
    bool wroteAny = false;
    if ((reasonMask & PanelUiSuppressionReason_CharacterCreation) != 0)
    {
        summary << "character_creation";
        wroteAny = true;
    }
    if ((reasonMask & PanelUiSuppressionReason_InventoryOpen) != 0)
    {
        if (wroteAny)
        {
            summary << ",";
        }
        summary << "inventory_open";
        wroteAny = true;
    }
    if ((reasonMask & PanelUiSuppressionReason_CharacterInteraction) != 0)
    {
        if (wroteAny)
        {
            summary << ",";
        }
        summary << "character_interaction";
        wroteAny = true;
    }
    if ((reasonMask & PanelUiSuppressionReason_MenuOpen) != 0)
    {
        if (wroteAny)
        {
            summary << ",";
        }
        summary << "menu_open";
        wroteAny = true;
    }
    if ((reasonMask & PanelUiSuppressionReason_HudHidden) != 0)
    {
        if (wroteAny)
        {
            summary << ",";
        }
        summary << "hud_hidden";
    }

    return summary.str();
}

static std::string BuildPanelVisibilityReasonSummary(int reasonMask, bool hiddenByToggle)
{
    const std::string uiSummary = BuildPanelUiSuppressionReasonSummary(reasonMask);
    if (!hiddenByToggle)
    {
        return uiSummary;
    }

    if (uiSummary == "none")
    {
        return "toggle_hidden";
    }

    return uiSummary + ",toggle_hidden";
}

static void BuildDisplayedJobRowsForSelection(
    Character* selectedMember,
    std::vector<JobRowModel>* rowsOut,
    int* selectedMemberCountOut)
{
    if (rowsOut)
    {
        rowsOut->clear();
    }
    if (selectedMemberCountOut)
    {
        *selectedMemberCountOut = 0;
    }
    if (!rowsOut)
    {
        return;
    }

    std::vector<Character*> selectedMembers;
    TryCollectSelectedCharactersForDisplay(&selectedMembers);

    if (selectedMembers.empty())
    {
        AddUniqueCharacterForSelection(&selectedMembers, selectedMember);
    }

    if (selectedMemberCountOut)
    {
        *selectedMemberCountOut = static_cast<int>(selectedMembers.size());
    }

    if (selectedMembers.empty())
    {
        return;
    }

    if (selectedMembers.size() == 1)
    {
        const char* snapshotResult = "not_run";
        if (!BuildSelectedMemberJobSnapshot(selectedMembers[0], rowsOut, &snapshotResult))
        {
            rowsOut->clear();
        }
        return;
    }

    for (size_t memberIndex = 0; memberIndex < selectedMembers.size(); ++memberIndex)
    {
        Character* member = selectedMembers[memberIndex];
        if (!member)
        {
            continue;
        }

        std::vector<JobRowModel> memberRows;
        const char* snapshotResult = "not_run";
        if (!BuildSelectedMemberJobSnapshot(member, &memberRows, &snapshotResult))
        {
            continue;
        }

        const size_t rowCount = memberRows.size();
        for (size_t rowIndex = 0; rowIndex < rowCount; ++rowIndex)
        {
            const JobRowModel& row = memberRows[rowIndex];
            if (HasJobSignature(*rowsOut, row.taskType, row.taskName))
            {
                continue;
            }
            rowsOut->push_back(row);
        }
    }
}

static int GetCollapsedHeaderYOffset()
{
    return GetExpandedPanelHeight() - kPanelCollapsedHeight;
}

static bool TryGetMousePosition(int* xOut, int* yOut)
{
    if (!xOut || !yOut)
    {
        return false;
    }

    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (!inputManager)
    {
        return false;
    }

    const MyGUI::IntPoint mouse = inputManager->getMousePosition();
    *xOut = mouse.left;
    *yOut = mouse.top;
    return true;
}

static int ClampIntValue(int value, int minValue, int maxValue)
{
    if (value < minValue)
    {
        return minValue;
    }
    if (value > maxValue)
    {
        return maxValue;
    }
    return value;
}

static MyGUI::IntCoord BuildPanelCoordFromAnchor(int left, int top)
{
    const int height = g_jobBGonePanelCollapsed ? kPanelCollapsedHeight : GetExpandedPanelHeight();
    return MyGUI::IntCoord(left, top, kPanelWidth, height);
}

static MyGUI::IntCoord ClampPanelCoordToViewport(const MyGUI::IntCoord& inputCoord)
{
    int left = inputCoord.left;
    int top = inputCoord.top;
    const int width = (inputCoord.width > 0) ? inputCoord.width : kPanelWidth;
    const int height = (inputCoord.height > 0) ? inputCoord.height : GetExpandedPanelHeight();

    int viewWidth = 0;
    int viewHeight = 0;
    if (!TryGetViewportSize(&viewWidth, &viewHeight))
    {
        if (left < kPanelViewportPadding)
        {
            left = kPanelViewportPadding;
        }
        if (top < kPanelViewportPadding)
        {
            top = kPanelViewportPadding;
        }
        return MyGUI::IntCoord(left, top, width, height);
    }

    int minLeft = kPanelViewportPadding;
    int minTop = kPanelViewportPadding;
    int maxLeft = viewWidth - width - kPanelViewportPadding;
    int maxTop = viewHeight - height - kPanelViewportPadding;

    if (maxLeft < minLeft)
    {
        minLeft = 0;
        maxLeft = viewWidth - width;
        if (maxLeft < minLeft)
        {
            maxLeft = minLeft;
        }
    }

    if (maxTop < minTop)
    {
        minTop = 0;
        maxTop = viewHeight - height;
        if (maxTop < minTop)
        {
            maxTop = minTop;
        }
    }

    left = ClampIntValue(left, minLeft, maxLeft);
    top = ClampIntValue(top, minTop, maxTop);
    return MyGUI::IntCoord(left, top, width, height);
}

static MyGUI::IntCoord GetFallbackButtonCoord()
{
    int left = 1200;
    int top = 760;

    int viewWidth = 0;
    int viewHeight = 0;
    if (TryGetViewportSize(&viewWidth, &viewHeight))
    {
        const int height = g_jobBGonePanelCollapsed ? kPanelCollapsedHeight : GetExpandedPanelHeight();
        // Anchor above the squad portraits panel (right-side roster block).
        left = viewWidth - kPanelWidth - 700;
        top = viewHeight - height - 250;
    }

    return ClampPanelCoordToViewport(BuildPanelCoordFromAnchor(left, top));
}

static MyGUI::IntCoord ResolvePanelCoordFromConfig()
{
    if (g_runtimePanelHasCustomPosition)
    {
        int top = g_runtimePanelPosY;
        if (g_jobBGonePanelCollapsed)
        {
            top += GetCollapsedHeaderYOffset();
        }
        return ClampPanelCoordToViewport(
            BuildPanelCoordFromAnchor(g_runtimePanelPosX, top));
    }

    if (g_config.jobBGonePanelHasCustomPosition)
    {
        int top = g_config.jobBGonePanelPosY;
        if (g_jobBGonePanelCollapsed)
        {
            top += GetCollapsedHeaderYOffset();
        }
        return ClampPanelCoordToViewport(
            BuildPanelCoordFromAnchor(g_config.jobBGonePanelPosX, top));
    }
    return GetFallbackButtonCoord();
}

static void ApplyPanelLayout(const MyGUI::IntCoord& panelCoord)
{
    if (!g_jobBGonePanel || !g_jobBGoneHeaderButton || !g_jobBGoneBodyFrame || !g_jobBGoneStatusText
        || !g_jobBGoneEmptyStateText
        || !g_deleteAllJobsTitleText || !g_deleteAllJobsSelectedMemberButton || !g_deleteAllJobsSelectedMembersButton
        || !g_deleteAllJobsWholeSquadButton || !g_deleteAllJobsEveryoneButton
        || !g_jobBGoneConfirmOverlay || !g_jobBGoneConfirmTitleText || !g_jobBGoneConfirmTitleTextBold || !g_jobBGoneConfirmBodyText
        || !g_jobBGoneConfirmYesButton || !g_jobBGoneConfirmNoButton)
    {
        return;
    }

    g_jobBGonePanel->setCoord(panelCoord);
    g_jobBGoneHeaderButton->setCoord(MyGUI::IntCoord(0, 0, panelCoord.width, 38));
    g_jobBGoneBodyFrame->setCoord(MyGUI::IntCoord(0, 40, panelCoord.width, panelCoord.height - 40));
    g_jobBGoneStatusText->setCoord(MyGUI::IntCoord(14, 50, panelCoord.width - 28, 22));
    g_jobBGoneEmptyStateText->setCoord(MyGUI::IntCoord(14, 122, panelCoord.width - 28, 24));
    if (g_jobBGoneHoverHintText)
    {
        g_jobBGoneHoverHintText->setCoord(MyGUI::IntCoord(14, panelCoord.height - 30, panelCoord.width - 28, 18));
    }

    const int overlayWidth = 460;
    const int overlayHeight = 210;
    int overlayLeft = (panelCoord.width - overlayWidth) / 2;
    int overlayTop = 114;
    if (overlayLeft < 8)
    {
        overlayLeft = 8;
    }
    if (overlayTop + overlayHeight > panelCoord.height - 8)
    {
        overlayTop = panelCoord.height - overlayHeight - 8;
    }
    if (overlayTop < 48)
    {
        overlayTop = 48;
    }

    g_jobBGoneConfirmOverlay->setCoord(MyGUI::IntCoord(overlayLeft, overlayTop, overlayWidth, overlayHeight));
    g_jobBGoneConfirmTitleText->setCoord(MyGUI::IntCoord(12, 10, overlayWidth - 24, 24));
    g_jobBGoneConfirmTitleTextBold->setCoord(MyGUI::IntCoord(13, 10, overlayWidth - 24, 24));
    g_jobBGoneConfirmBodyText->setCoord(MyGUI::IntCoord(12, 40, overlayWidth - 24, overlayHeight - 86));
    const int confirmButtonWidth = 110;
    const int confirmButtonHeight = 30;
    const int confirmButtonsTop = overlayHeight - confirmButtonHeight - 12;
    g_jobBGoneConfirmYesButton->setCoord(
        MyGUI::IntCoord(overlayWidth - (confirmButtonWidth * 2) - 18, confirmButtonsTop, confirmButtonWidth, confirmButtonHeight));
    g_jobBGoneConfirmNoButton->setCoord(
        MyGUI::IntCoord(overlayWidth - confirmButtonWidth - 12, confirmButtonsTop, confirmButtonWidth, confirmButtonHeight));

    const int meButtonWidth = kScopeButtonMeWidth;
    const int selectedButtonWidth = kScopeButtonSelectedWidth;
    const int squadButtonWidth = kScopeButtonSquadWidth;
    const int allButtonWidth = kScopeButtonAllSquadsWidth;
    const int buttonGap = kScopeButtonGap;
    const int buttonHeight = kScopeButtonHeight;
    const int rowScrollControlsWidth = (g_jobRowsScrollUpButton && g_jobRowsScrollDownButton)
        ? (kJobRowsScrollButtonWidth + 6)
        : 0;
    const int allButtonsWidth = meButtonWidth + selectedButtonWidth + squadButtonWidth + allButtonWidth + (buttonGap * 3);
    int topTitleWidth = panelCoord.width - 28 - allButtonsWidth - 8 - rowScrollControlsWidth;
    if (topTitleWidth < 90)
    {
        topTitleWidth = 90;
    }
    const int topButtonsLeft = 14 + topTitleWidth + 8;

    g_deleteAllJobsTitleText->setCoord(MyGUI::IntCoord(14, 80, topTitleWidth, buttonHeight));
    g_deleteAllJobsSelectedMemberButton->setCoord(MyGUI::IntCoord(topButtonsLeft, 80, meButtonWidth, buttonHeight));
    g_deleteAllJobsSelectedMembersButton->setCoord(
        MyGUI::IntCoord(topButtonsLeft + meButtonWidth + buttonGap, 80, selectedButtonWidth, buttonHeight));
    g_deleteAllJobsWholeSquadButton->setCoord(
        MyGUI::IntCoord(
            topButtonsLeft + meButtonWidth + selectedButtonWidth + (buttonGap * 2),
            80,
            squadButtonWidth,
            buttonHeight));
    g_deleteAllJobsEveryoneButton->setCoord(
        MyGUI::IntCoord(
            topButtonsLeft + meButtonWidth + selectedButtonWidth + squadButtonWidth + (buttonGap * 3),
            80,
            allButtonWidth,
            buttonHeight));

    int labelWidth = panelCoord.width - 28 - allButtonsWidth - 8 - rowScrollControlsWidth;
    if (labelWidth < 90)
    {
        labelWidth = 90;
    }

    const size_t rowCount = g_jobRowWidgets.size();
    for (size_t i = 0; i < rowCount; ++i)
    {
        JobRowWidgets& rowWidgets = g_jobRowWidgets[i];
        if (!rowWidgets.label || !rowWidgets.deleteSelectedMemberButton || !rowWidgets.deleteSelectedMembersButton
            || !rowWidgets.deleteWholeSquadButton || !rowWidgets.deleteEveryoneButton)
        {
            continue;
        }

        const int top = GetJobRowTop(static_cast<int>(i));
        const int buttonsLeft = 14 + labelWidth + 8;
        rowWidgets.label->setCoord(MyGUI::IntCoord(14, top, labelWidth, buttonHeight));
        rowWidgets.deleteSelectedMemberButton->setCoord(
            MyGUI::IntCoord(buttonsLeft, top, meButtonWidth, buttonHeight));
        rowWidgets.deleteSelectedMembersButton->setCoord(
            MyGUI::IntCoord(buttonsLeft + meButtonWidth + buttonGap, top, selectedButtonWidth, buttonHeight));
        rowWidgets.deleteWholeSquadButton->setCoord(
            MyGUI::IntCoord(buttonsLeft + meButtonWidth + selectedButtonWidth + (buttonGap * 2), top, squadButtonWidth, buttonHeight));
        rowWidgets.deleteEveryoneButton->setCoord(
            MyGUI::IntCoord(
                buttonsLeft + meButtonWidth + selectedButtonWidth + squadButtonWidth + (buttonGap * 3),
                top,
                allButtonWidth,
                buttonHeight));
    }

    if (g_jobRowsScrollUpButton && g_jobRowsScrollDownButton)
    {
        const int scrollButtonLeft = panelCoord.width - kJobRowsScrollButtonWidth - 8;
        const int upTop = kJobRowsTop;
        int downTop = panelCoord.height - kJobRowsBottomPadding - kJobRowsScrollButtonHeight;
        if (downTop < upTop + kJobRowsScrollButtonHeight + 4)
        {
            downTop = upTop + kJobRowsScrollButtonHeight + 4;
        }

        g_jobRowsScrollUpButton->setCoord(
            MyGUI::IntCoord(scrollButtonLeft, upTop, kJobRowsScrollButtonWidth, kJobRowsScrollButtonHeight));
        g_jobRowsScrollDownButton->setCoord(
            MyGUI::IntCoord(scrollButtonLeft, downTop, kJobRowsScrollButtonWidth, kJobRowsScrollButtonHeight));
    }
}

static void PersistPanelPositionIfChanged(const MyGUI::IntCoord& panelCoord, const char* source)
{
    int storedTop = panelCoord.top;
    if (g_jobBGonePanelCollapsed)
    {
        storedTop -= GetCollapsedHeaderYOffset();
    }
    if (storedTop < 0)
    {
        storedTop = 0;
    }

    if (g_config.jobBGonePanelHasCustomPosition && g_config.jobBGonePanelPosX == panelCoord.left
        && g_config.jobBGonePanelPosY == storedTop)
    {
        return;
    }

    g_config.jobBGonePanelHasCustomPosition = true;
    g_config.jobBGonePanelPosX = panelCoord.left;
    g_config.jobBGonePanelPosY = storedTop;
    g_runtimePanelHasCustomPosition = true;
    g_runtimePanelPosX = panelCoord.left;
    g_runtimePanelPosY = storedTop;
    if (!SaveConfigState())
    {
        ErrorLog("Job-B-Gone WARN: failed to persist Job-B-Gone panel position");
        return;
    }

    if (g_config.debugLogTransitions)
    {
        std::stringstream info;
        info << "Job-B-Gone INFO: persisted_panel_position source=" << (source ? source : "unknown")
             << " x=" << panelCoord.left
             << " y=" << storedTop;
        DebugLog(info.str().c_str());
    }
}

static void PersistPanelCollapsedStateIfChanged(const char* source)
{
    if (g_config.jobBGonePanelCollapsed == g_jobBGonePanelCollapsed)
    {
        return;
    }

    g_config.jobBGonePanelCollapsed = g_jobBGonePanelCollapsed;
    if (!SaveConfigState())
    {
        ErrorLog("Job-B-Gone WARN: failed to persist Job-B-Gone panel collapsed state");
        return;
    }

    if (g_config.debugLogTransitions)
    {
        std::stringstream info;
        info << "Job-B-Gone INFO: persisted_panel_collapsed_state source=" << (source ? source : "unknown")
             << " collapsed=" << (g_jobBGonePanelCollapsed ? "true" : "false");
        DebugLog(info.str().c_str());
    }
}

static void MoveJobBGonePanelByDelta(int deltaX, int deltaY)
{
    if (!g_jobBGonePanel || (deltaX == 0 && deltaY == 0))
    {
        return;
    }

    const int moveX = (deltaX < 0) ? -deltaX : deltaX;
    const int moveY = (deltaY < 0) ? -deltaY : deltaY;
    g_jobBGonePanelDragMovedDistance += moveX + moveY;
    if (g_jobBGonePanelDragMovedDistance >= 3)
    {
        g_jobBGonePanelDragMoved = true;
    }

    const MyGUI::IntCoord currentCoord = g_jobBGonePanel->getCoord();
    const MyGUI::IntCoord movedCoord = ClampPanelCoordToViewport(
        BuildPanelCoordFromAnchor(currentCoord.left + deltaX, currentCoord.top + deltaY));
    ApplyPanelLayout(movedCoord);
}

static void FinalizeJobBGonePanelDrag(const char* source)
{
    if (!g_jobBGonePanelDragging)
    {
        return;
    }

    g_jobBGonePanelDragging = false;
    if (!g_jobBGonePanel)
    {
        g_jobBGonePanelDragMovedDistance = 0;
        return;
    }

    const MyGUI::IntCoord clampedCoord = ClampPanelCoordToViewport(g_jobBGonePanel->getCoord());
    ApplyPanelLayout(clampedCoord);
    if (g_jobBGonePanelDragMoved)
    {
        PersistPanelPositionIfChanged(clampedCoord, source ? source : "drag_end");
    }

    g_jobBGonePanelDragMovedDistance = 0;
    g_lastLoggedConfirmOverlayVisible = false;
}

static void DestroySelectedMemberJobPanelButton()
{
    if (g_jobBGonePanel)
    {
        // Save moved position even if UI is being torn down during load transition.
        const MyGUI::IntCoord currentCoord = ClampPanelCoordToViewport(g_jobBGonePanel->getCoord());
        const MyGUI::IntCoord expectedCoord = ResolvePanelCoordFromConfig();
        if (g_jobBGonePanelDragging || g_jobBGonePanelDragMoved
            || currentCoord.left != expectedCoord.left
            || currentCoord.top != expectedCoord.top)
        {
            PersistPanelPositionIfChanged(currentCoord, "panel_destroy");
        }
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui && g_jobBGoneConfirmOverlay)
    {
        gui->destroyWidget(g_jobBGoneConfirmOverlay);
    }
    if (gui && g_jobBGonePanel)
    {
        gui->destroyWidget(g_jobBGonePanel);
    }

    g_jobBGonePanel = 0;
    g_jobBGoneHeaderButton = 0;
    g_jobBGoneBodyFrame = 0;
    g_jobBGoneStatusText = 0;
    g_jobBGoneEmptyStateText = 0;
    g_deleteAllJobsTitleText = 0;
    g_deleteAllJobsSelectedMemberButton = 0;
    g_deleteAllJobsSelectedMembersButton = 0;
    g_deleteAllJobsWholeSquadButton = 0;
    g_deleteAllJobsEveryoneButton = 0;
    g_jobBGoneHoverHintText = 0;
    g_jobRowsScrollUpButton = 0;
    g_jobRowsScrollDownButton = 0;
    g_jobBGoneConfirmOverlay = 0;
    g_jobBGoneConfirmTitleText = 0;
    g_jobBGoneConfirmTitleTextBold = 0;
    g_jobBGoneConfirmBodyText = 0;
    g_jobBGoneConfirmYesButton = 0;
    g_jobBGoneConfirmNoButton = 0;
    g_jobBGoneConfirmVisible = false;
    g_dangerScopeArmed = false;
    g_armedDangerScope = JobDeleteScope_SelectedMember;
    g_armedDangerButton = 0;
    g_dangerScopeArmedAtMs = 0;
    g_pendingConfirmationAction.type = PendingConfirmationAction_None;
    g_pendingConfirmationAction.scope = JobDeleteScope_SelectedMember;
    g_pendingConfirmationAction.jobKey.clear();
    g_pendingConfirmationAction.taskType = static_cast<TaskType>(0);
    g_pendingConfirmationAction.taskName.clear();
    g_jobRowWidgets.clear();
    g_selectedMemberJobRows.clear();
    g_buttonAttachedToGlobalLayer = false;
    g_jobBGonePanelDragging = false;
    g_jobBGonePanelDragMoved = false;
    g_jobBGonePanelDragLastMouseX = 0;
    g_jobBGonePanelDragLastMouseY = 0;
    g_jobBGonePanelDragMovedDistance = 0;
    g_panelExpandedHeight = kPanelExpandedHeight;
    g_jobRowScrollOffset = 0;
    g_lastLoggedPanelSuppressedByUiState = false;
    g_lastLoggedPanelSuppressionReasonMask = PanelUiSuppressionReason_None;
    g_lastLoggedPanelSuppressedByToggle = false;
    g_hudHiddenByToggleEvent = false;
    g_hudHiddenByToggleEventKnown = false;
}

static void OnSaveLoadTransitionStart(const char* source)
{
    DestroySelectedMemberJobPanelButton();
    g_lastPlayerInterface = 0;
    g_pendingSelectedMemberUiRefresh = false;
    g_pendingSelectedMemberUiRefreshStartMs = 0;
    g_lastSelectedMemberUiRefreshAttemptMs = 0;
    g_selectedMemberUiRefreshAttempts = 0;
    g_lastLoggedHasSelectedMemberForButton = false;
    g_lastLoggedButtonVisibleState = false;
    g_lastLoggedButtonExists = false;
    g_lastLoggedPanelSuppressedByUiState = false;
    g_lastLoggedPanelSuppressionReasonMask = PanelUiSuppressionReason_None;
    g_lastLoggedPanelSuppressedByToggle = false;
    g_hudHiddenByToggleEvent = false;
    g_hudHiddenByToggleEventKnown = false;

    if (g_config.debugLogTransitions)
    {
        std::stringstream info;
        info << "Job-B-Gone DEBUG: save_load_ui_reset source=" << (source ? source : "unknown");
        DebugLog(info.str().c_str());
    }
}

static void OnJobBGoneHeaderButtonClicked(MyGUI::Widget*)
{
    if (g_jobBGonePanelDragMoved)
    {
        g_jobBGonePanelDragMoved = false;
        g_jobBGonePanelDragMovedDistance = 0;
        return;
    }

    if (g_jobBGonePanel)
    {
        g_jobBGonePanelCollapsed = !g_jobBGonePanelCollapsed;
        PersistPanelCollapsedStateIfChanged("collapse_toggle");
        const MyGUI::IntCoord nextCoord = ResolvePanelCoordFromConfig();
        ApplyPanelLayout(nextCoord);

        if (g_config.jobBGonePanelHasCustomPosition)
        {
            PersistPanelPositionIfChanged(nextCoord, "collapse_toggle");
        }
        return;
    }

    g_jobBGonePanelCollapsed = !g_jobBGonePanelCollapsed;
    PersistPanelCollapsedStateIfChanged("collapse_toggle_no_panel");
}

static void OnJobBGoneHeaderMousePressed(MyGUI::Widget*, int left, int top, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left)
    {
        return;
    }

    g_jobBGonePanelDragging = true;
    g_jobBGonePanelDragMoved = false;
    if (!TryGetMousePosition(&g_jobBGonePanelDragLastMouseX, &g_jobBGonePanelDragLastMouseY))
    {
        g_jobBGonePanelDragLastMouseX = left;
        g_jobBGonePanelDragLastMouseY = top;
    }
    g_jobBGonePanelDragMovedDistance = 0;
}

static void OnJobBGoneHeaderMouseDrag(MyGUI::Widget*, int left, int top, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left)
    {
        return;
    }

    if (!g_jobBGonePanelDragging || !g_jobBGonePanel)
    {
        return;
    }

    int mouseX = left;
    int mouseY = top;
    TryGetMousePosition(&mouseX, &mouseY);

    const int deltaX = mouseX - g_jobBGonePanelDragLastMouseX;
    const int deltaY = mouseY - g_jobBGonePanelDragLastMouseY;
    if (deltaX == 0 && deltaY == 0)
    {
        return;
    }

    MoveJobBGonePanelByDelta(deltaX, deltaY);

    g_jobBGonePanelDragLastMouseX = mouseX;
    g_jobBGonePanelDragLastMouseY = mouseY;
}

static void OnJobBGoneHeaderMouseMove(MyGUI::Widget*, int left, int top)
{
    if (!g_jobBGonePanelDragging)
    {
        return;
    }

    OnJobBGoneHeaderMouseDrag(0, left, top, MyGUI::MouseButton::Left);
}

static void OnJobBGoneHeaderMouseReleased(MyGUI::Widget*, int, int, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left)
    {
        return;
    }

    FinalizeJobBGonePanelDrag("drag_release");
}

static void TickJobBGonePanelDrag()
{
    if (!g_jobBGonePanelDragging || !g_jobBGonePanel)
    {
        return;
    }

    int mouseX = 0;
    int mouseY = 0;
    if (TryGetMousePosition(&mouseX, &mouseY))
    {
        const int deltaX = mouseX - g_jobBGonePanelDragLastMouseX;
        const int deltaY = mouseY - g_jobBGonePanelDragLastMouseY;
        MoveJobBGonePanelByDelta(deltaX, deltaY);
        g_jobBGonePanelDragLastMouseX = mouseX;
        g_jobBGonePanelDragLastMouseY = mouseY;
    }

    if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)
    {
        FinalizeJobBGonePanelDrag("drag_release_poll");
    }
}

static void EnsureSelectedMemberJobPanelButton(PlayerInterface* thisptr)
{
    g_lastPlayerInterface = thisptr;

    if (!g_jobBGonePanel)
    {
        MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
        if (!gui)
        {
            return;
        }

        const MyGUI::IntCoord panelCoord = ResolvePanelCoordFromConfig();
        g_jobBGonePanel = gui->createWidget<MyGUI::Widget>(
            "PanelEmpty",
            panelCoord,
            MyGUI::Align::Default,
            "Main");
        if (!g_jobBGonePanel)
        {
            g_jobBGonePanel = gui->createWidget<MyGUI::Widget>(
                "PanelEmpty",
                panelCoord,
                MyGUI::Align::Default,
                "Overlapped");
        }

        if (!g_jobBGonePanel)
        {
            if (!g_loggedSelectedMemberButtonCreateFailure)
            {
                ErrorLog("Job-B-Gone: failed to create Job-B-Gone panel");
                g_loggedSelectedMemberButtonCreateFailure = true;
            }
            return;
        }

        g_jobBGoneHeaderButton = g_jobBGonePanel->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(0, 0, panelCoord.width, 38),
            MyGUI::Align::Default);
        g_jobBGoneBodyFrame = g_jobBGonePanel->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(0, 40, panelCoord.width, panelCoord.height - 40),
            MyGUI::Align::Default);
        g_jobBGoneStatusText = g_jobBGonePanel->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(14, 50, panelCoord.width - 28, 22),
            MyGUI::Align::Default);
        g_jobBGoneEmptyStateText = g_jobBGonePanel->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(14, 122, panelCoord.width - 28, 24),
            MyGUI::Align::Default);
        g_deleteAllJobsTitleText = g_jobBGonePanel->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(14, 80, 192, kScopeButtonHeight),
            MyGUI::Align::Default);
        g_deleteAllJobsSelectedMemberButton = g_jobBGonePanel->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(214, 80, kScopeButtonMeWidth, kScopeButtonHeight),
            MyGUI::Align::Default);
        g_deleteAllJobsSelectedMembersButton = g_jobBGonePanel->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(272, 80, kScopeButtonSelectedWidth, kScopeButtonHeight),
            MyGUI::Align::Default);
        g_deleteAllJobsWholeSquadButton = g_jobBGonePanel->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(384, 80, kScopeButtonSquadWidth, kScopeButtonHeight),
            MyGUI::Align::Default);
        g_deleteAllJobsEveryoneButton = g_jobBGonePanel->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(484, 80, kScopeButtonAllSquadsWidth, kScopeButtonHeight),
            MyGUI::Align::Default);
        g_jobBGoneHoverHintText = g_jobBGonePanel->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(14, panelCoord.height - 30, panelCoord.width - 28, 18),
            MyGUI::Align::Default);
        g_jobRowsScrollUpButton = g_jobBGonePanel->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(panelCoord.width - kJobRowsScrollButtonWidth - 8, kJobRowsTop, kJobRowsScrollButtonWidth, kJobRowsScrollButtonHeight),
            MyGUI::Align::Default);
        g_jobRowsScrollDownButton = g_jobBGonePanel->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(
                panelCoord.width - kJobRowsScrollButtonWidth - 8,
                panelCoord.height - kJobRowsBottomPadding - kJobRowsScrollButtonHeight,
                kJobRowsScrollButtonWidth,
                kJobRowsScrollButtonHeight),
            MyGUI::Align::Default);

        if (!g_jobBGoneHeaderButton || !g_jobBGoneBodyFrame || !g_jobBGoneStatusText || !g_jobBGoneEmptyStateText
            || !g_deleteAllJobsTitleText
            || !g_deleteAllJobsSelectedMemberButton || !g_deleteAllJobsSelectedMembersButton
            || !g_deleteAllJobsWholeSquadButton || !g_deleteAllJobsEveryoneButton || !g_jobBGoneHoverHintText
            || !g_jobRowsScrollUpButton || !g_jobRowsScrollDownButton)
        {
            DestroySelectedMemberJobPanelButton();
            if (!g_loggedSelectedMemberButtonCreateFailure)
            {
                ErrorLog("Job-B-Gone: failed to create Job-B-Gone panel widgets");
                g_loggedSelectedMemberButtonCreateFailure = true;
            }
            return;
        }

        g_buttonAttachedToGlobalLayer = true;
        g_loggedSelectedMemberButtonCreateFailure = false;
        g_jobBGoneBodyFrame->setCaption("");
        g_jobBGoneBodyFrame->setEnabled(false);
        g_jobBGoneStatusText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        g_jobBGoneEmptyStateText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        g_jobBGoneEmptyStateText->setCaption("Selected member has no queued jobs.");
        g_jobBGoneEmptyStateText->setVisible(false);
        g_deleteAllJobsTitleText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        g_jobBGoneHoverHintText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        g_jobBGoneHoverHintText->setCaption("");
        g_jobRowsScrollUpButton->setCaption("^");
        SetWidgetTooltipAndHoverHint(g_jobRowsScrollUpButton, "Scroll job rows up.");
        g_jobRowsScrollUpButton->eventMouseButtonClick += MyGUI::newDelegate(&OnJobRowsScrollUpButtonClicked);
        g_jobRowsScrollDownButton->setCaption("v");
        SetWidgetTooltipAndHoverHint(g_jobRowsScrollDownButton, "Scroll job rows down.");
        g_jobRowsScrollDownButton->eventMouseButtonClick += MyGUI::newDelegate(&OnJobRowsScrollDownButtonClicked);
        g_jobBGoneHeaderButton->setNeedMouseFocus(true);
        g_jobBGoneHeaderButton->setNeedKeyFocus(true);
        g_jobBGoneHeaderButton->eventKeyButtonPressed += MyGUI::newDelegate(&OnConfirmationKeyPressed);
        g_jobBGoneHeaderButton->eventMouseButtonClick += MyGUI::newDelegate(&OnJobBGoneHeaderButtonClicked);
        g_jobBGoneHeaderButton->eventMouseButtonPressed += MyGUI::newDelegate(&OnJobBGoneHeaderMousePressed);
        g_jobBGoneHeaderButton->eventMouseMove += MyGUI::newDelegate(&OnJobBGoneHeaderMouseMove);
        g_jobBGoneHeaderButton->eventMouseDrag += MyGUI::newDelegate(&OnJobBGoneHeaderMouseDrag);
        g_jobBGoneHeaderButton->eventMouseButtonReleased += MyGUI::newDelegate(&OnJobBGoneHeaderMouseReleased);
        g_jobBGonePanel->setNeedMouseFocus(true);
        g_jobBGonePanel->eventMouseWheel += MyGUI::newDelegate(&OnJobBGonePanelMouseWheel);
        g_jobBGoneHeaderButton->eventMouseWheel += MyGUI::newDelegate(&OnJobBGonePanelMouseWheel);
        g_jobBGoneBodyFrame->setNeedMouseFocus(true);
        g_jobBGoneBodyFrame->eventMouseWheel += MyGUI::newDelegate(&OnJobBGonePanelMouseWheel);
        g_jobBGoneStatusText->setNeedMouseFocus(true);
        g_jobBGoneStatusText->eventMouseWheel += MyGUI::newDelegate(&OnJobBGonePanelMouseWheel);
        g_jobBGoneEmptyStateText->setNeedMouseFocus(true);
        g_jobBGoneEmptyStateText->eventMouseWheel += MyGUI::newDelegate(&OnJobBGonePanelMouseWheel);
        g_deleteAllJobsTitleText->setCaption("Delete All Jobs");
        g_deleteAllJobsSelectedMemberButton->setCaption(kScopeCaptionMe);
        SetWidgetTooltipAndHoverHint(
            g_deleteAllJobsSelectedMemberButton,
            "Delete all queued jobs for the currently selected member.");
        g_deleteAllJobsSelectedMemberButton->eventMouseButtonClick
            += MyGUI::newDelegate(&OnDeleteAllJobsSelectedMemberButtonClicked);
        g_deleteAllJobsSelectedMembersButton->setCaption(kScopeCaptionSelected);
        SetWidgetTooltipAndHoverHint(
            g_deleteAllJobsSelectedMembersButton,
            "Delete all queued jobs for all selected members.");
        g_deleteAllJobsSelectedMembersButton->eventMouseButtonClick
            += MyGUI::newDelegate(&OnDeleteAllJobsSelectedMembersButtonClicked);
        g_deleteAllJobsWholeSquadButton->setCaption(kScopeCaptionSquadWarning);
        SetWidgetTooltipAndHoverHint(
            g_deleteAllJobsWholeSquadButton,
            "Warning: delete all queued jobs for the selected member's squad.");
        g_deleteAllJobsWholeSquadButton->eventMouseButtonClick
            += MyGUI::newDelegate(&OnDeleteAllJobsWholeSquadButtonClicked);
        g_deleteAllJobsEveryoneButton->setCaption(kScopeCaptionAllSquadsWarning);
        SetWidgetTooltipAndHoverHint(
            g_deleteAllJobsEveryoneButton,
            "Warning: delete all queued jobs for all player-controlled squads.");
        g_deleteAllJobsEveryoneButton->eventMouseButtonClick
            += MyGUI::newDelegate(&OnDeleteAllJobsEveryoneButtonClicked);

        g_jobRowWidgets.clear();
        g_jobRowWidgets.reserve(kMaxVisibleJobRows);
        for (int rowIndex = 0; rowIndex < kMaxVisibleJobRows; ++rowIndex)
        {
            JobRowWidgets rowWidgets = { 0, 0, 0, 0, 0 };
            const int top = GetJobRowTop(rowIndex);
            rowWidgets.label = g_jobBGonePanel->createWidget<MyGUI::TextBox>(
                "Kenshi_TextboxStandardText",
                MyGUI::IntCoord(14, top, 192, kScopeButtonHeight),
                MyGUI::Align::Default);
            rowWidgets.deleteSelectedMemberButton = g_jobBGonePanel->createWidget<MyGUI::Button>(
                "Kenshi_Button1",
                MyGUI::IntCoord(214, top, kScopeButtonMeWidth, kScopeButtonHeight),
                MyGUI::Align::Default);
            rowWidgets.deleteSelectedMembersButton = g_jobBGonePanel->createWidget<MyGUI::Button>(
                "Kenshi_Button1",
                MyGUI::IntCoord(272, top, kScopeButtonSelectedWidth, kScopeButtonHeight),
                MyGUI::Align::Default);
            rowWidgets.deleteWholeSquadButton = g_jobBGonePanel->createWidget<MyGUI::Button>(
                "Kenshi_Button1",
                MyGUI::IntCoord(384, top, kScopeButtonSquadWidth, kScopeButtonHeight),
                MyGUI::Align::Default);
            rowWidgets.deleteEveryoneButton = g_jobBGonePanel->createWidget<MyGUI::Button>(
                "Kenshi_Button1",
                MyGUI::IntCoord(484, top, kScopeButtonAllSquadsWidth, kScopeButtonHeight),
                MyGUI::Align::Default);

            if (!rowWidgets.label || !rowWidgets.deleteSelectedMemberButton || !rowWidgets.deleteSelectedMembersButton
                || !rowWidgets.deleteWholeSquadButton || !rowWidgets.deleteEveryoneButton)
            {
                DestroySelectedMemberJobPanelButton();
                if (!g_loggedSelectedMemberButtonCreateFailure)
                {
                    ErrorLog("Job-B-Gone: failed to create Job-B-Gone row widgets");
                    g_loggedSelectedMemberButtonCreateFailure = true;
                }
                return;
            }

            rowWidgets.label->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
            BindWidgetHoverHintHandlers(rowWidgets.label);
            rowWidgets.deleteSelectedMemberButton->setCaption(kScopeCaptionMe);
            SetWidgetTooltipAndHoverHint(
                rowWidgets.deleteSelectedMemberButton,
                "Delete this job for selected member.");
            rowWidgets.deleteSelectedMemberButton->eventMouseButtonClick
                += MyGUI::newDelegate(&OnDeleteJobSelectedMemberButtonClicked);

            rowWidgets.deleteSelectedMembersButton->setCaption(kScopeCaptionSelected);
            SetWidgetTooltipAndHoverHint(
                rowWidgets.deleteSelectedMembersButton,
                "Delete this job for all selected members.");
            rowWidgets.deleteSelectedMembersButton->eventMouseButtonClick
                += MyGUI::newDelegate(&OnDeleteJobSelectedMembersButtonClicked);

            rowWidgets.deleteWholeSquadButton->setCaption(kScopeCaptionSquadWarning);
            SetWidgetTooltipAndHoverHint(
                rowWidgets.deleteWholeSquadButton,
                "Warning: delete this job for the whole squad.");
            rowWidgets.deleteWholeSquadButton->eventMouseButtonClick
                += MyGUI::newDelegate(&OnDeleteJobWholeSquadButtonClicked);

            rowWidgets.deleteEveryoneButton->setCaption(kScopeCaptionAllSquadsWarning);
            SetWidgetTooltipAndHoverHint(
                rowWidgets.deleteEveryoneButton,
                "Warning: delete this job for all player-controlled squads.");
            rowWidgets.deleteEveryoneButton->eventMouseButtonClick
                += MyGUI::newDelegate(&OnDeleteJobEveryoneButtonClicked);

            g_jobRowWidgets.push_back(rowWidgets);
        }

        if (g_config.debugLogTransitions)
        {
            DebugLog("Job-B-Gone DEBUG: created Job-B-Gone collapsible panel");
        }
        g_lastLoggedButtonExists = true;
    }

    if (!g_jobBGoneConfirmOverlay)
    {
        if (g_jobBGonePanel)
        {
            g_jobBGoneConfirmOverlay = g_jobBGonePanel->createWidget<MyGUI::Button>(
                "Kenshi_Button1",
                MyGUI::IntCoord(100, 114, 460, 210),
                MyGUI::Align::Default);
        }

        if (g_jobBGoneConfirmOverlay)
        {
            // Keep modal above panel widgets.
            g_jobBGoneConfirmOverlay->setDepth(32000);
            g_jobBGoneConfirmTitleText = g_jobBGoneConfirmOverlay->createWidget<MyGUI::TextBox>(
                "Kenshi_TextboxStandardText",
                MyGUI::IntCoord(12, 10, 436, 24),
                MyGUI::Align::Default);
            g_jobBGoneConfirmTitleTextBold = g_jobBGoneConfirmOverlay->createWidget<MyGUI::TextBox>(
                "Kenshi_TextboxStandardText",
                MyGUI::IntCoord(13, 10, 436, 24),
                MyGUI::Align::Default);
            g_jobBGoneConfirmBodyText = g_jobBGoneConfirmOverlay->createWidget<MyGUI::TextBox>(
                "Kenshi_TextboxStandardText",
                MyGUI::IntCoord(12, 40, 436, 124),
                MyGUI::Align::Default);
            g_jobBGoneConfirmYesButton = g_jobBGoneConfirmOverlay->createWidget<MyGUI::Button>(
                "Kenshi_Button1",
                MyGUI::IntCoord(222, 168, 110, 30),
                MyGUI::Align::Default);
            g_jobBGoneConfirmNoButton = g_jobBGoneConfirmOverlay->createWidget<MyGUI::Button>(
                "Kenshi_Button1",
                MyGUI::IntCoord(338, 168, 110, 30),
                MyGUI::Align::Default);
        }

        if (g_jobBGoneConfirmOverlay && g_jobBGoneConfirmTitleText && g_jobBGoneConfirmTitleTextBold && g_jobBGoneConfirmBodyText
            && g_jobBGoneConfirmYesButton && g_jobBGoneConfirmNoButton)
        {
            if (g_config.debugLogTransitions)
            {
                std::stringstream info;
                info << "Job-B-Gone DEBUG: confirmation_overlay_created mode=panel_child"
                     << " overlay_ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(g_jobBGoneConfirmOverlay)
                     << " panel_ptr=0x" << reinterpret_cast<uintptr_t>(g_jobBGonePanel)
                     << std::dec;
                DebugLog(info.str().c_str());
            }

            g_jobBGoneConfirmOverlay->setCaption("");
            g_jobBGoneConfirmOverlay->setNeedMouseFocus(true);
            g_jobBGoneConfirmOverlay->setNeedKeyFocus(true);
            g_jobBGoneConfirmOverlay->eventKeyButtonPressed += MyGUI::newDelegate(&OnConfirmationKeyPressed);
            g_jobBGoneConfirmTitleText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
            g_jobBGoneConfirmTitleTextBold->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
            g_jobBGoneConfirmBodyText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::Top);
            g_jobBGoneConfirmYesButton->setCaption("Confirm");
            g_jobBGoneConfirmYesButton->setNeedKeyFocus(true);
            g_jobBGoneConfirmYesButton->eventMouseButtonClick += MyGUI::newDelegate(&OnConfirmationAcceptClicked);
            g_jobBGoneConfirmYesButton->eventKeyButtonPressed += MyGUI::newDelegate(&OnConfirmationKeyPressed);
            g_jobBGoneConfirmNoButton->setCaption("Cancel");
            g_jobBGoneConfirmNoButton->setNeedKeyFocus(true);
            g_jobBGoneConfirmNoButton->eventMouseButtonClick += MyGUI::newDelegate(&OnConfirmationCancelClicked);
            g_jobBGoneConfirmNoButton->eventKeyButtonPressed += MyGUI::newDelegate(&OnConfirmationKeyPressed);
            g_jobBGoneConfirmOverlay->setVisible(false);
            g_jobBGoneConfirmTitleText->setVisible(false);
            g_jobBGoneConfirmTitleTextBold->setVisible(false);
            g_jobBGoneConfirmBodyText->setVisible(false);
            g_jobBGoneConfirmYesButton->setVisible(false);
            g_jobBGoneConfirmNoButton->setVisible(false);
        }
    }

    if (!g_jobBGonePanel || !g_jobBGoneHeaderButton || !g_jobBGoneBodyFrame || !g_jobBGoneStatusText
        || !g_jobBGoneEmptyStateText
        || !g_deleteAllJobsTitleText || !g_deleteAllJobsSelectedMemberButton || !g_deleteAllJobsSelectedMembersButton
        || !g_deleteAllJobsWholeSquadButton || !g_deleteAllJobsEveryoneButton || !g_jobBGoneHoverHintText
        || !g_jobRowsScrollUpButton || !g_jobRowsScrollDownButton
        || !g_jobBGoneConfirmOverlay || !g_jobBGoneConfirmTitleText || !g_jobBGoneConfirmTitleTextBold || !g_jobBGoneConfirmBodyText
        || !g_jobBGoneConfirmYesButton || !g_jobBGoneConfirmNoButton)
    {
        return;
    }

    TickJobBGonePanelDrag();

    if (!g_jobBGonePanelDragging)
    {
        const MyGUI::IntCoord panelCoord = ResolvePanelCoordFromConfig();
        ApplyPanelLayout(panelCoord);

        if (g_config.jobBGonePanelHasCustomPosition
            && (panelCoord.left != g_config.jobBGonePanelPosX || panelCoord.top != g_config.jobBGonePanelPosY))
        {
            PersistPanelPositionIfChanged(panelCoord, "viewport_clamp");
        }
    }

    Character* selectedMember = ResolveSelectedMember();
    const bool hasSelectedMember = (selectedMember != 0);
    if (hasSelectedMember != g_lastLoggedHasSelectedMemberForButton)
    {
        if (g_config.debugLogTransitions)
        {
            std::stringstream logline;
            logline << "Job-B-Gone DEBUG: selected_member_resolved_for_button="
                    << (hasSelectedMember ? "true" : "false")
                    << " selected_character_handle_valid="
                    << ((g_lastPlayerInterface && g_lastPlayerInterface->selectedCharacter
                        && g_lastPlayerInterface->selectedCharacter.isValid()) ? "true" : "false");
            DebugLog(logline.str().c_str());
        }
        g_lastLoggedHasSelectedMemberForButton = hasSelectedMember;
    }

    const int panelUiSuppressionReasonMask = DetectPanelUiSuppressionReasonMask(selectedMember);
    const bool suppressPanelByUiState = (panelUiSuppressionReasonMask != PanelUiSuppressionReason_None);
    const std::string panelUiSuppressionReasonSummary = BuildPanelUiSuppressionReasonSummary(panelUiSuppressionReasonMask);
    TickPanelVisibilityToggleHotkey(!suppressPanelByUiState, panelUiSuppressionReasonSummary.c_str());
    const bool suppressPanelByToggle = g_panelHiddenByToggle;
    const bool suppressPanel = suppressPanelByUiState || suppressPanelByToggle;
    const std::string panelVisibilityReasonSummary = BuildPanelVisibilityReasonSummary(panelUiSuppressionReasonMask, suppressPanelByToggle);
    if (suppressPanel)
    {
        HideConfirmationOverlay();
    }

    g_jobBGonePanel->setVisible(!suppressPanel);

    const bool panelVisibilityGateChanged =
        suppressPanelByUiState != g_lastLoggedPanelSuppressedByUiState
        || panelUiSuppressionReasonMask != g_lastLoggedPanelSuppressionReasonMask
        || suppressPanelByToggle != g_lastLoggedPanelSuppressedByToggle;
    static DWORD s_lastPanelVisibilityGateChangeMs = 0;
    static DWORD s_lastPanelVisibilityGateHeartbeatMs = 0;
    const DWORD nowMs = GetTickCount();

    if (panelVisibilityGateChanged)
    {
        if (g_config.debugLogTransitions)
        {
            std::stringstream logline;
            logline << "Job-B-Gone DEBUG: panel_visibility_gate"
                    << " suppressed=" << (suppressPanel ? "true" : "false")
                    << " reasons=" << panelVisibilityReasonSummary;
            DebugLog(logline.str().c_str());
        }
        g_lastLoggedPanelSuppressedByUiState = suppressPanelByUiState;
        g_lastLoggedPanelSuppressionReasonMask = panelUiSuppressionReasonMask;
        g_lastLoggedPanelSuppressedByToggle = suppressPanelByToggle;
        s_lastPanelVisibilityGateChangeMs = nowMs;
        s_lastPanelVisibilityGateHeartbeatMs = nowMs;
    }
    else if (suppressPanel)
    {
        if (s_lastPanelVisibilityGateChangeMs == 0)
        {
            s_lastPanelVisibilityGateChangeMs = nowMs;
            s_lastPanelVisibilityGateHeartbeatMs = nowMs;
        }
        else if (g_config.debugLogTransitions
            && DebounceWindowElapsed(nowMs, s_lastPanelVisibilityGateHeartbeatMs, 5000))
        {
            std::stringstream info;
            info << "Job-B-Gone DEBUG: panel_visibility_gate_stuck"
                 << " duration_ms=" << (nowMs - s_lastPanelVisibilityGateChangeMs)
                 << " reasons=" << panelVisibilityReasonSummary
                 << " suppress_ui=" << (suppressPanelByUiState ? "true" : "false")
                 << " suppress_toggle=" << (suppressPanelByToggle ? "true" : "false");
            DebugLog(info.str().c_str());
            s_lastPanelVisibilityGateHeartbeatMs = nowMs;
        }
    }

    if (suppressPanel)
    {
        return;
    }

    int selectedMemberCount = 0;
    BuildDisplayedJobRowsForSelection(selectedMember, &g_selectedMemberJobRows, &selectedMemberCount);
    const int jobCount = static_cast<int>(g_selectedMemberJobRows.size());
    MaybeExpireDangerScopeArmState("panel_tick_timeout");

    std::stringstream headerCaption;
    headerCaption << (g_jobBGonePanelCollapsed ? "[+] " : "[-] ") << "Job-B-Gone";
    g_jobBGoneHeaderButton->setCaption(headerCaption.str());

    const bool topActionsEnabled = g_config.enableDeleteAllJobsTopActions;
    const bool topActionsVisible = topActionsEnabled && !g_jobBGonePanelCollapsed;
    const bool confirmationOverlayVisible = g_jobBGoneConfirmVisible && !g_jobBGonePanelCollapsed;
    const bool topActionsActuallyVisible = topActionsVisible && !confirmationOverlayVisible;
    const bool canDeleteForSelectedMember = hasSelectedMember;
    const bool canDeleteForSelectedMembers = selectedMemberCount > 0;
    const bool canDeleteForWholeSquad = hasSelectedMember;
    const bool canDeleteForEveryone = g_lastPlayerInterface != 0;
    const bool showJobRowsNow = selectedMemberCount > 0 && !g_jobBGonePanelCollapsed && !confirmationOverlayVisible;
    const bool showEmptyState = selectedMemberCount > 0 && jobCount == 0 && !g_jobBGonePanelCollapsed && !confirmationOverlayVisible;
    const bool rowActionsEnabled = selectedMemberCount > 0;

    const int nextExpandedHeight = ComputeAdaptiveExpandedPanelHeight(
        showJobRowsNow,
        showEmptyState,
        jobCount,
        confirmationOverlayVisible);
    if (nextExpandedHeight != g_panelExpandedHeight)
    {
        g_panelExpandedHeight = nextExpandedHeight;
        if (!g_jobBGonePanelDragging)
        {
            const MyGUI::IntCoord resizedCoord = ResolvePanelCoordFromConfig();
            ApplyPanelLayout(resizedCoord);

            if (g_config.jobBGonePanelHasCustomPosition
                && (resizedCoord.left != g_config.jobBGonePanelPosX || resizedCoord.top != g_config.jobBGonePanelPosY))
            {
                PersistPanelPositionIfChanged(resizedCoord, "adaptive_resize");
            }
        }
    }

    int visibleRows = 0;
    if (showJobRowsNow && jobCount > 0)
    {
        const int rowCapacity = ComputeVisibleJobRowCapacityForHeight(GetExpandedPanelHeight());
        visibleRows = ClampIntValue(rowCapacity, 1, kMaxVisibleJobRows);
        if (visibleRows > jobCount)
        {
            visibleRows = jobCount;
        }
    }

    const int maxScrollOffset = ClampIntValue(jobCount - visibleRows, 0, jobCount);
    g_jobRowScrollOffset = ClampIntValue(g_jobRowScrollOffset, 0, maxScrollOffset);
    const bool showRowScrollControls = showJobRowsNow && jobCount > visibleRows && visibleRows > 0;
    if (!showRowScrollControls)
    {
        g_jobRowScrollOffset = 0;
    }

    std::stringstream statusCaption;
    if (selectedMemberCount > 0)
    {
        statusCaption << "Selected members: " << selectedMemberCount << " | ";
        if (selectedMemberCount > 1)
        {
            statusCaption << "Selected members jobs: " << jobCount;
        }
        else
        {
            statusCaption << "Selected member jobs: " << jobCount;
        }

        if (showJobRowsNow && visibleRows > 0 && jobCount > visibleRows)
        {
            const int firstRowNumber = g_jobRowScrollOffset + 1;
            const int lastRowNumber = g_jobRowScrollOffset + visibleRows;
            statusCaption << " (showing " << firstRowNumber << "-" << lastRowNumber << ")";
        }
    }
    else
    {
        statusCaption << "Selected members: 0";
    }
    g_jobBGoneStatusText->setCaption(statusCaption.str());

    g_jobBGoneBodyFrame->setVisible(!g_jobBGonePanelCollapsed);
    g_jobBGoneStatusText->setVisible(!g_jobBGonePanelCollapsed);
    g_jobBGoneEmptyStateText->setVisible(showEmptyState);
    if (showEmptyState)
    {
        if (selectedMemberCount > 1)
        {
            g_jobBGoneEmptyStateText->setCaption("Selected members have no queued jobs.");
        }
        else
        {
            g_jobBGoneEmptyStateText->setCaption("Selected member has no queued jobs.");
        }
    }
    g_jobBGoneHoverHintText->setVisible(!g_jobBGonePanelCollapsed && !confirmationOverlayVisible);
    if (confirmationOverlayVisible)
    {
        g_jobBGoneHoverHintText->setCaption("");
    }
    g_deleteAllJobsTitleText->setVisible(topActionsActuallyVisible);
    g_deleteAllJobsSelectedMemberButton->setVisible(topActionsActuallyVisible);
    g_deleteAllJobsSelectedMembersButton->setVisible(topActionsActuallyVisible);
    g_deleteAllJobsWholeSquadButton->setVisible(topActionsActuallyVisible);
    g_deleteAllJobsEveryoneButton->setVisible(topActionsActuallyVisible);
    g_deleteAllJobsSelectedMemberButton->setEnabled(topActionsVisible && canDeleteForSelectedMember && !confirmationOverlayVisible);
    g_deleteAllJobsSelectedMembersButton->setEnabled(topActionsVisible && canDeleteForSelectedMembers && !confirmationOverlayVisible);
    g_deleteAllJobsWholeSquadButton->setEnabled(topActionsVisible && canDeleteForWholeSquad && !confirmationOverlayVisible);
    g_deleteAllJobsEveryoneButton->setEnabled(topActionsVisible && canDeleteForEveryone && !confirmationOverlayVisible);
    g_jobBGoneConfirmOverlay->setVisible(confirmationOverlayVisible);
    g_jobBGoneConfirmTitleText->setVisible(confirmationOverlayVisible);
    g_jobBGoneConfirmTitleTextBold->setVisible(confirmationOverlayVisible);
    g_jobBGoneConfirmBodyText->setVisible(confirmationOverlayVisible);
    g_jobBGoneConfirmYesButton->setVisible(confirmationOverlayVisible);
    g_jobBGoneConfirmNoButton->setVisible(confirmationOverlayVisible);
    g_jobBGoneConfirmOverlay->setEnabled(confirmationOverlayVisible);
    g_jobBGoneConfirmYesButton->setEnabled(confirmationOverlayVisible);
    g_jobBGoneConfirmNoButton->setEnabled(confirmationOverlayVisible);
    g_jobRowsScrollUpButton->setVisible(showRowScrollControls);
    g_jobRowsScrollDownButton->setVisible(showRowScrollControls);
    g_jobRowsScrollUpButton->setEnabled(showRowScrollControls && g_jobRowScrollOffset > 0 && !confirmationOverlayVisible);
    g_jobRowsScrollDownButton->setEnabled(showRowScrollControls && g_jobRowScrollOffset < maxScrollOffset && !confirmationOverlayVisible);
    if (confirmationOverlayVisible)
    {
        MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
        if (input)
        {
            MyGUI::Widget* focused = input->getKeyFocusWidget();
            if (focused != g_jobBGoneConfirmYesButton && focused != g_jobBGoneConfirmNoButton)
            {
                input->setKeyFocusWidget(g_jobBGoneConfirmYesButton);
            }
        }
    }
    if (confirmationOverlayVisible != g_lastLoggedConfirmOverlayVisible)
    {
        if (g_config.debugLogTransitions)
        {
            std::stringstream info;
            const MyGUI::IntCoord coord = g_jobBGoneConfirmOverlay->getCoord();
            info << "Job-B-Gone DEBUG: confirmation_overlay_visibility"
                 << " visible=" << (confirmationOverlayVisible ? "true" : "false")
                 << " widget_visible=" << (g_jobBGoneConfirmOverlay->getVisible() ? "true" : "false")
                 << " x=" << coord.left
                 << " y=" << coord.top
                 << " w=" << coord.width
                 << " h=" << coord.height
                 << " collapsed=" << (g_jobBGonePanelCollapsed ? "true" : "false");
            DebugLog(info.str().c_str());
        }
        g_lastLoggedConfirmOverlayVisible = confirmationOverlayVisible;
    }

    const size_t rowWidgetCount = g_jobRowWidgets.size();
    for (size_t i = 0; i < rowWidgetCount; ++i)
    {
        JobRowWidgets& rowWidgets = g_jobRowWidgets[i];
        const bool rowVisible = showJobRowsNow && static_cast<int>(i) < visibleRows;
        if (!rowVisible)
        {
            if (rowWidgets.label)
            {
                SetWidgetHoverHint(rowWidgets.label, "");
                rowWidgets.label->setVisible(false);
            }
            if (rowWidgets.deleteSelectedMemberButton)
            {
                rowWidgets.deleteSelectedMemberButton->setVisible(false);
            }
            if (rowWidgets.deleteSelectedMembersButton)
            {
                rowWidgets.deleteSelectedMembersButton->setVisible(false);
            }
            if (rowWidgets.deleteWholeSquadButton)
            {
                rowWidgets.deleteWholeSquadButton->setVisible(false);
            }
            if (rowWidgets.deleteEveryoneButton)
            {
                rowWidgets.deleteEveryoneButton->setVisible(false);
            }
            continue;
        }

        const int modelIndex = g_jobRowScrollOffset + static_cast<int>(i);
        if (modelIndex < 0 || modelIndex >= jobCount)
        {
            if (rowWidgets.label)
            {
                SetWidgetHoverHint(rowWidgets.label, "");
                rowWidgets.label->setVisible(false);
            }
            if (rowWidgets.deleteSelectedMemberButton)
            {
                rowWidgets.deleteSelectedMemberButton->setVisible(false);
            }
            if (rowWidgets.deleteSelectedMembersButton)
            {
                rowWidgets.deleteSelectedMembersButton->setVisible(false);
            }
            if (rowWidgets.deleteWholeSquadButton)
            {
                rowWidgets.deleteWholeSquadButton->setVisible(false);
            }
            if (rowWidgets.deleteEveryoneButton)
            {
                rowWidgets.deleteEveryoneButton->setVisible(false);
            }
            continue;
        }

        const JobRowModel& rowModel = g_selectedMemberJobRows[static_cast<size_t>(modelIndex)];
        std::string taskNameForDisplay = BuildCompactTaskNameForDisplay(rowModel.taskName);
        std::string hoverTaskName = NormalizeTaskNameForDisplay(rowModel.taskName);
        const TaskTargetInferenceKind inferenceKindFromType = DetectTaskTargetInferenceKindByTaskType(rowModel.taskType);
        const TaskTargetInferenceKind inferenceKind = (inferenceKindFromType != TaskTargetInference_None)
            ? inferenceKindFromType
            : DetectTaskTargetInferenceKind(hoverTaskName);
        bool missingTarget = false;
        if (inferenceKind != TaskTargetInference_None)
        {
            missingTarget = IsTaskNameMissingTarget(inferenceKind, hoverTaskName);
            if (!missingTarget && inferenceKindFromType != TaskTargetInference_None)
            {
                // Type-driven fallback: if this is a known target-bearing task and no
                // explicit separator is present, treat it as a missing-target label.
                missingTarget = !TaskNameHasExplicitTargetDelimiter(hoverTaskName);
            }
        }
        if (inferenceKind != TaskTargetInference_None && missingTarget)
        {
            std::string inferredTargetName;
            if (TryInferTaskTargetName(rowModel.taskDataPtr, inferenceKind, &inferredTargetName))
            {
                hoverTaskName = BuildExpandedTaskNameWithTarget(inferenceKind, inferredTargetName);
                taskNameForDisplay = BuildCompactTaskNameForDisplay(hoverTaskName);
            }
            else if (inferenceKind == TaskTargetInference_OperatingMachine
                && StartsWithAsciiNoCase(taskNameForDisplay, "Op. m:"))
            {
                std::string compactSuffix = taskNameForDisplay.substr(std::strlen("Op. m:"));
                size_t suffixStart = 0;
                while (suffixStart < compactSuffix.size()
                    && std::isspace(static_cast<unsigned char>(compactSuffix[suffixStart])) != 0)
                {
                    ++suffixStart;
                }
                if (suffixStart < compactSuffix.size())
                {
                    hoverTaskName = "Operating machine: " + compactSuffix.substr(suffixStart);
                }
            }
        }

        // Keep row title compact (Op. m:) while using inferred full machine target when available.
        if (StartsWithAsciiNoCase(taskNameForDisplay, "Op. m:")
            && StartsWithAsciiNoCase(hoverTaskName, "Operating machine"))
        {
            const std::string inferredCompactTitle = BuildCompactTaskNameForDisplay(hoverTaskName);
            if (StartsWithAsciiNoCase(inferredCompactTitle, "Op. m:")
                && inferredCompactTitle.size() > std::strlen("Op. m:"))
            {
                taskNameForDisplay = inferredCompactTitle;
            }
        }

        std::stringstream rowLabel;
        rowLabel << (modelIndex + 1) << ". " << taskNameForDisplay;
        if (taskNameForDisplay.empty())
        {
            rowLabel.str("");
            rowLabel.clear();
            rowLabel << (modelIndex + 1) << ". Task " << static_cast<int>(rowModel.taskType);
        }

        if (rowWidgets.label)
        {
            rowWidgets.label->setCaption(rowLabel.str());
            std::stringstream hoverHintCaption;
            if (!hoverTaskName.empty())
            {
                hoverHintCaption << (modelIndex + 1) << ". " << hoverTaskName;
            }
            else
            {
                hoverHintCaption << rowLabel.str();
            }
            SetWidgetHoverHint(rowWidgets.label, hoverHintCaption.str());
            rowWidgets.label->setVisible(true);
        }

        SetJobButtonPayload(rowWidgets.deleteSelectedMemberButton, rowModel);
        SetJobButtonPayload(rowWidgets.deleteSelectedMembersButton, rowModel);
        SetJobButtonPayload(rowWidgets.deleteWholeSquadButton, rowModel);
        SetJobButtonPayload(rowWidgets.deleteEveryoneButton, rowModel);

        if (rowWidgets.deleteSelectedMemberButton)
        {
            rowWidgets.deleteSelectedMemberButton->setVisible(true);
            rowWidgets.deleteSelectedMemberButton->setEnabled(rowActionsEnabled && !confirmationOverlayVisible);
        }
        if (rowWidgets.deleteSelectedMembersButton)
        {
            rowWidgets.deleteSelectedMembersButton->setVisible(true);
            rowWidgets.deleteSelectedMembersButton->setEnabled(rowActionsEnabled && !confirmationOverlayVisible);
        }
        if (rowWidgets.deleteWholeSquadButton)
        {
            rowWidgets.deleteWholeSquadButton->setVisible(true);
            rowWidgets.deleteWholeSquadButton->setEnabled(rowActionsEnabled && !confirmationOverlayVisible);
        }
        if (rowWidgets.deleteEveryoneButton)
        {
            rowWidgets.deleteEveryoneButton->setVisible(true);
            rowWidgets.deleteEveryoneButton->setEnabled(rowActionsEnabled && !confirmationOverlayVisible);
        }
    }
    if (topActionsVisible != g_lastLoggedButtonVisibleState)
    {
        if (g_config.debugLogTransitions)
        {
            std::stringstream logline;
            logline << "Job-B-Gone DEBUG: delete-all-controls visible="
                    << (topActionsVisible ? "true" : "false")
                    << " selected_member_resolved=" << (hasSelectedMember ? "true" : "false")
                    << " action_enabled=" << (topActionsEnabled ? "true" : "false")
                    << " button_ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(g_deleteAllJobsSelectedMemberButton);
            DebugLog(logline.str().c_str());
        }
        g_lastLoggedButtonVisibleState = topActionsVisible;
    }
}
