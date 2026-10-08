## Organize the Trader (RE_Kenshi plugin base)

This repository is a clean starter base for a `Organize-the-Trader` RE_Kenshi native plugin.

## Setup
Clone normally. Shared build scripts are tracked in `tools/build-scripts` via `git subtree`, so no submodule init step is required.

1) Open a PowerShell terminal in this repo.
2) (Optional) Create `.env` from `.env.example` to set local paths.
3) Source the env script:
   - `. .\scripts\setup_env.ps1`

This sets:
- `KENSHILIB_DEPS_DIR`
- `KENSHILIB_DIR`
- `BOOST_INCLUDE_PATH`

## Build
You can build in Visual Studio, or via the script below.

### Scripted build + deploy
Run:
- `.\scripts\build-deploy.ps1`

Optional parameters:
- `-KenshiPath "H:\SteamLibrary\steamapps\common\Kenshi"`
- `-Configuration "Release"`
- `-Platform "x64"`

## Deploy layout
Mod data folder name: `Organize-the-Trader`

After deploy, expected files:
- `[Kenshi install dir]\mods\Organize-the-Trader\Organize-the-Trader.mod`
- `[Kenshi install dir]\mods\Organize-the-Trader\RE_Kenshi.json`
- `[Kenshi install dir]\mods\Organize-the-Trader\Organize-the-Trader.dll`
- `[Kenshi install dir]\mods\Organize-the-Trader\mod-config.json`

## Config
`mod-config.json` supports:

- `enabled` (bool): master plugin toggle.
- `showSearchEntryCount` (bool): show visible/total item entries in the search bar.
- `showSearchQuantityCount` (bool): show visible stack quantity in the search bar.
- `showSearchClearButton` (bool): show the clear button in the search bar.
- `autoFocusSearchInput` (bool): auto-focus the search input when trader controls are injected.
- `debugLogging` (bool): master runtime debug switch for support diagnostics.
- `debugSearchLogging` (bool): enables search-path diagnostics when `debugLogging` is enabled.
- `debugBindingLogging` (bool): enables inventory-binding diagnostics when `debugLogging` is enabled.
- `searchInputWidth` (int): desired search input width in pixels, clamped at runtime.
- `searchInputHeight` (int): desired search input height in pixels, clamped at runtime.
- `searchInputPositionCustomized` (bool): when `true`, reuses the saved drag position instead of the default placement.
- `searchInputLeft` (int): saved left coordinate for the search bar inside the trader window.
- `searchInputTop` (int): saved top coordinate for the search bar inside the trader window.

Default:

```json
{
  "enabled": true,
  "showSearchEntryCount": true,
  "showSearchQuantityCount": true,
  "showSearchClearButton": true,
  "autoFocusSearchInput": true,
  "debugLogging": false,
  "debugSearchLogging": false,
  "debugBindingLogging": false,
  "searchInputWidth": 372,
  "searchInputHeight": 26,
  "searchInputPositionCustomized": false,
  "searchInputLeft": 0,
  "searchInputTop": 0
}
```

## Mod Hub (Optional)
If `Emkejs-Mod-Core` is loaded, Organize-the-Trader registers its user-facing settings in Mod Hub (`enabled`, search counts, clear button, auto-focus search, search width, search height) and writes committed changes back to `mod-config.json`.

Recommended load order:
- `Emkejs-Mod-Core`
- `Organize-the-Trader`

If Mod Core is absent, registration fails, or the helper retry path never attaches, this mod keeps using `mod-config.json` only.

### Runtime smoke check
After launching Kenshi with `Emkejs-Mod-Core` and `Organize-the-Trader` enabled, run:
- `.\scripts\phase22_mod_hub_runtime_smoke_test.ps1 -ExpectedMode attached`

The script reads the latest `RE_Kenshi_log.txt` session and passes only when the latest Organize-the-Trader startup reaches `event=mod_hub_attached`, which proves Mod Hub lookup and setting registration succeeded for that run. If you see `event=mod_hub_attach_retry_pending`, the helper-owned retry path is still pending and that run is not a clean attached pass yet.

## License
This project is licensed under the GNU General Public License v3.0.
It uses KenshiLib, which is released under GPLv3.

## Изменения форка (ветка leopard)
- Настройки - во вкладке MCM окна «Настройки»; Mod Hub не нужен.
- Производительность: панели поиска и сортировки и пристыкованная панель запоминаются при создании (shared/WidgetRef.h) вместо поиска по имени через весь интерфейс - до восьми полных обходов за кадр. Окно торговли, пока панели нет, ищется 10 раз в секунду.
- Поиск: найденные вещи собираются в начало сетки, скрытые поиском плотно занимают оставшееся место (крупные первыми). Раньше все вещи ставились полками с пустым рядом между ними, у торговца с сотней вещей полки не влезали, и раскладка отменялась целиком - найденное оставалось разбросанным. Если раскладка всё же не удалась, причина пишется в журнал строкой `search packing skipped reason=...`.
- Найденное и отсортированное ложится плотно, без пустого ряда между полками.
- v2: со страницы MCM убраны ширина и высота поиска и сортировки (`search_input_width/height`, `sort_panel_width/height`): панели встроены в окно торговца (`InlineSearchCoord`, `PositionInlineControls`) и берут размер у места в нём, так что эти числа ни на что не влияли - игрок жаловался. Ключи в mod-config.json по-прежнему читаются, но не используются.
- v2: «Подробный журнал» в MCM (debugLogging).
- v2: подсказка в пустом поле поиска - через перевод (Tr, «Search items...» в .po); раньше была по-русски и в английской игре.
- v2: проверка «изменились ли вещи торговца» (подпись всего ассортимента) - раз в 150 мс, а не каждый кадр; при пустом поиске без сортировки проход поиска только пересчитывает счётчик, без перекладки сетки (просадка FPS при каждой продаже).
