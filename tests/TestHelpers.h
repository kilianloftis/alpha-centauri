#pragma once

#include "game/GameDataPaths.h"
#include "game/effects/ActiveEffect.h"
#include "game/map/MapRulesConfigParser.h"
#include "game/worldgen/MapGenerationConfig.h"
#include "game/worldgen/WorldGenPresetConfigParser.h"
#include "game/effects/EffectConfig.h"
#include "game/population/GrowthConfigParser.h"
#include "game/units/UnitComponentConfigParser.h"
#include "TempConfigFile.h"

#include <stdexcept>

#include <deque>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace ac
{
class BaseManager;
}

namespace actest
{

// Default test map in SMAC coordinates. Lattice cell (a, b) in 0..8 x 0..8 sits at
// (a - b + 8, a + b); the width keeps the seam out of reach of that diamond.
constexpr int k_TestMapWidth = 36;
constexpr int k_TestMapHeight = 17;

inline std::string FixturePath(const std::string& rName)
{
    return std::string(AC_TEST_FIXTURES_DIR) + "/" + rName;
}

// Every file LoadGameData reads, from tests/fixtures/.
inline ac::GameDataPaths FixtureDataPaths()
{
    ac::GameDataPaths paths;
    paths.popTypes = FixturePath("pop_types.json");
    // buildings.json grants an unknown building on purpose, which the loader rejects.
    paths.buildings = FixturePath("buildings_loadable.json");
    paths.mapOverlayChannels = FixturePath("map_overlay_channels.json");
    paths.baseSpriteSizes = FixturePath("base_sprite_sizes.json");
    paths.stockpiles = FixturePath("stockpiles.json");
    paths.improvements = FixturePath("improvements.json");
    paths.terrain = FixturePath("terrain.json");
    paths.unitComponents = FixturePath("unit_components.json");
    paths.unitSlots = FixturePath("unit_slots.json");
    paths.nativeUnits = FixturePath("native_units.json");
    // techs.json plus every tech the other files cite.
    paths.techs = FixturePath("techs_full.json");
    paths.socialPolicies = FixturePath("social_policies.json");
    paths.socialRatings = FixturePath("social_rating_effects.json");
    paths.factions = FixturePath("factions");
    paths.popComposition = FixturePath("pop_composition.json");
    paths.popGrowth = FixturePath("pop_growth.json");
    paths.production = FixturePath("production.json");
    paths.commerce = FixturePath("commerce.json");
    paths.diplomacy = FixturePath("diplomacy.json");
    paths.techCost = FixturePath("tech_cost.lua");
    paths.worldGenPresets = FixturePath("worldGen/presets.json");
    paths.worldGenDecoration = FixturePath("worldGen/decoration.json");
    paths.worldGenLandmarks = FixturePath("worldGen/landmarks.json");
    paths.tileYieldRules = FixturePath("tile_yield_rules.json");
    paths.mapRules = FixturePath("map_rules.json");
    paths.interactionGrids = FixturePath("interaction_grids.json");
    paths.moraleLevels = FixturePath("morale_levels.json");
    paths.probeActions = FixturePath("probe_actions.json");
    paths.baseConquest = FixturePath("base_conquest.json");
    paths.combat = FixturePath("combat.json");
    paths.councilProposals = FixturePath("council/proposals.json");
    paths.councilRules = FixturePath("council/rules.json");
    paths.hotkeys = FixturePath("hotkeys.json");
    paths.standingCaptions = FixturePath("standing_captions.json");
    paths.talk = FixturePath("ui/talk.json");
    paths.textRoots = {FixturePath("text")};
    paths.textLocale = "en";
    paths.difficulty = FixturePath("difficulty.json");
    paths.policeRules = FixturePath("police_rules.json");
    paths.worldRules = FixturePath("world_rules.json");
    paths.atrocities = FixturePath("atrocities.json");
    paths.ecoDamage = FixturePath("eco_damage.json");
    paths.ecoDamageFormula = FixturePath("eco_damage.lua");
    paths.nativeLife = FixturePath("native_life.json");
    paths.nativeLifeFormula = FixturePath("native_life.lua");
    paths.worldEvents = FixturePath("world_events.json");
    return paths;
}

// Map-rules ocean line plus the session-default preset's elevation range.
// Address is stable for Tile::BindMapRules.
inline ac::MapRulesConfig_t LoadTestMapRules()
{
    ac::MapRulesConfig_t rules =
        ac::MapRulesConfigParser{}.ParseConfig(FixturePath("map_rules.json"));
    const std::string presetId = ac::MapGenerationConfig_t{}.presetId;
    const std::vector<ac::WorldGenPresetConfig_t> presets =
        ac::WorldGenPresetConfigParser{}.ParseConfig(FixturePath("worldGen/presets.json"));
    const auto it = std::ranges::find_if(
        presets, [&](const ac::WorldGenPresetConfig_t& rPreset) { return rPreset.id == presetId; });
    if (it == presets.end())
    {
        throw std::runtime_error("worldGen/presets.json has no '" + presetId + "' preset");
    }
    ac::WorldGenPresetConfigParser::ApplyElevationRange(rules, *it);
    return rules;
}

inline const ac::MapRulesConfig_t& TestMapRules()
{
    static const ac::MapRulesConfig_t rules = LoadTestMapRules();
    return rules;
}

// Owns EffectConfig_t storage for tests. In production ActiveEffect_t::config points into
// long-lived static config data; tests need the same address stability, which std::deque
// provides (elements never relocate on push_back).
class EffectPool
{
public:
    const ac::EffectConfig_t& Add(ac::EffectConfig_t config)
    {
        m_configs.push_back(std::move(config));
        return m_configs.back();
    }

