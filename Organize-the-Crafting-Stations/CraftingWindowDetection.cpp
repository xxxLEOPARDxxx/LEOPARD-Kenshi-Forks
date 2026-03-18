#include "CraftingWindowDetection.h"

#include "CraftingCore.h"
#include "CraftingSearchText.h"

#include <kenshi/Character.h>
#include <kenshi/Dialogue.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/Inventory.h>
#include <kenshi/PlayerInterface.h>

#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>

#include <Windows.h>

#include <sstream>
#include <vector>

namespace
{
struct CraftingProbeToken
{
    const char* token;
    int score;
};

struct CraftingResolutionProbeToken
{
    const char* token;
    int score;
};

bool WidgetMatchesCraftingProbeToken(MyGUI::Widget* widget, const char* token)
{
    if (widget == 0 || token == 0 || *token == '\0')
    {
        return false;
    }

    return ContainsAsciiCaseInsensitive(SafeWidgetName(widget), token)
        || ContainsAsciiCaseInsensitive(WidgetCaptionForLog(widget), token);
}

bool FindCraftingProbeTokenMatchRecursive(
    MyGUI::Widget* widget,
    const char* token,
    std::size_t depth,
    std::size_t maxDepth,
    std::string* outMatchedWidgetName)
{
    if (widget == 0 || token == 0 || depth > maxDepth)
    {
        return false;
    }

    if (WidgetMatchesCraftingProbeToken(widget, token))
    {
        if (outMatchedWidgetName != 0)
        {
            *outMatchedWidgetName = SafeWidgetName(widget);
        }
        return true;
    }

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        if (FindCraftingProbeTokenMatchRecursive(
                widget->getChildAt(childIndex),
                token,
                depth + 1,
                maxDepth,
                outMatchedWidgetName))
        {
            return true;
        }
    }

    return false;
}

int ComputeCraftingWindowCandidateScore(MyGUI::Widget* parent, std::string* outReason)
{
    if (outReason != 0)
    {
        outReason->clear();
    }

    if (parent == 0)
    {
        return 0;
    }

    MyGUI::Window* window = FindOwningWindow(parent);
    const std::string caption = window == 0 ? "" : window->getCaption().asUTF8();

    int score = 0;
    bool hasSignal = false;
    std::stringstream reason;

    if (ContainsAsciiCaseInsensitive(caption, "craft"))
    {
        score += 2200;
        hasSignal = true;
        reason << " caption=craft";
    }

    if (ContainsAsciiCaseInsensitive(caption, "recipe"))
    {
        score += 2400;
        hasSignal = true;
        reason << " caption=recipe";
    }

    if (ContainsAsciiCaseInsensitive(caption, "cook"))
    {
        score += 1600;
        hasSignal = true;
        reason << " caption=cook";
    }

    static const CraftingProbeToken kCraftingProbeTokens[] =
    {
        { "recipe", 1800 },
        { "ingredient", 1500 },
        { "material", 1300 },
        { "craft", 1200 },
        { "queue", 900 },
        { "blueprint", 800 },
        { "production", 700 },
        { "cook", 600 },
    };

    for (std::size_t index = 0; index < sizeof(kCraftingProbeTokens) / sizeof(kCraftingProbeTokens[0]); ++index)
    {
        std::string matchedWidgetName;
        if (!FindCraftingProbeTokenMatchRecursive(
                parent,
                kCraftingProbeTokens[index].token,
                0,
                5,
                &matchedWidgetName))
        {
            continue;
        }

        score += kCraftingProbeTokens[index].score;
        hasSignal = true;
        reason << " token=" << kCraftingProbeTokens[index].token
               << "@" << TruncateForLog(matchedWidgetName, 48);
    }

    if (!hasSignal)
    {
        return 0;
    }

    if (outReason != 0)
    {
        *outReason = reason.str();
    }

    return score;
}

