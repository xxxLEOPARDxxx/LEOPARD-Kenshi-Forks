#include "InventoryWindowDetection.h"

#include "InventoryCore.h"

#include <kenshi/Character.h>
#include <kenshi/Dialogue.h>
#include <kenshi/GameData.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/Inventory.h>
#include <kenshi/PlayerInterface.h>

#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>

#include <cctype>
#include <sstream>
#include <vector>

namespace
{
static const std::size_t kMaxCaptionMatchPlayerCharacters = 256;
static const std::size_t kMaxCaptionMatchNameLength = 256;

bool ContainsAsciiCaseInsensitive(const std::string& haystack, const char* needle)
{
    if (needle == 0 || *needle == '\0')
    {
        return true;
    }

    std::string needleUpper;
    for (const char* current = needle; *current != '\0'; ++current)
    {
        needleUpper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(*current))));
    }

    std::string haystackUpper;
    haystackUpper.reserve(haystack.size());
    for (std::size_t index = 0; index < haystack.size(); ++index)
    {
        haystackUpper.push_back(static_cast<char>(
            std::toupper(static_cast<unsigned char>(haystack[index]))));
    }

    return haystackUpper.find(needleUpper) != std::string::npos;
}

std::string NormalizeCaptionMatchText(const std::string& value)
{
    std::string normalized;
    normalized.reserve(value.size());

    bool previousWasSpace = true;
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(value[index]);
        if (std::isalnum(ch) != 0)
        {
            normalized.push_back(static_cast<char>(std::toupper(ch)));
            previousWasSpace = false;
            continue;
        }

        if (!previousWasSpace && !normalized.empty())
        {
            normalized.push_back(' ');
            previousWasSpace = true;
        }
    }

    while (!normalized.empty() && normalized[normalized.size() - 1] == ' ')
    {
        normalized.erase(normalized.size() - 1);
    }

    return normalized;
}

bool IsGenericCaptionToken(const std::string& token)
{
    return token == "TRADER"
        || token == "SHOP"
        || token == "MERCHANT"
        || token == "STORE"
        || token == "THE"
        || token == "OF"
        || token == "AND";
}

int ComputeCaptionNameMatchBias(
    const std::string& captionNormalized,
    const std::string& nameNormalized)
{
    if (captionNormalized.empty() || nameNormalized.empty())
    {
        return 0;
    }

    int score = 0;
    if (captionNormalized == nameNormalized)
    {
        score += 2200;
    }
    if (captionNormalized.find(nameNormalized) != std::string::npos)
    {
        score += 1400;
    }
    if (nameNormalized.find(captionNormalized) != std::string::npos)
    {
        score += 900;
    }

    std::size_t start = 0;
    while (start < captionNormalized.size())
    {
        while (start < captionNormalized.size() && captionNormalized[start] == ' ')
        {
            ++start;
        }
        if (start >= captionNormalized.size())
        {
            break;
        }

        std::size_t end = start;
        while (end < captionNormalized.size() && captionNormalized[end] != ' ')
        {
            ++end;
        }

        const std::string token = captionNormalized.substr(start, end - start);
        if (token.size() >= 3
            && !IsGenericCaptionToken(token)
            && nameNormalized.find(token) != std::string::npos)
        {
            score += 220;
        }

        start = end + 1;
    }

    return score;
}

std::string TruncateForLog(const std::string& value, std::size_t maxLength)
{
    if (value.size() <= maxLength)
    {
        return value;
    }

    if (maxLength <= 3)
    {
        return value.substr(0, maxLength);
    }

    return value.substr(0, maxLength - 3) + "...";
}

std::string BuildParentChainForLog(MyGUI::Widget* widget)
{
    std::stringstream chain;
    std::size_t depth = 0;
    while (widget != 0 && depth < 10)
    {
        if (depth > 0)
        {
            chain << " <- ";
        }

        chain << SafeWidgetName(widget);
        widget = widget->getParent();
        ++depth;
    }

    if (widget != 0)
    {
        chain << " <- ...";
    }

    return chain.str();
}

