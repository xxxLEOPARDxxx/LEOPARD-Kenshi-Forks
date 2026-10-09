#ifndef EMC_MOD_HUB_CONSUMER_HELPERS_H
#define EMC_MOD_HUB_CONSUMER_HELPERS_H

#include "emc/mod_hub_api.h"

#include <cctype>
#include <cstring>
#include <string>

namespace emc
{
namespace consumer
{
inline void WriteErrorMessage(char* err_buf, uint32_t err_buf_size, const char* message)
{
    if (err_buf == 0 || err_buf_size == 0u)
    {
        return;
    }

    if (message == 0)
    {
        err_buf[0] = '\0';
        return;
    }

    uint32_t index = 0u;
    while (index + 1u < err_buf_size && message[index] != '\0')
    {
        err_buf[index] = message[index];
        ++index;
    }

    err_buf[index] = '\0';
}

inline std::string TrimAsciiCopy(const char* value)
{
    if (value == 0)
    {
        return std::string();
    }

    const char* begin = value;
    while (*begin != '\0' && std::isspace(static_cast<unsigned char>(*begin)) != 0)
    {
        ++begin;
    }

    const char* end = begin + std::strlen(begin);
    while (end > begin && std::isspace(static_cast<unsigned char>(*(end - 1))) != 0)
    {
        --end;
    }

    return std::string(begin, static_cast<std::size_t>(end - begin));
}

inline EMC_Result ValidateBoolValue(
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size,
    const char* invalid_message = "invalid_bool")
{
    if (value != 0 && value != 1)
    {
        WriteErrorMessage(err_buf, err_buf_size, invalid_message);
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_OK;
}

template <typename ValueType>
inline EMC_Result ValidateValueInRange(
    ValueType value,
    ValueType min_value,
    ValueType max_value,
    char* err_buf,
    uint32_t err_buf_size,
    const char* invalid_message = "value_out_of_range")
{
    if (value < min_value || value > max_value)
    {
        WriteErrorMessage(err_buf, err_buf_size, invalid_message);
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_OK;
}

template <typename StateType, typename ApplyFn, typename PersistFn>
inline EMC_Result ApplyUpdateWithRollback(
    const StateType& previous,
    const StateType& updated,
    char* err_buf,
    uint32_t err_buf_size,
    ApplyFn apply,
    PersistFn persist,
    const char* persist_failed_message = "persist_failed")
{
    apply(updated);

    if (!persist(updated))
    {
        apply(previous);
        WriteErrorMessage(err_buf, err_buf_size, persist_failed_message);
        return EMC_ERR_INTERNAL;
    }

    WriteErrorMessage(err_buf, err_buf_size, 0);
    return EMC_OK;
}

template <typename StateType, typename ValueType>
inline EMC_Result GetFieldValue(void* user_data, ValueType* out_value, ValueType StateType::*field)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    StateType* state = static_cast<StateType*>(user_data);
    *out_value = state->*field;
    return EMC_OK;
}

template <typename StateType>
inline EMC_Result GetStringFieldValue(
    void* user_data,
    char* out_value,
    uint32_t out_value_size,
    const std::string StateType::*field)
{
    if (user_data == 0 || out_value == 0 || out_value_size == 0u)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    StateType* state = static_cast<StateType*>(user_data);
    const std::string& value = state->*field;
    if (value.size() + 1u > out_value_size)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    std::memcpy(out_value, value.c_str(), value.size() + 1u);
    return EMC_OK;
}

template <typename StateType>
inline EMC_Result GetBoolFieldValue(void* user_data, int32_t* out_value, int32_t StateType::*field)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    StateType* state = static_cast<StateType*>(user_data);
    *out_value = (state->*field) != 0 ? 1 : 0;
    return EMC_OK;
}

template <typename StateType>
inline EMC_Result GetBoolSelectionFieldValue(
    void* user_data,
    int32_t* out_value,
    bool StateType::*field,
    int32_t false_value = 0,
    int32_t true_value = 1)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    StateType* state = static_cast<StateType*>(user_data);
    *out_value = (state->*field) ? true_value : false_value;
    return EMC_OK;
}

inline EMC_Result NormalizeBoolSelectionValue(
    int32_t value,
    int32_t false_value,
    int32_t true_value,
    bool* out_bool_value,
    char* err_buf,
    uint32_t err_buf_size,
    const char* invalid_message = "invalid_select_option")
{
    if (out_bool_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value != false_value && value != true_value)
    {
        WriteErrorMessage(err_buf, err_buf_size, invalid_message);
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_bool_value = value == true_value;
    WriteErrorMessage(err_buf, err_buf_size, 0);
    return EMC_OK;
}

template <typename StateType, typename ValueType>
inline EMC_Result SetFieldValueWithRollback(
    void* user_data,
    ValueType value,
    char* err_buf,
    uint32_t err_buf_size,
    ValueType StateType::*field)
{
    if (user_data == 0)
    {
        WriteErrorMessage(err_buf, err_buf_size, "invalid_state");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    StateType* state = static_cast<StateType*>(user_data);
    ValueType& target = state->*field;
    const ValueType previous_value = target;
    target = value;

    // TODO: Persist the updated value. If persistence fails, restore previous_value and return an error.
    (void)previous_value;
    WriteErrorMessage(err_buf, err_buf_size, 0);
    return EMC_OK;
}

template <typename StateType>
inline EMC_Result SetBoolFieldValueWithRollback(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size,
    int32_t StateType::*field)
{
    if (user_data == 0)
    {
        WriteErrorMessage(err_buf, err_buf_size, "invalid_state");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const EMC_Result bool_validation = ValidateBoolValue(value, err_buf, err_buf_size);
    if (bool_validation != EMC_OK)
    {
        return bool_validation;
    }

    StateType* state = static_cast<StateType*>(user_data);
    int32_t& target = state->*field;
    const int32_t previous_value = target;
    target = value != 0 ? 1 : 0;

    // TODO: Persist the updated value. If persistence fails, restore previous_value and return an error.
    (void)previous_value;
    WriteErrorMessage(err_buf, err_buf_size, 0);
    return EMC_OK;
}

inline EMC_Result NormalizeTextValue(
    const char* raw_value,
    uint32_t max_length,
    std::string& out_value,
    char* err_buf,
    uint32_t err_buf_size,
    bool trim_ascii = true,
    bool allow_empty = false,
    const char* required_message = "text_required",
    const char* too_long_message = "text_too_long")
{
    if (raw_value == 0)
    {
        WriteErrorMessage(err_buf, err_buf_size, "invalid_text");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    out_value = trim_ascii ? TrimAsciiCopy(raw_value) : std::string(raw_value);

    if (!allow_empty && out_value.empty())
    {
        WriteErrorMessage(err_buf, err_buf_size, required_message);
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if ((max_length == 0u && !out_value.empty()) || out_value.size() > max_length)
    {
        WriteErrorMessage(err_buf, err_buf_size, too_long_message);
        return EMC_ERR_INVALID_ARGUMENT;
    }

    WriteErrorMessage(err_buf, err_buf_size, 0);
    return EMC_OK;
}

inline EMC_Result ActionNoopSuccess(void* user_data, char* err_buf, uint32_t err_buf_size)
{
    (void)user_data;
    WriteErrorMessage(err_buf, err_buf_size, 0);
    return EMC_OK;
}
}
}

#endif
