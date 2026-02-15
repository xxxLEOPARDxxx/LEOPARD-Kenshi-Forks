static Character* ResolveSelectedMember()
{
    if (!g_lastPlayerInterface)
    {
        return 0;
    }

    __try
    {
        const hand selectedMemberHandle = g_lastPlayerInterface->selectedCharacter;
        if (!selectedMemberHandle || !selectedMemberHandle.isValid())
        {
            return 0;
        }

        return selectedMemberHandle.getCharacter();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ErrorLog("Job-B-Gone WARN: exception while resolving selected member");
        return 0;
    }
}

static bool TryResolveSelectedMemberForDebug()
{
    return ResolveSelectedMember() != 0;
}

static bool TryGetPermajobCount(Character* member, int* countOut)
{
    if (!member || !countOut)
    {
        return false;
    }

    __try
    {
        int count = member->getPermajobCount();
        if (count < 0)
        {
            count = 0;
        }
        *countOut = count;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ErrorLog("Job-B-Gone WARN: exception while reading permajob count");
        return false;
    }
}

static bool TryGetPermajobRow(
    Character* member,
    int slot,
    TaskType* taskTypeOut,
    std::string* nameOut,
    uintptr_t* taskDataPtrOut)
{
    if (!member || slot < 0)
    {
        return false;
    }

    __try
    {
        const int count = member->getPermajobCount();
        if (slot >= count)
        {
            return false;
        }

        if (taskTypeOut)
        {
            *taskTypeOut = member->getPermajob(slot);
        }
        if (nameOut)
        {
            *nameOut = member->getPermajobName(slot);
        }
        if (taskDataPtrOut)
        {
            const Tasker* taskData = member->getPermajobData(slot);
            *taskDataPtrOut = reinterpret_cast<uintptr_t>(taskData);
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ErrorLog("Job-B-Gone WARN: exception while reading permajob row");
        return false;
    }
}

static std::string BuildJobKeyBase(TaskType taskType, const std::string& taskName, uintptr_t taskDataPtr)
{
    std::stringstream keyBase;
    if (taskDataPtr != 0)
    {
        // Prefer opaque task pointer identity when available to reduce reorder sensitivity.
        keyBase << "ptr:0x" << std::hex << taskDataPtr;
    }
    else
    {
        // Fallback to deterministic signature when task pointer is unavailable.
        keyBase << "sig:type=" << std::dec << static_cast<int>(taskType) << "|name=" << taskName;
    }
    return keyBase.str();
}

static int CountExistingRowsWithKeyBase(const std::vector<JobRowModel>& rows, const std::string& keyBase)
{
    int duplicateOrdinal = 0;
    const size_t size = rows.size();
    for (size_t i = 0; i < size; ++i)
    {
        if (rows[i].jobKeyBase == keyBase)
        {
            ++duplicateOrdinal;
        }
    }
    return duplicateOrdinal;
}

static bool BuildSelectedMemberJobSnapshot(Character* member, std::vector<JobRowModel>* rowsOut, const char** resultOut)
{
    if (rowsOut)
    {
        rowsOut->clear();
    }
    if (resultOut)
    {
        *resultOut = "not_run";
    }
    if (!member || !rowsOut)
    {
        if (resultOut)
        {
            *resultOut = "invalid_input";
        }
        return false;
    }

    int count = 0;
    if (!TryGetPermajobCount(member, &count))
    {
        if (resultOut)
        {
            *resultOut = "count_read_failed";
        }
        return false;
    }

    rowsOut->reserve(static_cast<size_t>(count));
    for (int slot = 0; slot < count; ++slot)
    {
        TaskType taskType = static_cast<TaskType>(0);
        std::string taskName;
        uintptr_t taskDataPtr = 0;
        if (!TryGetPermajobRow(member, slot, &taskType, &taskName, &taskDataPtr))
        {
            if (resultOut)
            {
                *resultOut = "row_read_failed";
            }
            return false;
        }

        JobRowModel row;
        row.slot = slot;
        row.taskType = taskType;
        row.taskName = taskName;
        row.taskDataPtr = taskDataPtr;
        row.jobKeyBase = BuildJobKeyBase(taskType, taskName, taskDataPtr);
        row.duplicateOrdinal = CountExistingRowsWithKeyBase(*rowsOut, row.jobKeyBase);

        std::stringstream key;
        key << row.jobKeyBase << "|dup=" << row.duplicateOrdinal;
        row.jobKey = key.str();

        rowsOut->push_back(row);
    }

    if (resultOut)
    {
        *resultOut = "ok";
    }
    return true;
}

static void ValidateSelectedMemberJobKeyStability(Character* member, const std::vector<JobRowModel>& snapshotRows, const char* phase)
{
    std::vector<JobRowModel> secondSnapshotRows;
    const char* validationResult = "not_run";
    bool stable = false;
    int mismatchCount = 0;
    int duplicateKeyCount = 0;

    if (BuildSelectedMemberJobSnapshot(member, &secondSnapshotRows, &validationResult))
    {
        stable = (snapshotRows.size() == secondSnapshotRows.size());
        if (stable)
        {
            const size_t count = snapshotRows.size();
            for (size_t i = 0; i < count; ++i)
            {
                if (snapshotRows[i].jobKey != secondSnapshotRows[i].jobKey)
                {
                    ++mismatchCount;
                    stable = false;
                }
            }
        }

        const size_t count = snapshotRows.size();
        for (size_t i = 0; i < count; ++i)
        {
            for (size_t j = i + 1; j < count; ++j)
            {
                if (snapshotRows[i].jobKey == snapshotRows[j].jobKey)
                {
                    ++duplicateKeyCount;
                }
            }
        }
    }

    std::stringstream validationLog;
    validationLog << "Job-B-Gone INFO: selected_member_job_key_validation phase="
                  << (phase ? phase : "unknown")
                  << " result=" << validationResult
                  << " stable=" << (stable ? "true" : "false")
                  << " size_a=" << snapshotRows.size()
                  << " size_b=" << secondSnapshotRows.size()
                  << " key_mismatches=" << mismatchCount
                  << " duplicate_keys=" << duplicateKeyCount;
    DebugLog(validationLog.str().c_str());
}

static void LogSelectedMemberJobSnapshot(Character* member, const char* phase)
{
    if (!g_config.logSelectedMemberJobSnapshot || !member)
    {
        return;
    }

    std::vector<JobRowModel> rows;
    const char* snapshotResult = "not_run";
    if (!BuildSelectedMemberJobSnapshot(member, &rows, &snapshotResult))
    {
        std::stringstream warn;
        warn << "Job-B-Gone WARN: selected_member_job_snapshot_failed phase="
             << (phase ? phase : "unknown")
             << " reason=" << snapshotResult;
        ErrorLog(warn.str().c_str());
        return;
    }

    std::stringstream summary;
    summary << "Job-B-Gone INFO: selected_member_job_snapshot phase="
            << (phase ? phase : "unknown")
            << " selected_member_ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(member)
            << std::dec << " count=" << rows.size();
    DebugLog(summary.str().c_str());

    const size_t rowCount = rows.size();
    for (size_t i = 0; i < rowCount; ++i)
    {
        const JobRowModel& row = rows[i];

        std::stringstream rowLog;
        rowLog << "Job-B-Gone INFO: selected_member_job_snapshot_row phase="
               << (phase ? phase : "unknown")
               << " slot=" << row.slot
               << " task_type=" << static_cast<int>(row.taskType)
               << " task_name=\"" << row.taskName << "\""
               << " task_data_ptr=0x" << std::hex << row.taskDataPtr
               << " job_key=\"" << row.jobKey << "\"";
        DebugLog(rowLog.str().c_str());
    }

    ValidateSelectedMemberJobKeyStability(member, rows, phase);
}

static bool TryDeleteAllJobsForSelectedMember(int* beforeOut, int* afterOut, int* deletedOut, const char** resultOut)
{
    if (beforeOut)
    {
        *beforeOut = -1;
    }
    if (afterOut)
    {
        *afterOut = -1;
    }
    if (deletedOut)
    {
        *deletedOut = -1;
    }
    if (resultOut)
    {
        *resultOut = "not_run";
    }

    Character* member = ResolveSelectedMember();
    if (!member)
    {
        if (resultOut)
        {
            *resultOut = "no_selected_member";
        }
        return false;
    }

    int beforeCount = 0;
    if (!TryGetPermajobCount(member, &beforeCount))
    {
        if (resultOut)
        {
            *resultOut = "before_count_failed";
        }
        return false;
    }
    if (beforeOut)
    {
        *beforeOut = beforeCount;
    }

    LogSelectedMemberJobSnapshot(member, "before_delete_all");

    __try
    {
        member->clearPermajobs();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (resultOut)
        {
            *resultOut = "clear_exception";
        }
        return false;
    }

    int afterCount = 0;
    if (!TryGetPermajobCount(member, &afterCount))
    {
        if (resultOut)
        {
            *resultOut = "after_count_failed";
        }
        return false;
    }
    if (afterOut)
    {
        *afterOut = afterCount;
    }

    const int deletedCount = (beforeCount >= afterCount) ? (beforeCount - afterCount) : 0;
    if (deletedOut)
    {
        *deletedOut = deletedCount;
    }

    LogSelectedMemberJobSnapshot(member, "after_delete_all");

    if (beforeCount == 0 && afterCount == 0)
    {
        if (resultOut)
        {
            *resultOut = "already_empty";
        }
        return true;
    }

    if (afterCount == 0)
    {
        if (resultOut)
        {
            *resultOut = "cleared_all";
        }
        return true;
    }

    if (resultOut)
    {
        *resultOut = "non_empty_after_clear";
    }
    return false;
}

static void ExecuteDeleteAllJobsForScope(JobDeleteScope scope);
static void ExecuteDeleteJobForScope(
    JobDeleteScope scope,
    const std::string& jobKey,
    TaskType taskType,
    const std::string& taskName);

static bool ShouldRequireConfirmationForScope(JobDeleteScope scope)
{
    return scope == JobDeleteScope_WholeSquad || scope == JobDeleteScope_EveryoneGlobal;
}

static void OnDeleteAllJobsSelectedMemberButtonClicked(MyGUI::Widget*)
{
    if (ShouldRequireConfirmationForScope(JobDeleteScope_SelectedMember) && ShowDeleteAllConfirmation(JobDeleteScope_SelectedMember))
    {
        return;
    }
    ExecuteDeleteAllJobsForScope(JobDeleteScope_SelectedMember);
}

static void OnDeleteAllJobsSelectedMembersButtonClicked(MyGUI::Widget*)
{
    if (ShouldRequireConfirmationForScope(JobDeleteScope_SelectedMembers) && ShowDeleteAllConfirmation(JobDeleteScope_SelectedMembers))
    {
        return;
    }
    ExecuteDeleteAllJobsForScope(JobDeleteScope_SelectedMembers);
}

static void OnDeleteAllJobsWholeSquadButtonClicked(MyGUI::Widget*)
{
    if (ShouldRequireConfirmationForScope(JobDeleteScope_WholeSquad) && ShowDeleteAllConfirmation(JobDeleteScope_WholeSquad))
    {
        return;
    }
    ExecuteDeleteAllJobsForScope(JobDeleteScope_WholeSquad);
}

static void OnDeleteAllJobsEveryoneButtonClicked(MyGUI::Widget*)
{
    if (ShouldRequireConfirmationForScope(JobDeleteScope_EveryoneGlobal)
        && ShowDeleteAllConfirmation(JobDeleteScope_EveryoneGlobal))
    {
        return;
    }
    ExecuteDeleteAllJobsForScope(JobDeleteScope_EveryoneGlobal);
}

static const char* GetScopeLogName(JobDeleteScope scope)
{
    switch (scope)
    {
    case JobDeleteScope_SelectedMember:
        return "selected_member";
    case JobDeleteScope_SelectedMembers:
        return "selected_members";
    case JobDeleteScope_WholeSquad:
        return "whole_squad";
    case JobDeleteScope_EveryoneGlobal:
        return "everyone_global";
    default:
        break;
    }
    return "unknown";
}

static bool TryRemovePermajobAtSlot(Character* member, int slot, const char** resultOut);

static void AddUniqueMember(std::vector<Character*>* membersOut, Character* member)
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

static bool ResolveMembersForScope(JobDeleteScope scope, std::vector<Character*>* membersOut, const char** resultOut)
{
    if (membersOut)
    {
        membersOut->clear();
    }
    if (resultOut)
    {
        *resultOut = "not_run";
    }
    if (!membersOut || !g_lastPlayerInterface)
    {
        if (resultOut)
        {
            *resultOut = "player_interface_unavailable";
        }
        return false;
    }

    Character* selectedMember = ResolveSelectedMember();
    if (scope == JobDeleteScope_SelectedMember)
    {
        if (!selectedMember)
        {
            if (resultOut)
            {
                *resultOut = "no_selected_member";
            }
            return false;
        }
        AddUniqueMember(membersOut, selectedMember);
        if (resultOut)
        {
            *resultOut = "ok";
        }
        return true;
    }

    if (scope == JobDeleteScope_SelectedMembers)
    {
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
                AddUniqueMember(membersOut, member);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            if (resultOut)
            {
                *resultOut = "selected_members_exception";
            }
            return false;
        }

        if (membersOut->empty() && selectedMember)
        {
            AddUniqueMember(membersOut, selectedMember);
        }

        if (membersOut->empty())
        {
            if (resultOut)
            {
                *resultOut = "no_selected_members";
            }
            return false;
        }

        if (resultOut)
        {
            *resultOut = "ok";
        }
        return true;
    }

    const lektor<Character*>& playerCharacters = g_lastPlayerInterface->getAllPlayerCharacters();
    if (scope == JobDeleteScope_EveryoneGlobal)
    {
        const uint32_t count = playerCharacters.size();
        for (uint32_t i = 0; i < count; ++i)
        {
            Character* member = playerCharacters[i];
            if (!member)
            {
                continue;
            }

            bool isPlayer = false;
            __try
            {
                isPlayer = member->isPlayerCharacter();
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                continue;
            }

            if (isPlayer)
            {
                AddUniqueMember(membersOut, member);
            }
        }

        if (membersOut->empty())
        {
            if (resultOut)
            {
                *resultOut = "no_player_members";
            }
            return false;
        }

        if (resultOut)
        {
            *resultOut = "ok";
        }
        return true;
    }

    if (scope == JobDeleteScope_WholeSquad)
    {
        if (!selectedMember)
        {
            if (resultOut)
            {
                *resultOut = "no_selected_member";
            }
            return false;
        }

        Character* selectedLeader = 0;
        __try
        {
            selectedLeader = selectedMember->getSquadLeader();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            selectedLeader = 0;
        }
        if (!selectedLeader)
        {
            selectedLeader = selectedMember;
        }

        const uint32_t count = playerCharacters.size();
        for (uint32_t i = 0; i < count; ++i)
        {
            Character* member = playerCharacters[i];
            if (!member)
            {
                continue;
            }

            Character* leader = 0;
            bool isPlayer = false;
            __try
            {
                leader = member->getSquadLeader();
                isPlayer = member->isPlayerCharacter();
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                continue;
            }

            if (!leader)
            {
                leader = member;
            }

            if (isPlayer && leader == selectedLeader)
            {
                AddUniqueMember(membersOut, member);
            }
        }

        if (membersOut->empty())
        {
            AddUniqueMember(membersOut, selectedMember);
        }

        if (membersOut->empty())
        {
            if (resultOut)
            {
                *resultOut = "no_whole_squad_members";
            }
            return false;
        }

        if (resultOut)
        {
            *resultOut = "ok";
        }
        return true;
    }

    if (resultOut)
    {
        *resultOut = "unsupported_scope";
    }
    return false;
}

