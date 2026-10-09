#include "hub_commit.h"

#include "hub_color.h"
#include "logging.h"
#include "hub_ui.h"

#include <Debug.h>

#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace
{
const char* kLogNone = "none";
const char* kSetCallbackFailedMessage = "set_callback_failed";
const char* kGetCallbackFailedMessage = "get_callback_failed";
const char* kInvalidBoolValueMessage = "invalid_bool_value";
const char* kInvalidSelectValueMessage = "invalid_select_value";
const char* kInvalidColorValueMessage = "invalid_color_value";
const char* kTextExceedsMaxLengthMessage = "text_exceeds_max_length";

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

bool IsSelectValueValid(const HubUiRowView& row, int32_t value)
{
    for (uint32_t option_index = 0u; option_index < row.select_option_count; ++option_index)
    {
        if (row.select_options != nullptr && row.select_options[option_index].value == value)
        {
            return true;
        }
    }

    return false;
}

bool TryReadCommittedText(
    const HubUiRowView& row,
    std::string* out_value,
    EMC_Result* out_result,
    const char** out_message)
{
    if (row.get_text == nullptr)
    {
        if (out_result != nullptr)
        {
            *out_result = EMC_ERR_INVALID_ARGUMENT;
        }
        if (out_message != nullptr)
        {
            *out_message = "missing_get_callback";
        }
        return false;
    }

    std::vector<char> buffer(row.text_max_length + 1u, '\0');
    const EMC_Result result = row.get_text(row.user_data, &buffer[0], row.text_max_length + 1u);
    if (result != EMC_OK)
    {
        if (out_result != nullptr)
        {
            *out_result = result;
        }
        if (out_message != nullptr)
        {
            *out_message = kGetCallbackFailedMessage;
        }
        return false;
    }

    const void* terminator = std::memchr(&buffer[0], '\0', buffer.size());
    if (terminator == nullptr)
    {
        if (out_result != nullptr)
        {
            *out_result = EMC_ERR_CALLBACK_FAILED;
        }
        if (out_message != nullptr)
        {
            *out_message = kTextExceedsMaxLengthMessage;
        }
        return false;
    }

    if (out_value != nullptr)
    {
        const size_t length = static_cast<const char*>(terminator) - &buffer[0];
        out_value->assign(&buffer[0], length);
    }

    return true;
}

bool TryReadCommittedColor(
    const HubUiRowView& row,
    std::string* out_value,
    EMC_Result* out_result,
    const char** out_message)
{
    std::string value;
    if (!TryReadCommittedText(row, &value, out_result, out_message))
    {
        return false;
    }

    std::string normalized_value;
    if (!hub_color::TryNormalizeColorHex(value.c_str(), &normalized_value))
    {
        if (out_result != nullptr)
        {
            *out_result = EMC_ERR_CALLBACK_FAILED;
        }
        if (out_message != nullptr)
        {
            *out_message = kInvalidColorValueMessage;
        }
        return false;
    }

    if (out_value != nullptr)
    {
        *out_value = normalized_value;
    }

    return true;
}

bool IsCommitSupportedRowKind(int32_t kind)
{
    return kind == HUB_UI_ROW_KIND_BOOL
        || kind == HUB_UI_ROW_KIND_KEYBIND
        || kind == HUB_UI_ROW_KIND_INT
        || kind == HUB_UI_ROW_KIND_FLOAT
        || kind == HUB_UI_ROW_KIND_SELECT
        || kind == HUB_UI_ROW_KIND_TEXT
        || kind == HUB_UI_ROW_KIND_COLOR;
}

bool IsRowCommitBlocked(const HubUiRowView& row)
{
    return (row.kind == HUB_UI_ROW_KIND_INT && row.int_text_parse_error)
        || (row.kind == HUB_UI_ROW_KIND_FLOAT && row.float_text_parse_error)
        || (row.kind == HUB_UI_ROW_KIND_COLOR && row.color_text_parse_error);
}

void CommitSingleRow(const HubUiRowView& row, HubCommitSummary* summary)
{
    if (summary == nullptr)
    {
        return;
    }

    summary->attempted += 1u;

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
            summary->failed += 1u;
            const char* message = ResolveErrorMessage(err_buf, kSetCallbackFailedMessage);
            HubUi_OnCommitSetFailure(row.token, message);
            LogCommitFailure(row, set_result, message);
            return;
        }

        summary->succeeded += 1u;

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
            return;
        }

        HubUi_OnCommitSyncBool(row.token, canonical_value);
        return;
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
            summary->failed += 1u;
            const char* message = ResolveErrorMessage(err_buf, kSetCallbackFailedMessage);
            HubUi_OnCommitSetFailure(row.token, message);
            LogCommitFailure(row, set_result, message);
            return;
        }

        summary->succeeded += 1u;

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
            return;
        }

        HubUi_OnCommitSyncKeybind(row.token, canonical_value);
        return;
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
            summary->failed += 1u;
            const char* message = ResolveErrorMessage(err_buf, kSetCallbackFailedMessage);
            HubUi_OnCommitSetFailure(row.token, message);
            LogCommitFailure(row, set_result, message);
            return;
        }

        summary->succeeded += 1u;

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
            return;
        }

        HubUi_OnCommitSyncInt(row.token, canonical_value);
        return;
    }

    if (row.kind == HUB_UI_ROW_KIND_SELECT)
    {
        EMC_Result set_result = EMC_ERR_INVALID_ARGUMENT;
        if (row.set_select != nullptr)
        {
            set_result = row.set_select(row.user_data, row.pending_select_value, err_buf, (uint32_t)sizeof(err_buf));
        }

        if (set_result != EMC_OK)
        {
            summary->failed += 1u;
            const char* message = ResolveErrorMessage(err_buf, kSetCallbackFailedMessage);
            HubUi_OnCommitSetFailure(row.token, message);
            LogCommitFailure(row, set_result, message);
            return;
        }

        summary->succeeded += 1u;

        int32_t canonical_value = row.pending_select_value;
        EMC_Result get_result = EMC_ERR_INVALID_ARGUMENT;
        const char* get_message = kGetCallbackFailedMessage;
        if (row.get_select != nullptr)
        {
            get_result = row.get_select(row.user_data, &canonical_value);
            if (get_result == EMC_OK && !IsSelectValueValid(row, canonical_value))
            {
                get_result = EMC_ERR_CALLBACK_FAILED;
                get_message = kInvalidSelectValueMessage;
            }
        }

        if (get_result != EMC_OK)
        {
            LogCommitGetFailure(row, get_result, get_message);
            HubUi_OnCommitSyncSelect(row.token, row.pending_select_value);
            return;
        }

        HubUi_OnCommitSyncSelect(row.token, canonical_value);
        return;
    }

    if (row.kind == HUB_UI_ROW_KIND_TEXT)
    {
        const char* pending_text = row.pending_text != nullptr ? row.pending_text : "";
        EMC_Result set_result = EMC_ERR_INVALID_ARGUMENT;
        if (row.set_text != nullptr)
        {
            set_result = row.set_text(row.user_data, pending_text, err_buf, (uint32_t)sizeof(err_buf));
        }

        if (set_result != EMC_OK)
        {
            summary->failed += 1u;
            const char* message = ResolveErrorMessage(err_buf, kSetCallbackFailedMessage);
            HubUi_OnCommitSetFailure(row.token, message);
            LogCommitFailure(row, set_result, message);
            return;
        }

        summary->succeeded += 1u;

        std::string canonical_value;
        EMC_Result get_result = EMC_ERR_INVALID_ARGUMENT;
        const char* get_message = kGetCallbackFailedMessage;
        if (!TryReadCommittedText(row, &canonical_value, &get_result, &get_message))
        {
            LogCommitGetFailure(row, get_result, get_message);
            HubUi_OnCommitSyncText(row.token, pending_text);
            return;
        }

        HubUi_OnCommitSyncText(row.token, canonical_value.c_str());
        return;
    }

    if (row.kind == HUB_UI_ROW_KIND_COLOR)
    {
        std::string normalized_pending_text;
        const char* pending_text = row.pending_text != nullptr ? row.pending_text : "";
        if (!hub_color::TryNormalizeColorHex(pending_text, &normalized_pending_text))
        {
            summary->failed += 1u;
            HubUi_OnCommitSetFailure(row.token, kInvalidColorValueMessage);
            LogCommitFailure(row, EMC_ERR_INVALID_ARGUMENT, kInvalidColorValueMessage);
            return;
        }

        EMC_Result set_result = EMC_ERR_INVALID_ARGUMENT;
        if (row.set_text != nullptr)
        {
            set_result = row.set_text(
                row.user_data,
                normalized_pending_text.c_str(),
                err_buf,
                (uint32_t)sizeof(err_buf));
        }

        if (set_result != EMC_OK)
        {
            summary->failed += 1u;
            const char* message = ResolveErrorMessage(err_buf, kSetCallbackFailedMessage);
            HubUi_OnCommitSetFailure(row.token, message);
            LogCommitFailure(row, set_result, message);
            return;
        }

        summary->succeeded += 1u;

        std::string canonical_value;
        EMC_Result get_result = EMC_ERR_INVALID_ARGUMENT;
        const char* get_message = kGetCallbackFailedMessage;
        if (!TryReadCommittedColor(row, &canonical_value, &get_result, &get_message))
        {
            LogCommitGetFailure(row, get_result, get_message);
            HubUi_OnCommitSyncColor(row.token, normalized_pending_text.c_str());
            return;
        }

        HubUi_OnCommitSyncColor(row.token, canonical_value.c_str());
        return;
    }

    EMC_Result set_result = EMC_ERR_INVALID_ARGUMENT;
    if (row.set_float != nullptr)
    {
        set_result = row.set_float(row.user_data, row.pending_float_value, err_buf, (uint32_t)sizeof(err_buf));
    }

    if (set_result != EMC_OK)
    {
        summary->failed += 1u;
        const char* message = ResolveErrorMessage(err_buf, kSetCallbackFailedMessage);
        HubUi_OnCommitSetFailure(row.token, message);
        LogCommitFailure(row, set_result, message);
        return;
    }

    summary->succeeded += 1u;

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
        return;
    }

    if (FloatBitsEqual(canonical_value, row.pending_float_value))
    {
        HubUi_OnCommitSyncFloat(row.token, row.pending_float_value);
        return;
    }

    HubUi_OnCommitSyncFloat(row.token, canonical_value);
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

        if (!row.dirty || !IsCommitSupportedRowKind(row.kind) || IsRowCommitBlocked(row))
        {
            continue;
        }
        CommitSingleRow(row, &summary);
    }

    g_last_summary = summary;
    LogCommitSummary(summary);
}

