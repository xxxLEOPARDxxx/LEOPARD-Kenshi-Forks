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
int32_t ModHubDummyConsumer_GetRegisterKeybindCalls();
int32_t ModHubDummyConsumer_GetRegisterIntCalls();
int32_t ModHubDummyConsumer_GetRegisterFloatCalls();
int32_t ModHubDummyConsumer_GetRegisterActionCalls();
int32_t ModHubDummyConsumer_GetOrderChecksPassed();
int32_t ModHubDummyConsumer_GetDescriptorChecksPassed();

#endif

#endif
