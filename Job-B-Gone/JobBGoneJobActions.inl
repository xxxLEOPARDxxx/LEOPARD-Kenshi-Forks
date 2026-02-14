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

static void OnDeleteAllJobsSelectedMemberButtonClicked(MyGUI::Widget*)
{
    const bool actionEnabled = g_config.enableDeleteAllJobsSelectedMemberAction;
    int beforeCount = -1;
    int afterCount = -1;
    int deletedCount = -1;
    const char* result = "disabled_by_config";
    bool success = false;

    if (actionEnabled)
    {
        success = TryDeleteAllJobsForSelectedMember(&beforeCount, &afterCount, &deletedCount, &result);
        if (!result)
        {
            result = success ? "success" : "failed";
        }
        if (success)
        {
            RefreshSelectedMemberUi("post_delete_immediate");
            ArmSelectedMemberUiRefresh("post_delete_deferred");
        }
    }

    std::stringstream logline;
    logline << "Job-B-Gone INFO: action=delete_all_jobs_selected_member"
            << " success=" << (success ? "true" : "false")
            << " result=" << result
            << " before=" << beforeCount
            << " after=" << afterCount
            << " deleted=" << deletedCount
            << " action_enabled=" << (actionEnabled ? "true" : "false")
            << " anchor_mode=job_b_gone_panel";
    DebugLog(logline.str().c_str());
}
