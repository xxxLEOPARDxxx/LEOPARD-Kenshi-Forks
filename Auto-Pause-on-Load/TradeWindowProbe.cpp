#include "TradeWindowProbe.h"

#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>

#include <cctype>
#include <cstring>
#include <sstream>
#include <string>

namespace
{
MyGUI::Widget* FindNamedDescendantByTokenRecursive(
    MyGUI::Widget* root,
    const char* token,
    bool requireVisible);
bool ContainsAsciiCaseInsensitive(const std::string& haystack, const char* needle);

MyGUI::Widget* FindFirstVisibleWidgetByToken(const char* token)
{
    if (token == 0)
    {
        return 0;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return 0;
    }

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        MyGUI::Widget* found = FindNamedDescendantByTokenRecursive(root, token, false);
        if (found != 0)
        {
            return found;
        }
    }

    return 0;
}

bool AnyVisibleWindowCaptionContains(const char* token)
{
    if (token == 0 || *token == '\0')
    {
        return false;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return false;
    }

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        MyGUI::Window* window = root->castType<MyGUI::Window>(false);
        if (window != 0 && ContainsAsciiCaseInsensitive(window->getCaption().asUTF8(), token))
        {
            return true;
        }
    }

    return false;
}

bool NameMatchesToken(const std::string& name, const char* token)
{
    if (token == 0 || *token == '\0' || name.empty())
    {
        return false;
    }

    const std::string tokenValue(token);
    if (name == tokenValue)
    {
        return true;
    }

    if (name.size() <= tokenValue.size() + 1)
    {
        return false;
    }

    const std::size_t offset = name.size() - tokenValue.size();
    if (name[offset - 1] != '_')
    {
        return false;
    }

    return name.compare(offset, tokenValue.size(), tokenValue) == 0;
}

std::string UpperAsciiCopy(const std::string& value)
{
    std::string upper(value);
    for (std::size_t index = 0; index < upper.size(); ++index)
    {
        upper[index] = static_cast<char>(std::toupper(static_cast<unsigned char>(upper[index])));
    }
    return upper;
}

bool ContainsAsciiCaseInsensitive(const std::string& haystack, const char* needle)
{
    if (needle == 0 || *needle == '\0')
    {
        return false;
    }

    return UpperAsciiCopy(haystack).find(UpperAsciiCopy(std::string(needle))) != std::string::npos;
}

MyGUI::Widget* FindNamedDescendantByTokenRecursive(
    MyGUI::Widget* root,
    const char* token,
    bool requireVisible)
{
    if (root == 0 || token == 0)
    {
        return 0;
    }

    if ((!requireVisible || root->getInheritedVisible()) && NameMatchesToken(root->getName(), token))
    {
        return root;
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        MyGUI::Widget* found = FindNamedDescendantByTokenRecursive(root->getChildAt(childIndex), token, requireVisible);
        if (found != 0)
        {
            return found;
        }
    }

    return 0;
}

bool HasTraderInventoryMarkers(MyGUI::Widget* root)
{
    if (root == 0)
    {
        return false;
    }

    return FindNamedDescendantByTokenRecursive(root, "ArrangeButton", false) != 0
        && FindNamedDescendantByTokenRecursive(root, "scrollview_backpack_content", false) != 0
        && FindNamedDescendantByTokenRecursive(root, "backpack_content", false) != 0;
}

bool HasTraderMoneyMarkers(MyGUI::Widget* root)
{
    if (root == 0)
    {
        return false;
    }

    const char* moneyTokens[] =
    {
        "MoneyAmountTextBox",
        "MoneyAmountText",
        "TotalMoneyBuyer",
        "lbTotalMoney",
        "MoneyLabelText",
        "lbBuyersMoney"
    };

    for (std::size_t index = 0; index < sizeof(moneyTokens) / sizeof(moneyTokens[0]); ++index)
    {
        if (FindNamedDescendantByTokenRecursive(root, moneyTokens[index], false) != 0)
        {
            return true;
        }
    }

    return false;
}

int ComputeTraderWindowCandidateScore(MyGUI::Window* window, std::string* outReason)
{
    if (outReason != 0)
    {
        outReason->clear();
    }

    if (window == 0)
    {
        return 0;
    }

    MyGUI::Widget* parent = window->getClientWidget();
    if (parent == 0)
    {
        parent = window;
    }

    if (!HasTraderInventoryMarkers(parent))
    {
        return 0;
    }

    const bool hasMoneyMarkers = HasTraderMoneyMarkers(parent);
    const bool captionHasTrader = ContainsAsciiCaseInsensitive(window->getCaption().asUTF8(), "TRADER");
    if (!hasMoneyMarkers && !captionHasTrader)
    {
        return 0;
    }

    int score = 100;
    std::stringstream reason;
    reason << "inventory_markers";

    if (hasMoneyMarkers)
    {
        score += 1600;
        reason << " money_markers";
    }

    if (captionHasTrader)
    {
        score += 1400;
        reason << " caption_token=trader";
    }

    if (outReason != 0)
    {
        *outReason = reason.str();
    }

    return score;
}
}

bool QueryVisibleTraderWindow(bool* tradeActiveOut, std::string* reasonOut)
{
    if (tradeActiveOut == 0)
    {
        return false;
    }

    *tradeActiveOut = false;
    if (reasonOut != 0)
    {
        reasonOut->clear();
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return true;
    }

    int bestScore = 0;
    std::string bestReason;

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        MyGUI::Window* window = root->castType<MyGUI::Window>(false);
        if (window == 0)
        {
            continue;
        }

        std::string candidateReason;
        const int candidateScore = ComputeTraderWindowCandidateScore(window, &candidateReason);
        if (candidateScore <= bestScore)
        {
            continue;
        }

        bestScore = candidateScore;
        bestReason = candidateReason;
    }

    if (bestScore <= 0)
    {
        const bool hasArrangeButton = FindFirstVisibleWidgetByToken("ArrangeButton") != 0;
        const bool hasScrollView = FindFirstVisibleWidgetByToken("scrollview_backpack_content") != 0;
        const bool hasBackpack = FindFirstVisibleWidgetByToken("backpack_content") != 0;
        const bool hasMoneyAmount = FindFirstVisibleWidgetByToken("MoneyAmountTextBox") != 0
            || FindFirstVisibleWidgetByToken("MoneyAmountText") != 0
            || FindFirstVisibleWidgetByToken("TotalMoneyBuyer") != 0
            || FindFirstVisibleWidgetByToken("lbTotalMoney") != 0
            || FindFirstVisibleWidgetByToken("MoneyLabelText") != 0
            || FindFirstVisibleWidgetByToken("lbBuyersMoney") != 0;
        const bool captionHasTrader = AnyVisibleWindowCaptionContains("TRADER");

        if (hasArrangeButton && hasScrollView && hasBackpack && (hasMoneyAmount || captionHasTrader))
        {
            *tradeActiveOut = true;
            if (reasonOut != 0)
            {
                std::stringstream reason;
                reason << "global_markers";
                if (hasMoneyAmount)
                {
                    reason << " money_markers";
                }
                if (captionHasTrader)
                {
                    reason << " caption_token=trader";
                }
                *reasonOut = reason.str();
            }
        }

        return true;
    }

    *tradeActiveOut = true;
    if (reasonOut != 0)
    {
        *reasonOut = bestReason;
    }
    return true;
}
