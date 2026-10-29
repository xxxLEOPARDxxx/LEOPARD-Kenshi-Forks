#pragma once

#include <string>
#include <vector>

struct ResistTriple
{
    float cut;
    float blunt;
    float pierce;

    ResistTriple() : cut(0.f), blunt(0.f), pierce(0.f) {}
};

struct ArmourPieceInput
{
    std::string name;
    std::string iconTexture; // Kenshi inventory icon texture name; empty if unavailable
    float coverage;
    float cutResistance;
    float bluntResistance;
    float pierceResistance;
    float cutEfficiency;
    float minCutResistance;

    ArmourPieceInput()
        : coverage(0.f)
        , cutResistance(0.f)
        , bluntResistance(0.f)
        , pierceResistance(0.f)
        , cutEfficiency(0.f)
        , minCutResistance(0.f)
    {
    }
};

// Защита от среды. В Kenshi это характеристика персонажа целиком
// (CharStats::getWeatherProtection), а не отдельной части тела.
struct WeatherResists
{
    float dust;
    float acid;
    float burning;
    float gas;
    float rain;

    WeatherResists() : dust(0.f), acid(0.f), burning(0.f), gas(0.f), rain(0.f) {}
};

struct BodyPartInput
{
    std::string name;
    std::string layerId;
    void* partKey;
    std::vector<ArmourPieceInput> covering;

    BodyPartInput() : partKey(0) {}
};

struct PartProtectionView
{
    std::string partName;
    std::string layerId;
    ResistTriple weighted;
    std::vector<ArmourPieceInput> pieces;
};

struct CharacterProtectionView
{
    std::string characterName;
    ResistTriple totalsApprox;
    WeatherResists weather;
    std::vector<PartProtectionView> parts;
};

float normalizeCoverage(float raw);

CharacterProtectionView buildProtectionView(
    const std::string& characterName,
    const std::vector<BodyPartInput>& parts);