bool HasInventoryMarkers(MyGUI::Widget* parent)
{
    if (parent == 0)
    {
        return false;
    }

    return FindWidgetInParentByToken(parent, "scrollview_backpack_content") != 0
        && FindWidgetInParentByToken(parent, "backpack_content") != 0;
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

bool HasInventoryStructure(MyGUI::Widget* parent)
{
    return HasInventoryMarkers(parent) || HasTraderMoneyMarkers(parent);
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

Character* TryGetSelectedPlayerCharacter()
{
    if (ou == 0 || ou->player == 0)
    {
        return 0;
    }

    return ou->player->selectedCharacter.getCharacter();
}

bool TryResolveSelectedInventoryVisible(bool* visibleOut)
{
    return TryResolveCharacterInventoryVisible(TryGetSelectedPlayerCharacter(), visibleOut);
}

std::string CharacterNameForLog(Character* character)
{
    if (character == 0)
    {
        return "";
    }

    if (!character->displayName.empty())
    {
        return character->displayName;
    }

    const std::string objectName = character->getName();
    if (!objectName.empty())
    {
        return objectName;
    }

    if (character->data != 0 && !character->data->name.empty())
    {
        return character->data->name;
    }

    return "";
}

bool TryCopyCharacterCaptionMatchName(
    Character* candidate,
    char* outNameBuffer,
    std::size_t outNameBufferSize)
{
    if (outNameBuffer == 0 || outNameBufferSize == 0)
    {
        return false;
    }

    outNameBuffer[0] = '\0';

    if (candidate == 0)
    {
        return true;
    }

    __try
    {
        if (candidate->inventory == 0)
        {
            return true;
        }

        const std::string* sourceName = 0;
        if (!candidate->displayName.empty())
        {
            sourceName = &candidate->displayName;
        }
        else if (candidate->data != 0 && !candidate->data->name.empty())
        {
            sourceName = &candidate->data->name;
        }

        if (sourceName == 0 || sourceName->empty())
        {
            return true;
        }

        std::size_t copyLength = sourceName->size();
        if (copyLength >= outNameBufferSize)
        {
            copyLength = outNameBufferSize - 1;
        }

        for (std::size_t index = 0; index < copyLength; ++index)
        {
            outNameBuffer[index] = (*sourceName)[index];
        }
        outNameBuffer[copyLength] = '\0';
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        outNameBuffer[0] = '\0';
        return false;
    }
}

bool TryComputeCaptionMatchScoreForCharacter(
    const std::string& normalizedCaption,
    Character* candidate,
    int* outScore,
    std::string* outCharacterName)
{
    if (outScore == 0)
    {
        return false;
    }

    *outScore = 0;
    if (outCharacterName != 0)
    {
        outCharacterName->clear();
    }

    char candidateNameBuffer[kMaxCaptionMatchNameLength];
    if (!TryCopyCharacterCaptionMatchName(
            candidate,
            candidateNameBuffer,
            sizeof(candidateNameBuffer) / sizeof(candidateNameBuffer[0])))
    {
        return false;
    }

    if (candidateNameBuffer[0] == '\0')
    {
        return true;
    }

    const std::string candidateName(candidateNameBuffer);
    *outScore = ComputeCaptionNameMatchBias(
        normalizedCaption,
        NormalizeCaptionMatchText(candidateName));

    if (outCharacterName != 0)
    {
        *outCharacterName = candidateName;
    }

    return true;
}

bool PointerArrayContainsCharacter(
    Character* const* characters,
    std::size_t count,
    Character* candidate)
{
    if (characters == 0 || candidate == 0)
    {
        return false;
    }

    for (std::size_t index = 0; index < count; ++index)
    {
        if (characters[index] == candidate)
        {
            return true;
        }
    }

    return false;
}

bool TryCollectPlayerCharactersForCaptionMatch(
    Character** outCharacters,
    std::size_t capacity,
    std::size_t* outCount,
    bool* outTruncated)
{
    if (outCharacters == 0 || capacity == 0 || outCount == 0)
    {
        return false;
    }

    *outCount = 0;
    if (outTruncated != 0)
    {
        *outTruncated = false;
    }

    if (ou == 0 || ou->player == 0)
    {
        return false;
    }

    __try
    {
        Character* selectedCharacter = ou->player->selectedCharacter.getCharacter();
        if (selectedCharacter != 0)
        {
            outCharacters[*outCount] = selectedCharacter;
            ++(*outCount);
        }

        const lektor<Character*>& allPlayerCharacters = ou->player->getAllPlayerCharacters();
        const uint32_t playerCharacterCount = allPlayerCharacters.size();
        for (uint32_t index = 0; index < playerCharacterCount; ++index)
        {
            Character* candidate = allPlayerCharacters[index];
            if (candidate == 0 || PointerArrayContainsCharacter(outCharacters, *outCount, candidate))
            {
                continue;
            }

            if (*outCount >= capacity)
            {
                if (outTruncated != 0)
                {
                    *outTruncated = true;
                }
                break;
            }

            outCharacters[*outCount] = candidate;
            ++(*outCount);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return *outCount > 0;
}

int ComputePlayerCharacterCaptionMatchScore(
    const std::string& caption,
    std::string* outCharacterName,
    std::size_t* outCandidateCount,
    bool* outCandidateListTruncated)
{
    if (outCharacterName != 0)
    {
        outCharacterName->clear();
    }
    if (outCandidateCount != 0)
    {
        *outCandidateCount = 0;
    }
    if (outCandidateListTruncated != 0)
    {
        *outCandidateListTruncated = false;
    }

    if (ou == 0 || ou->player == 0)
    {
        return 0;
    }

    const std::string normalizedCaption = NormalizeCaptionMatchText(caption);
    int bestScore = 0;
    std::string bestCharacterName;
    Character* playerCharacters[kMaxCaptionMatchPlayerCharacters] = {0};
    std::size_t playerCharacterCount = 0;
    bool playerCharacterListTruncated = false;

    if (TryCollectPlayerCharactersForCaptionMatch(
            playerCharacters,
            kMaxCaptionMatchPlayerCharacters,
            &playerCharacterCount,
            &playerCharacterListTruncated))
    {
        for (std::size_t index = 0; index < playerCharacterCount; ++index)
        {
            Character* candidate = playerCharacters[index];
            int candidateScore = 0;
            std::string candidateName;
            if (!TryComputeCaptionMatchScoreForCharacter(
                    normalizedCaption,
                    candidate,
                    &candidateScore,
                    &candidateName))
            {
                continue;
            }
            if (candidateScore <= bestScore)
            {
                continue;
            }

            bestScore = candidateScore;
            bestCharacterName = candidateName;
        }
    }

    if (bestScore <= 0)
    {
        Character* selectedCharacter = TryGetSelectedPlayerCharacter();
        if (selectedCharacter != 0)
        {
            int candidateScore = 0;
            std::string candidateName;
            if (!TryComputeCaptionMatchScoreForCharacter(
                    normalizedCaption,
                    selectedCharacter,
                    &candidateScore,
                    &candidateName))
            {
                candidateScore = 0;
            }
            if (candidateScore > bestScore)
            {
                bestScore = candidateScore;
                bestCharacterName = candidateName;
            }

            if (bestScore <= 0)
            {
                bool selectedCharacterAccessible = false;
                if (TryResolveCharacterInventoryVisible(selectedCharacter, &selectedCharacterAccessible))
                {
                    const std::string selectedCharacterName = CharacterNameForLog(selectedCharacter);
                    const int directSelectedCharacterScore = ComputeCaptionNameMatchBias(
                        normalizedCaption,
                        NormalizeCaptionMatchText(selectedCharacterName));
                    if (directSelectedCharacterScore > bestScore)
                    {
                        bestScore = directSelectedCharacterScore;
                        bestCharacterName = selectedCharacterName;
                    }
                }
            }
        }
    }

    if (outCharacterName != 0)
    {
        *outCharacterName = bestCharacterName;
    }
    if (outCandidateCount != 0)
    {
        *outCandidateCount = playerCharacterCount;
    }
    if (outCandidateListTruncated != 0)
    {
        *outCandidateListTruncated = playerCharacterListTruncated;
    }

    return bestScore;
}

bool TryResolveTraderDialogueContextActive(bool* activeOut)
{
    if (activeOut == 0)
    {
        return false;
    }

    *activeOut = false;

    Character* selectedCharacter = TryGetSelectedPlayerCharacter();
    if (selectedCharacter == 0 || selectedCharacter->dialogue == 0)
    {
        return true;
    }

    __try
    {
        Character* target = selectedCharacter->dialogue->getConversationTarget().getCharacter();
        if (target == 0 || target->inventory == 0)
        {
            return true;
        }

        bool targetInventoryVisible = false;
        TryResolveCharacterInventoryVisible(target, &targetInventoryVisible);

        const bool dialogActive = !selectedCharacter->dialogue->conversationHasEndedPrettyMuch();
        const bool engaged = selectedCharacter->_isEngagedWithAPlayer || target->_isEngagedWithAPlayer;
        const bool targetIsTrader = target->isATrader();
        *activeOut = targetIsTrader && (dialogActive || targetInventoryVisible || engaged);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

int ComputeInventoryWindowCandidateScore(MyGUI::Widget* parent, std::string* outReason)
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
    const bool hasInventoryMarkers = HasInventoryMarkers(parent);
    const bool hasMoneyMarkers = HasTraderMoneyMarkers(parent);
    const bool captionHasTrader = ContainsAsciiCaseInsensitive(caption, "TRADER");
    const bool captionHasLoot = ContainsAsciiCaseInsensitive(caption, "LOOT");
    const bool captionHasContainer = ContainsAsciiCaseInsensitive(caption, "CONTAINER");
    const bool hasCharacterSelection = FindWidgetInParentByToken(parent, "CharacterSelectionItemBox") != 0;
    const bool hasInventoryToken = FindWidgetInParentByToken(parent, "Inventory") != 0;
    const bool hasEquipmentToken = FindWidgetInParentByToken(parent, "Equipment") != 0;
    std::string selectedCharacterName;
    const int selectedCaptionScore =
        ComputePlayerCharacterCaptionMatchScore(caption, &selectedCharacterName, 0, 0);
    const bool captionMatchesSelectedCharacter = selectedCaptionScore > 0;

    bool selectedInventoryVisible = false;
    TryResolveSelectedInventoryVisible(&selectedInventoryVisible);

    bool traderDialogueContextActive = false;
    TryResolveTraderDialogueContextActive(&traderDialogueContextActive);

    const bool inventoryTokenFallback =
        hasInventoryToken
        && !hasMoneyMarkers
        && !captionHasTrader
        && !captionHasLoot
        && !captionHasContainer
        && !traderDialogueContextActive;

    if (!hasInventoryMarkers
        && !(selectedInventoryVisible && (hasInventoryToken || hasEquipmentToken || hasCharacterSelection))
        && !(captionMatchesSelectedCharacter && (hasInventoryToken || hasEquipmentToken))
        && !inventoryTokenFallback)
    {
        return 0;
    }

    if (hasMoneyMarkers || captionHasTrader || traderDialogueContextActive)
    {
        return 0;
    }

    int score = 0;
    std::stringstream reason;
    if (hasInventoryMarkers)
    {
        score += 100;
        reason << " inventory_markers";
    }

    if (selectedInventoryVisible)
    {
        score += 340;
        reason << " selected_inventory_visible";
    }

    if (captionMatchesSelectedCharacter)
    {
        score += 900 + (selectedCaptionScore > 2600 ? 2600 : selectedCaptionScore);
        reason << " caption_match_selected=\""
               << TruncateForLog(selectedCharacterName, 40)
               << "\"(" << selectedCaptionScore << ")";
    }

    if (hasCharacterSelection)
    {
        score += 520;
        reason << " character_selection";
    }

    if (hasInventoryToken)
    {
        score += 360;
        reason << " inventory_token";
    }

    if (hasEquipmentToken)
    {
        score += 280;
        reason << " equipment_token";
    }

    if (hasInventoryToken && hasEquipmentToken)
    {
        score += 220;
        reason << " inventory_equipment_pair";
    }

    if (inventoryTokenFallback)
    {
        score += 520;
        reason << " inventory_token_fallback";
    }

    if (captionHasLoot || captionHasContainer)
    {
        score -= 160;
        reason << " storage_caption_penalty";
    }

    if (score < 300)
    {
        return 0;
    }

    if (outReason != 0)
    {
        *outReason = reason.str().empty() ? "inventory_candidate" : reason.str();
    }

    return score;
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
}

std::string SafeWidgetName(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "<null>";
    }

    const std::string& name = widget->getName();
    if (name.empty())
    {
        return "<unnamed>";
    }

    return name;
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
        MyGUI::Widget* found = FindNamedDescendantRecursive(
            root->getChildAt(childIndex),
            widgetName,
            requireVisible);
        if (found != 0)
        {
            return found;
        }
    }

    return 0;
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

    if ((!requireVisible || root->getInheritedVisible())
        && ContainsAsciiCaseInsensitive(root->getName(), token))
    {
        return root;
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        MyGUI::Widget* found = FindNamedDescendantByTokenRecursive(
            root->getChildAt(childIndex),
            token,
            requireVisible);
        if (found != 0)
        {
            return found;
        }
    }

    return 0;
}

void CollectNamedDescendantsByTokenRecursive(
    MyGUI::Widget* root,
    const char* token,
    bool requireVisible,
    std::vector<MyGUI::Widget*>* outWidgets)
{
    if (root == 0 || token == 0 || outWidgets == 0)
    {
        return;
    }

    if ((!requireVisible || root->getInheritedVisible())
        && ContainsAsciiCaseInsensitive(root->getName(), token))
    {
        for (std::size_t index = 0; index < outWidgets->size(); ++index)
        {
            if ((*outWidgets)[index] == root)
            {
                return;
            }
        }

        outWidgets->push_back(root);
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        CollectNamedDescendantsByTokenRecursive(
            root->getChildAt(childIndex),
            token,
            requireVisible,
            outWidgets);
    }
}

MyGUI::Widget* FindWidgetInParentByToken(MyGUI::Widget* parent, const char* token)
{
    if (parent == 0 || token == 0)
    {
        return 0;
    }

    return FindNamedDescendantByTokenRecursive(parent, token, false);
}

void CollectVisibleWidgetsByToken(const char* token, std::vector<MyGUI::Widget*>* outWidgets)
{
    if (token == 0 || outWidgets == 0)
    {
        return;
    }

    outWidgets->clear();

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return;
    }

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        CollectNamedDescendantsByTokenRecursive(root, token, true, outWidgets);
    }
}

void CollectNamedDescendantsByToken(
    MyGUI::Widget* root,
    const char* token,
    bool requireVisible,
    std::vector<MyGUI::Widget*>* outWidgets)
{
    CollectNamedDescendantsByTokenRecursive(root, token, requireVisible, outWidgets);
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

MyGUI::Widget* ResolveInventoryEntriesRoot(MyGUI::Widget* inventoryContentRoot)
{
    if (inventoryContentRoot == 0)
    {
        return 0;
    }

    MyGUI::Widget* current = inventoryContentRoot;
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

bool IsLikelyInventoryWindow(MyGUI::Widget* parent)
{
    return ComputeInventoryWindowCandidateScore(parent, 0) > 0;
}

void DumpInventoryTargetProbe()
{
    const char* probeTokens[] =
    {
        "scrollview_backpack_content",
        "backpack_content",
        "CharacterSelectionItemBox",
        "Inventory",
        "Equipment",
        "Loot",
        "Container",
        "MoneyAmountTextBox"
    };

    for (std::size_t index = 0; index < sizeof(probeTokens) / sizeof(probeTokens[0]); ++index)
    {
        const char* token = probeTokens[index];
        const bool exactAny = FindWidgetByName(token) != 0;
        const bool exactVisible = FindFirstVisibleWidgetByName(token) != 0;
        const bool tokenVisible = FindFirstVisibleWidgetByToken(token) != 0;

        std::stringstream line;
        line << "inventory probe token=" << token
             << " exact_any=" << (exactAny ? "true" : "false")
             << " exact_visible=" << (exactVisible ? "true" : "false")
             << " token_visible=" << (tokenVisible ? "true" : "false");
        LogInfoLine(line.str());
    }
}

void DumpVisibleInventoryWindowCandidateDiagnostics()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        LogWarnLine("GUI singleton unavailable while dumping inventory window candidates");
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

        const bool hasMarkers = HasInventoryStructure(parent);
        const bool hasCharacterSelection = FindWidgetInParentByToken(parent, "CharacterSelectionItemBox") != 0;
        const bool hasInventoryToken = FindWidgetInParentByToken(parent, "Inventory") != 0;
        const bool hasEquipmentToken = FindWidgetInParentByToken(parent, "Equipment") != 0;
        std::string selectedCharacterName;
        std::size_t selectedCaptionCandidateCount = 0;
        bool selectedCaptionCandidateListTruncated = false;
        const int selectedCaptionScore =
            ComputePlayerCharacterCaptionMatchScore(
                window->getCaption().asUTF8(),
                &selectedCharacterName,
                &selectedCaptionCandidateCount,
                &selectedCaptionCandidateListTruncated);
        const bool captionInteresting =
            ContainsAsciiCaseInsensitive(window->getCaption().asUTF8(), "inventory")
            || ContainsAsciiCaseInsensitive(window->getCaption().asUTF8(), "loot")
            || ContainsAsciiCaseInsensitive(window->getCaption().asUTF8(), "container");

        bool selectedInventoryVisible = false;
        TryResolveSelectedInventoryVisible(&selectedInventoryVisible);

        bool traderDialogueContextActive = false;
        TryResolveTraderDialogueContextActive(&traderDialogueContextActive);

        if (!hasMarkers
            && !hasCharacterSelection
            && !hasInventoryToken
            && !hasEquipmentToken
            && selectedCaptionScore <= 0
            && !captionInteresting)
        {
            continue;
        }

        std::string candidateReason;
        const int candidateScore = ComputeInventoryWindowCandidateScore(parent, &candidateReason);
        const MyGUI::IntCoord coord = root->getCoord();
        std::stringstream line;
        line << "inventory window-candidate[" << index << "]"
             << " name=" << SafeWidgetName(root)
             << " caption=\"" << TruncateForLog(window->getCaption().asUTF8(), 60) << "\""
             << " has_markers=" << (hasMarkers ? "true" : "false")
             << " character_selection=" << (hasCharacterSelection ? "true" : "false")
             << " inventory_token=" << (hasInventoryToken ? "true" : "false")
             << " equipment_token=" << (hasEquipmentToken ? "true" : "false")
             << " selected_caption_score=" << selectedCaptionScore
             << " selected_caption_name=\"" << TruncateForLog(selectedCharacterName, 40) << "\""
             << " selected_caption_candidates=" << selectedCaptionCandidateCount
             << " selected_caption_candidates_truncated="
             << (selectedCaptionCandidateListTruncated ? "true" : "false")
             << " selected_inventory_visible=" << (selectedInventoryVisible ? "true" : "false")
             << " trader_dialogue_context=" << (traderDialogueContextActive ? "true" : "false")
             << " likely_inventory=" << (candidateScore > 0 ? "true" : "false")
             << " candidate_score=" << candidateScore
             << " candidate_reason=\"" << TruncateForLog(candidateReason, 160) << "\""
             << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";
        LogInfoLine(line.str());
        ++index;
    }

    if (index == 0)
    {
        LogInfoLine("inventory window-candidate scan found no visible candidates");
    }
}

bool TryResolveVisibleInventoryTarget(MyGUI::Widget** outAnchor, MyGUI::Widget** outParent)
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
        const int candidateScore = ComputeInventoryWindowCandidateScore(parent, &candidateReason);
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
    line << "resolved inventory target via window-scan"
         << " anchor=" << SafeWidgetName(bestAnchor)
         << " parent=" << SafeWidgetName(bestParent)
         << " caption=\"" << (window == 0 ? "" : TruncateForLog(window->getCaption().asUTF8(), 60)) << "\""
         << " candidate_score=" << bestScore
         << " candidate_reason=\"" << TruncateForLog(bestReason, 160) << "\"";
    LogDebugLine(line.str());
    return true;
}