static const char* GetScopeDisplayName(JobDeleteScope scope)
{
    switch (scope)
    {
    case JobDeleteScope_SelectedMember:
        return "Me";
    case JobDeleteScope_SelectedMembers:
        return "Selected";
    case JobDeleteScope_WholeSquad:
        return "Squad";
    case JobDeleteScope_EveryoneGlobal:
        return "All Squads";
    default:
        break;
    }
    return "Unknown";
}

static bool MemberHasMatchingJobByKey(Character* member, const std::string& jobKey)
{
    if (!member || jobKey.empty())
    {
        return false;
    }

    std::vector<JobRowModel> rows;
    const char* snapshotResult = "not_run";
    if (!BuildSelectedMemberJobSnapshot(member, &rows, &snapshotResult))
    {
        return false;
    }

    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (rows[i].jobKey == jobKey)
        {
            return true;
        }
    }
    return false;
}

static bool MemberHasMatchingJobBySignature(Character* member, TaskType taskType, const std::string& taskName)
{
    if (!member)
    {
        return false;
    }

    int count = 0;
    if (!TryGetPermajobCount(member, &count))
    {
        return false;
    }

    for (int slot = 0; slot < count; ++slot)
    {
        TaskType rowTaskType = static_cast<TaskType>(0);
        std::string rowTaskName;
        if (!TryGetPermajobRow(member, slot, &rowTaskType, &rowTaskName, 0))
        {
            continue;
        }
        if (rowTaskType == taskType && rowTaskName == taskName)
        {
            return true;
        }
    }
    return false;
}

