#pragma once

#include "graphics/Graphics.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ac
{

class BaseManager;
class Faction;
class SpriteLibrary;
struct BaseSpriteSizesConfig_t;
struct MapOverlayChannelsConfig_t;

// Map art extracted from a faction .pcx (extract_faction.py) into assets/factions/<stem>/.
struct FactionColors_t
{
    Color_t textPrimary{};
    Color_t factionPrimary{};
    bool bHasTextPrimary = false;
    bool bHasFactionPrimary = false;

    // Label colour: text primary when present, else faction primary.
    Color_t LabelColor(const Color_t& rFallback) const;
};

// One resolved building overlay ready to draw (after channel priority / layer sort).
// pathTemplate has {faction} substituted; {size} remains for per-stage fallback.
struct ResolvedBaseOverlay_t
{
    std::string buildingId;
    std::string pathTemplate;
    int sizeStage = 1;
    int layer = 0;
};

// assets/factions/<faction.id>/ when that directory exists (extract_faction.py writes there).
std::optional<std::string> FactionSheetStem(const Faction& rFaction);

// 1-based size stage from base_sprite_sizes.json (highest min_population ≤ pop, then bumps).
int BaseSpriteSizeStage(int population, const BaseManager& rBase,
                        const BaseSpriteSizesConfig_t& rConfig);
int BaseSpriteSizeStage(int population, int bumpCount, const BaseSpriteSizesConfig_t& rConfig);

// Bare land/water base sprite path (no defense row). sizeStage is 1-based and uncapped.
std::string BareBaseSpritePath(const std::string& rSheetStem, bool bWater, int sizeStage);

// Substitute {faction} and {size} in a building map_overlay path template.
std::string ResolveMapOverlayPathTemplate(const std::string& rTemplate,
                                          const std::string& rSheetStem, int sizeStage);

// Substitute {faction} only, leaving {size} for ResolveSizedSpritePath.
std::string SubstituteMapOverlayFaction(const std::string& rTemplate,
                                        const std::string& rSheetStem);

// Collect constructed + granted map overlays, keep channel winners, sort by effective layer.
std::vector<ResolvedBaseOverlay_t> ResolveBaseMapOverlays(
    const BaseManager& rBase, const std::string& rSheetStem, bool bWater, int sizeStage,
    const MapOverlayChannelsConfig_t& rChannels);

// Load colors.json written by extract_faction.py. Throws on a present but invalid file.
FactionColors_t LoadFactionColorsFile(const std::string& rPath);

// Path to colors.json for a sheet stem under assets/factions/.
std::string FactionColorsPath(const std::string& rSheetStem);

// Walk wantedStage…1 via pathForStage; first existing path wins. Warns on fallback.
std::optional<std::string> ResolveSizedSpritePath(
    int wantedStage, const std::function<std::string(int)>& pathForStage,
    const std::function<bool(const std::string&)>& pathExists, std::string_view assetLabel);

// Loads faction base textures through the SpriteLibrary and caches colors.json per sheet stem.
class FactionBaseArtCache
{
public:
    explicit FactionBaseArtCache(SpriteLibrary& rSprites);

    // Empty when the faction has no sheet stem or no size stage PNG loads (after fallback).
    std::optional<std::string> EnsureBareBaseSprite(const Faction& rFaction,
                                                    const BaseManager& rBase,
                                                    const BaseSpriteSizesConfig_t& rSizes);

    // Loads each overlay, falling back to lower size stages when the preferred PNG is missing.
    std::vector<std::string> EnsureOverlaySprites(
        const std::vector<ResolvedBaseOverlay_t>& rOverlays);

    // Empty when there is no sheet stem or colors.json is absent. Throws if colors.json
    // exists but is invalid.
    std::optional<FactionColors_t> ColorsFor(const Faction& rFaction);

private:
    struct StemCache_t
    {
        bool bColorsTried = false;
        bool bColorsPresent = false;
        FactionColors_t colors{};
    };

    SpriteLibrary& m_rSprites;
    std::unordered_map<std::string, StemCache_t> m_byStem;
};

} // namespace ac