int ComputeCraftingWindowResolutionBias(MyGUI::Widget* parent, std::string* outReason)
{
    if (outReason != 0)
    {
        outReason->clear();
    }

    if (parent == 0)
    {
        return 0;
    }

    MyGUI::Window* window = FindOwningWindow(parent);
    const std::string caption = window == 0 ? "" : window->getCaption().asUTF8();

    int score = 0;
    std::stringstream reason;

    if (ContainsAsciiCaseInsensitive(caption, "bench"))
    {
        score += 400;
        reason << " caption=bench";
    }

    if (ContainsAsciiCaseInsensitive(caption, "oven"))
    {
        score += 300;
        reason << " caption=oven";
    }

    if (ContainsAsciiCaseInsensitive(caption, "stove"))
    {
        score += 300;
        reason << " caption=stove";
    }

    static const CraftingResolutionProbeToken kResolutionTokens[] =
    {
        { "CraftingPanel", 800 },
        { "QueueButton", 600 },
        { "QueueRepeat", 1200 },
        { "BlueprintsAvailablePanel", 2200 },
        { "ResearchQueuePanel", 1800 },
        { "CraftingStationsList", 2200 },
        { "lbCraftingItems", 900 },
        { "lbCraftingQueue", 900 },
        { "lbCraftingStations", 900 },
        { "lbCraftingDescription", 600 },
    };

    std::size_t richSignalCount = 0;
    bool hasHeaderOnlySignal = false;
    for (std::size_t index = 0; index < sizeof(kResolutionTokens) / sizeof(kResolutionTokens[0]); ++index)
    {
        MyGUI::Widget* matched = FindWidgetInParentByToken(parent, kResolutionTokens[index].token);
        if (matched == 0)
        {
            continue;
        }

        score += kResolutionTokens[index].score;
        reason << " token=" << kResolutionTokens[index].token
               << "@" << TruncateForLog(SafeWidgetName(matched), 48);

        const std::string token(kResolutionTokens[index].token);
        if (token == "CraftingPanel" || token == "QueueButton")
        {
            hasHeaderOnlySignal = true;
        }
        else
        {
            ++richSignalCount;
        }
    }

    if (richSignalCount >= 2)
    {
        score += 2600;
        reason << " rich_layout=true";
    }
    else if (hasHeaderOnlySignal && richSignalCount == 0)
    {
        score -= 1200;
        reason << " header_only=true";
    }

    if (outReason != 0)
    {
        *outReason = reason.str();
    }

    return score;
}

void DumpVisibleWindowSummaryForCraftingProbe()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return;
    }

    std::size_t index = 0;
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

        const MyGUI::IntCoord coord = root->getCoord();
        std::stringstream line;
        line << "craft_search.window_detect visible_window[" << index << "]"
             << " name=" << SafeWidgetName(root)
             << " caption=\"" << TruncateForLog(window->getCaption().asUTF8(), 60) << "\""
             << " child_count=" << root->getChildCount()
             << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";
        LogInfoLine(line.str());

        ++index;
        if (index >= 12)
        {
            break;
        }
    }
}
}

MyGUI::Widget* FindNamedDescendantRecursive(
    MyGUI::Widget* root,
    const char* widgetName,
    bool requireVisible)
{
    if (root == 0 || widgetName == 0)
    {
        return 0;
    }

    if ((!requireVisible || root->getInheritedVisible()) && root->getName() == widgetName)
    {
        return root;
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        MyGUI::Widget* child = root->getChildAt(childIndex);
        MyGUI::Widget* found = FindNamedDescendantRecursive(child, widgetName, requireVisible);
        if (found != 0)
        {
            return found;
        }
    }

    return 0;
}

MyGUI::Widget* FindFirstVisibleWidgetByName(const char* widgetName)
{
    if (widgetName == 0)
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

        MyGUI::Widget* found = FindNamedDescendantRecursive(root, widgetName, true);
        if (found != 0)
        {
            return found;
        }
    }

    return 0;
}

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

        MyGUI::Widget* found = FindNamedDescendantByTokenRecursive(root, token, true);
        if (found != 0)
        {
            return found;
        }
    }

    return 0;
}

MyGUI::Widget* FindWidgetInParentByToken(MyGUI::Widget* parent, const char* token)
{
    if (parent == 0 || token == 0)
    {
        return 0;
    }
    return FindNamedDescendantByTokenRecursive(parent, token, false);
}

MyGUI::Window* FindOwningWindow(MyGUI::Widget* widget)
{
    while (widget != 0)
    {
        MyGUI::Window* window = widget->castType<MyGUI::Window>(false);
        if (window != 0)
        {
            return window;
        }
        widget = widget->getParent();
    }

    return 0;
}

bool HasTraderInventoryMarkers(MyGUI::Widget* parent)
{
    if (parent == 0)
    {
        return false;
    }

    const bool hasArrangeButton = FindWidgetInParentByToken(parent, "ArrangeButton") != 0;
    const bool hasScrollView = FindWidgetInParentByToken(parent, "scrollview_backpack_content") != 0;
    const bool hasBackpack = FindWidgetInParentByToken(parent, "backpack_content") != 0;

    return hasArrangeButton && hasScrollView && hasBackpack;
}

bool HasTraderMoneyMarkers(MyGUI::Widget* parent)
{
    if (parent == 0)
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
        if (FindWidgetInParentByToken(parent, moneyTokens[index]) != 0)
        {
            return true;
        }
    }

    return false;
}

