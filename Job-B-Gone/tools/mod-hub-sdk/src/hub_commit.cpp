#include "hub_commit.h"

#include "logging.h"
#include "hub_ui.h"

#include <Debug.h>

#include <cstring>
#include <sstream>

namespace
{
const char* kLogNone = "none";
const char* kSetCallbackFailedMessage = "set_callback_failed";
const char* kGetCallbackFailedMessage = "get_callback_failed";
const char* kInvalidBoolValueMessage = "invalid_bool_value";

HubCommitSummary g_last_summary = { 0u, 0u, 0u, 0u, HUB_COMMIT_SKIP_REASON_NONE };

const char* SafeLogValue(const char* value)
{
    if (value == nullptr || value[0] == '\0')
    {
        return kLogNone;
    }

    return value;
}

const char* ResolveErrorMessage(const char* err_buf, const char* fallback)
{
    if (err_buf != nullptr && err_buf[0] != '\0')
    {
        return err_buf;
    }

    return fallback;
}

const char* SkipReasonToText(int32_t skip_reason)
{
    if (skip_reason == HUB_COMMIT_SKIP_REASON_KEYBIND_CAPTURE_ACTIVE)
    {
        return "keybind_capture_active";
    }

    return kLogNone;
}

void LogCommitFailure(const HubUiRowView& row, EMC_Result result, const char* message)
{
    std::ostringstream line;
    line << "event=hub_commit_failure"
         << " namespace=" << SafeLogValue(row.namespace_id)
         << " mod=" << SafeLogValue(row.mod_id)
         << " setting=" << SafeLogValue(row.setting_id)
         << " result=" << result
         << " message=" << SafeLogValue(message);
    ErrorLog(line.str().c_str());
}

void LogCommitGetFailure(const HubUiRowView& row, EMC_Result result, const char* message)
{
    std::ostringstream line;
    line << "event=hub_commit_get_failure"
         << " namespace=" << SafeLogValue(row.namespace_id)
         << " mod=" << SafeLogValue(row.mod_id)
         << " setting=" << SafeLogValue(row.setting_id)
         << " result=" << result
         << " message=" << SafeLogValue(message);
    ErrorLog(line.str().c_str());
}

void LogCommitSummary(const HubCommitSummary& summary)
{
    std::ostringstream line;
    line << "event=hub_commit_summary"
         << " attempted=" << summary.attempted
         << " succeeded=" << summary.succeeded
         << " failed=" << summary.failed
         << " skipped=" << summary.skipped
         << " reason=" << SkipReasonToText(summary.skip_reason);
    LogDebugLine(line.str());
}

bool IsBoolValueValid(int32_t value)
{
    return value == 0 || value == 1;
}

bool FloatBitsEqual(float lhs, float rhs)
{
    uint32_t lhs_bits = 0;
    uint32_t rhs_bits = 0;
    std::memcpy(&lhs_bits, &lhs, sizeof(lhs_bits));
    std::memcpy(&rhs_bits, &rhs, sizeof(rhs_bits));
    return lhs_bits == rhs_bits;
}
}

