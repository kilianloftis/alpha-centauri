#pragma once

#include "game/effects/ActiveEffect.h"
#include "game/map/ElevationRulesConfigParser.h"
#include "game/map/MapGenerationConfig.h"
#include "game/map/WorldGenPresetConfigParser.h"
#include "game/effects/EffectConfig.h"
#include "game/population/pop-types/GrowthConfigParser.h"

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

inline std::string FixturePath(const std::string& rName)
{
    return std::string(AC_TEST_FIXTURES_DIR) + "/" + rName;
}

// Map-rules ocean line plus the session-default preset's elevation range.
// Address is stable for Tile::BindMapRules.
inline ac::ElevationRulesConfig_t LoadTestMapRules()
{
    ac::ElevationRulesConfig_t rules =
        ac::ElevationRulesConfigParser{}.ParseConfig(FixturePath("map_rules.json"));
    const std::string presetId = ac::MapGenerationConfig_t{}.presetId;
    const std::vector<ac::WorldGenPresetConfig_t> presets =
        ac::WorldGenPresetConfigParser{}.ParseConfig(std::string(AC_CONFIG_DIR)
                                                     + "/worldGen/presets.json");
    const auto it = std::ranges::find_if(
        presets, [&](const ac::WorldGenPresetConfig_t& rPreset) { return rPreset.id == presetId; });
    if (it == presets.end())
    {
        throw std::runtime_error("config/worldGen/presets.json has no '" + presetId + "' preset");
    }
    ac::WorldGenPresetConfigParser::ApplyElevationRange(rules, *it);
    return rules;
}

inline const ac::ElevationRulesConfig_t& TestMapRules()
{
    static const ac::ElevationRulesConfig_t rules = LoadTestMapRules();
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

// Replace (or append) a population baseline on a programmatic GrowthConfig_t.
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
    rConfig.effects.push_back(ac::MakeGrowthBaselineStat(stat, amount));
}

inline void SetMaxBaseSize(ac::GrowthConfig_t& rConfig, double maxSize)
{
    SetGrowthBaseline(rConfig, ac::StatId_t::MaxBaseSize, maxSize);
}

inline void SetStartingSize(ac::GrowthConfig_t& rConfig, double startingSize)
{
    SetGrowthBaseline(rConfig, ac::StatId_t::StartingSize, startingSize);
}

} // namespace actest