int ComputeTraderWindowCandidateScore(MyGUI::Widget* parent, std::string* outReason)
{
    if (outReason != 0)
    {
        outReason->clear();
    }

    if (parent == 0 || !HasTraderInventoryMarkers(parent))
    {
        return 0;
    }

    MyGUI::Window* window = FindOwningWindow(parent);
    const bool hasMoneyMarkers = HasTraderMoneyMarkers(parent);
    const std::string caption = window == 0 ? "" : window->getCaption().asUTF8();
    const bool captionHasTrader = ContainsAsciiCaseInsensitive(caption, "TRADER");
    const std::string normalizedCaption = NormalizeSearchText(caption);

    int score = 100;
    bool hasTraderSignal = false;
    std::stringstream reason;
    reason << "inventory_markers";

    if (hasMoneyMarkers)
    {
        score += 1600;
        hasTraderSignal = true;
        reason << " money_markers";
    }

    if (captionHasTrader)
    {
        score += 1400;
        hasTraderSignal = true;
        reason << " caption_token=trader";
    }

    Character* captionTrader = 0;
    int captionScore = 0;
    if (!normalizedCaption.empty()
        && TryResolveCaptionMatchedTraderCharacter(parent, &captionTrader, &captionScore)
        && captionTrader != 0
        && captionScore > 0)
    {
        score += 900 + (captionScore > 2600 ? 2600 : captionScore);
        hasTraderSignal = true;
        reason << " caption_match=\"" << TruncateForLog(CharacterNameForLog(captionTrader), 40)
               << "\"(" << captionScore << ")";
    }

    Character* dialogueTarget = 0;
    Character* dialogueSpeaker = 0;
    std::string dialogueReason;
    if (!normalizedCaption.empty()
        && TryResolvePreferredDialogueTraderTarget(&dialogueTarget, &dialogueSpeaker, &dialogueReason)
        && dialogueTarget != 0)
    {
        const int dialogueCaptionScore = ComputeCaptionNameMatchBias(
            normalizedCaption,
            NormalizeSearchText(CharacterNameForLog(dialogueTarget)));
        if (dialogueCaptionScore > 0)
        {
            score += 1800 + (dialogueCaptionScore > 2800 ? 2800 : dialogueCaptionScore);
            hasTraderSignal = true;
            reason << " dialogue_match=\"" << TruncateForLog(CharacterNameForLog(dialogueTarget), 40)
                   << "\"(" << dialogueCaptionScore << ")";
            if (captionTrader != 0 && captionTrader == dialogueTarget)
            {
                score += 700;
                reason << " caption_dialogue_same=true";
            }
        }
    }

    if (!hasTraderSignal)
    {
        return 0;
    }

    if (outReason != 0)
    {
        *outReason = reason.str();
    }

    return score;
}

bool IsLikelyTraderWindow(MyGUI::Widget* parent)
{
    return ComputeTraderWindowCandidateScore(parent, 0) > 0;
}

bool HasTraderStructure(MyGUI::Widget* parent)
{
    return HasTraderInventoryMarkers(parent) || HasTraderMoneyMarkers(parent);
}

MyGUI::Widget* FindWidgetByName(const char* widgetName)
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return 0;
    }
    return gui->findWidgetT(widgetName, false);
}

void DumpTraderTargetProbe()
{
    const char* probeTokens[] =
    {
        "scrollview_backpack_content",
        "backpack_content",
        "ArrangeButton",
        "datapanel",
        "TradePanel",
        "CharacterSelectionItemBox",
        "MoneyAmountTextBox",
        "TotalMoneyBuyer"
    };

    for (std::size_t index = 0; index < sizeof(probeTokens) / sizeof(probeTokens[0]); ++index)
    {
        const char* token = probeTokens[index];
        const bool exactAny = FindWidgetByName(token) != 0;
        const bool exactVisible = FindFirstVisibleWidgetByName(token) != 0;
        const bool tokenVisible = FindFirstVisibleWidgetByToken(token) != 0;

        std::stringstream line;
        line << "probe token=" << token
             << " exact_any=" << (exactAny ? "true" : "false")
             << " exact_visible=" << (exactVisible ? "true" : "false")
             << " token_visible=" << (tokenVisible ? "true" : "false");
        LogInfoLine(line.str());
    }
}

void DumpVisibleWindowCandidateDiagnostics()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        LogWarnLine("GUI singleton unavailable while dumping window candidates");
        return;
    }

    std::size_t index = 0;
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

        MyGUI::Widget* parent = ResolveInjectionParent(root);
        if (parent == 0)
        {
            continue;
        }

        const bool hasMarkers = HasTraderStructure(parent);
        const bool hasMoneyMarkers = HasTraderMoneyMarkers(parent);
        const bool captionHasTrader = ContainsAsciiCaseInsensitive(window->getCaption().asUTF8(), "TRADER");
        if (!hasMarkers && !captionHasTrader)
        {
            continue;
        }

        std::string candidateReason;
        const int candidateScore = ComputeTraderWindowCandidateScore(parent, &candidateReason);
        const bool likelyTrader = candidateScore > 0;
        const MyGUI::IntCoord coord = root->getCoord();
        std::stringstream line;
        line << "window-candidate[" << index << "]"
             << " name=" << SafeWidgetName(root)
             << " caption=\"" << TruncateForLog(window->getCaption().asUTF8(), 60) << "\""
             << " has_markers=" << (hasMarkers ? "true" : "false")
             << " has_money_markers=" << (hasMoneyMarkers ? "true" : "false")
             << " caption_has_trader=" << (captionHasTrader ? "true" : "false")
             << " likely_trader=" << (likelyTrader ? "true" : "false")
             << " candidate_score=" << candidateScore
             << " candidate_reason=\"" << TruncateForLog(candidateReason, 120) << "\""
             << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";
        LogInfoLine(line.str());
        ++index;
    }

    if (index == 0)
    {
        LogInfoLine("window-candidate scan found no likely trader windows");
    }
}

