"""Regression guard for the visual-icon / inventory-cell desynchronization.

Reject direct geometry writes anywhere in the filtering translation unit:
Kenshi owns hit testing, so changing only a widget is never a valid item move.
Run against the original source too to demonstrate the guard catches the bug.
"""
import pathlib
import re
import sys

source = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(__file__).resolve().parents[1] / 'src/TraderSearchPipeline.cpp'
code = source.read_text(encoding='utf-8')
code = re.sub(r'/\*.*?\*/|//[^\n]*', '', code, flags=re.S)
for setter in ('setCoord', 'setPosition', 'setSize'):
    assert not re.search(r'->\s*' + setter + r'\s*\(', code), 'Unsafe presentation-only geometry write: ' + setter
if len(sys.argv) == 1:
    main = (source.parents[1] / 'Organize-the-Trader.cpp').read_text(encoding='utf-8')
    sync = main[main.index('void SynchronizeTraderIcons('):main.index('namespace', main.index('void SynchronizeTraderIcons('))]
    assert 'widget->setPosition(InventoryIcon::getItemPosition(icon->item->inventoryPos.x, icon->item->inventoryPos.y))' in sync
    assert 'targetCoords' not in sync
print('PASS: filter has no pixel-target writes; icon synchronization uses native item cells')