static void HideConfirmationOverlay()
{
    g_jobBGoneConfirmVisible = false;
    if (g_jobBGoneConfirmOverlay)
    {
        g_jobBGoneConfirmOverlay->setVisible(false);
    }
    if (g_jobBGoneConfirmTitleText)
    {
        g_jobBGoneConfirmTitleText->setVisible(false);
        g_jobBGoneConfirmTitleText->setCaption("");
    }
    if (g_jobBGoneConfirmBodyText)
    {
        g_jobBGoneConfirmBodyText->setVisible(false);
        g_jobBGoneConfirmBodyText->setCaption("");
    }
    if (g_jobBGoneConfirmYesButton)
    {
        g_jobBGoneConfirmYesButton->setVisible(false);
    }
    if (g_jobBGoneConfirmNoButton)
    {
        g_jobBGoneConfirmNoButton->setVisible(false);
    }
    if (g_jobBGoneHoverHintText)
    {
        g_jobBGoneHoverHintText->setCaption("");
    }

    g_pendingConfirmationAction.type = PendingConfirmationAction_None;
    g_pendingConfirmationAction.scope = JobDeleteScope_SelectedMember;
    g_pendingConfirmationAction.jobKey.clear();
    g_pendingConfirmationAction.taskType = static_cast<TaskType>(0);
    g_pendingConfirmationAction.taskName.clear();
}

