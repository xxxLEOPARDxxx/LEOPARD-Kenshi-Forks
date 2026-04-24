#pragma once

#include "emc/mod_hub_consumer_helpers.h"

#include <cctype>
#include <cstring>
#include <string>

namespace vs_mod_hub_helpers
{

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
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, invalid_message);
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_bool_value = value == true_value;
    emc::consumer::WriteErrorMessage(err_buf, err_buf_size, 0);
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
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_text");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    out_value = trim_ascii ? TrimAsciiCopy(raw_value) : std::string(raw_value);

    if (!allow_empty && out_value.empty())
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, required_message);
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if ((max_length == 0u && !out_value.empty()) || out_value.size() > max_length)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, too_long_message);
        return EMC_ERR_INVALID_ARGUMENT;
    }

    emc::consumer::WriteErrorMessage(err_buf, err_buf_size, 0);
    return EMC_OK;
}

} // namespace vs_mod_hub_helpers
