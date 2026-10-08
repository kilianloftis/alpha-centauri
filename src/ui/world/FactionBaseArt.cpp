#include "ui/world/FactionBaseArt.h"

#include "game/Faction.h"
#include "game/buildings/BaseSpriteSizesConfig.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/MapOverlayChannelsConfig.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/map/Tile.h"
#include "ui/SpriteLibrary.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace ac
{

namespace
{

Color_t ColorFromRgbJson_(const nlohmann::json& rEntry, const std::string& rKey)
{
    if (!rEntry.contains("rgb") || !rEntry.at("rgb").is_array() || rEntry.at("rgb").size() != 3)
    {
        throw std::runtime_error("colors.json '" + rKey + "' needs rgb: [r, g, b]");
    }
    const auto& rgb = rEntry.at("rgb");
    return Color_t{rgb.at(0).get<uint8_t>(), rgb.at(1).get<uint8_t>(), rgb.at(2).get<uint8_t>(),
                   255};
}

std::string ReplaceToken_(std::string text, const std::string& rToken, const std::string& rValue)
{
    const std::string needle = "{" + rToken + "}";
    for (size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at))
    {
        text.replace(at, needle.size(), rValue);
        at += rValue.size();
    }
    return text;
}

struct CandidateOverlay_t
{
    const BuildingConfig_t* pBuilding = nullptr;
    std::string pathTemplate;
    int sizeStage = 1;
    int priority = 0;
    int layer = 0;
};

void CollectBuildingOverlay_(std::vector<CandidateOverlay_t>& rOut, const BuildingConfig_t& rBuilding,
                             const std::string& rSheetStem, bool bWater, int sizeStage,
                             const MapOverlayChannelsConfig_t& rChannels,
                             std::unordered_set<std::string>& rSeenIds)
{
    if (!rBuilding.mapOverlay || !rSeenIds.insert(rBuilding.id).second)
    {
        return;
    }
    const BuildingMapOverlay_t& rOverlay = *rBuilding.mapOverlay;
    const std::string& rTemplate = bWater ? rOverlay.seaPath : rOverlay.landPath;
    if (rTemplate.empty())
    {
        return;
    }
    CandidateOverlay_t candidate;
    candidate.pBuilding = &rBuilding;
    candidate.pathTemplate = SubstituteMapOverlayFaction(rTemplate, rSheetStem);
    candidate.sizeStage = sizeStage;
    candidate.priority = rBuilding.mapOverlayPriority;
    if (rBuilding.mapOverlayLayer)
    {
        candidate.layer = *rBuilding.mapOverlayLayer;
    }
    else if (!rBuilding.mapOverlayChannel.empty())
    {
        candidate.layer = rChannels.layersByChannel.at(rBuilding.mapOverlayChannel);
    }
    else
    {
        candidate.layer = 0;
    }
    rOut.push_back(std::move(candidate));
}

} // namespace

Color_t FactionColors_t::LabelColor(const Color_t& rFallback) const
{
    if (bHasTextPrimary)
    {
        return textPrimary;
    }
    if (bHasFactionPrimary)
    {
        return factionPrimary;
    }
    return rFallback;
}

std::optional<std::string> FactionSheetStem(const Faction& rFaction)
{
    const std::string& rId = rFaction.GetDefinition().id;
    if (!std::filesystem::is_directory("assets/factions/" + rId))
    {
        return std::nullopt;
    }
    return rId;
}

int BaseSpriteSizeStage(int population, int bumpCount, const BaseSpriteSizesConfig_t& rConfig)
{
    if (rConfig.sizeStages.empty())
    {
        throw std::runtime_error("BaseSpriteSizeStage: size_stages is empty");
    }
    int stageIndex = 0;
    for (size_t i = 0; i < rConfig.sizeStages.size(); ++i)
    {
        if (population >= rConfig.sizeStages[i].minPopulation)
        {
            stageIndex = static_cast<int>(i);
        }
    }
    stageIndex = std::min(stageIndex + std::max(0, bumpCount),
                          static_cast<int>(rConfig.sizeStages.size()) - 1);
    return stageIndex + 1;
}

