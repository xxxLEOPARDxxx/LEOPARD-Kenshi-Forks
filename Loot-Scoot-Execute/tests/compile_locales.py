"""Compile all three source catalogs and verify exact gettext round trips."""
import ast
import gettext
from pathlib import Path
import re
import struct

root = Path(__file__).resolve().parents[1]
# Validate the actual active MCM registration, not a hand-maintained subset.
# Old button-geometry keys may remain in catalogs but are not visible settings.
hub = (root/'LootScootExecuteModHubBridge.inl').read_text(encoding='utf-8-sig')
quoted = r'("(?:[^"\\]|\\.)*")'
definition = r'static const EMC_(?:Bool|Int)SettingDefV1\s+(\w+)\s*=\s*\{\s*(?:"[^"\n]*"|\w+),\s*' + quoted + r',\s*' + quoted
labels = {name: (ast.literal_eval(label), ast.literal_eval(tip))
          for name, label, tip in re.findall(definition, hub)}
rows = re.findall(r'\{\s*emc::MOD_HUB_CLIENT_SETTING_KIND_\w+,\s*&(\w+)\s*\}', hub)
assert len(rows) == len(set(rows)) and set(rows) == set(labels), (rows, labels)
assert rows[-1] == 'kDebugExecuteLoggingSettingDef', 'Detailed log must stay last'
required = {text for name in rows for text in labels[name]}
info = re.search(r'MCM_MODHUB_BRIDGE_TABLE\(GetModHubTableRegistration\(\),\s*' + quoted + r'\)', hub)
assert info, 'MCM information text missing'
required.add(ast.literal_eval(info.group(1)))
for filename in ('Loot-Scoot-Execute.cpp', 'LootScootExecuteJobs.inl'):
    source = (root/filename).read_text(encoding='utf-8-sig')
    required.update(ast.literal_eval(text) for text in re.findall(r'\bTr\(' + quoted + r'\)', source))
print(len(rows), 'active MCM rows;', len(required), 'active message keys extracted from production')
expected_keys = None
for language in ('en_GB', 'ru_RU', 'zh_CN'):
    po = root/'Loot-Scoot-Execute/locale'/language/'LC_MESSAGES/loot_scoot_execute.po'
    catalog = {}
    key = None
    value = ''
    mode = None
    for line in po.read_text(encoding='utf-8-sig').splitlines() + ['']:
        if not line.strip():
            if key is not None:
                assert key not in catalog, ('duplicate', key)
                catalog[key] = value
            key, value, mode = None, '', None
        elif line.startswith('msgid '):
            key, mode = ast.literal_eval(line[6:]), 'id'
        elif line.startswith('msgstr '):
            value, mode = ast.literal_eval(line[7:]), 'str'
        elif line.startswith('"'):
            if mode == 'id': key += ast.literal_eval(line)
            elif mode == 'str': value += ast.literal_eval(line)
        else:
            assert not line.startswith(('msgid_plural', 'msgstr[')), 'Plural parser required'
    assert required <= catalog.keys(), required-catalog.keys()
    if expected_keys is None: expected_keys = set(catalog)
    assert set(catalog) == expected_keys, ('locale key mismatch', language, set(catalog)^expected_keys)
    assert all(not key or value for key, value in catalog.items())
    if language == 'ru_RU':
        assert all(not re.search('[\u2010-\u2015\u2212]', value) for value in catalog.values())
    keys = sorted(catalog)
    count = len(keys)
    base = 28 + 16*count
    tables, data = [], b''
    for values in ([key.encode('utf-8') for key in keys], [catalog[key].encode('utf-8') for key in keys]):
        table = []
        for encoded in values:
            table.append((len(encoded), base+len(data)))
            data += encoded + b'\0'
        tables.append(b''.join(struct.pack('<II', *entry) for entry in table))
    mo = po.with_suffix('.mo')
    mo.write_bytes(struct.pack('<7I', 0x950412de, 0, count, 28, 28+8*count, 0, 0) + b''.join(tables) + data)
    with mo.open('rb') as stream:
        translations = gettext.GNUTranslations(stream)
    assert all(translations.gettext(key) == value for key, value in catalog.items() if key)
    print(language, count-1, 'messages, required IDs and MO roundtrip PASS')