bool HubCommit_RunImmediateApply(const char* namespace_id, const char* mod_id, const char* setting_id)
{
    if (namespace_id == nullptr || mod_id == nullptr || setting_id == nullptr)
    {
        return false;
    }

    if (HubUi_IsAnyKeybindCaptureActive())
    {
        return false;
    }

    const uint32_t row_count = HubUi_GetRowCount();
    for (uint32_t row_index = 0; row_index < row_count; ++row_index)
    {
        HubUiRowView row;
        if (!HubUi_GetRowViewByIndex(row_index, &row))
        {
            continue;
        }

        if (row.namespace_id == nullptr
            || row.mod_id == nullptr
            || row.setting_id == nullptr
            || std::strcmp(row.namespace_id, namespace_id) != 0
            || std::strcmp(row.mod_id, mod_id) != 0
            || std::strcmp(row.setting_id, setting_id) != 0)
        {
            continue;
        }

        if (!row.dirty || !IsCommitSupportedRowKind(row.kind) || IsRowCommitBlocked(row))
        {
            return false;
        }

        HubCommitSummary summary = { 0u, 0u, 0u, 0u, HUB_COMMIT_SKIP_REASON_NONE };
        CommitSingleRow(row, &summary);
        g_last_summary = summary;
        LogCommitSummary(summary);
        return summary.failed == 0u && summary.succeeded > 0u;
    }

    return false;
}

void HubCommit_GetLastSummary(HubCommitSummary* out_summary)
{
    if (out_summary == nullptr)
    {
        return;
    }

    *out_summary = g_last_summary;
}