void DumpVisibleCraftingWindowCandidateDiagnostics()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        LogWarnLine("craft_search.window_detect gui_unavailable");
        return;
    }

    std::size_t candidateIndex = 0;
    std::size_t subtreeDumpCount = 0;
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

        MyGUI::Widget* parent = ResolveInjectionParent(root);
        if (parent == 0)
        {
            continue;
        }

        std::string candidateReason;
        const int candidateScore = ComputeCraftingWindowCandidateScore(parent, &candidateReason);
        if (candidateScore <= 0)
        {
            continue;
        }

        const MyGUI::IntCoord coord = root->getCoord();
        std::stringstream line;
        line << "craft_search.window_detect candidate[" << candidateIndex << "]"
             << " name=" << SafeWidgetName(root)
             << " parent=" << SafeWidgetName(parent)
             << " caption=\"" << TruncateForLog(window->getCaption().asUTF8(), 60) << "\""
             << " score=" << candidateScore
             << " reason=\"" << TruncateForLog(candidateReason, 220) << "\""
             << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";
        LogInfoLine(line.str());

        if (subtreeDumpCount < 3)
        {
            DumpWidgetSubtreeDiagnostics("craft_search.widget_probe", parent);
            ++subtreeDumpCount;
        }

        ++candidateIndex;
    }

    if (candidateIndex > 0)
    {
        return;
    }

    LogInfoLine("craft_search.window_detect no candidates found; dumping visible window summary");
    DumpVisibleWindowSummaryForCraftingProbe();

    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        LogWarnLine("craft_search.widget_probe hovered_widget_unavailable");
        return;
    }

    MyGUI::Widget* hovered = inputManager->getMouseFocusWidget();
    if (hovered == 0)
    {
        LogWarnLine("craft_search.widget_probe no hovered widget for fallback diagnostics");
        return;
    }

    MyGUI::Widget* anchor = FindBestWindowAnchor(hovered);
    MyGUI::Widget* parent = ResolveInjectionParent(anchor);
    DumpHoveredAttachDiagnostics(hovered, anchor, parent);
    if (parent != 0)
    {
        DumpWidgetSubtreeDiagnostics("craft_search.widget_probe_hovered_parent", parent);
    }
}

bool TryResolveVisibleCraftingTarget(MyGUI::Widget** outAnchor, MyGUI::Widget** outParent)
{
    if (outAnchor != 0)
    {
        *outAnchor = 0;
    }
    if (outParent != 0)
    {
        *outParent = 0;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return false;
    }

    MyGUI::Widget* bestAnchor = 0;
    MyGUI::Widget* bestParent = 0;
    std::string bestReason;
    int bestScore = -1;
    int bestArea = 0;

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

        MyGUI::Widget* parent = ResolveInjectionParent(root);
        if (parent == 0)
        {
            continue;
        }

        std::string candidateReason;
        const int candidateScore = ComputeCraftingWindowCandidateScore(parent, &candidateReason);
        if (candidateScore <= 0)
        {
            continue;
        }

        std::string resolutionReason;
        const int resolutionBias = ComputeCraftingWindowResolutionBias(parent, &resolutionReason);
        const int totalScore = candidateScore + resolutionBias;
        const MyGUI::IntCoord coord = root->getCoord();
        const int area = coord.width * coord.height;

        if (bestAnchor == 0
            || totalScore > bestScore
            || (totalScore == bestScore && area < bestArea))
        {
            bestAnchor = root;
            bestParent = parent;
            bestScore = totalScore;
            bestArea = area;
            bestReason = candidateReason;
            if (!resolutionReason.empty())
            {
                if (!bestReason.empty())
                {
                    bestReason += " ";
                }
                bestReason += resolutionReason;
            }
        }
    }

    if (bestAnchor == 0 || bestParent == 0)
    {
        return false;
    }

    if (outAnchor != 0)
    {
        *outAnchor = bestAnchor;
    }
    if (outParent != 0)
    {
        *outParent = bestParent;
    }

    MyGUI::Window* window = bestAnchor->castType<MyGUI::Window>(false);
    std::stringstream line;
    line << "resolved crafting target via window-scan"
         << " anchor=" << SafeWidgetName(bestAnchor)
         << " parent=" << SafeWidgetName(bestParent)
         << " caption=\"" << (window == 0 ? "" : TruncateForLog(window->getCaption().asUTF8(), 60)) << "\""
         << " candidate_score=" << bestScore
         << " candidate_reason=\"" << TruncateForLog(bestReason, 200) << "\"";
    LogDebugLine(line.str());
    return true;
}

