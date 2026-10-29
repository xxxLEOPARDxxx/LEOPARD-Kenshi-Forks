#pragma once

#include "ResistDisplay.h"

#define KLOC_DOMAIN "character_inspector"
#include <Localization.h>

#include <GameTheme.h>

#include <mygui/MyGUI_Colour.h>
#include <mygui/MyGUI_ImageBox.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>

#include <cstdio>
#include <string>

// Цвета обычного текста берём из палитры игры, а не зашиваем: моды
// на интерфейс её перекрывают, и плагин должен следовать теме.
//
// Но не вслепую. Надписи лежат на тёмной панели, а ключи палитры у
// модов бывают почти чёрными: у «Russian Dark UI» Main = #767676,
// Greyed = #555555 - своим окнам мод задаёт цвет скином, а не этими
// ключами. Один раз уже получили чёрные надписи ровно поэтому.
// ReadableColour берёт цвет темы, а слишком тёмный отбрасывает и
// оставляет запасной.
inline MyGUI::Colour HeaderColour()
{
    return GameTheme::ReadableColour("Main", "#EBE5D1");
}

inline MyGUI::Colour MutedColour()
{
    return GameTheme::ReadableColour("Greyed", "#B3B3AD");
}

// 0-33 red, 34-66 orange, 67-100 green.
inline MyGUI::Colour MitigationColour(float percent0to100)
{
    if (percent0to100 <= 33.f)
        return MyGUI::Colour(0.95f, 0.32f, 0.28f);
    if (percent0to100 <= 66.f)
        return MyGUI::Colour(1.0f, 0.68f, 0.28f);
    return MyGUI::Colour(0.40f, 0.85f, 0.38f);
}

// Asset band name for silhouette fill PNGs (body_<part>_<band>.png).
inline const char* MitigationBandName(float percent0to100)
{
    if (percent0to100 <= 33.f)
        return "red";
    if (percent0to100 <= 66.f)
        return "orange";
    return "green";
}

// Cap at 100% for display — naive stacking can exceed 100 and looks wrong.
inline float displayPercent(float value)
{
    float p = resistToPercent(value);
    if (p < 0.f)
        return 0.f;
    if (p > 100.f)
        return 100.f;
    return p;
}

inline float clampDisplayPercent(float percent0to100)
{
    if (percent0to100 < 0.f)
        return 0.f;
    if (percent0to100 > 100.f)
        return 100.f;
    return percent0to100;
}

inline std::string FormatPercent(float value)
{
    char buf[64];
    sprintf_s(buf, "%.0f%%", displayPercent(value));
    return buf;
}

inline std::string FormatPercentNumber(float percent0to100)
{
    char buf[64];
    sprintf_s(buf, "%.0f%%", clampDisplayPercent(percent0to100));
    return buf;
}

inline float estimateMitigationPercent(float cut, float blunt, float pierce)
{
    float avg = (displayPercent(cut) + displayPercent(blunt) + displayPercent(pierce)) / 3.0f;
    if (avg > 100.f)
        return 100.f;
    return avg;
}

inline MyGUI::TextBox* AddTextLine(
    MyGUI::Widget* parent,
    const char* widgetName,
    float x,
    float y,
    float w,
    float h,
    const std::string& text)
{
    MyGUI::TextBox* box = parent->createWidgetReal<MyGUI::TextBox>(
        "Kenshi_TextboxStandardText", x, y, w, h, MyGUI::Align::Default, widgetName);
    if (box)
    {
        box->setCaption(text);
        // Светлый тон заголовка окна. Цвет скина по умолчанию здесь (20,8,5) -
        // темнее фона панели (30,30,30), название предмета почти не видно.
        // AddColoredLine перекроет это своим цветом, простые строки останутся
        // читаемыми на любом интерфейсе.
        box->setTextColour(HeaderColour());
    }
    return box;
}

inline MyGUI::TextBox* AddColoredLine(
    MyGUI::Widget* parent,
    const char* widgetName,
    float x,
    float y,
    float w,
    float h,
    const std::string& text,
    const MyGUI::Colour& colour)
{
    MyGUI::TextBox* box = AddTextLine(parent, widgetName, x, y, w, h, text);
    if (box)
        box->setTextColour(colour);
    return box;
}

inline MyGUI::TextBox* AddHeaderLine(
    MyGUI::Widget* parent,
    const char* widgetName,
    float x,
    float y,
    float w,
    float h,
    const std::string& text)
{
    return AddColoredLine(parent, widgetName, x, y, w, h, text, HeaderColour());
}

