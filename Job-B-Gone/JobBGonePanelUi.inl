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

static std::string BuildCompactTaskNameForDisplay(const std::string& taskName)
{
    static const char* kOperatingMachinePrefix = "Operating machine";
    if (!StartsWithAsciiNoCase(taskName, kOperatingMachinePrefix))
    {
        return taskName;
    }

    size_t suffixPos = taskName.size();
    const size_t lastColon = taskName.find_last_of(':');
    if (lastColon != std::string::npos)
    {
        suffixPos = lastColon + 1;
    }
    else
    {
        suffixPos = std::strlen(kOperatingMachinePrefix);
    }

    while (suffixPos < taskName.size() && std::isspace(static_cast<unsigned char>(taskName[suffixPos])) != 0)
    {
        ++suffixPos;
    }

    if (suffixPos >= taskName.size())
    {
        // Keep original text when no machine target can be extracted.
        return taskName;
    }

    std::stringstream compact;
    compact << "Op. m.: " << taskName.substr(suffixPos);
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

static void SetWidgetTooltipAndHoverHint(MyGUI::Widget* widget, const char* tooltipText)
{
    if (!widget || !tooltipText)
    {
        return;
    }

    widget->setNeedToolTip(true);
    widget->setUserString("ToolTip", tooltipText);
    widget->setUserString("JobBGoneHoverHint", tooltipText);
    widget->eventMouseSetFocus += MyGUI::newDelegate(&OnActionButtonMouseSetFocus);
    widget->eventMouseLostFocus += MyGUI::newDelegate(&OnActionButtonMouseLostFocus);
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
    if (g_jobBGoneHoverHintText)
    {
        g_jobBGoneHoverHintText->setCoord(MyGUI::IntCoord(14, panelCoord.height - 30, panelCoord.width - 28, 18));
    }

    const int overlayWidth = 460;
    const int overlayHeight = 210;
    int overlayLeft = panelCoord.left + ((panelCoord.width - overlayWidth) / 2);
    int overlayTop = panelCoord.top + 70;
    int viewWidth = 0;
    int viewHeight = 0;
    if (TryGetViewportSize(&viewWidth, &viewHeight))
    {
        overlayLeft = ClampIntValue(overlayLeft, 10, viewWidth - overlayWidth - 10);
        overlayTop = ClampIntValue(overlayTop, 10, viewHeight - overlayHeight - 10);
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
}

static void DestroySelectedMemberJobPanelButton()
{
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

        if (!g_jobBGoneHeaderButton || !g_jobBGoneBodyFrame || !g_jobBGoneStatusText || !g_deleteAllJobsTitleText
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
        MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
        if (gui)
        {
            g_jobBGoneConfirmOverlay = gui->createWidget<MyGUI::Button>(
                "Kenshi_Button1",
                MyGUI::IntCoord(180, 180, 460, 210),
                MyGUI::Align::Default,
                "Overlapped");
            if (!g_jobBGoneConfirmOverlay)
            {
                g_jobBGoneConfirmOverlay = gui->createWidget<MyGUI::Button>(
                    "Kenshi_Button1",
                    MyGUI::IntCoord(180, 180, 460, 210),
                    MyGUI::Align::Default,
                    "Main");
            }

            if (g_jobBGoneConfirmOverlay)
            {
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
        }

        if (g_jobBGoneConfirmOverlay && g_jobBGoneConfirmTitleText && g_jobBGoneConfirmBodyText
            && g_jobBGoneConfirmYesButton && g_jobBGoneConfirmNoButton)
        {
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
    const bool canDeleteForSelectedMember = hasSelectedMember;
    const bool canDeleteForSelectedMembers = selectedMemberCount > 0;
    const bool canDeleteForWholeSquad = hasSelectedMember;
    const bool canDeleteForEveryone = g_lastPlayerInterface != 0;
    const bool showJobRowsNow = selectedMemberCount > 0 && !g_jobBGonePanelCollapsed;
    const bool rowActionsEnabled = selectedMemberCount > 0;
    g_jobBGoneBodyFrame->setVisible(!g_jobBGonePanelCollapsed);
    g_jobBGoneStatusText->setVisible(!g_jobBGonePanelCollapsed);
    g_jobBGoneHoverHintText->setVisible(!g_jobBGonePanelCollapsed && !confirmationOverlayVisible);
    if (confirmationOverlayVisible)
    {
        g_jobBGoneHoverHintText->setCaption("");
    }
    g_deleteAllJobsTitleText->setVisible(topActionsVisible);
    g_deleteAllJobsSelectedMemberButton->setVisible(topActionsVisible);
    g_deleteAllJobsSelectedMembersButton->setVisible(topActionsVisible);
    g_deleteAllJobsWholeSquadButton->setVisible(topActionsVisible);
    g_deleteAllJobsEveryoneButton->setVisible(topActionsVisible);
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
        const std::string taskNameForDisplay = BuildCompactTaskNameForDisplay(rowModel.taskName);
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