    const ac::EffectConfig_t& StatMod(ac::StatId_t stat, double amount,
                                      ac::ModifierOp_t op = ac::ModifierOp_t::Add,
                                      ac::EffectScope_t scope = ac::EffectScope_t::FactionGlobal,
                                      std::optional<ac::TileSelector_t> selector = std::nullopt,
                                      std::optional<ac::Condition_t> condition = std::nullopt,
                                      std::optional<ac::BuildingFilter_t> buildingFilter = std::nullopt)
    {
        ac::StatModifierEffect_t modifier;
        modifier.stat = stat;
        modifier.amount = amount;
        modifier.op = op;
        modifier.selector = std::move(selector);

        ac::EffectConfig_t config;
        config.effect = modifier;
        config.scope = scope;
        config.condition = std::move(condition);
        config.buildingFilter = std::move(buildingFilter);
        return Add(std::move(config));
    }

    const ac::EffectConfig_t& RuleFlag(ac::RuleFlagId_t flag,
                                       ac::EffectScope_t scope = ac::EffectScope_t::FactionGlobal,
                                       std::optional<ac::Condition_t> condition = std::nullopt)
    {
        ac::EffectConfig_t config;
        config.effect = ac::RuleFlagEffect_t{flag};
        config.scope = scope;
        config.condition = std::move(condition);
        return Add(std::move(config));
    }

    const ac::EffectConfig_t& RatingMod(ac::SocialRatingId_t rating, int amount,
                                        ac::EffectScope_t scope = ac::EffectScope_t::FactionGlobal)
    {
        ac::EffectConfig_t config;
        config.effect = ac::SocialRatingModifierEffect_t{rating, amount};
        config.scope = scope;
        return Add(std::move(config));
    }

private:
    std::deque<ac::EffectConfig_t> m_configs;
};

inline ac::ActiveEffect_t Active(const ac::EffectConfig_t& rConfig, std::string sourceId = "src",
                                 const ac::BaseManager* pOriginBase = nullptr)
{
    return ac::ActiveEffect_t(rConfig, std::move(sourceId), pOriginBase);
}

// Materializes a lazy Filter*(...) result into an owned, indexable vector — needed
// wherever a test checks .size()/operator[] on the result, since filter_view (unlike a
// vector) supports neither. Prefer std::ranges::distance(...) instead when a test only
// needs a count and never indexes the result.
template <std::ranges::input_range Range>
std::vector<ac::ActiveEffect_t> Materialize(Range&& range)
{
    return std::vector<ac::ActiveEffect_t>(range.begin(), range.end());
}

inline ac::TileSelector_t BaseTileSelector()
{
    return ac::TileSelectorBaseTile_t{};
}

inline ac::TileSelector_t AnyTileSelector()
{
    return ac::TileSelectorAnyTile_t{};
}

inline ac::TileSelector_t ImprovementSelector(std::string improvementId)
{
    return ac::TileSelectorHasImprovement_t{std::move(improvementId)};
}

inline ac::Condition_t TargetTileHas(std::string featureId)
{
    return ac::TargetTileHas_t{std::move(featureId)};
}

inline ac::Condition_t SubjectDomainCondition(ac::UnitDomain_t domain)
{
    return ac::SubjectDomain_t{domain};
}

inline ac::Condition_t HasComponentCondition(std::string componentId)
{
    return ac::HasComponent_t{std::move(componentId)};
}

// The fixture pop_growth.json, for tests that adjust a growth config of their own.
inline ac::GrowthConfig_t FixtureGrowthConfig()
{
    return ac::GrowthConfigParser{}.ParseConfig(FixturePath("pop_growth.json"));
}

// An AllOwnerBases Add of amount to stat, the shape pop_growth.json's baselines take.
inline ac::EffectConfig_t GrowthBaselineStat(ac::StatId_t stat, double amount)
{
    ac::EffectConfig_t effect;
    effect.scope = ac::EffectScope_t::AllOwnerBases;
    ac::StatModifierEffect_t modifier;
    modifier.stat = stat;
    modifier.amount = amount;
    modifier.op = ac::ModifierOp_t::Add;
    effect.effect = modifier;
    return effect;
}

// Replace (or append) a population baseline on a growth config.
// Mutates in place so FactionEffectsPool ActiveEffect_t pointers stay valid.
inline void SetGrowthBaseline(ac::GrowthConfig_t& rConfig, ac::StatId_t stat, double amount)
{
    for (ac::EffectConfig_t& rEffect : rConfig.effects)
    {
        if (auto* pMod = std::get_if<ac::StatModifierEffect_t>(&rEffect.effect);
            pMod && pMod->stat == stat)
        {
            pMod->amount = amount;
            pMod->op = ac::ModifierOp_t::Add;
            return;
        }
    }
    rConfig.effects.push_back(GrowthBaselineStat(stat, amount));
}

inline void SetMaxBaseSize(ac::GrowthConfig_t& rConfig, double maxSize)
{
    SetGrowthBaseline(rConfig, ac::StatId_t::MaxBaseSize, maxSize);
}

inline void SetStartingSize(ac::GrowthConfig_t& rConfig, double startingSize)
{
    SetGrowthBaseline(rConfig, ac::StatId_t::StartingSize, startingSize);
}

// One component entry through the file parser, as a config holding only it loads.
inline ac::UnitComponentConfig_t ParseOneComponent(const nlohmann::json& rEntry)
{
    const TempConfigFile file("one_component.json", nlohmann::json::array({rEntry}).dump());
    return ac::UnitComponentConfigParser{}.ParseConfig(file.Path()).at(0);
}

} // namespace actest