void HubCommit_RunOptionsSave()
{
    HubCommitSummary summary;
    summary.attempted = 0u;
    summary.succeeded = 0u;
    summary.failed = 0u;
    summary.skipped = 0u;
    summary.skip_reason = HUB_COMMIT_SKIP_REASON_NONE;

    if (HubUi_IsAnyKeybindCaptureActive())
    {
        summary.skipped = 1u;
        summary.skip_reason = HUB_COMMIT_SKIP_REASON_KEYBIND_CAPTURE_ACTIVE;
        g_last_summary = summary;
        LogCommitSummary(summary);
        return;
    }

    const uint32_t row_count = HubUi_GetRowCount();
    for (uint32_t row_index = 0; row_index < row_count; ++row_index)
    {
        HubUiRowView row;
        if (!HubUi_GetRowViewByIndex(row_index, &row))
        {
            continue;
        }

        if (!row.dirty)
        {
            continue;
        }

        if (row.kind != HUB_UI_ROW_KIND_BOOL
            && row.kind != HUB_UI_ROW_KIND_KEYBIND
            && row.kind != HUB_UI_ROW_KIND_INT
            && row.kind != HUB_UI_ROW_KIND_FLOAT)
        {
            continue;
        }

        if ((row.kind == HUB_UI_ROW_KIND_INT && row.int_text_parse_error)
            || (row.kind == HUB_UI_ROW_KIND_FLOAT && row.float_text_parse_error))
        {
            continue;
        }

        summary.attempted += 1u;
        char err_buf[256];
        std::memset(err_buf, 0, sizeof(err_buf));

        if (row.kind == HUB_UI_ROW_KIND_BOOL)
        {
            EMC_Result set_result = EMC_ERR_INVALID_ARGUMENT;
            if (row.set_bool != nullptr)
            {
                set_result = row.set_bool(row.user_data, row.pending_bool_value, err_buf, (uint32_t)sizeof(err_buf));
            }

            if (set_result != EMC_OK)
            {
                summary.failed += 1u;
                const char* message = ResolveErrorMessage(err_buf, kSetCallbackFailedMessage);
                HubUi_OnCommitSetFailure(row.token, message);
                LogCommitFailure(row, set_result, message);
                continue;
            }

            summary.succeeded += 1u;

            int32_t canonical_value = row.pending_bool_value;
            EMC_Result get_result = EMC_ERR_INVALID_ARGUMENT;
            const char* get_message = kGetCallbackFailedMessage;
            if (row.get_bool != nullptr)
            {
                get_result = row.get_bool(row.user_data, &canonical_value);
                if (get_result == EMC_OK && !IsBoolValueValid(canonical_value))
                {
                    get_result = EMC_ERR_CALLBACK_FAILED;
                    get_message = kInvalidBoolValueMessage;
                }
            }

            if (get_result != EMC_OK)
            {
                LogCommitGetFailure(row, get_result, get_message);
                HubUi_OnCommitSyncBool(row.token, row.pending_bool_value);
                continue;
            }

            HubUi_OnCommitSyncBool(row.token, canonical_value);
            continue;
        }

        if (row.kind == HUB_UI_ROW_KIND_KEYBIND)
        {
            EMC_Result set_result = EMC_ERR_INVALID_ARGUMENT;
            if (row.set_keybind != nullptr)
            {
                set_result = row.set_keybind(row.user_data, row.pending_keybind_value, err_buf, (uint32_t)sizeof(err_buf));
            }

            if (set_result != EMC_OK)
            {
                summary.failed += 1u;
                const char* message = ResolveErrorMessage(err_buf, kSetCallbackFailedMessage);
                HubUi_OnCommitSetFailure(row.token, message);
                LogCommitFailure(row, set_result, message);
                continue;
            }

            summary.succeeded += 1u;

            EMC_KeybindValueV1 canonical_value = row.pending_keybind_value;
            EMC_Result get_result = EMC_ERR_INVALID_ARGUMENT;
            const char* get_message = kGetCallbackFailedMessage;
            if (row.get_keybind != nullptr)
            {
                get_result = row.get_keybind(row.user_data, &canonical_value);
            }

            if (get_result != EMC_OK)
            {
                LogCommitGetFailure(row, get_result, get_message);
                HubUi_OnCommitSyncKeybind(row.token, row.pending_keybind_value);
                continue;
            }

            HubUi_OnCommitSyncKeybind(row.token, canonical_value);
            continue;
        }

        if (row.kind == HUB_UI_ROW_KIND_INT)
        {
            EMC_Result set_result = EMC_ERR_INVALID_ARGUMENT;
            if (row.set_int != nullptr)
            {
                set_result = row.set_int(row.user_data, row.pending_int_value, err_buf, (uint32_t)sizeof(err_buf));
            }

            if (set_result != EMC_OK)
            {
                summary.failed += 1u;
                const char* message = ResolveErrorMessage(err_buf, kSetCallbackFailedMessage);
                HubUi_OnCommitSetFailure(row.token, message);
                LogCommitFailure(row, set_result, message);
                continue;
            }

            summary.succeeded += 1u;

            int32_t canonical_value = row.pending_int_value;
            EMC_Result get_result = EMC_ERR_INVALID_ARGUMENT;
            const char* get_message = kGetCallbackFailedMessage;
            if (row.get_int != nullptr)
            {
                get_result = row.get_int(row.user_data, &canonical_value);
            }

            if (get_result != EMC_OK)
            {
                LogCommitGetFailure(row, get_result, get_message);
                HubUi_OnCommitSyncInt(row.token, row.pending_int_value);
                continue;
            }

            HubUi_OnCommitSyncInt(row.token, canonical_value);
            continue;
        }

        EMC_Result set_result = EMC_ERR_INVALID_ARGUMENT;
        if (row.set_float != nullptr)
        {
            set_result = row.set_float(row.user_data, row.pending_float_value, err_buf, (uint32_t)sizeof(err_buf));
        }

        if (set_result != EMC_OK)
        {
            summary.failed += 1u;
            const char* message = ResolveErrorMessage(err_buf, kSetCallbackFailedMessage);
            HubUi_OnCommitSetFailure(row.token, message);
            LogCommitFailure(row, set_result, message);
            continue;
        }

        summary.succeeded += 1u;

        float canonical_value = row.pending_float_value;
        EMC_Result get_result = EMC_ERR_INVALID_ARGUMENT;
        const char* get_message = kGetCallbackFailedMessage;
        if (row.get_float != nullptr)
        {
            get_result = row.get_float(row.user_data, &canonical_value);
        }

        if (get_result != EMC_OK)
        {
            LogCommitGetFailure(row, get_result, get_message);
            HubUi_OnCommitSyncFloat(row.token, row.pending_float_value);
            continue;
        }

        if (FloatBitsEqual(canonical_value, row.pending_float_value))
        {
            HubUi_OnCommitSyncFloat(row.token, row.pending_float_value);
            continue;
        }

        HubUi_OnCommitSyncFloat(row.token, canonical_value);
    }

    g_last_summary = summary;
    LogCommitSummary(summary);
}

void HubCommit_GetLastSummary(HubCommitSummary* out_summary)
{
    if (out_summary == nullptr)
    {
        return;
    }

    *out_summary = g_last_summary;
}