bool IsLikelyCraftingWindow(MyGUI::Widget* parent)
{
    return ComputeCraftingWindowCandidateScore(parent, 0) > 0;
}

bool TryResolveVisibleTraderTarget(MyGUI::Widget** outAnchor, MyGUI::Widget** outParent)
{
    if (outAnchor != 0)
    {
        *outAnchor = 0;
    }
    if (outParent != 0)
    {
        *outParent = 0;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return false;
    }

    MyGUI::Widget* bestAnchor = 0;
    MyGUI::Widget* bestParent = 0;
    std::string bestReason;
    int bestScore = -1;
    int bestArea = -1;

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

        MyGUI::Widget* parent = ResolveInjectionParent(root);
        if (parent == 0)
        {
            continue;
        }

        std::string candidateReason;
        const int candidateScore = ComputeTraderWindowCandidateScore(parent, &candidateReason);
        if (candidateScore <= 0)
        {
            continue;
        }

        const MyGUI::IntCoord coord = root->getCoord();
        const int area = coord.width * coord.height;
        if (bestAnchor == 0
            || candidateScore > bestScore
            || (candidateScore == bestScore && area > bestArea))
        {
            bestAnchor = root;
            bestParent = parent;
            bestReason = candidateReason;
            bestScore = candidateScore;
            bestArea = area;
        }
    }

    if (bestAnchor == 0 || bestParent == 0)
    {
        return false;
    }

    if (outAnchor != 0)
    {
        *outAnchor = bestAnchor;
    }
    if (outParent != 0)
    {
        *outParent = bestParent;
    }

    MyGUI::Window* window = bestAnchor->castType<MyGUI::Window>(false);
    std::stringstream line;
    line << "resolved trader target via window-scan"
         << " anchor=" << SafeWidgetName(bestAnchor)
         << " parent=" << SafeWidgetName(bestParent)
         << " caption=\"" << (window == 0 ? "" : TruncateForLog(window->getCaption().asUTF8(), 60)) << "\""
         << " candidate_score=" << bestScore
         << " candidate_reason=\"" << TruncateForLog(bestReason, 160) << "\"";
    LogDebugLine(line.str());
    return true;
}

bool TryResolveHoveredTarget(MyGUI::Widget** outAnchor, MyGUI::Widget** outParent, bool logFailures)
{
    if (outAnchor != 0)
    {
        *outAnchor = 0;
    }
    if (outParent != 0)
    {
        *outParent = 0;
    }

    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        if (logFailures)
        {
            LogWarnLine("hover attach failed: MyGUI InputManager unavailable");
        }
        return false;
    }

    MyGUI::Widget* hovered = inputManager->getMouseFocusWidget();
    if (hovered == 0)
    {
        if (logFailures)
        {
            LogWarnLine("hover attach failed: no mouse-focused widget");
        }
        return false;
    }

    MyGUI::Widget* anchor = FindBestWindowAnchor(hovered);
    MyGUI::Widget* parent = ResolveInjectionParent(anchor);
    if (anchor == 0 || parent == 0)
    {
        if (logFailures)
        {
            std::stringstream line;
            line << "hover attach failed: anchor/parent unresolved hovered_chain=" << BuildParentChainForLog(hovered);
            LogWarnLine(line.str());
        }
        return false;
    }

    std::string candidateReason;
    const int candidateScore = ComputeTraderWindowCandidateScore(parent, &candidateReason);
    if (candidateScore <= 0)
    {
        if (logFailures)
        {
            std::stringstream line;
            line << "hover attach rejected"
                 << " anchor=" << SafeWidgetName(anchor)
                 << " parent=" << SafeWidgetName(parent)
                 << " candidate_score=" << candidateScore
                 << " candidate_reason=\"" << TruncateForLog(candidateReason, 120) << "\""
                 << " parent_coord=(" << parent->getCoord().left << "," << parent->getCoord().top << ","
                 << parent->getCoord().width << "," << parent->getCoord().height << ")"
                 << " hovered_chain=" << BuildParentChainForLog(hovered);
            LogWarnLine(line.str());
        }
        return false;
    }

    if (outAnchor != 0)
    {
        *outAnchor = anchor;
    }
    if (outParent != 0)
    {
        *outParent = parent;
    }

    std::stringstream line;
    line << "resolved trader target via hover attach"
         << " anchor=" << SafeWidgetName(anchor)
         << " parent=" << SafeWidgetName(parent)
         << " candidate_score=" << candidateScore
         << " candidate_reason=\"" << TruncateForLog(candidateReason, 160) << "\""
         << " parent_coord=(" << parent->getCoord().left << "," << parent->getCoord().top << ","
         << parent->getCoord().width << "," << parent->getCoord().height << ")"
         << " hovered_chain=" << BuildParentChainForLog(hovered);
    LogInfoLine(line.str());
    return true;
}

