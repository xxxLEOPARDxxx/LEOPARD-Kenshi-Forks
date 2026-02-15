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

static int GetJobRowTop(int rowIndex)
{
    return 114 + (rowIndex * 38);
}

static const int kScopeButtonMeWidth = 52;
static const int kScopeButtonSelectedWidth = 114;
static const int kScopeButtonSquadWidth = 102;
static const int kScopeButtonAllSquadsWidth = 130;
static const int kScopeButtonGap = 6;
static const int kScopeButtonHeight = 30;

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

static bool IsGenericOperatingMachineName(const std::string& taskName)
{
    static const char* kOperatingMachinePrefix = "Operating machine";
    if (!StartsWithAsciiNoCase(taskName, kOperatingMachinePrefix))
    {
        return false;
    }

    size_t pos = std::strlen(kOperatingMachinePrefix);
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

static bool TryInferOperatingMachineTargetName(uintptr_t taskDataPtr, std::string* targetNameOut)
{
    if (!targetNameOut || taskDataPtr == 0)
    {
        return false;
    }

    struct CachedTaskTargetName
    {
        uintptr_t taskDataPtr;
        bool hasValue;
        std::string value;
    };
    static std::vector<CachedTaskTargetName> cache;

    const size_t cacheSize = cache.size();
    for (size_t i = 0; i < cacheSize; ++i)
    {
        if (cache[i].taskDataPtr != taskDataPtr)
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

        if (IsGenericOperatingMachineName(resolvedName))
        {
            continue;
        }

        if (firstName.empty())
        {
            firstName = resolvedName;
        }

        if (ContainsAsciiNoCase(resolvedName, "resource")
            || ContainsAsciiNoCase(resolvedName, "copper")
            || ContainsAsciiNoCase(resolvedName, "iron")
            || ContainsAsciiNoCase(resolvedName, "machine"))
        {
            preferredName = resolvedName;
            break;
        }
    }

    CachedTaskTargetName cached = { 0, false, std::string() };
    cached.taskDataPtr = taskDataPtr;
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
        info << "Job-B-Gone DEBUG: operating_machine_target_infer"
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
    return kPanelExpandedHeight - kPanelCollapsedHeight;
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
    const int height = g_jobBGonePanelCollapsed ? kPanelCollapsedHeight : kPanelExpandedHeight;
    return MyGUI::IntCoord(left, top, kPanelWidth, height);
}

static MyGUI::IntCoord ClampPanelCoordToViewport(const MyGUI::IntCoord& inputCoord)
{
    int left = inputCoord.left;
    int top = inputCoord.top;
    const int width = (inputCoord.width > 0) ? inputCoord.width : kPanelWidth;
    const int height = (inputCoord.height > 0) ? inputCoord.height : kPanelExpandedHeight;

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
        const int height = g_jobBGonePanelCollapsed ? kPanelCollapsedHeight : kPanelExpandedHeight;
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
        || !g_jobBGoneConfirmOverlay || !g_jobBGoneConfirmTitleText || !g_jobBGoneConfirmBodyText
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
    const int allButtonsWidth = meButtonWidth + selectedButtonWidth + squadButtonWidth + allButtonWidth + (buttonGap * 3);
    const int topTitleWidth = panelCoord.width - 28 - allButtonsWidth - 8;
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

    int labelWidth = panelCoord.width - 28 - allButtonsWidth - 8;
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

    std::stringstream info;
    info << "Job-B-Gone INFO: persisted_panel_position source=" << (source ? source : "unknown")
         << " x=" << panelCoord.left
         << " y=" << storedTop;
    DebugLog(info.str().c_str());
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
    g_jobBGoneConfirmOverlay = 0;
    g_jobBGoneConfirmTitleText = 0;
    g_jobBGoneConfirmBodyText = 0;
    g_jobBGoneConfirmYesButton = 0;
    g_jobBGoneConfirmNoButton = 0;
    g_jobBGoneConfirmVisible = false;
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

    std::stringstream info;
    info << "Job-B-Gone DEBUG: save_load_ui_reset source=" << (source ? source : "unknown");
    DebugLog(info.str().c_str());
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
        const MyGUI::IntCoord nextCoord = ResolvePanelCoordFromConfig();
        ApplyPanelLayout(nextCoord);

        if (g_config.jobBGonePanelHasCustomPosition)
        {
            PersistPanelPositionIfChanged(nextCoord, "collapse_toggle");
        }
        return;
    }

    g_jobBGonePanelCollapsed = !g_jobBGonePanelCollapsed;
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

        if (!g_jobBGoneHeaderButton || !g_jobBGoneBodyFrame || !g_jobBGoneStatusText || !g_jobBGoneEmptyStateText
            || !g_deleteAllJobsTitleText
            || !g_deleteAllJobsSelectedMemberButton || !g_deleteAllJobsSelectedMembersButton
            || !g_deleteAllJobsWholeSquadButton || !g_deleteAllJobsEveryoneButton || !g_jobBGoneHoverHintText)
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
        g_jobBGoneHeaderButton->setNeedMouseFocus(true);
        g_jobBGoneHeaderButton->setNeedKeyFocus(true);
        g_jobBGoneHeaderButton->eventKeyButtonPressed += MyGUI::newDelegate(&OnConfirmationKeyPressed);
        g_jobBGoneHeaderButton->eventMouseButtonClick += MyGUI::newDelegate(&OnJobBGoneHeaderButtonClicked);
        g_jobBGoneHeaderButton->eventMouseButtonPressed += MyGUI::newDelegate(&OnJobBGoneHeaderMousePressed);
        g_jobBGoneHeaderButton->eventMouseMove += MyGUI::newDelegate(&OnJobBGoneHeaderMouseMove);
        g_jobBGoneHeaderButton->eventMouseDrag += MyGUI::newDelegate(&OnJobBGoneHeaderMouseDrag);
        g_jobBGoneHeaderButton->eventMouseButtonReleased += MyGUI::newDelegate(&OnJobBGoneHeaderMouseReleased);
        g_deleteAllJobsTitleText->setCaption("Delete All Jobs");
        g_deleteAllJobsSelectedMemberButton->setCaption("Me");
        SetWidgetTooltipAndHoverHint(
            g_deleteAllJobsSelectedMemberButton,
            "Delete all queued jobs for the currently selected member.");
        g_deleteAllJobsSelectedMemberButton->eventMouseButtonClick
            += MyGUI::newDelegate(&OnDeleteAllJobsSelectedMemberButtonClicked);
        g_deleteAllJobsSelectedMembersButton->setCaption("Selected");
        SetWidgetTooltipAndHoverHint(
            g_deleteAllJobsSelectedMembersButton,
            "Delete all queued jobs for all selected members.");
        g_deleteAllJobsSelectedMembersButton->eventMouseButtonClick
            += MyGUI::newDelegate(&OnDeleteAllJobsSelectedMembersButtonClicked);
        g_deleteAllJobsWholeSquadButton->setCaption("Squad");
        SetWidgetTooltipAndHoverHint(
            g_deleteAllJobsWholeSquadButton,
            "Delete all queued jobs for the selected member's squad.");
        g_deleteAllJobsWholeSquadButton->eventMouseButtonClick
            += MyGUI::newDelegate(&OnDeleteAllJobsWholeSquadButtonClicked);
        g_deleteAllJobsEveryoneButton->setCaption("All Squads");
        SetWidgetTooltipAndHoverHint(
            g_deleteAllJobsEveryoneButton,
            "Delete all queued jobs for all player-controlled squads.");
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
            rowWidgets.deleteSelectedMemberButton->setCaption("Me");
            SetWidgetTooltipAndHoverHint(
                rowWidgets.deleteSelectedMemberButton,
                "Delete this job for selected member.");
            rowWidgets.deleteSelectedMemberButton->eventMouseButtonClick
                += MyGUI::newDelegate(&OnDeleteJobSelectedMemberButtonClicked);

            rowWidgets.deleteSelectedMembersButton->setCaption("Selected");
            SetWidgetTooltipAndHoverHint(
                rowWidgets.deleteSelectedMembersButton,
                "Delete this job for all selected members.");
            rowWidgets.deleteSelectedMembersButton->eventMouseButtonClick
                += MyGUI::newDelegate(&OnDeleteJobSelectedMembersButtonClicked);

            rowWidgets.deleteWholeSquadButton->setCaption("Squad");
            SetWidgetTooltipAndHoverHint(
                rowWidgets.deleteWholeSquadButton,
                "Delete this job for the whole squad.");
            rowWidgets.deleteWholeSquadButton->eventMouseButtonClick
                += MyGUI::newDelegate(&OnDeleteJobWholeSquadButtonClicked);

            rowWidgets.deleteEveryoneButton->setCaption("All Squads");
            SetWidgetTooltipAndHoverHint(
                rowWidgets.deleteEveryoneButton,
                "Delete this job for all player-controlled squads.");
            rowWidgets.deleteEveryoneButton->eventMouseButtonClick
                += MyGUI::newDelegate(&OnDeleteJobEveryoneButtonClicked);

            g_jobRowWidgets.push_back(rowWidgets);
        }

        DebugLog("Job-B-Gone DEBUG: created Job-B-Gone collapsible panel");
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

        if (g_jobBGoneConfirmOverlay && g_jobBGoneConfirmTitleText && g_jobBGoneConfirmBodyText
            && g_jobBGoneConfirmYesButton && g_jobBGoneConfirmNoButton)
        {
            std::stringstream info;
            info << "Job-B-Gone DEBUG: confirmation_overlay_created mode=panel_child"
                 << " overlay_ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(g_jobBGoneConfirmOverlay)
                 << " panel_ptr=0x" << reinterpret_cast<uintptr_t>(g_jobBGonePanel)
                 << std::dec;
            DebugLog(info.str().c_str());

            g_jobBGoneConfirmOverlay->setCaption("");
            g_jobBGoneConfirmOverlay->setNeedMouseFocus(true);
            g_jobBGoneConfirmOverlay->setNeedKeyFocus(true);
            g_jobBGoneConfirmOverlay->eventKeyButtonPressed += MyGUI::newDelegate(&OnConfirmationKeyPressed);
            g_jobBGoneConfirmTitleText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
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
            g_jobBGoneConfirmBodyText->setVisible(false);
            g_jobBGoneConfirmYesButton->setVisible(false);
            g_jobBGoneConfirmNoButton->setVisible(false);
        }
    }

    if (!g_jobBGonePanel || !g_jobBGoneHeaderButton || !g_jobBGoneBodyFrame || !g_jobBGoneStatusText
        || !g_jobBGoneEmptyStateText
        || !g_deleteAllJobsTitleText || !g_deleteAllJobsSelectedMemberButton || !g_deleteAllJobsSelectedMembersButton
        || !g_deleteAllJobsWholeSquadButton || !g_deleteAllJobsEveryoneButton || !g_jobBGoneHoverHintText
        || !g_jobBGoneConfirmOverlay || !g_jobBGoneConfirmTitleText || !g_jobBGoneConfirmBodyText
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
        std::stringstream logline;
        logline << "Job-B-Gone DEBUG: selected_member_resolved_for_button="
                << (hasSelectedMember ? "true" : "false")
                << " selected_character_handle_valid="
                << ((g_lastPlayerInterface && g_lastPlayerInterface->selectedCharacter
                    && g_lastPlayerInterface->selectedCharacter.isValid()) ? "true" : "false");
        DebugLog(logline.str().c_str());
        g_lastLoggedHasSelectedMemberForButton = hasSelectedMember;
    }

    int selectedMemberCount = 0;
    BuildDisplayedJobRowsForSelection(selectedMember, &g_selectedMemberJobRows, &selectedMemberCount);
    const int jobCount = static_cast<int>(g_selectedMemberJobRows.size());

    std::stringstream headerCaption;
    headerCaption << (g_jobBGonePanelCollapsed ? "[+] " : "[-] ") << "Job-B-Gone";
    g_jobBGoneHeaderButton->setCaption(headerCaption.str());

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
        if (jobCount > kMaxVisibleJobRows)
        {
            statusCaption << " (showing " << kMaxVisibleJobRows << ")";
        }
    }
    else
    {
        statusCaption << "Selected members: 0";
    }
    g_jobBGoneStatusText->setCaption(statusCaption.str());

    const bool topActionsEnabled = g_config.enableDeleteAllJobsSelectedMemberAction;
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
    g_jobBGoneConfirmBodyText->setVisible(confirmationOverlayVisible);
    g_jobBGoneConfirmYesButton->setVisible(confirmationOverlayVisible);
    g_jobBGoneConfirmNoButton->setVisible(confirmationOverlayVisible);
    g_jobBGoneConfirmOverlay->setEnabled(confirmationOverlayVisible);
    g_jobBGoneConfirmYesButton->setEnabled(confirmationOverlayVisible);
    g_jobBGoneConfirmNoButton->setEnabled(confirmationOverlayVisible);
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
        g_lastLoggedConfirmOverlayVisible = confirmationOverlayVisible;
    }

    size_t visibleRows = g_selectedMemberJobRows.size();
    if (visibleRows > static_cast<size_t>(kMaxVisibleJobRows))
    {
        visibleRows = static_cast<size_t>(kMaxVisibleJobRows);
    }

    const size_t rowWidgetCount = g_jobRowWidgets.size();
    for (size_t i = 0; i < rowWidgetCount; ++i)
    {
        JobRowWidgets& rowWidgets = g_jobRowWidgets[i];
        const bool rowVisible = showJobRowsNow && i < visibleRows;
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

        const JobRowModel& rowModel = g_selectedMemberJobRows[i];
        std::string taskNameForDisplay = BuildCompactTaskNameForDisplay(rowModel.taskName);
        std::string hoverTaskName = NormalizeTaskNameForDisplay(rowModel.taskName);
        if (IsGenericOperatingMachineName(hoverTaskName))
        {
            std::string inferredTargetName;
            if (TryInferOperatingMachineTargetName(rowModel.taskDataPtr, &inferredTargetName))
            {
                hoverTaskName = "Operating machine: " + inferredTargetName;
            }
            else if (StartsWithAsciiNoCase(taskNameForDisplay, "Op. m:"))
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
        rowLabel << (i + 1) << ". " << taskNameForDisplay;
        if (taskNameForDisplay.empty())
        {
            rowLabel.str("");
            rowLabel.clear();
            rowLabel << (i + 1) << ". Task " << static_cast<int>(rowModel.taskType);
        }

        if (rowWidgets.label)
        {
            rowWidgets.label->setCaption(rowLabel.str());
            std::stringstream hoverHintCaption;
            if (!hoverTaskName.empty())
            {
                hoverHintCaption << (i + 1) << ". " << hoverTaskName;
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
        std::stringstream logline;
        logline << "Job-B-Gone DEBUG: delete-all-controls visible="
                << (topActionsVisible ? "true" : "false")
                << " selected_member_resolved=" << (hasSelectedMember ? "true" : "false")
                << " action_enabled=" << (topActionsEnabled ? "true" : "false")
                << " button_ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(g_deleteAllJobsSelectedMemberButton);
        DebugLog(logline.str().c_str());
        g_lastLoggedButtonVisibleState = topActionsVisible;
    }
}