static bool ShowConfirmationOverlay(
    PendingConfirmationActionType type,
    JobDeleteScope scope,
    const std::string& jobKey,
    TaskType taskType,
    const std::string& taskName,
    const std::string& title,
    const std::string& body)
{
    if (!g_jobBGoneConfirmOverlay || !g_jobBGoneConfirmTitleText || !g_jobBGoneConfirmBodyText
        || !g_jobBGoneConfirmYesButton || !g_jobBGoneConfirmNoButton)
    {
        DebugLog("Job-B-Gone WARN: confirmation_overlay_missing_widgets");
        return false;
    }

    g_pendingConfirmationAction.type = type;
    g_pendingConfirmationAction.scope = scope;
    g_pendingConfirmationAction.jobKey = jobKey;
    g_pendingConfirmationAction.taskType = taskType;
    g_pendingConfirmationAction.taskName = taskName;
    g_jobBGoneConfirmTitleText->setCaption(title);
    g_jobBGoneConfirmBodyText->setCaption(body);
    g_jobBGoneConfirmVisible = true;

    const MyGUI::IntCoord overlayCoord = g_jobBGoneConfirmOverlay->getCoord();
    std::stringstream info;
    info << "Job-B-Gone DEBUG: confirmation_overlay_show"
         << " type=" << static_cast<int>(type)
         << " scope=" << GetScopeLogName(scope)
         << " x=" << overlayCoord.left
         << " y=" << overlayCoord.top
         << " w=" << overlayCoord.width
         << " h=" << overlayCoord.height;
    DebugLog(info.str().c_str());

    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    if (input && g_jobBGoneConfirmYesButton)
    {
        input->setKeyFocusWidget(g_jobBGoneConfirmYesButton);
    }
    return true;
}