MyGUI::Widget* FindBestWindowAnchor(MyGUI::Widget* fromWidget)
{
    MyGUI::Widget* current = fromWidget;
    MyGUI::Widget* rootMost = 0;
    MyGUI::Widget* windowAncestor = 0;

    while (current != 0)
    {
        rootMost = current;
        if (current->castType<MyGUI::Window>(false) != 0)
        {
            windowAncestor = current;
        }
        current = current->getParent();
    }

    if (windowAncestor != 0)
    {
        return windowAncestor;
    }
    return rootMost;
}

MyGUI::Widget* ResolveInjectionParent(MyGUI::Widget* anchor)
{
    if (anchor == 0)
    {
        return 0;
    }

    MyGUI::Window* window = anchor->castType<MyGUI::Window>(false);
    if (window != 0)
    {
        MyGUI::Widget* client = window->getClientWidget();
        if (client != 0)
        {
            return client;
        }
    }

    return anchor;
}

MyGUI::Widget* ResolveTraderParentFromControlsContainer(MyGUI::Widget* controlsContainer)
{
    if (controlsContainer == 0)
    {
        return 0;
    }

    MyGUI::Widget* anchor = FindBestWindowAnchor(controlsContainer);
    MyGUI::Widget* parent = ResolveInjectionParent(anchor);
    if (parent != 0 && IsLikelyTraderWindow(parent))
    {
        return parent;
    }

    MyGUI::Widget* current = controlsContainer->getParent();
    while (current != 0)
    {
        if (IsLikelyTraderWindow(current))
        {
            return current;
        }

        current = current->getParent();
    }

    return parent;
}

MyGUI::Widget* ResolveInventoryEntriesRoot(MyGUI::Widget* backpackContent)
{
    if (backpackContent == 0)
    {
        return 0;
    }

    MyGUI::Widget* current = backpackContent;
    for (std::size_t unwrapDepth = 0; unwrapDepth < 8; ++unwrapDepth)
    {
        if (current->getChildCount() != 1)
        {
            break;
        }

        MyGUI::Widget* onlyChild = current->getChildAt(0);
        if (onlyChild == 0)
        {
            break;
        }

        current = onlyChild;
    }

    return current;
}

std::size_t CountOccupiedEntriesInEntriesRoot(MyGUI::Widget* entriesRoot)
{
    if (entriesRoot == 0)
    {
        return 0;
    }

    std::size_t occupiedCount = 0;
    const std::size_t childCount = entriesRoot->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        int quantity = 0;
        if (TryResolveItemQuantityFromWidget(entriesRoot->getChildAt(childIndex), &quantity)
            && quantity > 0)
        {
            ++occupiedCount;
        }
    }

    return occupiedCount;
}

