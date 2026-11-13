"""Compile all three source catalogs and verify exact gettext round trips."""
import ast
import gettext
from pathlib import Path
import re
import struct

root = Path(__file__).resolve().parents[1]
required = {
    'Execute own defeats', 'Execute enemies', 'Execute All search radius, m',
    'Search radius in metres: around the selected target for one-time orders, around the worker for permanent jobs. Default 10, maximum 200. Does not increase strike distance.'
}
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
