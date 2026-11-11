#pragma once

#include "game/effects/EffectConfig.h"

#include <array>
#include <string>
#include <vector>

namespace ac
{

class LuaRuntime;

// One native life abundance level; its effects enter every faction's pool.
struct NativeLifeLevel_t
{
    std::string id;
    std::string name;
    // Rare/normal/abundant → 0/1/2. Feeds Planet fauna formulas
    // (docs/thinker/terranx-map.md, Native life scaling).
    int faunaForms = 0;
    // Scales decoration fungus coverage: rare 0.5, normal 1.0, abundant 1.5.
    float fungusCoverageMultiplier = 1.0f;
    std::vector<EffectConfig_t> effects;
};

// config/native_life.json plus formula strings from config/native_life.lua.
struct NativeLifeConfig_t
{
    // Inclusive uniform range for how many native lifeforms a fungal bloom spawns.
    int fungalBloomNativeLifeformsMin = 0;
    int fungalBloomNativeLifeformsMax = 0;

    // Turn thresholds for Planet lifecycle floor indices 0..5; 0 means that index is skipped
    // (vanilla Great Boil). See docs/thinker/terranx-map.md morale_alien.
    std::array<int, 6> planetLifecycleTurnThresholds = {45, 90, 170, 250, 0, 330};

    // Near-base ambush BaseFindDist band (inclusive).
    int ambushNearBaseMinDist = 2;
    int ambushNearBaseMaxDist = 5;

    // Empty-ocean boat sites need this many tiles in the water region (SMAC Continents.tile_count).
    int boatEmptyOceanMinRegionTiles = 0;

    std::string boatPeriodFormula;
    std::string earlyLimitThresholdFormula;
    std::string boatCargoFormula;
    std::string ambushAttemptsFormula;
    std::string swarmYearThresholdFormula;

    std::string defaultId;
    std::vector<NativeLifeLevel_t> levels;

    const NativeLifeLevel_t* FindById(const std::string& rId) const;
    // Session lookup: empty rLevelId selects defaultId. Throws when the id is unknown.
    const NativeLifeLevel_t& RequireForSession(const std::string& rLevelId) const;
};

class NativeLifeConfigParser
{
public:
    // Every JSON key is required. rFormulaPath is a Lua script returning the five formula names.
    NativeLifeConfig_t ParseConfig(const std::string& rConfigPath, const std::string& rFormulaPath,
                                   LuaRuntime& rLua);
};

} // namespace ac
