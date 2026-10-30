"""Spot-check translated MO captions and installed font/skin width offline.

This reads the installed fonts and skin only; no game-process or installed-mod
writes. It conservatively sums glyph advances at rounded-up FreeType pixels.
No runtime layout polling, measuring or widget allocation is introduced.
"""
import argparse, gettext, json, math, pathlib, re, xml.etree.ElementTree as ET
from PIL import ImageFont
ROOT = pathlib.Path(__file__).resolve().parents[1]
GAME = pathlib.Path('D:/SteamLibrary/steamapps/common/Kenshi')
EXPECTED = {'en_GB': 'Sort [{n}]', 'ru_RU': 'Сортировать [{n}]', 'zh_CN': '整理 [{n}]'}

def properties(resource):
    return {p.attrib['key']: p.attrib['value'] for p in resource.findall('Property')}

def main():
    parser = argparse.ArgumentParser();parser.add_argument('--output', type=pathlib.Path, required=True);args = parser.parse_args()
    default_width = int(re.search(r'sort_button_width\s*=\s*(\d+)', (ROOT/'StackSort.cfg').read_text()).group(1))
    # Original and Dark UI use the same six-pixel horizontal text inset.
    skins = [GAME/'data/gui/skins/kenshi_skins.xml', GAME/'mods/Dark UI Advanced/gui/skins/kenshi_skins.xml']
    usable_widths = []
    for path in skins:
        root = ET.parse(path).getroot()
        skin = next(r for r in root.findall('Resource') if r.attrib.get('name') == 'Kenshi_Button1Skin')
        skin_width = int(skin.attrib['size'].split()[0])
        text = next(t for t in skin.findall('BasisSkin') if t.attrib['type'] == 'SimpleText')
        text_width = int(text.attrib['offset'].split()[2])
        usable_widths.append(default_width - (skin_width - text_width))
    usable = min(usable_widths)
    reports = []
    for lang, expected in EXPECTED.items():
        with (args.output/'locale'/lang/'LC_MESSAGES/stack_sort.mo').open('rb') as stream:
            catalog = gettext.GNUTranslations(stream)
        caption = catalog.gettext('Sort [{n}]')
        assert caption == expected, (lang, caption)
        assert caption.count('{n}') == 1
        font_dir = GAME/'data/gui/fonts' if lang == 'en_GB' else GAME/'locale'/lang/'gui/fonts'
        if lang == 'ru_RU':
            override = GAME/'mods/Russian Translation Fix Plus/locale/ru_RU/gui/fonts'
            if override.exists(): font_dir = override
        fonts = ET.parse(font_dir/'kenshi_fonts.xml').getroot()
        font = next(r for r in fonts.findall('Resource') if r.attrib.get('name') == 'Kenshi_PaintedTextFont_Medium')
        props = properties(font)
        font_path = font_dir/props['Source']
        if not font_path.exists(): font_path = GAME/'data/gui/fonts'/props['Source']
        pixels = math.ceil(float(props['Size']) * float(props['Resolution']) / 72.0)
        rendered = ImageFont.truetype(str(font_path), pixels)
        for target in (0, 22, 126):
            text = caption.replace('{n}', str(target))
            width = sum(rendered.getlength(char) for char in text)
            assert width <= usable, (lang, text, width, usable)
            reports.append({'language': lang, 'caption': text, 'font': str(font_path), 'font_pixels': pixels,
                            'glyph_width': width, 'button_width': default_width, 'usable_width': usable})
    output = args.output/'StackSort-caption-verification.json'
    output.write_text(json.dumps(reports, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f'{len(reports)} MO caption/font-fit checks passed; button {default_width}px, usable text {usable}px')
    for report in reports:
        if report['caption'].endswith('[126]'): print(f'{report["language"]}: {report["glyph_width"]:.1f}px <= {usable}px')
    print(output)

if __name__ == '__main__': main()
