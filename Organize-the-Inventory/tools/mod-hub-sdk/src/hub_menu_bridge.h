#ifndef EMC_HUB_MENU_BRIDGE_H
#define EMC_HUB_MENU_BRIDGE_H

#include <string>

typedef void (*HubMenuBridgeOptionsWindowInitObserver)();

void HubMenuBridge_SetHubEnabled(bool is_enabled);
bool HubMenuBridge_IsHubEnabled();
void HubMenuBridge_SetOptionsWindowInitObserver(HubMenuBridgeOptionsWindowInitObserver observer);

bool HubMenuBridge_InstallHooks(unsigned int platform, const std::string& version);

void HubMenuBridge_OnOptionsWindowInit();
void HubMenuBridge_OnOptionsWindowSave();
void HubMenuBridge_OnOptionsWindowClose();

#endif