struct JobNameCountEntry
{
    std::string name;
    int count;
};

static void AccumulateJobNameCount(std::vector<JobNameCountEntry>* namesOut, const std::string& taskName)
{
    if (!namesOut)
    {
        return;
    }

    const std::string normalizedName = taskName.empty() ? std::string("(Unnamed job)") : taskName;
    for (size_t i = 0; i < namesOut->size(); ++i)
    {
        if ((*namesOut)[i].name == normalizedName)
        {
            ++((*namesOut)[i].count);
            return;
        }
    }

    JobNameCountEntry entry;
    entry.name = normalizedName;
    entry.count = 1;
    namesOut->push_back(entry);
}

static int BuildDeleteAllJobNameCounts(
    const std::vector<Character*>& targetList,
    std::vector<JobNameCountEntry>* namesOut)
{
    if (namesOut)
    {
        namesOut->clear();
    }

    int jobsToDelete = 0;
    for (size_t i = 0; i < targetList.size(); ++i)
    {
        Character* member = targetList[i];
        if (!member)
        {
            continue;
        }

        std::vector<JobRowModel> rows;
        const char* snapshotResult = "not_run";
        if (!BuildSelectedMemberJobSnapshot(member, &rows, &snapshotResult))
        {
            int memberCount = 0;
            if (TryGetPermajobCount(member, &memberCount) && memberCount > 0)
            {
                jobsToDelete += memberCount;
                for (int missing = 0; missing < memberCount; ++missing)
                {
                    AccumulateJobNameCount(namesOut, "(Unknown job)");
                }
            }
            continue;
        }

        jobsToDelete += static_cast<int>(rows.size());
        for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
        {
            AccumulateJobNameCount(namesOut, rows[rowIndex].taskName);
        }
    }

    return jobsToDelete;
}

static bool ShowDeleteAllConfirmation(JobDeleteScope scope)
{
    std::vector<Character*> targetList;
    const char* result = "not_run";
    if (!ResolveMembersForScope(scope, &targetList, &result))
    {
        return false;
    }

    std::vector<JobNameCountEntry> jobNameCounts;
    const int jobsToDelete = BuildDeleteAllJobNameCounts(targetList, &jobNameCounts);

    std::stringstream title;
    title << "Confirm " << GetScopeDisplayName(scope) << " Delete-All";
    std::stringstream body;
    body << "Scope: " << GetScopeDisplayName(scope)
         << "\nMembers affected: " << targetList.size()
         << "\nJobs queued to delete: " << jobsToDelete;
    if (!jobNameCounts.empty())
    {
        body << "\n\nJobs to delete:";
        const size_t maxVisibleJobTypes = 10;
        size_t visibleTypes = jobNameCounts.size();
        if (visibleTypes > maxVisibleJobTypes)
        {
            visibleTypes = maxVisibleJobTypes;
        }

        for (size_t i = 0; i < visibleTypes; ++i)
        {
            body << "\n" << (i + 1) << ". " << jobNameCounts[i].name;
            if (jobNameCounts[i].count > 1)
            {
                body << " x" << jobNameCounts[i].count;
            }
        }

        if (jobNameCounts.size() > visibleTypes)
        {
            body << "\n... +" << (jobNameCounts.size() - visibleTypes) << " more job types";
        }
    }
    else
    {
        body << "\n\nNo jobs found in this scope.";
    }
    body << "\n\nPress Enter to confirm.";
    return ShowConfirmationOverlay(
        PendingConfirmationAction_DeleteAllScope,
        scope,
        std::string(),
        static_cast<TaskType>(0),
        std::string(),
        title.str(),
        body.str());
}

