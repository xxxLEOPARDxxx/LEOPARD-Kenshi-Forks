#ifndef EMC_MOD_HUB_CONSUMER_HELPERS_H
#define EMC_MOD_HUB_CONSUMER_HELPERS_H

#include "emc/mod_hub_api.h"

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

    if (value != 0 && value != 1)
    {
        WriteErrorMessage(err_buf, err_buf_size, "invalid_bool");
        return EMC_ERR_INVALID_ARGUMENT;
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

inline EMC_Result ActionNoopSuccess(void* user_data, char* err_buf, uint32_t err_buf_size)
{
    (void)user_data;
    WriteErrorMessage(err_buf, err_buf_size, 0);
    return EMC_OK;
}
}
}

#endif