bool TryResolveHoveredInventoryTarget(
    MyGUI::Widget** outAnchor,
    MyGUI::Widget** outParent,
    bool logFailures)
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
            LogWarnLine("inventory hover attach failed: MyGUI InputManager unavailable");
        }
        return false;
    }

    MyGUI::Widget* hovered = inputManager->getMouseFocusWidget();
    if (hovered == 0)
    {
        if (logFailures)
        {
            LogWarnLine("inventory hover attach failed: no mouse-focused widget");
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
            line << "inventory hover attach failed: anchor/parent unresolved hovered_chain="
                 << BuildParentChainForLog(hovered);
            LogWarnLine(line.str());
        }
        return false;
    }

    std::string candidateReason;
    const int candidateScore = ComputeInventoryWindowCandidateScore(parent, &candidateReason);
    if (candidateScore <= 0)
    {
        if (logFailures)
        {
            std::stringstream line;
            line << "inventory hover attach rejected"
                 << " anchor=" << SafeWidgetName(anchor)
                 << " parent=" << SafeWidgetName(parent)
                 << " candidate_score=" << candidateScore
                 << " candidate_reason=\"" << TruncateForLog(candidateReason, 160) << "\""
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
    line << "resolved inventory target via hover attach"
         << " anchor=" << SafeWidgetName(anchor)
         << " parent=" << SafeWidgetName(parent)
         << " candidate_score=" << candidateScore
         << " candidate_reason=\"" << TruncateForLog(candidateReason, 160) << "\""
         << " parent_coord=(" << parent->getCoord().left << "," << parent->getCoord().top << ","
         << parent->getCoord().width << "," << parent->getCoord().height << ")";
    LogDebugLine(line.str());
    return true;
}