static bool ShowDeleteRowConfirmation(
    JobDeleteScope scope,
    const std::string& jobKey,
    TaskType taskType,
    const std::string& taskName)
{
    std::vector<Character*> targetList;
    const char* result = "not_run";
    if (!ResolveMembersForScope(scope, &targetList, &result))
    {
        return false;
    }

    int jobsToDelete = 0;
    for (size_t i = 0; i < targetList.size(); ++i)
    {
        bool found = false;
        if (scope == JobDeleteScope_SelectedMember)
        {
            found = MemberHasMatchingJobByKey(targetList[i], jobKey);
        }
        else
        {
            found = MemberHasMatchingJobBySignature(targetList[i], taskType, taskName);
        }
        if (found)
        {
            ++jobsToDelete;
        }
    }

    std::stringstream title;
    title << "Confirm " << GetScopeDisplayName(scope) << " Job Delete";
    std::stringstream body;
    body << "Scope: " << GetScopeDisplayName(scope)
         << "\nMembers affected: " << targetList.size()
         << "\nJobs queued to delete: " << jobsToDelete
         << "\nTask: " << taskName
         << "\n\nPress Enter to confirm.";
    return ShowConfirmationOverlay(
        PendingConfirmationAction_DeleteRowScope,
        scope,
        jobKey,
        taskType,
        taskName,
        title.str(),
        body.str());
}

static void ExecutePendingConfirmationAction()
{
    const PendingConfirmationAction pending = g_pendingConfirmationAction;
    HideConfirmationOverlay();

    if (pending.type == PendingConfirmationAction_DeleteAllScope)
    {
        ExecuteDeleteAllJobsForScope(pending.scope);
        return;
    }

    if (pending.type == PendingConfirmationAction_DeleteRowScope)
    {
        ExecuteDeleteJobForScope(
            pending.scope,
            pending.jobKey,
            pending.taskType,
            pending.taskName);
    }
}

static void OnConfirmationAcceptClicked(MyGUI::Widget*)
{
    if (!g_jobBGoneConfirmVisible)
    {
        return;
    }
    ExecutePendingConfirmationAction();
}

static void OnConfirmationCancelClicked(MyGUI::Widget*)
{
    if (!g_jobBGoneConfirmVisible)
    {
        return;
    }
    HideConfirmationOverlay();
}

static void OnConfirmationKeyPressed(MyGUI::Widget*, MyGUI::KeyCode key, MyGUI::Char)
{
    if (!g_jobBGoneConfirmVisible)
    {
        return;
    }

    if (key == MyGUI::KeyCode::Return || key == MyGUI::KeyCode::NumpadEnter)
    {
        ExecutePendingConfirmationAction();
        return;
    }
    if (key == MyGUI::KeyCode::Escape)
    {
        HideConfirmationOverlay();
    }
}

static bool TryClearAllJobsForMember(Character* member, int* deletedOut, const char** resultOut)
{
    if (deletedOut)
    {
        *deletedOut = 0;
    }
    if (resultOut)
    {
        *resultOut = "not_run";
    }
    if (!member)
    {
        if (resultOut)
        {
            *resultOut = "invalid_member";
        }
        return false;
    }

    int beforeCount = 0;
    if (!TryGetPermajobCount(member, &beforeCount))
    {
        if (resultOut)
        {
            *resultOut = "before_count_failed";
        }
        return false;
    }

    __try
    {
        member->clearPermajobs();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (resultOut)
        {
            *resultOut = "clear_exception";
        }
        return false;
    }

    int afterCount = 0;
    if (!TryGetPermajobCount(member, &afterCount))
    {
        if (resultOut)
        {
            *resultOut = "after_count_failed";
        }
        return false;
    }

    const int memberDeleted = (beforeCount >= afterCount) ? (beforeCount - afterCount) : 0;
    if (deletedOut)
    {
        *deletedOut = memberDeleted;
    }
    if (resultOut)
    {
        *resultOut = (memberDeleted > 0) ? "ok" : "already_empty";
    }
    return true;
}