int BaseSpriteSizeStage(int population, const BaseManager& rBase,
                        const BaseSpriteSizesConfig_t& rConfig)
{
    int bumpCount = 0;
    const BuildingManager& rBuildings = rBase.GetBuildingManager();
    for (const std::string& rBumpId : rConfig.stageBumpBuildings)
    {
        if (rBuildings.HasBuilding(rBumpId))
        {
            ++bumpCount;
        }
    }
    return BaseSpriteSizeStage(population, bumpCount, rConfig);
}

std::string BareBaseSpritePath(const std::string& rSheetStem, bool bWater, int sizeStage)
{
    const int stage = std::max(1, sizeStage);
    const char* row = bWater ? "water_base" : "base";
    return "assets/factions/" + rSheetStem + "/bases/" + row + "_size" + std::to_string(stage)
           + ".png";
}

std::string SubstituteMapOverlayFaction(const std::string& rTemplate,
                                        const std::string& rSheetStem)
{
    return ReplaceToken_(rTemplate, "faction", rSheetStem);
}

std::string ResolveMapOverlayPathTemplate(const std::string& rTemplate,
                                          const std::string& rSheetStem, int sizeStage)
{
    const int stage = std::max(1, sizeStage);
    std::string path = SubstituteMapOverlayFaction(rTemplate, rSheetStem);
    return ReplaceToken_(path, "size", std::to_string(stage));
}

std::optional<std::string> ResolveSizedSpritePath(
    int wantedStage, const std::function<std::string(int)>& pathForStage,
    const std::function<bool(const std::string&)>& pathExists, std::string_view assetLabel)
{
    const int stage = std::max(1, wantedStage);
    for (int tryStage = stage; tryStage >= 1; --tryStage)
    {
        const std::string path = pathForStage(tryStage);
        if (!pathExists(path))
        {
            continue;
        }
        if (tryStage != stage)
        {
            std::cerr << "[FactionBaseArt] missing " << assetLabel << " size" << stage
                      << "; falling back to size" << tryStage << " (" << path << ")\n";
        }
        return path;
    }
    std::cerr << "[FactionBaseArt] missing " << assetLabel << " size" << stage
              << " and all lower stages\n";
    return std::nullopt;
}

std::vector<ResolvedBaseOverlay_t> ResolveBaseMapOverlays(
    const BaseManager& rBase, const std::string& rSheetStem, bool bWater, int sizeStage,
    const MapOverlayChannelsConfig_t& rChannels)
{
    std::vector<CandidateOverlay_t> candidates;
    std::unordered_set<std::string> seenIds;
    for (const BuildingConfig_t* pBuilding : rBase.GetBuildingManager().GetBuildings())
    {
        if (pBuilding)
        {
            CollectBuildingOverlay_(candidates, *pBuilding, rSheetStem, bWater, sizeStage, rChannels,
                                    seenIds);
        }
    }
    for (const BuildingConfig_t* pGranted : rBase.GetGrantedBuildings())
    {
        if (pGranted)
        {
            CollectBuildingOverlay_(candidates, *pGranted, rSheetStem, bWater, sizeStage, rChannels,
                                    seenIds);
        }
    }

    // Channel → best candidate index.
    std::unordered_map<std::string, size_t> channelWinner;
    std::vector<size_t> keep;
    for (size_t i = 0; i < candidates.size(); ++i)
    {
        const CandidateOverlay_t& rCand = candidates[i];
        if (rCand.pBuilding->mapOverlayChannel.empty())
        {
            keep.push_back(i);
            continue;
        }
        const auto it = channelWinner.find(rCand.pBuilding->mapOverlayChannel);
        if (it == channelWinner.end())
        {
            channelWinner.emplace(rCand.pBuilding->mapOverlayChannel, i);
            continue;
        }
        const CandidateOverlay_t& rPrev = candidates[it->second];
        if (rCand.priority > rPrev.priority
            || (rCand.priority == rPrev.priority && rCand.pBuilding->id < rPrev.pBuilding->id))
        {
            it->second = i;
        }
    }
    for (const auto& [rUnused, index] : channelWinner)
    {
        keep.push_back(index);
    }

    std::vector<ResolvedBaseOverlay_t> resolved;
    resolved.reserve(keep.size());
    for (size_t index : keep)
    {
        const CandidateOverlay_t& rCand = candidates[index];
        resolved.push_back(ResolvedBaseOverlay_t{rCand.pBuilding->id, rCand.pathTemplate,
                                                 rCand.sizeStage, rCand.layer});
    }
    std::sort(resolved.begin(), resolved.end(),
              [](const ResolvedBaseOverlay_t& a, const ResolvedBaseOverlay_t& b) {
                  if (a.layer != b.layer)
                  {
                      return a.layer < b.layer;
                  }
                  return a.buildingId < b.buildingId;
              });
    return resolved;
}