MyGUI::Widget* ResolveBestBackpackContentWidget(
    MyGUI::Widget* traderParent,
    bool logDiagnostics,
    bool allowSelectionLogging)
{
    if (traderParent == 0)
    {
        return 0;
    }

    std::vector<MyGUI::Widget*> candidates;
    CollectNamedDescendantsByToken(
        traderParent,
        "backpack_content",
        false,
        24,
        &candidates);
    if (candidates.empty())
    {
        return 0;
    }

    struct CandidateScore
    {
        MyGUI::Widget* backpack;
        MyGUI::Widget* entriesRoot;
        std::size_t childCount;
        std::size_t occupiedCount;
        MyGUI::IntCoord absoluteCoord;
        int score;
    };

    std::vector<CandidateScore> scoredCandidates;
    scoredCandidates.reserve(candidates.size());

    int bestScore = -1000000;
    int bestLeft = -1000000;
    MyGUI::Widget* bestBackpack = 0;
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        MyGUI::Widget* backpack = candidates[index];
        if (backpack == 0)
        {
            continue;
        }

        MyGUI::Widget* entriesRoot = ResolveInventoryEntriesRoot(backpack);
        const std::size_t childCount = entriesRoot == 0 ? 0 : entriesRoot->getChildCount();
        const std::size_t occupiedCount = CountOccupiedEntriesInEntriesRoot(entriesRoot);
        const MyGUI::IntCoord absoluteCoord =
            entriesRoot != 0 ? entriesRoot->getAbsoluteCoord() : backpack->getAbsoluteCoord();

        int score = 0;
        score += backpack->getInheritedVisible() ? 1800 : 0;
        score += entriesRoot != 0 ? 800 : 0;
        score += static_cast<int>(occupiedCount) * 240;
        score += static_cast<int>(childCount) * 14;
        if (childCount > 0 && occupiedCount * 2 >= childCount)
        {
            score += 600;
        }

        CandidateScore candidateScore;
        candidateScore.backpack = backpack;
        candidateScore.entriesRoot = entriesRoot;
        candidateScore.childCount = childCount;
        candidateScore.occupiedCount = occupiedCount;
        candidateScore.absoluteCoord = absoluteCoord;
        candidateScore.score = score;
        scoredCandidates.push_back(candidateScore);

        if (bestBackpack == 0
            || score > bestScore
            || (score == bestScore && absoluteCoord.left > bestLeft))
        {
            bestScore = score;
            bestLeft = absoluteCoord.left;
            bestBackpack = backpack;
        }
    }

    std::size_t selectedOccupied = 0;
    std::size_t selectedChildCount = 0;
    MyGUI::Widget* selectedEntriesRoot = 0;
    MyGUI::IntCoord selectedCoord;
    for (std::size_t index = 0; index < scoredCandidates.size(); ++index)
    {
        if (scoredCandidates[index].backpack != bestBackpack)
        {
            continue;
        }

        selectedOccupied = scoredCandidates[index].occupiedCount;
        selectedChildCount = scoredCandidates[index].childCount;
        selectedEntriesRoot = scoredCandidates[index].entriesRoot;
        selectedCoord = scoredCandidates[index].absoluteCoord;
        break;
    }

    std::stringstream signature;
    signature << SafeWidgetName(traderParent) << "|candidates=" << scoredCandidates.size()
              << "|selected=" << SafeWidgetName(bestBackpack)
              << "|entries=" << SafeWidgetName(selectedEntriesRoot)
              << "|occupied=" << selectedOccupied
              << "|children=" << selectedChildCount
              << "|x=" << selectedCoord.left;
    const bool shouldLog =
        allowSelectionLogging
        &&
        (logDiagnostics || (ShouldLogBindingDebug() && scoredCandidates.size() > 1))
        && signature.str() != TraderState().windowDetection.g_lastBackpackResolutionSignature;
    if (shouldLog)
    {
        std::stringstream summary;
        summary << "backpack resolver selected="
                << SafeWidgetName(bestBackpack)
                << " entries_root=" << SafeWidgetName(selectedEntriesRoot)
                << " occupied=" << selectedOccupied
                << " child_count=" << selectedChildCount
                << " candidates=" << scoredCandidates.size()
                << " parent=" << SafeWidgetName(traderParent);
        LogInfoLine(summary.str());

        for (std::size_t index = 0; index < scoredCandidates.size(); ++index)
        {
            const CandidateScore& candidate = scoredCandidates[index];
            std::stringstream line;
            line << "backpack resolver candidate[" << index << "]"
                 << " name=" << SafeWidgetName(candidate.backpack)
                 << " entries_root=" << SafeWidgetName(candidate.entriesRoot)
                 << " occupied=" << candidate.occupiedCount
                 << " child_count=" << candidate.childCount
                 << " visible=" << (candidate.backpack->getInheritedVisible() ? "true" : "false")
                 << " score=" << candidate.score
                 << " abs_coord=(" << candidate.absoluteCoord.left
                 << "," << candidate.absoluteCoord.top
                 << "," << candidate.absoluteCoord.width
                 << "," << candidate.absoluteCoord.height << ")";
            LogInfoLine(line.str());
        }

        TraderState().windowDetection.g_lastBackpackResolutionSignature = signature.str();
    }

    return bestBackpack;
}

