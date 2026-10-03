#pragma once

#include <cstdint>
#include <string>

// Страница ModConfigMenu: список рисуется в выданной области (MyGUI::Widget*).
void HiddenFactionRelationsPanel_McmAttach(void* parent);
void HiddenFactionRelationsPanel_McmDetach();

bool HiddenFactionRelationsPanel_Initialize(
    unsigned int platform,
    const std::string& version,
    uintptr_t baseAddress);
