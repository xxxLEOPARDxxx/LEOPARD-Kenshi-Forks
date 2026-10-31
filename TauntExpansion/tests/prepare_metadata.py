"""Prepare isolated release metadata for the root release-tool integration."""
import json,pathlib
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
    release=ROOT/'release'
    note=json.loads((release/'release_notes.json').read_text(encoding='utf-8'))['TauntExpansion']
    changelog='v1:\n'+'\n'.join('- '+x for x in note['en'])+'\n\nv1:\n'+'\n'.join('- '+x for x in note['ru'])
    (release/'changelog.json').write_text(json.dumps({'TauntExpansion':{'1':changelog}},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (release/'nexus_description.txt').write_text((release/'description_en.bbcode.txt').read_text(encoding='utf-8')+'\n\n'+(release/'description_ru.bbcode.txt').read_text(encoding='utf-8'),encoding='utf-8')
    data={
        'mod_id':'TauntExpansion','name':'Taunt Expansion','version':'1',
        'source_directory':str(ROOT),'dll_path':'D:/DEV/Kenshi/build/TauntExpansion/TauntExpansion.dll',
        'source_locales':'locale/<language>/LC_MESSAGES/taunt_expansion.po',
        'runtime_locales':'D:/DEV/Kenshi/build/TauntExpansion/locale/<language>/LC_MESSAGES/taunt_expansion.mo',
        'languages':['en_GB','ru_RU','zh_CN'],'mcm_id':'TauntExpansion','mcm_title':'Taunt Expansion','mcm_settings':9,
        'mcm_restart_settings':[],'detailed_log_last':True,'detailed_log_default':False,
        'author':'tata / Tata-Gari','original_url':'https://steamcommunity.com/workshop/filedetails/?id=3771499164',
        'source_url':'https://github.com/Tata-Gari/RE_Kenshi_Plugins_Source',
        'upstream_commit':'b7cc05a3a1b9d6c065bd7c96272abebcd0de8be8',
        'offline_binary_audit':json.loads((release/'binary_audit.json').read_text()) if (release/'binary_audit.json').exists() else None,
        'fork_nexus_url':None,'fork_steam_id':None,'publication':'not created; Nexus user upload first; no Steam publication before user link',
        'player_changes':{
            'en':['Optional live MCM settings.','English, Russian and Simplified Chinese interface.','Consistent Taunt strength during simultaneous target evaluations.','Original package defaults preserved.'],
            'ru':['Необязательные настройки MCM с применением во время игры.','Русский, английский и китайский интерфейс.','Стабильная сила провокации при одновременном выборе целей.','Сохранены значения поставки оригинала.']},
        'runtime_validation':'Game behavior, skin UI, CPU/FPS and sustained-memory measurements pending; offline audit is not a runtime claim.'
    }
    ledger=pathlib.Path('D:/DEV/Kenshi/workshop/published_releases.json')
    published=json.loads(ledger.read_text(encoding='utf-8')).get('mods',{}).get('TauntExpansion') if ledger.exists() else None
    if published:
        data.update(fork_nexus_url=f'https://www.nexusmods.com/kenshi/mods/{published["nexus"]}',fork_steam_id=published['workshop_id'],published_version=published['version'],publication_record=published,
            publication='Nexus upload verified by user; Steam published and remotely verified.' if published['version']==data['version'] else 'Prepared candidate; earlier public version '+published['version'])
    (release/'integration_metadata.json').write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    for lang in ('en','ru'):
        text=(release/f'description_{lang}.bbcode.txt').read_text(encoding='utf-8')
        assert len(text)<8000,(lang,len(text))
        assert text.count('[list]')==text.count('[/list]')
        assert text.count('[h1]')==text.count('[/h1]') and text.count('[h2]')==text.count('[/h2]')
        assert text.count('[b]')==text.count('[/b]')
        assert text.count('[url=')==text.count('[/url]')
        assert 'changelog' not in text.lower() and 'изменения версии' not in text.lower()
        print(f'{lang}: {len(text)} characters, balanced BBCode, no embedded release history')
if __name__=='__main__':main()
