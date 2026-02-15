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
    return 112 + (rowIndex * 38);
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
        || !g_deleteAllJobsSelectedMemberButton)
    {
        return;
    }

    g_jobBGonePanel->setCoord(panelCoord);
    g_jobBGoneHeaderButton->setCoord(MyGUI::IntCoord(0, 0, panelCoord.width, 38));
    g_jobBGoneBodyFrame->setCoord(MyGUI::IntCoord(0, 40, panelCoord.width, panelCoord.height - 40));
    g_jobBGoneStatusText->setCoord(MyGUI::IntCoord(14, 50, panelCoord.width - 28, 22));
    g_deleteAllJobsSelectedMemberButton->setCoord(MyGUI::IntCoord(14, 74, panelCoord.width - 28, 36));

    const int meButtonWidth = 58;
    const int selectedButtonWidth = 60;
    const int squadButtonWidth = 82;
    const int allButtonWidth = 56;
    const int buttonGap = 6;
    const int buttonHeight = 30;
    const int allButtonsWidth = meButtonWidth + selectedButtonWidth + squadButtonWidth + allButtonWidth + (buttonGap * 3);
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
    if (gui && g_jobBGonePanel)
    {
        gui->destroyWidget(g_jobBGonePanel);
    }

    g_jobBGonePanel = 0;
    g_jobBGoneHeaderButton = 0;
    g_jobBGoneBodyFrame = 0;
    g_jobBGoneStatusText = 0;
    g_deleteAllJobsSelectedMemberButton = 0;
    g_deleteAllJobsSelectedMemberButtonParent = 0;
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
        g_deleteAllJobsSelectedMemberButton = g_jobBGonePanel->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(14, 74, panelCoord.width - 28, 36),
            MyGUI::Align::Default);

        if (!g_jobBGoneHeaderButton || !g_jobBGoneBodyFrame || !g_jobBGoneStatusText || !g_deleteAllJobsSelectedMemberButton)
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
        g_deleteAllJobsSelectedMemberButtonParent = g_jobBGonePanel;
        g_jobBGoneBodyFrame->setCaption("");
        g_jobBGoneBodyFrame->setEnabled(false);
        g_jobBGoneStatusText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        g_jobBGoneHeaderButton->setNeedMouseFocus(true);
        g_jobBGoneHeaderButton->eventMouseButtonClick += MyGUI::newDelegate(&OnJobBGoneHeaderButtonClicked);
        g_jobBGoneHeaderButton->eventMouseButtonPressed += MyGUI::newDelegate(&OnJobBGoneHeaderMousePressed);
        g_jobBGoneHeaderButton->eventMouseMove += MyGUI::newDelegate(&OnJobBGoneHeaderMouseMove);
        g_jobBGoneHeaderButton->eventMouseDrag += MyGUI::newDelegate(&OnJobBGoneHeaderMouseDrag);
        g_jobBGoneHeaderButton->eventMouseButtonReleased += MyGUI::newDelegate(&OnJobBGoneHeaderMouseReleased);
        g_deleteAllJobsSelectedMemberButton->setCaption("Delete All Jobs (Selected Member)");
        g_deleteAllJobsSelectedMemberButton->setNeedToolTip(true);
        g_deleteAllJobsSelectedMemberButton->setUserString(
            "ToolTip",
            "Delete all queued jobs for the currently selected squad member.");
        g_deleteAllJobsSelectedMemberButton->eventMouseButtonClick
            += MyGUI::newDelegate(&OnDeleteAllJobsSelectedMemberButtonClicked);

        g_jobRowWidgets.clear();
        g_jobRowWidgets.reserve(kMaxVisibleJobRows);
        for (int rowIndex = 0; rowIndex < kMaxVisibleJobRows; ++rowIndex)
        {
            JobRowWidgets rowWidgets = { 0, 0, 0, 0, 0 };
            const int top = GetJobRowTop(rowIndex);
            rowWidgets.label = g_jobBGonePanel->createWidget<MyGUI::TextBox>(
                "Kenshi_TextboxStandardText",
                MyGUI::IntCoord(14, top, 200, 30),
                MyGUI::Align::Default);
            rowWidgets.deleteSelectedMemberButton = g_jobBGonePanel->createWidget<MyGUI::Button>(
                "Kenshi_Button1",
                MyGUI::IntCoord(220, top, 58, 30),
                MyGUI::Align::Default);
            rowWidgets.deleteSelectedMembersButton = g_jobBGonePanel->createWidget<MyGUI::Button>(
                "Kenshi_Button1",
                MyGUI::IntCoord(284, top, 60, 30),
                MyGUI::Align::Default);
            rowWidgets.deleteWholeSquadButton = g_jobBGonePanel->createWidget<MyGUI::Button>(
                "Kenshi_Button1",
                MyGUI::IntCoord(350, top, 82, 30),
                MyGUI::Align::Default);
            rowWidgets.deleteEveryoneButton = g_jobBGonePanel->createWidget<MyGUI::Button>(
                "Kenshi_Button1",
                MyGUI::IntCoord(434, top, 56, 30),
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
            rowWidgets.deleteSelectedMemberButton->setNeedToolTip(true);
            rowWidgets.deleteSelectedMemberButton->setUserString("ToolTip", "Delete this job for selected member.");
            rowWidgets.deleteSelectedMemberButton->eventMouseButtonClick
                += MyGUI::newDelegate(&OnDeleteJobSelectedMemberButtonClicked);

            rowWidgets.deleteSelectedMembersButton->setCaption("Sel");
            rowWidgets.deleteSelectedMembersButton->setNeedToolTip(true);
            rowWidgets.deleteSelectedMembersButton->setUserString("ToolTip", "Delete this job for selected members.");
            rowWidgets.deleteSelectedMembersButton->eventMouseButtonClick
                += MyGUI::newDelegate(&OnDeleteJobSelectedMembersButtonClicked);

            rowWidgets.deleteWholeSquadButton->setCaption("Squad");
            rowWidgets.deleteWholeSquadButton->setNeedToolTip(true);
            rowWidgets.deleteWholeSquadButton->setUserString("ToolTip", "Delete this job for the whole squad.");
            rowWidgets.deleteWholeSquadButton->eventMouseButtonClick
                += MyGUI::newDelegate(&OnDeleteJobWholeSquadButtonClicked);

            rowWidgets.deleteEveryoneButton->setCaption("All");
            rowWidgets.deleteEveryoneButton->setNeedToolTip(true);
            rowWidgets.deleteEveryoneButton->setUserString("ToolTip", "Delete this job for all player squads.");
            rowWidgets.deleteEveryoneButton->eventMouseButtonClick
                += MyGUI::newDelegate(&OnDeleteJobEveryoneButtonClicked);

            g_jobRowWidgets.push_back(rowWidgets);
        }

        DebugLog("Job-B-Gone DEBUG: created Job-B-Gone collapsible panel");
        g_lastLoggedButtonExists = true;
    }

    if (!g_jobBGonePanel || !g_jobBGoneHeaderButton || !g_jobBGoneBodyFrame || !g_jobBGoneStatusText || !g_deleteAllJobsSelectedMemberButton)
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

    const bool shouldShowButton = hasSelectedMember && g_config.enableDeleteAllJobsSelectedMemberAction;
    const bool buttonVisibleNow = shouldShowButton && !g_jobBGonePanelCollapsed;
    const bool showJobRowsNow = selectedMemberCount > 0 && !g_jobBGonePanelCollapsed;
    const bool rowActionsEnabled = selectedMemberCount > 0;
    g_jobBGoneBodyFrame->setVisible(!g_jobBGonePanelCollapsed);
    g_jobBGoneStatusText->setVisible(!g_jobBGonePanelCollapsed);
    g_deleteAllJobsSelectedMemberButton->setVisible(buttonVisibleNow);

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
        std::stringstream rowLabel;
        rowLabel << (i + 1) << ". " << rowModel.taskName;
        if (rowModel.taskName.empty())
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
            rowWidgets.deleteSelectedMemberButton->setEnabled(rowActionsEnabled);
        }
        if (rowWidgets.deleteSelectedMembersButton)
        {
            rowWidgets.deleteSelectedMembersButton->setVisible(true);
            rowWidgets.deleteSelectedMembersButton->setEnabled(rowActionsEnabled);
        }
        if (rowWidgets.deleteWholeSquadButton)
        {
            rowWidgets.deleteWholeSquadButton->setVisible(true);
            rowWidgets.deleteWholeSquadButton->setEnabled(rowActionsEnabled);
        }
        if (rowWidgets.deleteEveryoneButton)
        {
            rowWidgets.deleteEveryoneButton->setVisible(true);
            rowWidgets.deleteEveryoneButton->setEnabled(rowActionsEnabled);
        }
    }
    if (buttonVisibleNow != g_lastLoggedButtonVisibleState)
    {
        std::stringstream logline;
        logline << "Job-B-Gone DEBUG: delete-button visible="
                << (buttonVisibleNow ? "true" : "false")
                << " selected_member_resolved=" << (hasSelectedMember ? "true" : "false")
                << " action_enabled=" << (g_config.enableDeleteAllJobsSelectedMemberAction ? "true" : "false")
                << " button_ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(g_deleteAllJobsSelectedMemberButton);
        DebugLog(logline.str().c_str());
        g_lastLoggedButtonVisibleState = buttonVisibleNow;
    }
}
