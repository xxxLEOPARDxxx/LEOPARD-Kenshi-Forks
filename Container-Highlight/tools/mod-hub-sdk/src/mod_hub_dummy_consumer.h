#ifndef EMC_MOD_HUB_DUMMY_CONSUMER_H
#define EMC_MOD_HUB_DUMMY_CONSUMER_H

#include <stdint.h>

#if defined(EMC_ENABLE_TEST_EXPORTS)

void ModHubDummyConsumer_Reset();
void ModHubDummyConsumer_SetMode(int32_t mode);
int32_t ModHubDummyConsumer_OnStartup();
int32_t ModHubDummyConsumer_UseHubUi();
int32_t ModHubDummyConsumer_LastAttemptFailureResult();

int32_t ModHubDummyConsumer_GetRegisterModCalls();
int32_t ModHubDummyConsumer_GetRegisterBoolCalls();
int32_t ModHubDummyConsumer_GetRegisterBoolV2Calls();
int32_t ModHubDummyConsumer_GetRegisterKeybindCalls();
int32_t ModHubDummyConsumer_GetRegisterKeybindV2Calls();
int32_t ModHubDummyConsumer_GetRegisterIntCalls();
int32_t ModHubDummyConsumer_GetRegisterIntV2Calls();
int32_t ModHubDummyConsumer_GetRegisterFloatCalls();
int32_t ModHubDummyConsumer_GetRegisterSelectCalls();
int32_t ModHubDummyConsumer_GetRegisterSelectV2Calls();
int32_t ModHubDummyConsumer_GetRegisterTextCalls();
int32_t ModHubDummyConsumer_GetRegisterTextV2Calls();
int32_t ModHubDummyConsumer_GetRegisterColorCalls();
int32_t ModHubDummyConsumer_GetRegisterSectionCalls();
int32_t ModHubDummyConsumer_GetRegisterActionCalls();
int32_t ModHubDummyConsumer_GetRegisterActionV2Calls();
int32_t ModHubDummyConsumer_GetRegisterBoolConditionRuleCalls();
int32_t ModHubDummyConsumer_GetOrderChecksPassed();
int32_t ModHubDummyConsumer_GetDescriptorChecksPassed();

#endif

#endif
