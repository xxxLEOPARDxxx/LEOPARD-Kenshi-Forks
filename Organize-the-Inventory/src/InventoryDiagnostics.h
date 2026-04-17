#pragma once

namespace MyGUI
{
class Widget;
}

void DumpOnDemandInventoryDiagnosticsSnapshot(MyGUI::Widget* controlsContainer);
void DumpInventoryBackpackCandidateDiagnosticsIfChanged(MyGUI::Widget* inventoryParent);