inline void AddMitigationBanner(
    MyGUI::Widget* parent,
    const char* widgetName,
    float x,
    float& y,
    float lineH,
    float gap,
    float cut,
    float blunt,
    float pierce,
    float width = 0.92f)
{
    float mit = estimateMitigationPercent(cut, blunt, pierce);
    char line[128];
    sprintf_s(line, TrFmt("Mitigation ~%.0f%%"), mit);
    AddColoredLine(parent, widgetName, x, y, width, lineH, line, MitigationColour(mit));
    y += lineH + gap;
}

// Cut / Blunt / Pierce with each % coloured independently.
inline void AddResistLine(
    MyGUI::Widget* parent,
    const char* baseName,
    float x,
    float& y,
    float lineH,
    float gap,
    float cut,
    float blunt,
    float pierce,
    float width = 0.92f)
{
    const float cutP = displayPercent(cut);
    const float bluntP = displayPercent(blunt);
    const float pierceP = displayPercent(pierce);
    const float colW = width / 3.0f;

    char nameBuf[64];
    char textBuf[64];

    sprintf_s(nameBuf, "%sCut", baseName);
    sprintf_s(textBuf, TrFmt("Cut %s"), FormatPercentNumber(cutP).c_str());
    AddColoredLine(parent, nameBuf, x, y, colW, lineH, textBuf, MitigationColour(cutP));

    sprintf_s(nameBuf, "%sBlunt", baseName);
    sprintf_s(textBuf, TrFmt("Blunt %s"), FormatPercentNumber(bluntP).c_str());
    AddColoredLine(parent, nameBuf, x + colW, y, colW, lineH, textBuf, MitigationColour(bluntP));

    sprintf_s(nameBuf, "%sPierce", baseName);
    sprintf_s(textBuf, TrFmt("Pierce %s"), FormatPercentNumber(pierceP).c_str());
    AddColoredLine(parent, nameBuf, x + colW * 2.0f, y, colW, lineH, textBuf, MitigationColour(pierceP));

    y += lineH + gap;
}

// Защита от среды: пять значений в три колонки, двумя строками.
// В одну строку по пятой части ширины не влезает: «Опаление 100%» шире
// колонки и наезжает на соседа. Сетка 3 + 2 повторяет ритм строки
// Cut / Blunt / Pierce выше, поэтому выглядит как её продолжение.
inline void AddWeatherLine(
    MyGUI::Widget* parent,
    const char* baseName,
    float x,
    float& y,
    float lineH,
    float gap,
    float dust,
    float acid,
    float burning,
    float gas,
    float rain,
    float width = 0.92f)
{
    const char* labels[5] = { "Dust", "Acid", "Burn", "Gas", "Rain" };
    float values[5];
    values[0] = dust;
    values[1] = acid;
    values[2] = burning;
    values[3] = gas;
    values[4] = rain;

    const int perRow = 3;
    const float colW = width / (float)perRow;
    const float rowStep = lineH + gap * 0.5f;
    char nameBuf[64];
    char textBuf[64];

    for (int i = 0; i < 5; ++i)
    {
        const int row = i / perRow;
        const int col = i % perRow;
        const float p = displayPercent(values[i]);
        sprintf_s(nameBuf, "%sW%d", baseName, i);
        // labels[] остаются английскими ключами словаря, перевод берётся здесь
                sprintf_s(textBuf, "%s %s", Tr(labels[i]), FormatPercentNumber(p).c_str());
        AddColoredLine(
            parent, nameBuf,
            x + colW * col,
            y + rowStep * row,
            colW, lineH, textBuf, MitigationColour(p));
    }

    y += rowStep + lineH + gap;
}

// Part name (plain) + mitigation % (coloured).
inline void AddPartSummaryLine(
    MyGUI::Widget* parent,
    const char* baseName,
    float x,
    float y,
    float w,
    float h,
    const std::string& partName,
    float mitigationPercent)
{
    // Reserve a fraction of *this column* for the %, not of the whole panel —
    // absolute widths were overlapping names in the two-column layout.
    const float pctW = w * 0.38f;
    const float gap = w * 0.04f;
    const float nameW = w - pctW - gap;
    if (nameW <= 0.05f || pctW <= 0.05f)
        return;

    char nameBuf[64];
    sprintf_s(nameBuf, "%sName", baseName);
    AddTextLine(parent, nameBuf, x, y, nameW, h, partName);

    sprintf_s(nameBuf, "%sPct", baseName);
    char pctBuf[32];
    sprintf_s(pctBuf, "~%.0f%%", clampDisplayPercent(mitigationPercent));
    AddColoredLine(
        parent, nameBuf, x + w - pctW, y, pctW, h,
        pctBuf, MitigationColour(mitigationPercent));
}

