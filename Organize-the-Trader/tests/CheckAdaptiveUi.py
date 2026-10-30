"""Source boundary regressions for the old asymmetric footer / GOG hook."""
from pathlib import Path
import xml.etree.ElementTree as ET
root = Path(__file__).resolve().parents[1]
ui = (root / 'src/TraderSearchUi.cpp').read_text(encoding='utf-8')
main = (root / 'Organize-the-Trader.cpp').read_text(encoding='utf-8')
checks = {
    'footer uses measured allocator': 'TraderAdaptiveLayout::Footer' in ui and 'int width = rowWidth / 2' not in ui,
    'GOG uses runtime Character setupSections': 'CharacterInventoryLayout_setupSections_PLACEHOLDER' in main and 'return baseAddress + 0x0014F450' not in main,
    'no nearest-address hook guess': 'bestDelta <= kExpectedAddressTolerance' not in main,
    'native plate ownership boundary is enforced': 'SafeMoneyPlate(moneyBox == parent || moneyBox == owner' in ui,
    'zero-sized clear and count rectangles overwrite stale geometry':
        'CoordIfChanged(clear, MyGUI::IntCoord(row.clearLeft, 0, clearSize, height));' in ui
        and 'CoordIfChanged(count, MyGUI::IntCoord(row.countLeft, 0, row.countWidth, height));' in ui,
    'money style comes from rendered client text': 'RenderedText(nativeValue)' in ui
        and 'nativeValue->getFontHeight()' not in ui and 'nativeValue->getTextColour()' not in ui,
    'native background retained and own XML palette preserved': 'moneyBox->setAlpha(0.0f)' not in ui
        and 'text->setTextColour(nativeColour)' not in ui
        and 'moneyBox->createWidget<MyGUI::TextBox>' in ui
        and 'data->setAlpha(0.0f)' in ui and '#73D966' not in ui and '#EB6152' not in ui,
    'caption measurement uses native text renderer and cache':
        'return measure->getSubWidgetText()->getTextSize().width;' in ui
        and '.Get(rendered->getFontName(), rendered->getFontHeight(), cache.comboItems, renderer)' in ui,
    'header contains only merchant and deal without player wallet calls':
        'OTT_PlayerMoney' not in ui and 'playerMoney' not in ui and '->getMoney()' not in ui
        and 'const int kHeaderCount = 2;' in ui,
}
game = Path('D:/SteamLibrary/steamapps/common/Kenshi')
for path in (game/'data/gui/layout/Kenshi_InventoryTraderWindow.layout',
             game/'mods/Dark UI Advanced/gui/layout/Kenshi_InventoryTraderWindow.layout'):
    xml = ET.parse(path).getroot()
    parents = {child: parent for parent in xml.iter() for child in parent}
    data = next(node for node in xml.iter('Widget') if node.get('name') == 'datapanel')
    plate = parents[data]
    content = parents[plate]
    assert content.get('name') == 'Content' and plate is not content
    # Both installed layouts give money a dedicated small plate; root is never faded.
    assert float(plate.get('position_real').split()[3]) < 1/3
    print('PASS: dedicated money plate in ' + str(path.relative_to(game)))
for name, ok in checks.items():
    print(('PASS' if ok else 'FAIL') + ': ' + name)
assert all(checks.values())
