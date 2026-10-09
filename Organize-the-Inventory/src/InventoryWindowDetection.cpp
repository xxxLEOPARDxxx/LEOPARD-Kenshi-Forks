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
#include <cstring>
#include <sstream>
#include <vector>

namespace
{
static const std::size_t kMaxCaptionMatchPlayerCharacters = 256;
static const std::size_t kMaxCaptionMatchNameLength = 256;
static const std::size_t kMaxWidgetNameCopyLength = 256;

bool ContainsAsciiCaseInsensitiveRange(
    const char* haystack,
    std::size_t haystackLength,
    const char* needle)
{
    if (needle == 0 || *needle == '\0')
    {
        return true;
    }
    if (haystack == 0)
    {
        return false;
    }

    const std::size_t needleLength = std::strlen(needle);
    if (needleLength > haystackLength)
    {
        return false;
    }

    for (std::size_t start = 0; start + needleLength <= haystackLength; ++start)
    {
        bool matched = true;
        for (std::size_t offset = 0; offset < needleLength; ++offset)
        {
            const unsigned char haystackChar =
                static_cast<unsigned char>(haystack[start + offset]);
            const unsigned char needleChar =
                static_cast<unsigned char>(needle[offset]);
            if (std::toupper(haystackChar) != std::toupper(needleChar))
            {
                matched = false;
                break;
            }
        }

        if (matched)
        {
            return true;
        }
    }

    return false;
}

bool ContainsAsciiCaseInsensitive(const char* haystack, const char* needle)
{
    return ContainsAsciiCaseInsensitiveRange(
        haystack,
        haystack == 0 ? 0 : std::strlen(haystack),
        needle);
}

bool ContainsAsciiCaseInsensitive(const std::string& haystack, const char* needle)
{
    return ContainsAsciiCaseInsensitiveRange(haystack.c_str(), haystack.size(), needle);
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
    if (nameNormalized.size() >= 3
        && captionNormalized.find(nameNormalized) != std::string::npos)
    {
        score += 1400;
    }
    if (nameNormalized.size() >= 3
        && nameNormalized.find(captionNormalized) != std::string::npos)
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

std::string ExtractWidgetSessionPrefix(const std::string& widgetName)
{
    if (widgetName.empty())
    {
        return std::string();
    }

    std::size_t commaCount = 0;
    for (std::size_t index = 0; index < widgetName.size(); ++index)
    {
        if (widgetName[index] != ',')
        {
            continue;
        }

        ++commaCount;
        if (commaCount >= 4)
        {
            return widgetName.substr(0, index);
        }
    }

    return std::string();
}

std::string BuildCompanionCaptionKey(const std::string& caption)
{
    std::string normalized = NormalizeCaptionMatchText(caption);
    if (normalized.empty())
    {
        return normalized;
    }

    const std::string backpackToken = "BACKPACK";
    std::size_t found = normalized.find(backpackToken);
    while (found != std::string::npos)
    {
        normalized.erase(found, backpackToken.size());
        found = normalized.find(backpackToken);
    }

    std::string collapsed;
    collapsed.reserve(normalized.size());
    bool previousWasSpace = true;
    for (std::size_t index = 0; index < normalized.size(); ++index)
    {
        const char ch = normalized[index];
        if (ch == ' ')
        {
            if (!previousWasSpace)
            {
                collapsed.push_back(ch);
            }
            previousWasSpace = true;
            continue;
        }

        collapsed.push_back(ch);
        previousWasSpace = false;
    }

    while (!collapsed.empty() && collapsed[collapsed.size() - 1] == ' ')
    {
        collapsed.erase(collapsed.size() - 1);
    }

    return collapsed;
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

bool TryResolveSelectedInventoryVisible(bool* visibleOut);
bool TryResolveTraderDialogueContextActive(bool* activeOut);

struct InventoryWindowDetectionContext
{
    InventoryWindowDetectionContext()
        : selectedInventoryVisible(false)
        , traderDialogueContextActive(false)
    {
    }

    bool selectedInventoryVisible;
    bool traderDialogueContextActive;
};

struct InventoryWindowTokenProfile
{
    InventoryWindowTokenProfile()
        : hasScrollviewBackpackContentToken(false)
        , hasBackpackContentToken(false)
        , hasMoneyMarkers(false)
        , hasCharacterSelection(false)
        , hasInventoryToken(false)
        , hasEquipmentToken(false)
        , hasContainerToken(false)
    {
    }

    bool HasInventoryMarkers() const
    {
        return hasScrollviewBackpackContentToken && hasBackpackContentToken;
    }

    bool HasAnyInventoryHint() const
    {
        return HasInventoryMarkers()
            || hasMoneyMarkers
            || hasCharacterSelection
            || hasInventoryToken
            || hasEquipmentToken
            || hasContainerToken
            || hasBackpackContentToken;
    }

    bool HasAllTrackedTokens() const
    {
        return hasScrollviewBackpackContentToken
            && hasBackpackContentToken
            && hasMoneyMarkers
            && hasCharacterSelection
            && hasInventoryToken
            && hasEquipmentToken
            && hasContainerToken;
    }

    bool hasScrollviewBackpackContentToken;
    bool hasBackpackContentToken;
    bool hasMoneyMarkers;
    bool hasCharacterSelection;
    bool hasInventoryToken;
    bool hasEquipmentToken;
    bool hasContainerToken;
};

bool NameHasTraderMoneyToken(const char* name)
{
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
        if (ContainsAsciiCaseInsensitive(name, moneyTokens[index]))
        {
            return true;
        }
    }

    return false;
}

void UpdateInventoryWindowTokenProfileFromName(
    const char* name,
    InventoryWindowTokenProfile* profile)
{
    if (profile == 0 || name == 0 || *name == '\0')
    {
        return;
    }

    if (!profile->hasScrollviewBackpackContentToken
        && ContainsAsciiCaseInsensitive(name, "scrollview_backpack_content"))
    {
        profile->hasScrollviewBackpackContentToken = true;
    }
    if (!profile->hasBackpackContentToken
        && ContainsAsciiCaseInsensitive(name, "backpack_content"))
    {
        profile->hasBackpackContentToken = true;
    }
    if (!profile->hasMoneyMarkers && NameHasTraderMoneyToken(name))
    {
        profile->hasMoneyMarkers = true;
    }
    if (!profile->hasCharacterSelection
        && ContainsAsciiCaseInsensitive(name, "CharacterSelectionItemBox"))
    {
        profile->hasCharacterSelection = true;
    }
    if (!profile->hasInventoryToken && ContainsAsciiCaseInsensitive(name, "Inventory"))
    {
        profile->hasInventoryToken = true;
    }
    if (!profile->hasEquipmentToken && ContainsAsciiCaseInsensitive(name, "Equipment"))
    {
        profile->hasEquipmentToken = true;
    }
    if (!profile->hasContainerToken && ContainsAsciiCaseInsensitive(name, "Container"))
    {
        profile->hasContainerToken = true;
    }
}

bool TryCopyWidgetName(
    MyGUI::Widget* widget,
    char* outNameBuffer,
    std::size_t outNameBufferSize)
{
    if (outNameBuffer == 0 || outNameBufferSize == 0)
    {
        return false;
    }

    outNameBuffer[0] = '\0';
    if (widget == 0)
    {
        return true;
    }

    __try
    {
        const std::string& name = widget->getName();
        std::size_t copyLength = name.size();
        if (copyLength >= outNameBufferSize)
        {
            copyLength = outNameBufferSize - 1;
        }

        for (std::size_t index = 0; index < copyLength; ++index)
        {
            outNameBuffer[index] = name[index];
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

bool TryGetWidgetInheritedVisible(MyGUI::Widget* widget, bool* outVisible)
{
    if (outVisible != 0)
    {
        *outVisible = false;
    }
    if (widget == 0)
    {
        return false;
    }

    __try
    {
        if (outVisible != 0)
        {
            *outVisible = widget->getInheritedVisible();
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool TryGetWidgetChildCount(MyGUI::Widget* widget, std::size_t* outChildCount)
{
    if (outChildCount != 0)
    {
        *outChildCount = 0;
    }
    if (widget == 0)
    {
        return false;
    }

    __try
    {
        if (outChildCount != 0)
        {
            *outChildCount = widget->getChildCount();
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool TryGetWidgetChildAt(
    MyGUI::Widget* widget,
    std::size_t childIndex,
    MyGUI::Widget** outChild)
{
    if (outChild != 0)
    {
        *outChild = 0;
    }
    if (widget == 0)
    {
        return false;
    }

    __try
    {
        if (outChild != 0)
        {
            *outChild = widget->getChildAt(childIndex);
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool TryWidgetNameEquals(MyGUI::Widget* widget, const char* expectedName, bool* outMatches)
{
    if (outMatches != 0)
    {
        *outMatches = false;
    }
    if (expectedName == 0)
    {
        return false;
    }

    char nameBuffer[kMaxWidgetNameCopyLength];
    if (!TryCopyWidgetName(widget, nameBuffer, sizeof(nameBuffer)))
    {
        return false;
    }

    if (outMatches != 0)
    {
        *outMatches = std::strcmp(nameBuffer, expectedName) == 0;
    }
    return true;
}

bool TryWidgetNameContainsToken(MyGUI::Widget* widget, const char* token, bool* outMatches)
{
    if (outMatches != 0)
    {
        *outMatches = false;
    }
    if (token == 0)
    {
        return false;
    }

    char nameBuffer[kMaxWidgetNameCopyLength];
    if (!TryCopyWidgetName(widget, nameBuffer, sizeof(nameBuffer)))
    {
        return false;
    }

    if (outMatches != 0)
    {
        *outMatches = ContainsAsciiCaseInsensitive(nameBuffer, token);
    }
    return true;
}

void CollectInventoryWindowTokenProfileRecursive(
    MyGUI::Widget* root,
    InventoryWindowTokenProfile* profile)
{
    if (root == 0 || profile == 0 || profile->HasAllTrackedTokens())
    {
        return;
    }

    char nameBuffer[kMaxWidgetNameCopyLength];
    if (TryCopyWidgetName(root, nameBuffer, sizeof(nameBuffer)))
    {
        UpdateInventoryWindowTokenProfileFromName(nameBuffer, profile);
    }
    if (profile->HasAllTrackedTokens())
    {
        return;
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        CollectInventoryWindowTokenProfileRecursive(root->getChildAt(childIndex), profile);
        if (profile->HasAllTrackedTokens())
        {
            return;
        }
    }
}

bool TryBuildInventoryWindowTokenProfile(
    MyGUI::Widget* parent,
    InventoryWindowTokenProfile* outProfile)
{
    if (outProfile == 0)
    {
        return false;
    }

    *outProfile = InventoryWindowTokenProfile();
    if (parent == 0)
    {
        return true;
    }

    __try
    {
        CollectInventoryWindowTokenProfileRecursive(parent, outProfile);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        *outProfile = InventoryWindowTokenProfile();
        return false;
    }
}

InventoryWindowTokenProfile BuildInventoryWindowTokenProfile(MyGUI::Widget* parent)
{
    InventoryWindowTokenProfile profile;
    TryBuildInventoryWindowTokenProfile(parent, &profile);
    return profile;
}

InventoryWindowDetectionContext BuildInventoryWindowDetectionContext()
{
    InventoryWindowDetectionContext context;
    TryResolveSelectedInventoryVisible(&context.selectedInventoryVisible);
    TryResolveTraderDialogueContextActive(&context.traderDialogueContextActive);
    return context;
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

int ComputeInventoryWindowCandidateScore(
    MyGUI::Widget* parent,
    const InventoryWindowTokenProfile& tokenProfile,
    const InventoryWindowDetectionContext& detectionContext,
    std::string* outReason)
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
    const bool hasInventoryMarkers = tokenProfile.HasInventoryMarkers();
    const bool hasMoneyMarkers = tokenProfile.hasMoneyMarkers;
    const bool captionHasTrader = ContainsAsciiCaseInsensitive(caption, "TRADER");
    const bool captionHasLoot = ContainsAsciiCaseInsensitive(caption, "LOOT");
    const bool captionHasContainer = ContainsAsciiCaseInsensitive(caption, "CONTAINER");
    const bool captionHasBackpack = ContainsAsciiCaseInsensitive(caption, "BACKPACK");
    const bool hasCharacterSelection = tokenProfile.hasCharacterSelection;
    const bool hasInventoryToken = tokenProfile.hasInventoryToken;
    const bool hasEquipmentToken = tokenProfile.hasEquipmentToken;
    const bool hasContainerToken = tokenProfile.hasContainerToken;
    const bool hasBackpackContentToken = tokenProfile.hasBackpackContentToken;
    std::string selectedCharacterName;
    int selectedCaptionScore = 0;
    if (hasInventoryToken || hasEquipmentToken)
    {
        selectedCaptionScore =
            ComputePlayerCharacterCaptionMatchScore(caption, &selectedCharacterName, 0, 0);
    }
    const bool captionMatchesSelectedCharacter = selectedCaptionScore > 0;

    const bool selectedInventoryVisible = detectionContext.selectedInventoryVisible;
    const bool traderDialogueContextActive = detectionContext.traderDialogueContextActive;
    const bool inventoryTokenFallback =
        hasInventoryToken
        && !hasMoneyMarkers
        && !captionHasTrader
        && !captionHasLoot
        && !captionHasContainer
        && !traderDialogueContextActive;
    const bool backpackContentFallback =
        hasBackpackContentToken
        && captionHasBackpack
        && !hasMoneyMarkers
        && !captionHasTrader
        && !captionHasLoot
        && !captionHasContainer
        && !traderDialogueContextActive;

    if (!hasInventoryMarkers
        && !(selectedInventoryVisible && (hasInventoryToken || hasEquipmentToken || hasCharacterSelection))
        && !(captionMatchesSelectedCharacter && (hasInventoryToken || hasEquipmentToken))
        && !inventoryTokenFallback
        && !backpackContentFallback)
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

    if (hasContainerToken)
    {
        score += 180;
        reason << " container_token";
    }

    if (hasInventoryToken && hasEquipmentToken)
    {
        score += 220;
        reason << " inventory_equipment_pair";
    }

    if (hasInventoryToken && hasContainerToken)
    {
        score += 320;
        reason << " inventory_container_pair";
    }

    if (inventoryTokenFallback)
    {
        score += 520;
        reason << " inventory_token_fallback";
    }

    if (backpackContentFallback)
    {
        score += 460;
        reason << " backpack_content_fallback";
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

int ComputeInventoryWindowCandidateScore(MyGUI::Widget* parent, std::string* outReason)
{
    InventoryWindowTokenProfile tokenProfile;
    if (!TryBuildInventoryWindowTokenProfile(parent, &tokenProfile))
    {
        return 0;
    }

    return ComputeInventoryWindowCandidateScore(
        parent,
        tokenProfile,
        BuildInventoryWindowDetectionContext(),
        outReason);
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
        bool rootVisible = false;
        if (root == 0 || !TryGetWidgetInheritedVisible(root, &rootVisible) || !rootVisible)
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

    char nameBuffer[kMaxWidgetNameCopyLength];
    if (!TryCopyWidgetName(widget, nameBuffer, sizeof(nameBuffer)))
    {
        return "<stale>";
    }

    if (nameBuffer[0] == '\0')
    {
        return "<unnamed>";
    }

    return nameBuffer;
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

    bool rootVisible = true;
    if (requireVisible && !TryGetWidgetInheritedVisible(root, &rootVisible))
    {
        return 0;
    }

    bool nameMatches = false;
    if ((!requireVisible || rootVisible)
        && TryWidgetNameEquals(root, widgetName, &nameMatches)
        && nameMatches)
    {
        return root;
    }

    std::size_t childCount = 0;
    if (!TryGetWidgetChildCount(root, &childCount))
    {
        return 0;
    }
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        MyGUI::Widget* child = 0;
        if (!TryGetWidgetChildAt(root, childIndex, &child))
        {
            continue;
        }

        MyGUI::Widget* found = FindNamedDescendantRecursive(
            child,
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

    bool rootVisible = true;
    if (requireVisible && !TryGetWidgetInheritedVisible(root, &rootVisible))
    {
        return 0;
    }

    bool nameMatches = false;
    if ((!requireVisible || rootVisible)
        && TryWidgetNameContainsToken(root, token, &nameMatches)
        && nameMatches)
    {
        return root;
    }

    std::size_t childCount = 0;
    if (!TryGetWidgetChildCount(root, &childCount))
    {
        return 0;
    }
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        MyGUI::Widget* child = 0;
        if (!TryGetWidgetChildAt(root, childIndex, &child))
        {
            continue;
        }

        MyGUI::Widget* found = FindNamedDescendantByTokenRecursive(
            child,
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

    bool rootVisible = true;
    if (requireVisible && !TryGetWidgetInheritedVisible(root, &rootVisible))
    {
        return;
    }

    bool nameMatches = false;
    if ((!requireVisible || rootVisible)
        && TryWidgetNameContainsToken(root, token, &nameMatches)
        && nameMatches)
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

    std::size_t childCount = 0;
    if (!TryGetWidgetChildCount(root, &childCount))
    {
        return;
    }
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        MyGUI::Widget* child = 0;
        if (!TryGetWidgetChildAt(root, childIndex, &child))
        {
            continue;
        }

        CollectNamedDescendantsByTokenRecursive(
            child,
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

bool TryResolveCompanionControlsParentForTarget(
    MyGUI::Widget* targetAnchor,
    MyGUI::Widget* targetParent,
    MyGUI::Widget** outParent)
{
    if (outParent != 0)
    {
        *outParent = 0;
    }

    if (targetAnchor == 0 || targetParent == 0)
    {
        return false;
    }

    MyGUI::Window* targetWindow = FindOwningWindow(targetAnchor);
    const std::string targetCaption = targetWindow == 0 ? "" : targetWindow->getCaption().asUTF8();
    const bool targetHasBackpackContent = FindWidgetInParentByToken(targetParent, "backpack_content") != 0;
    if (!targetHasBackpackContent || !ContainsAsciiCaseInsensitive(targetCaption, "BACKPACK"))
    {
        return false;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return false;
    }

    const std::string targetSessionPrefix = ExtractWidgetSessionPrefix(SafeWidgetName(targetAnchor));
    const MyGUI::IntCoord targetCoord = targetParent->getAbsoluteCoord();
    const int targetArea = targetCoord.width * targetCoord.height;
    const int targetCenterX = targetCoord.left + (targetCoord.width / 2);
    const int targetCenterY = targetCoord.top + (targetCoord.height / 2);
    const std::string targetCaptionKey = BuildCompanionCaptionKey(targetCaption);

    MyGUI::Widget* bestParent = 0;
    std::string bestReason;
    int bestScore = 0;

    std::vector<MyGUI::Widget*> tokenCandidates;
    CollectVisibleWidgetsByToken("backpack_attach", &tokenCandidates);
    CollectVisibleWidgetsByToken("lbBackpack", &tokenCandidates);

    for (std::size_t index = 0; index < tokenCandidates.size(); ++index)
    {
        MyGUI::Widget* tokenWidget = tokenCandidates[index];
        if (tokenWidget == 0 || !tokenWidget->getInheritedVisible())
        {
            continue;
        }

        MyGUI::Widget* anchor = FindBestWindowAnchor(tokenWidget);
        MyGUI::Widget* parent = ResolveInjectionParent(anchor);
        if (anchor == 0 || parent == 0 || parent == targetParent || !parent->getInheritedVisible())
        {
            continue;
        }

        const bool hasBackpackContentToken = FindWidgetInParentByToken(parent, "backpack_content") != 0;
        if (hasBackpackContentToken)
        {
            continue;
        }

        MyGUI::Window* window = FindOwningWindow(anchor);
        const std::string caption = window == 0 ? "" : window->getCaption().asUTF8();
        const bool captionHasBackpack = ContainsAsciiCaseInsensitive(caption, "BACKPACK");
        const bool captionHasLoot = ContainsAsciiCaseInsensitive(caption, "LOOT");
        const bool captionHasTrader = ContainsAsciiCaseInsensitive(caption, "TRADER");
        const bool captionHasContainer = ContainsAsciiCaseInsensitive(caption, "CONTAINER");
        if (captionHasBackpack || captionHasLoot || captionHasTrader || captionHasContainer)
        {
            continue;
        }

        const bool hasInventoryToken = FindWidgetInParentByToken(parent, "Inventory") != 0;
        const bool hasEquipmentToken = FindWidgetInParentByToken(parent, "Equipment") != 0;
        const bool hasContainerToken = FindWidgetInParentByToken(parent, "Container") != 0;
        const MyGUI::IntCoord coord = parent->getAbsoluteCoord();
        const int area = coord.width * coord.height;
        if (area <= 0)
        {
            continue;
        }

        const int candidateCenterX = coord.left + (coord.width / 2);
        const int candidateCenterY = coord.top + (coord.height / 2);
        int distance = candidateCenterX - targetCenterX;
        if (distance < 0)
        {
            distance = -distance;
        }
        int distanceY = candidateCenterY - targetCenterY;
        if (distanceY < 0)
        {
            distanceY = -distanceY;
        }
        distance += distanceY;

        int score = 0;
        std::stringstream reason;
        score += 900;
        reason << " backpack_anchor_token";

        bool tokenHasBackpackAttach = false;
        bool tokenHasLbBackpack = false;
        TryWidgetNameContainsToken(tokenWidget, "backpack_attach", &tokenHasBackpackAttach);
        TryWidgetNameContainsToken(tokenWidget, "lbBackpack", &tokenHasLbBackpack);

        if (tokenHasBackpackAttach)
        {
            score += 260;
            reason << " backpack_attach_token";
        }
        if (tokenHasLbBackpack)
        {
            score += 140;
            reason << " lbbackpack_token";
        }
        if (hasContainerToken)
        {
            score += 220;
            reason << " container_token";
        }
        if (hasInventoryToken)
        {
            score += 120;
            reason << " inventory_token";
        }
        if (hasEquipmentToken)
        {
            score += 60;
            reason << " equipment_token";
        }
        if (coord.width >= 250 && coord.width <= 650 && coord.height >= 140 && coord.height <= 320)
        {
            score += 220;
            reason << " creature_panel_size";
        }
        else if (coord.width >= 200 && coord.width <= 700 && coord.height >= 120 && coord.height <= 380)
        {
            score += 100;
            reason << " panel_size";
        }
        if (area < targetArea)
        {
            score += 180;
            reason << " smaller_than_popup";
        }
        if (distance < 1200)
        {
            score += 300 - (distance / 6);
            reason << " near_popup";
        }

        const std::string candidateCaptionKey = BuildCompanionCaptionKey(caption);
        if (!targetCaptionKey.empty() && !candidateCaptionKey.empty())
        {
            if (candidateCaptionKey == targetCaptionKey)
            {
                score += 700;
                reason << " caption_match";
            }
            else if (candidateCaptionKey.find(targetCaptionKey) != std::string::npos
                || targetCaptionKey.find(candidateCaptionKey) != std::string::npos)
            {
                score += 420;
                reason << " caption_partial_match";
            }
        }
        else if (caption.empty())
        {
            score += 80;
            reason << " blank_caption";
        }

        if (score <= bestScore)
        {
            continue;
        }

        bestParent = parent;
        bestScore = score;
        bestReason = reason.str();
    }

    if (bestParent != 0)
    {
        if (outParent != 0)
        {
            *outParent = bestParent;
        }

        if (ShouldLogDebug())
        {
            MyGUI::Window* bestWindow = FindOwningWindow(bestParent);
            const MyGUI::IntCoord coord = bestParent->getAbsoluteCoord();
            std::stringstream line;
            line << "resolved companion controls parent"
                 << " target_anchor=" << SafeWidgetName(targetAnchor)
                 << " target_parent=" << SafeWidgetName(targetParent)
                 << " target_caption=\"" << TruncateForLog(targetCaption, 60) << "\""
                 << " controls_parent=" << SafeWidgetName(bestParent)
                 << " controls_caption=\""
                 << (bestWindow == 0 ? "" : TruncateForLog(bestWindow->getCaption().asUTF8(), 60))
                 << "\""
                 << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")"
                 << " reason=\"" << TruncateForLog(bestReason, 160) << "\""
                 << " score=" << bestScore;
            LogDebugLine(line.str());
        }

        return true;
    }

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || root == targetAnchor || !root->getInheritedVisible())
        {
            continue;
        }

        MyGUI::Widget* parent = ResolveInjectionParent(root);
        if (parent == 0 || parent == targetParent || !parent->getInheritedVisible())
        {
            continue;
        }

        const std::string sessionPrefix = ExtractWidgetSessionPrefix(SafeWidgetName(root));
        if (!targetSessionPrefix.empty() && sessionPrefix != targetSessionPrefix)
        {
            continue;
        }

        MyGUI::Window* window = FindOwningWindow(root);
        const std::string caption = window == 0 ? "" : window->getCaption().asUTF8();
        const bool captionHasBackpack = ContainsAsciiCaseInsensitive(caption, "BACKPACK");
        const bool captionHasLoot = ContainsAsciiCaseInsensitive(caption, "LOOT");
        const bool captionHasTrader = ContainsAsciiCaseInsensitive(caption, "TRADER");
        const bool captionHasContainer = ContainsAsciiCaseInsensitive(caption, "CONTAINER");
        const bool hasInventoryToken = FindWidgetInParentByToken(parent, "Inventory") != 0;
        const bool hasEquipmentToken = FindWidgetInParentByToken(parent, "Equipment") != 0;
        const bool hasContainerToken = FindWidgetInParentByToken(parent, "Container") != 0;
        const bool hasBackpackContentToken = FindWidgetInParentByToken(parent, "backpack_content") != 0;
        if (captionHasBackpack
            || captionHasLoot
            || captionHasTrader
            || captionHasContainer
            || hasBackpackContentToken)
        {
            continue;
        }

        const MyGUI::IntCoord coord = parent->getAbsoluteCoord();
        const int area = coord.width * coord.height;
        if (area <= 0)
        {
            continue;
        }

        int score = 0;
        std::stringstream reason;
        score += 800;
        reason << " shared_session_prefix";

        if (hasContainerToken)
        {
            score += 220;
            reason << " container_token";
        }
        if (hasInventoryToken)
        {
            score += 120;
            reason << " inventory_token";
        }
        if (hasEquipmentToken)
        {
            score += 60;
            reason << " equipment_token";
        }
        if (caption.empty())
        {
            score += 160;
            reason << " blank_caption";
        }
        if (coord.width >= 250 && coord.width <= 650 && coord.height >= 140 && coord.height <= 320)
        {
            score += 220;
            reason << " creature_panel_size";
        }
        else if (coord.width >= 200 && coord.width <= 700 && coord.height >= 120 && coord.height <= 380)
        {
            score += 100;
            reason << " panel_size";
        }
        if (area < targetArea)
        {
            score += 180;
            reason << " smaller_than_popup";
        }
        if (IsLikelyInventoryWindow(parent))
        {
            score -= 120;
            reason << " likely_inventory_penalty";
        }

        if (score <= bestScore)
        {
            continue;
        }

        bestParent = parent;
        bestScore = score;
        bestReason = reason.str();
    }

    if (bestParent == 0)
    {
        return false;
    }

    if (outParent != 0)
    {
        *outParent = bestParent;
    }

    if (ShouldLogDebug())
    {
        MyGUI::Window* bestWindow = FindOwningWindow(bestParent);
        const MyGUI::IntCoord coord = bestParent->getAbsoluteCoord();
        std::stringstream line;
        line << "resolved companion controls parent"
             << " target_anchor=" << SafeWidgetName(targetAnchor)
             << " target_parent=" << SafeWidgetName(targetParent)
             << " target_caption=\"" << TruncateForLog(targetCaption, 60) << "\""
             << " controls_parent=" << SafeWidgetName(bestParent)
             << " controls_caption=\""
             << (bestWindow == 0 ? "" : TruncateForLog(bestWindow->getCaption().asUTF8(), 60))
             << "\""
             << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")"
             << " reason=\"" << TruncateForLog(bestReason, 160) << "\""
             << " score=" << bestScore;
        LogDebugLine(line.str());
    }

    return true;
}

bool TryResolveCompanionBackpackFilterRootForTarget(
    MyGUI::Widget* targetAnchor,
    MyGUI::Widget* targetParent,
    MyGUI::Widget** outFilterRoot)
{
    if (outFilterRoot != 0)
    {
        *outFilterRoot = 0;
    }

    if (targetAnchor == 0 || targetParent == 0)
    {
        return false;
    }

    const bool targetAlreadyBackpack =
        FindWidgetInParentByToken(targetParent, "backpack_content") != 0;
    if (targetAlreadyBackpack)
    {
        if (outFilterRoot != 0)
        {
            *outFilterRoot = targetAnchor;
        }
        return true;
    }

    const bool targetLooksLikeCompanionWindow =
        FindWidgetInParentByToken(targetParent, "backpack_attach") != 0
        || FindWidgetInParentByToken(targetParent, "lbBackpack") != 0;
    if (!targetLooksLikeCompanionWindow)
    {
        return false;
    }

    const MyGUI::IntCoord targetCoord = targetParent->getAbsoluteCoord();
    const bool targetLooksLikeCreaturePanel =
        targetCoord.width >= 200
        && targetCoord.width <= 700
        && targetCoord.height >= 120
        && targetCoord.height <= 380;
    if (!targetLooksLikeCreaturePanel)
    {
        return false;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return false;
    }

    MyGUI::Window* targetWindow = FindOwningWindow(targetAnchor);
    const std::string targetCaption = targetWindow == 0 ? "" : targetWindow->getCaption().asUTF8();
    const std::string targetCaptionKey = BuildCompanionCaptionKey(targetCaption);
    const int targetCenterX = targetCoord.left + (targetCoord.width / 2);
    const int targetCenterY = targetCoord.top + (targetCoord.height / 2);
    const int targetArea = targetCoord.width * targetCoord.height;

    MyGUI::Widget* bestRoot = 0;
    int bestScore = 0;
    std::string bestReason;

    std::vector<MyGUI::Widget*> tokenCandidates;
    CollectVisibleWidgetsByToken("backpack_content", &tokenCandidates);

    for (std::size_t index = 0; index < tokenCandidates.size(); ++index)
    {
        MyGUI::Widget* contentWidget = tokenCandidates[index];
        if (contentWidget == 0 || !contentWidget->getInheritedVisible())
        {
            continue;
        }

        MyGUI::Widget* anchor = FindBestWindowAnchor(contentWidget);
        MyGUI::Widget* parent = ResolveInjectionParent(anchor);
        if (anchor == 0 || parent == 0 || parent == targetParent || !parent->getInheritedVisible())
        {
            continue;
        }

        const bool hasBackpackContent = FindWidgetInParentByToken(parent, "backpack_content") != 0;
        if (!hasBackpackContent)
        {
            continue;
        }

        MyGUI::Window* window = FindOwningWindow(anchor);
        const std::string caption = window == 0 ? "" : window->getCaption().asUTF8();
        const std::string candidateCaptionKey = BuildCompanionCaptionKey(caption);
        const bool captionHasBackpack = ContainsAsciiCaseInsensitive(caption, "BACKPACK");
        const bool captionHasLoot = ContainsAsciiCaseInsensitive(caption, "LOOT");
        const bool captionHasTrader = ContainsAsciiCaseInsensitive(caption, "TRADER");
        const bool hasInventoryToken = FindWidgetInParentByToken(parent, "Inventory") != 0;
        const bool hasContainerToken = FindWidgetInParentByToken(parent, "Container") != 0;
        const MyGUI::IntCoord coord = anchor->getAbsoluteCoord();
        const int area = coord.width * coord.height;
        if (area <= 0 || captionHasLoot || captionHasTrader)
        {
            continue;
        }

        const int candidateCenterX = coord.left + (coord.width / 2);
        const int candidateCenterY = coord.top + (coord.height / 2);
        int distance = candidateCenterX - targetCenterX;
        if (distance < 0)
        {
            distance = -distance;
        }
        int distanceY = candidateCenterY - targetCenterY;
        if (distanceY < 0)
        {
            distanceY = -distanceY;
        }
        distance += distanceY;

        int score = 900;
        std::stringstream reason;
        reason << " visible_backpack_content";

        if (captionHasBackpack)
        {
            score += 260;
            reason << " caption_backpack";
        }
        if (hasInventoryToken)
        {
            score += 160;
            reason << " inventory_token";
        }
        if (hasContainerToken)
        {
            score += 120;
            reason << " container_token";
        }
        if (coord.width >= 250 && coord.height >= 250)
        {
            score += 80;
            reason << " popup_size";
        }
        if (area > targetArea)
        {
            score += 120;
            reason << " larger_than_target";
        }
        if (distance < 1400)
        {
            score += 320 - (distance / 8);
            reason << " near_target";
        }
        if (!targetCaptionKey.empty() && !candidateCaptionKey.empty())
        {
            if (candidateCaptionKey == targetCaptionKey)
            {
                score += 700;
                reason << " caption_match";
            }
            else if (candidateCaptionKey.find(targetCaptionKey) != std::string::npos
                || targetCaptionKey.find(candidateCaptionKey) != std::string::npos)
            {
                score += 420;
                reason << " caption_partial_match";
            }
        }

        if (score <= bestScore)
        {
            continue;
        }

        bestRoot = anchor;
        bestScore = score;
        bestReason = reason.str();
    }

    if (bestRoot != 0)
    {
        if (outFilterRoot != 0)
        {
            *outFilterRoot = bestRoot;
        }

        if (ShouldLogDebug())
        {
            MyGUI::Window* filterWindow = FindOwningWindow(bestRoot);
            std::stringstream line;
            line << "resolved companion backpack filter root"
                 << " target_anchor=" << SafeWidgetName(targetAnchor)
                 << " target_parent=" << SafeWidgetName(targetParent)
                 << " target_caption=\""
                 << (targetWindow == 0 ? "" : TruncateForLog(targetWindow->getCaption().asUTF8(), 60))
                 << "\""
                 << " filter_root=" << SafeWidgetName(bestRoot)
                 << " filter_caption=\""
                 << (filterWindow == 0 ? "" : TruncateForLog(filterWindow->getCaption().asUTF8(), 60))
                 << "\""
                 << " reason=\"" << TruncateForLog(bestReason, 160) << "\""
                 << " score=" << bestScore;
            LogDebugLine(line.str());
        }

        return true;
    }

    const std::string targetSessionPrefix = ExtractWidgetSessionPrefix(SafeWidgetName(targetAnchor));
    if (targetSessionPrefix.empty())
    {
        return false;
    }

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || root == targetAnchor || !root->getInheritedVisible())
        {
            continue;
        }

        const std::string sessionPrefix = ExtractWidgetSessionPrefix(SafeWidgetName(root));
        if (sessionPrefix != targetSessionPrefix)
        {
            continue;
        }

        MyGUI::Widget* parent = ResolveInjectionParent(root);
        if (parent == 0 || !parent->getInheritedVisible())
        {
            continue;
        }

        const bool hasBackpackContent = FindWidgetInParentByToken(parent, "backpack_content") != 0;
        if (!hasBackpackContent)
        {
            continue;
        }

        MyGUI::Window* window = FindOwningWindow(root);
        const std::string caption = window == 0 ? "" : window->getCaption().asUTF8();
        const bool captionHasBackpack = ContainsAsciiCaseInsensitive(caption, "BACKPACK");
        const bool hasInventoryToken = FindWidgetInParentByToken(parent, "Inventory") != 0;
        const bool hasContainerToken = FindWidgetInParentByToken(parent, "Container") != 0;
        const MyGUI::IntCoord coord = root->getAbsoluteCoord();

        int score = 800;
        std::stringstream reason;
        reason << " shared_session_prefix backpack_content";

        if (captionHasBackpack)
        {
            score += 260;
            reason << " caption_backpack";
        }
        if (hasInventoryToken)
        {
            score += 160;
            reason << " inventory_token";
        }
        if (hasContainerToken)
        {
            score += 120;
            reason << " container_token";
        }
        if (coord.width >= 250 && coord.height >= 250)
        {
            score += 80;
            reason << " popup_size";
        }

        if (score <= bestScore)
        {
            continue;
        }

        bestRoot = root;
        bestScore = score;
        bestReason = reason.str();
    }

    if (bestRoot == 0)
    {
        return false;
    }

    if (outFilterRoot != 0)
    {
        *outFilterRoot = bestRoot;
    }

    if (ShouldLogDebug())
    {
        MyGUI::Window* filterWindow = FindOwningWindow(bestRoot);
        std::stringstream line;
        line << "resolved companion backpack filter root"
             << " target_anchor=" << SafeWidgetName(targetAnchor)
             << " target_parent=" << SafeWidgetName(targetParent)
             << " target_caption=\""
             << (FindOwningWindow(targetAnchor) == 0
                    ? ""
                    : TruncateForLog(FindOwningWindow(targetAnchor)->getCaption().asUTF8(), 60))
             << "\""
             << " filter_root=" << SafeWidgetName(bestRoot)
             << " filter_caption=\""
             << (filterWindow == 0 ? "" : TruncateForLog(filterWindow->getCaption().asUTF8(), 60))
             << "\""
             << " reason=\"" << TruncateForLog(bestReason, 160) << "\""
             << " score=" << bestScore;
        LogDebugLine(line.str());
    }

    return true;
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

        MyGUI::Widget* parent = ResolveInjectionParent(root);
        if (parent == 0)
        {
            continue;
        }

        MyGUI::Window* window = root->castType<MyGUI::Window>(false);
        const bool hasMarkers = HasInventoryStructure(parent);
        const bool hasCharacterSelection = FindWidgetInParentByToken(parent, "CharacterSelectionItemBox") != 0;
        const bool hasInventoryToken = FindWidgetInParentByToken(parent, "Inventory") != 0;
        const bool hasEquipmentToken = FindWidgetInParentByToken(parent, "Equipment") != 0;
        const bool hasContainerToken = FindWidgetInParentByToken(parent, "Container") != 0;
        const bool hasBackpackContentToken = FindWidgetInParentByToken(parent, "backpack_content") != 0;
        const std::string caption = window == 0 ? "" : window->getCaption().asUTF8();
        std::string selectedCharacterName;
        std::size_t selectedCaptionCandidateCount = 0;
        bool selectedCaptionCandidateListTruncated = false;
        const int selectedCaptionScore =
            ComputePlayerCharacterCaptionMatchScore(
                caption,
                &selectedCharacterName,
                &selectedCaptionCandidateCount,
                &selectedCaptionCandidateListTruncated);
        const bool captionInteresting =
            ContainsAsciiCaseInsensitive(caption, "inventory")
            || ContainsAsciiCaseInsensitive(caption, "loot")
            || ContainsAsciiCaseInsensitive(caption, "container")
            || ContainsAsciiCaseInsensitive(caption, "backpack");

        bool selectedInventoryVisible = false;
        TryResolveSelectedInventoryVisible(&selectedInventoryVisible);

        bool traderDialogueContextActive = false;
        TryResolveTraderDialogueContextActive(&traderDialogueContextActive);

        if (!hasMarkers
            && !hasCharacterSelection
            && !hasInventoryToken
            && !hasEquipmentToken
            && !hasContainerToken
            && !hasBackpackContentToken
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
             << " caption=\"" << TruncateForLog(caption, 60) << "\""
             << " has_markers=" << (hasMarkers ? "true" : "false")
             << " character_selection=" << (hasCharacterSelection ? "true" : "false")
             << " inventory_token=" << (hasInventoryToken ? "true" : "false")
             << " equipment_token=" << (hasEquipmentToken ? "true" : "false")
             << " container_token=" << (hasContainerToken ? "true" : "false")
             << " backpack_content_token=" << (hasBackpackContentToken ? "true" : "false")
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
    const InventoryWindowDetectionContext detectionContext =
        BuildInventoryWindowDetectionContext();

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        MyGUI::Widget* parent = ResolveInjectionParent(root);
        if (parent == 0)
        {
            continue;
        }

        std::string candidateReason;
        InventoryWindowTokenProfile tokenProfile;
        if (!TryBuildInventoryWindowTokenProfile(parent, &tokenProfile)
            || !tokenProfile.HasAnyInventoryHint())
        {
            continue;
        }

        const int candidateScore =
            ComputeInventoryWindowCandidateScore(
                parent,
                tokenProfile,
                detectionContext,
                &candidateReason);
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