// Armour piece resist row: labels muted, each % coloured.
inline void AddPieceStatsLine(
    MyGUI::Widget* parent,
    const char* baseName,
    float x,
    float& y,
    float w,
    float lineH,
    float gap,
    float coverage,
    float cut,
    float blunt,
    float pierce)
{
    const float coverP = clampDisplayPercent(coverageToPercent(coverage));
    const float cutP = displayPercent(cut);
    const float bluntP = displayPercent(blunt);
    const float pierceP = displayPercent(pierce);

    // Колонки неравные: первая подпись - слово («Cover», «Покр.»), три
    // остальные - одна буква. При равной ширине русская «Покр. 100%» лезет
    // на соседа, а справа остаётся пустое место.
    //
    // Разрыв после первой колонки задан отдельно: текст прижат к левому краю
    // своей колонки, поэтому просвет до соседа - это её незанятый остаток.
    // «Покр. 100%» заполняет колонку почти целиком, и без gapX соседняя
    // подпись начинается сразу за последним символом.
    const float gapX = w * 0.04f;
    const float coverW = w * 0.30f;
    const float colW = w * 0.22f;       // 0.30 + 0.04 + 0.22*3 = 1.0
    const float col1X = x + coverW + gapX;
    char nameBuf[64];
    char textBuf[48];

    sprintf_s(nameBuf, "%sCover", baseName);
    sprintf_s(textBuf, TrFmt("Cover %s"), FormatPercentNumber(coverP).c_str());
    AddColoredLine(parent, nameBuf, x, y, coverW, lineH, textBuf, MitigationColour(coverP));

    sprintf_s(nameBuf, "%sCut", baseName);
    sprintf_s(textBuf, TrFmt("C %s"), FormatPercentNumber(cutP).c_str());
    AddColoredLine(parent, nameBuf, col1X, y, colW, lineH, textBuf, MitigationColour(cutP));

    sprintf_s(nameBuf, "%sBlunt", baseName);
    sprintf_s(textBuf, TrFmt("B %s"), FormatPercentNumber(bluntP).c_str());
    AddColoredLine(parent, nameBuf, col1X + colW, y, colW, lineH, textBuf, MitigationColour(bluntP));

    sprintf_s(nameBuf, "%sPierce", baseName);
    sprintf_s(textBuf, TrFmt("P %s"), FormatPercentNumber(pierceP).c_str());
    AddColoredLine(parent, nameBuf, col1X + colW * 2.0f, y, colW, lineH, textBuf, MitigationColour(pierceP));

    y += lineH + gap;
}

// Part-mode armour row: optional inventory icon left, name + Cover/C/B/P to the right.
inline void AddPieceRowWithIcon(
    MyGUI::Widget* parent,
    unsigned index,
    float x,
    float& y,
    float w,
    float lineH,
    float gap,
    const std::string& name,
    const std::string& iconTexture,
    float coverage,
    float cut,
    float blunt,
    float pierce)
{
    const bool hasIcon = !iconTexture.empty();
    // Relative height ~ two text lines; width tuned to look square on the content panel.
    const float iconH = lineH * 2.0f + 0.006f;
    const float iconW = hasIcon ? 0.16f : 0.0f;
    const float iconGap = hasIcon ? 0.02f : 0.0f;
    const float textX = x + iconW + iconGap;
    const float textW = w - (iconW + iconGap);
    if (textW <= 0.05f)
        return;

    const float rowStartY = y;
    char widgetName[64];

    if (hasIcon)
    {
        sprintf_s(widgetName, "PieceIcon%u", index);
        MyGUI::ImageBox* icon = parent->createWidgetReal<MyGUI::ImageBox>(
            "ImageBox", x, y, iconW, iconH, MyGUI::Align::Default, widgetName);
        if (icon)
            icon->setImageTexture(iconTexture);
    }

    sprintf_s(widgetName, "PieceName%u", index);
    AddTextLine(parent, widgetName, textX, y, textW, lineH, name);

    float statsY = y + lineH + 0.006f;
    sprintf_s(widgetName, "PieceStats%u", index);
    AddPieceStatsLine(
        parent, widgetName, textX, statsY, textW, lineH, gap,
        coverage, cut, blunt, pierce);

    // AddPieceStatsLine advances statsY past the stats line + gap.
    y = statsY;
    if (hasIcon)
    {
        const float iconBottom = rowStartY + iconH + 0.012f;
        if (iconBottom > y)
            y = iconBottom;
    }
}