static void ExecuteDeleteAllJobsForScope(JobDeleteScope scope)
{
    const bool actionEnabled = g_config.enableDeleteAllJobsSelectedMemberAction;
    bool success = false;
    const char* result = "disabled_by_config";
    int targetMembers = 0;
    int affectedMembers = 0;
    int deletedJobs = 0;

    if (actionEnabled)
    {
        if (scope == JobDeleteScope_SelectedMember)
        {
            int beforeCount = -1;
            int afterCount = -1;
            int deletedCount = -1;
            success = TryDeleteAllJobsForSelectedMember(&beforeCount, &afterCount, &deletedCount, &result);
            targetMembers = (beforeCount >= 0 || afterCount >= 0) ? 1 : 0;
            if (deletedCount > 0)
            {
                affectedMembers = 1;
                deletedJobs = deletedCount;
            }
            else if (success && beforeCount == 0 && afterCount == 0)
            {
                affectedMembers = 0;
                deletedJobs = 0;
            }
        }
        else
        {
            std::vector<Character*> targetList;
            if (!ResolveMembersForScope(scope, &targetList, &result))
            {
                if (!result)
                {
                    result = "resolve_members_failed";
                }
            }
            else
            {
                targetMembers = static_cast<int>(targetList.size());
                bool encounteredFailure = false;
                for (size_t i = 0; i < targetList.size(); ++i)
                {
                    Character* member = targetList[i];
                    int memberDeleted = 0;
                    const char* memberResult = "not_run";
                    if (!TryClearAllJobsForMember(member, &memberDeleted, &memberResult))
                    {
                        encounteredFailure = true;
                        continue;
                    }

                    if (memberDeleted > 0)
                    {
                        ++affectedMembers;
                        deletedJobs += memberDeleted;
                    }
                }

                if (deletedJobs > 0)
                {
                    success = true;
                    result = "ok";
                }
                else if (!encounteredFailure)
                {
                    success = true;
                    result = "already_empty";
                }
                else
                {
                    success = false;
                    result = "delete_failed_for_scope";
                }
            }
        }
    }

    if (success)
    {
        RefreshSelectedMemberUi("post_delete_all_scope_immediate");
        ArmSelectedMemberUiRefresh("post_delete_all_scope_deferred");
    }

    std::stringstream logline;
    logline << "Job-B-Gone INFO: action=delete_all_jobs_scope"
            << " scope=" << GetScopeLogName(scope)
            << " success=" << (success ? "true" : "false")
            << " result=" << (result ? result : "unknown")
            << " target_members=" << targetMembers
            << " affected_members=" << affectedMembers
            << " deleted_jobs=" << deletedJobs
            << " action_enabled=" << (actionEnabled ? "true" : "false")
            << " anchor_mode=job_b_gone_panel";
    DebugLog(logline.str().c_str());
}

static bool TryDeleteJobForMemberByKey(Character* member, const std::string& jobKey, const char** resultOut)
{
    if (resultOut)
    {
        *resultOut = "not_run";
    }
    if (!member || jobKey.empty())
    {
        if (resultOut)
        {
            *resultOut = "invalid_input";
        }
        return false;
    }

    std::vector<JobRowModel> rows;
    const char* snapshotResult = "not_run";
    if (!BuildSelectedMemberJobSnapshot(member, &rows, &snapshotResult))
    {
        if (resultOut)
        {
            *resultOut = snapshotResult;
        }
        return false;
    }

    int slotToDelete = -1;
    const size_t count = rows.size();
    for (size_t i = 0; i < count; ++i)
    {
        if (rows[i].jobKey == jobKey)
        {
            slotToDelete = rows[i].slot;
            break;
        }
    }

    if (slotToDelete < 0)
    {
        if (resultOut)
        {
            *resultOut = "job_key_not_found";
        }
        return false;
    }

    return TryRemovePermajobAtSlot(member, slotToDelete, resultOut);
}

static bool TryRemovePermajobAtSlot(Character* member, int slot, const char** resultOut)
{
    if (resultOut)
    {
        *resultOut = "not_run";
    }
    if (!member || slot < 0)
    {
        if (resultOut)
        {
            *resultOut = "invalid_input";
        }
        return false;
    }

    __try
    {
        member->removePermajob(slot);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (resultOut)
        {
            *resultOut = "remove_exception";
        }
        return false;
    }

    if (resultOut)
    {
        *resultOut = "ok";
    }
    return true;
}

static bool TryDeleteJobForMemberBySignature(
    Character* member,
    TaskType taskType,
    const std::string& taskName,
    const char** resultOut)
{
    if (resultOut)
    {
        *resultOut = "not_run";
    }
    if (!member)
    {
        if (resultOut)
        {
            *resultOut = "invalid_input";
        }
        return false;
    }

    int count = 0;
    if (!TryGetPermajobCount(member, &count))
    {
        if (resultOut)
        {
            *resultOut = "count_read_failed";
        }
        return false;
    }

    int slotToDelete = -1;
    for (int slot = 0; slot < count; ++slot)
    {
        TaskType rowTaskType = static_cast<TaskType>(0);
        std::string rowTaskName;
        if (!TryGetPermajobRow(member, slot, &rowTaskType, &rowTaskName, 0))
        {
            continue;
        }

        if (rowTaskType == taskType && rowTaskName == taskName)
        {
            slotToDelete = slot;
            break;
        }
    }

    if (slotToDelete < 0)
    {
        if (resultOut)
        {
            *resultOut = "job_signature_not_found";
        }
        return false;
    }

    return TryRemovePermajobAtSlot(member, slotToDelete, resultOut);
}

