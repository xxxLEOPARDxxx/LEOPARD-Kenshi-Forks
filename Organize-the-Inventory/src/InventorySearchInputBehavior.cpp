#include "InventorySearchInputBehavior.h"

#include <cctype>

namespace InventorySearchInputBehavior
{
Selection::Selection()
    : active(false)
    , start(0u)
    , length(0u)
{
}

Selection::Selection(bool activeValue, std::size_t startValue, std::size_t lengthValue)
    : active(activeValue)
    , start(startValue)
    , length(lengthValue)
{
}

Snapshot::Snapshot()
    : cursor(0u)
{
}

Snapshot::Snapshot(const Text& textValue, std::size_t cursorValue, const Selection& selectionValue)
    : text(textValue)
    , cursor(cursorValue)
    , selection(selectionValue)
{
}

EditResult::EditResult()
    : handled(false)
    , rewriteText(false)
    , cursor(0u)
{
}

std::size_t ClampCursor(std::size_t cursor, std::size_t textLength)
{
    return cursor > textLength ? textLength : cursor;
}

Selection NormalizeSelection(const Selection& selection, std::size_t textLength)
{
    if (!selection.active || selection.length == 0u)
    {
        return Selection(false, ClampCursor(selection.start, textLength), 0u);
    }

    const std::size_t start = ClampCursor(selection.start, textLength);
    const std::size_t maxLength = textLength - start;
    const std::size_t length = selection.length > maxLength ? maxLength : selection.length;
    if (length == 0u)
    {
        return Selection(false, start, 0u);
    }

    return Selection(true, start, length);
}

bool IsTokenSeparator(Codepoint value)
{
    if (value < 0x80u)
    {
        const unsigned char byte = static_cast<unsigned char>(value);
        return byte == ':' || std::isspace(byte) != 0 || std::isalnum(byte) == 0;
    }

    return false;
}

std::size_t FindPreviousTokenBoundary(const Text& text, std::size_t cursor)
{
    std::size_t position = ClampCursor(cursor, text.size());

    while (position > 0u && IsTokenSeparator(text[position - 1u]))
    {
        --position;
    }

    while (position > 0u && !IsTokenSeparator(text[position - 1u]))
    {
        --position;
    }

    return position;
}

std::size_t FindNextTokenBoundary(const Text& text, std::size_t cursor)
{
    const std::size_t length = text.size();
    std::size_t position = ClampCursor(cursor, length);

    while (position < length && !IsTokenSeparator(text[position]))
    {
        ++position;
    }

    while (position < length && IsTokenSeparator(text[position]))
    {
        ++position;
    }

    return position;
}

EditResult ApplyShortcut(ShortcutKind shortcut, const Snapshot& snapshot)
{
    EditResult result;

    if (shortcut == ShortcutKind_None)
    {
        return result;
    }

    const std::size_t textLength = snapshot.text.size();
    const std::size_t cursor = ClampCursor(snapshot.cursor, textLength);
    const Selection selection = NormalizeSelection(snapshot.selection, textLength);

    result.handled = true;
    result.text = snapshot.text;
    result.cursor = cursor;
    result.selection = Selection(false, cursor, 0u);

    if (shortcut == ShortcutKind_CtrlLeft)
    {
        result.cursor = FindPreviousTokenBoundary(snapshot.text, cursor);
        result.selection = Selection(false, result.cursor, 0u);
        return result;
    }

    if (shortcut == ShortcutKind_CtrlRight)
    {
        result.cursor = FindNextTokenBoundary(snapshot.text, cursor);
        result.selection = Selection(false, result.cursor, 0u);
        return result;
    }

    if (selection.active)
    {
        result.rewriteText = true;
        result.text.erase(result.text.begin() + selection.start, result.text.begin() + selection.start + selection.length);
        result.cursor = selection.start;
        result.selection = Selection(false, result.cursor, 0u);
        return result;
    }

    result.rewriteText = true;
    const std::size_t deleteStart = FindPreviousTokenBoundary(snapshot.text, cursor);
    if (deleteStart != cursor)
    {
        result.text.erase(result.text.begin() + deleteStart, result.text.begin() + cursor);
    }
    result.cursor = deleteStart;
    result.selection = Selection(false, result.cursor, 0u);
    return result;
}
}