FactionColors_t LoadFactionColorsFile(const std::string& rPath)
{
    std::ifstream file(rPath);
    if (!file)
    {
        throw std::runtime_error("Cannot open colors.json: " + rPath);
    }
    nlohmann::json json;
    try
    {
        file >> json;
    }
    catch (const nlohmann::json::exception& e)
    {
        throw std::runtime_error(std::string("Invalid colors.json: ") + e.what());
    }
    if (!json.is_object())
    {
        throw std::runtime_error("colors.json must be an object");
    }

    FactionColors_t colors;
    if (json.contains("faction_text_color_primary"))
    {
        colors.textPrimary = ColorFromRgbJson_(json.at("faction_text_color_primary"),
                                               "faction_text_color_primary");
        colors.bHasTextPrimary = true;
    }
    if (json.contains("faction_color_primary"))
    {
        colors.factionPrimary =
            ColorFromRgbJson_(json.at("faction_color_primary"), "faction_color_primary");
        colors.bHasFactionPrimary = true;
    }
    return colors;
}

std::string FactionColorsPath(const std::string& rSheetStem)
{
    return "assets/factions/" + rSheetStem + "/colors.json";
}

FactionBaseArtCache::FactionBaseArtCache(SpriteLibrary& rSprites)
    : m_rSprites(rSprites)
{
}

std::optional<std::string> FactionBaseArtCache::EnsureBareBaseSprite(
    const Faction& rFaction, const BaseManager& rBase, const BaseSpriteSizesConfig_t& rSizes)
{
    const auto stem = FactionSheetStem(rFaction);
    if (!stem)
    {
        return std::nullopt;
    }

    const bool bWater = rBase.GetTile().IsWater();
    const int sizeStage =
        BaseSpriteSizeStage(rBase.GetPopulation().GetSize(), rBase, rSizes);
    const std::string label =
        std::string(bWater ? "water base" : "base") + " '" + *stem + "'";
    return ResolveSizedSpritePath(
        sizeStage,
        [&](int stage) { return BareBaseSpritePath(*stem, bWater, stage); },
        [&](const std::string& rPath) { return m_rSprites.Ensure(rPath); }, label);
}

std::vector<std::string> FactionBaseArtCache::EnsureOverlaySprites(
    const std::vector<ResolvedBaseOverlay_t>& rOverlays)
{
    std::vector<std::string> loaded;
    loaded.reserve(rOverlays.size());
    for (const ResolvedBaseOverlay_t& rOverlay : rOverlays)
    {
        const std::string label = "overlay '" + rOverlay.buildingId + "'";
        if (const auto path = ResolveSizedSpritePath(
                rOverlay.sizeStage,
                [&](int stage) {
                    return ReplaceToken_(rOverlay.pathTemplate, "size", std::to_string(stage));
                },
                [&](const std::string& rPath) { return m_rSprites.Ensure(rPath); }, label))
        {
            loaded.push_back(*path);
        }
    }
    return loaded;
}

std::optional<FactionColors_t> FactionBaseArtCache::ColorsFor(const Faction& rFaction)
{
    const auto stem = FactionSheetStem(rFaction);
    if (!stem)
    {
        return std::nullopt;
    }

    StemCache_t& rStem = m_byStem[*stem];
    if (!rStem.bColorsTried)
    {
        rStem.bColorsTried = true;
        const std::string path = FactionColorsPath(*stem);
        std::ifstream probe(path);
        if (!probe)
        {
            rStem.bColorsPresent = false;
            return std::nullopt;
        }
        probe.close();
        rStem.colors = LoadFactionColorsFile(path);
        rStem.bColorsPresent = true;
    }
    if (!rStem.bColorsPresent)
    {
        return std::nullopt;
    }
    return rStem.colors;
}

} // namespace ac