static bool TryGetRowActionPayload(MyGUI::Widget* sender, std::string* jobKeyOut, TaskType* taskTypeOut, std::string* taskNameOut)
{
    if (!sender || !jobKeyOut || !taskTypeOut || !taskNameOut)
    {
        return false;
    }

    const std::string jobKey = sender->getUserString("JobKey");
    const std::string taskTypeValue = sender->getUserString("JobTaskType");
    const std::string taskName = sender->getUserString("JobTaskName");
    if (jobKey.empty() || taskTypeValue.empty())
    {
        return false;
    }

    int taskTypeInt = 0;
    std::stringstream parser(taskTypeValue);
    parser >> taskTypeInt;
    if (!parser || !parser.eof())
    {
        return false;
    }

    *jobKeyOut = jobKey;
    *taskTypeOut = static_cast<TaskType>(taskTypeInt);
    *taskNameOut = taskName;
    return true;
}

static void ExecuteDeleteJobForScope(
    JobDeleteScope scope,
    const std::string& jobKey,
    TaskType taskType,
    const std::string& taskName)
{
    const bool actionEnabled = true;
    bool success = false;
    const char* result = "disabled_by_config";
    int targetMembers = 0;
    int affectedMembers = 0;
    int deletedJobs = 0;

    if (actionEnabled)
    {
        std::vector<Character*> targetList;
        if (!ResolveMembersForScope(scope, &targetList, &result))
        {
            if (!result)
            {
                result = "resolve_members_failed";
            }
        }
        else
        {
            targetMembers = static_cast<int>(targetList.size());
            for (size_t i = 0; i < targetList.size(); ++i)
            {
                Character* member = targetList[i];
                const char* memberResult = "not_run";
                bool memberDeleted = false;
                if (scope == JobDeleteScope_SelectedMember)
                {
                    memberDeleted = TryDeleteJobForMemberByKey(member, jobKey, &memberResult);
                    if (!memberDeleted)
                    {
                        memberDeleted = TryDeleteJobForMemberBySignature(member, taskType, taskName, &memberResult);
                    }
                }
                else
                {
                    memberDeleted = TryDeleteJobForMemberBySignature(member, taskType, taskName, &memberResult);
                }

                if (memberDeleted)
                {
                    ++affectedMembers;
                    ++deletedJobs;
                }
            }

            success = deletedJobs > 0;
            result = success ? "ok" : "job_not_found_in_scope";
        }
    }

    if (success)
    {
        RefreshSelectedMemberUi("post_delete_row_scope_immediate");
        ArmSelectedMemberUiRefresh("post_delete_row_scope_deferred");
    }

    std::stringstream logline;
    logline << "Job-B-Gone INFO: action=delete_job_scope"
            << " scope=" << GetScopeLogName(scope)
            << " success=" << (success ? "true" : "false")
            << " result=" << (result ? result : "unknown")
            << " target_members=" << targetMembers
            << " affected_members=" << affectedMembers
            << " deleted_jobs=" << deletedJobs
            << " task_type=" << static_cast<int>(taskType)
            << " task_name=\"" << taskName << "\""
            << " action_enabled=" << (actionEnabled ? "true" : "false");
    DebugLog(logline.str().c_str());
}

static void ExecuteDeleteJobForScopeFromRow(MyGUI::Widget* sender, JobDeleteScope scope)
{
    std::string jobKey;
    std::string taskName;
    TaskType taskType = static_cast<TaskType>(0);
    if (!TryGetRowActionPayload(sender, &jobKey, &taskType, &taskName))
    {
        DebugLog("Job-B-Gone INFO: action=delete_job_scope success=false result=invalid_row_payload");
        return;
    }

    if (ShouldRequireConfirmationForScope(scope) && ShowDeleteRowConfirmation(scope, jobKey, taskType, taskName))
    {
        return;
    }

    ExecuteDeleteJobForScope(scope, jobKey, taskType, taskName);
}

static void OnDeleteJobSelectedMemberButtonClicked(MyGUI::Widget* sender)
{
    ExecuteDeleteJobForScopeFromRow(sender, JobDeleteScope_SelectedMember);
}

static void OnDeleteJobSelectedMembersButtonClicked(MyGUI::Widget* sender)
{
    ExecuteDeleteJobForScopeFromRow(sender, JobDeleteScope_SelectedMembers);
}

static void OnDeleteJobWholeSquadButtonClicked(MyGUI::Widget* sender)
{
    ExecuteDeleteJobForScopeFromRow(sender, JobDeleteScope_WholeSquad);
}

static void OnDeleteJobEveryoneButtonClicked(MyGUI::Widget* sender)
{
    ExecuteDeleteJobForScopeFromRow(sender, JobDeleteScope_EveryoneGlobal);
}
