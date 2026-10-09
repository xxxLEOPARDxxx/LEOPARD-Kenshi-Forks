#pragma once

#include <cstddef>
#include <vector>

namespace InventorySearchInputBehavior
{
typedef unsigned int Codepoint;
typedef std::vector<Codepoint> Text;

enum ShortcutKind
{
    ShortcutKind_None = 0,
    ShortcutKind_CtrlLeft,
    ShortcutKind_CtrlRight,
    ShortcutKind_CtrlBackspace,
};

struct Selection
{
    Selection();
    Selection(bool activeValue, std::size_t startValue, std::size_t lengthValue);

    bool active;
    std::size_t start;
    std::size_t length;
};

struct Snapshot
{
    Snapshot();
    Snapshot(const Text& textValue, std::size_t cursorValue, const Selection& selectionValue);

    Text text;
    std::size_t cursor;
    Selection selection;
};

struct EditResult
{
    EditResult();

    bool handled;
    bool rewriteText;
    Text text;
    std::size_t cursor;
    Selection selection;
};

std::size_t ClampCursor(std::size_t cursor, std::size_t textLength);
Selection NormalizeSelection(const Selection& selection, std::size_t textLength);
bool IsTokenSeparator(Codepoint value);
std::size_t FindPreviousTokenBoundary(const Text& text, std::size_t cursor);
std::size_t FindNextTokenBoundary(const Text& text, std::size_t cursor);
EditResult ApplyShortcut(ShortcutKind shortcut, const Snapshot& snapshot);
}
