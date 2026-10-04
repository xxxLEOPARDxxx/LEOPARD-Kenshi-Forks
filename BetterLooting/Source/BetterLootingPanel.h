#pragma once

// Точки, которыми код MCM и сохранения дотягивается до правил BetterLooting.
// Окна настроек (SHIFT+L) с 04.10.2026 нет - все настройки во вкладке MCM.

// Запросить «разложить всё». Определено в BetterLooting.cpp: работа с
// инвентарями идёт из игрового потока, здесь только взводится флаг.
void BetterLootingRequestStoreAll();

// Записать текущие правила в BetterLooting.ini.
void BetterLootingSaveRules();

// Пресет из MCM и сразу в ini: 0 баланс, 1 деньги, 2 ресурсы,
// 3 все категории, 4 ни одной.
void BetterLootingApplyPreset(int which);