bool TryResolveCharacterInventoryVisible(Character* character, bool* visibleOut)
{
    if (visibleOut == 0)
    {
        return false;
    }

    *visibleOut = false;
    if (character == 0)
    {
        return true;
    }

    __try
    {
        if (character->inventory != 0 && character->inventory->isVisible())
        {
            *visibleOut = true;
            return true;
        }

        *visibleOut = character->isInventoryVisible();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool TryResolvePreferredDialogueTraderTarget(
    Character** outTarget,
    Character** outSpeaker,
    std::string* outReason)
{
    if (outTarget == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    *outTarget = 0;
    if (outSpeaker != 0)
    {
        *outSpeaker = 0;
    }
    if (outReason != 0)
    {
        outReason->clear();
    }

    Character* selectedCharacter = ou->player->selectedCharacter.getCharacter();
    std::vector<Character*> playerCharacters;
    if (selectedCharacter != 0)
    {
        playerCharacters.push_back(selectedCharacter);
    }

    const lektor<Character*>& allPlayerCharacters = ou->player->getAllPlayerCharacters();
    for (lektor<Character*>::const_iterator iter = allPlayerCharacters.begin(); iter != allPlayerCharacters.end(); ++iter)
    {
        Character* candidate = *iter;
        if (candidate == 0 || candidate == selectedCharacter)
        {
            continue;
        }
        playerCharacters.push_back(candidate);
    }

    Character* bestTarget = 0;
    Character* bestSpeaker = 0;
    int bestScore = -1000000;
    std::string bestReason;

    for (std::size_t charIndex = 0; charIndex < playerCharacters.size(); ++charIndex)
    {
        Character* playerChar = playerCharacters[charIndex];
        if (playerChar == 0 || playerChar->dialogue == 0)
        {
            continue;
        }

        Character* target = playerChar->dialogue->getConversationTarget().getCharacter();
        if (target == 0 || target->inventory == 0)
        {
            continue;
        }

        bool playerInventoryVisible = false;
        bool targetInventoryVisible = false;
        TryResolveCharacterInventoryVisible(playerChar, &playerInventoryVisible);
        TryResolveCharacterInventoryVisible(target, &targetInventoryVisible);

        const bool dialogActive = !playerChar->dialogue->conversationHasEndedPrettyMuch();
        const bool engaged = playerChar->_isEngagedWithAPlayer || target->_isEngagedWithAPlayer;
        const bool targetIsTrader = target->isATrader();
        if (!dialogActive && !targetIsTrader)
        {
            continue;
        }

        int score = 0;
        if (targetIsTrader)
        {
            score += 1800;
        }
        if (dialogActive)
        {
            score += 600;
        }
        if (engaged)
        {
            score += 240;
        }
        if (targetInventoryVisible)
        {
            score += 160;
        }
        if (playerInventoryVisible)
        {
            score += 90;
        }
        if (playerChar == selectedCharacter)
        {
            score += 180;
        }
        score += static_cast<int>(InventoryItemCountForLog(target->inventory));

        if (score > bestScore)
        {
            bestScore = score;
            bestTarget = target;
            bestSpeaker = playerChar;
            std::stringstream reason;
            reason << "dialog_target:" << CharacterNameForLog(target)
                   << " speaker=" << CharacterNameForLog(playerChar)
                   << " trader=" << (targetIsTrader ? "true" : "false")
                   << " dialog_active=" << (dialogActive ? "true" : "false")
                   << " engaged=" << (engaged ? "true" : "false")
                   << " visible=" << (targetInventoryVisible ? "true" : "false");
            bestReason = reason.str();
        }
    }

    if (bestTarget == 0)
    {
        return false;
    }

    *outTarget = bestTarget;
    if (outSpeaker != 0)
    {
        *outSpeaker = bestSpeaker;
    }
    if (outReason != 0)
    {
        *outReason = bestReason;
    }
    return true;
}

bool TryResolveCaptionMatchedTraderCharacter(
    MyGUI::Widget* traderParent,
    Character** outCharacter,
    int* outCaptionScore)
{
    if (outCharacter == 0 || traderParent == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    *outCharacter = 0;
    if (outCaptionScore != 0)
    {
        *outCaptionScore = 0;
    }

    MyGUI::Window* owningWindow = FindOwningWindow(traderParent);
    if (owningWindow == 0)
    {
        return false;
    }

    const std::string normalizedCaption = NormalizeSearchText(owningWindow->getCaption().asUTF8());
    if (normalizedCaption.empty())
    {
        return false;
    }

    Character* bestCharacter = 0;
    int bestScore = 0;
    const ogre_unordered_set<Character*>::type& activeCharacters = ou->getCharacterUpdateList();
    for (ogre_unordered_set<Character*>::type::const_iterator it = activeCharacters.begin();
         it != activeCharacters.end();
         ++it)
    {
        Character* candidate = *it;
        if (candidate == 0 || candidate->inventory == 0 || !candidate->isATrader())
        {
            continue;
        }

        const int captionScore = ComputeCaptionNameMatchBias(
            normalizedCaption,
            NormalizeSearchText(CharacterNameForLog(candidate)));
        if (captionScore <= 0)
        {
            continue;
        }

        if (captionScore > bestScore)
        {
            bestScore = captionScore;
            bestCharacter = candidate;
        }
    }

    if (bestCharacter == 0)
    {
        return false;
    }

    *outCharacter = bestCharacter;
    if (outCaptionScore != 0)
    {
        *outCaptionScore = bestScore;
    }
    return true;
}

std::string BuildTraderTargetIdentity(MyGUI::Widget* anchor, MyGUI::Widget* parent)
{
    MyGUI::Widget* identityWidget = parent != 0 ? parent : anchor;

    Character* captionTrader = 0;
    int captionScore = 0;
    if (identityWidget != 0
        && TryResolveCaptionMatchedTraderCharacter(identityWidget, &captionTrader, &captionScore)
        && captionTrader != 0
        && captionScore > 0)
    {
        return std::string("caption_trader:") + NormalizeSearchText(CharacterNameForLog(captionTrader));
    }

    MyGUI::Window* owningWindow = FindOwningWindow(identityWidget);
    const std::string normalizedCaption =
        owningWindow == 0 ? std::string() : NormalizeSearchText(owningWindow->getCaption().asUTF8());
    if (!normalizedCaption.empty())
    {
        Character* dialogueTarget = 0;
        Character* dialogueSpeaker = 0;
        std::string dialogueReason;
        if (TryResolvePreferredDialogueTraderTarget(&dialogueTarget, &dialogueSpeaker, &dialogueReason)
            && dialogueTarget != 0)
        {
            const int dialogueCaptionScore = ComputeCaptionNameMatchBias(
                normalizedCaption,
                NormalizeSearchText(CharacterNameForLog(dialogueTarget)));
            if (dialogueCaptionScore > 0)
            {
                return std::string("dialogue_trader:")
                    + NormalizeSearchText(CharacterNameForLog(dialogueTarget));
            }
        }

        return std::string("caption:") + normalizedCaption;
    }

    return std::string("widget:") + NormalizeSearchText(SafeWidgetName(identityWidget));
}
