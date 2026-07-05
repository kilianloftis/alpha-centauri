#pragma once

#include "lib/effects/ActiveEffect.h"
#include "lib/effects/BonusEffect.h"

#include <deque>
#include <optional>
#include <string>
#include <utility>

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

    const ac::EffectConfig_t& StatMod(ac::StatId stat, double amount,
                                      ac::ModifierOp op = ac::ModifierOp::Add,
                                      ac::EffectScope_t scope = ac::EffectScope_t::FactionGlobal,
                                      std::optional<ac::TileSelector_t> selector = std::nullopt,
                                      std::optional<ac::Condition_t> condition = std::nullopt,
                                      ac::EffectPersistence_t persistence = ac::EffectPersistence_t::Continuous)
    {
        ac::StatModifierEffect_t modifier;
        modifier.stat = stat;
        modifier.amount = amount;
        modifier.op = op;
        modifier.selector = std::move(selector);

        ac::EffectConfig_t config;
        config.effect = modifier;
        config.scope = scope;
        config.persistence = persistence;
        config.condition = std::move(condition);
        return Add(std::move(config));
    }

    const ac::EffectConfig_t& RuleFlag(ac::RuleFlagId flag,
                                       ac::EffectScope_t scope = ac::EffectScope_t::FactionGlobal)
    {
        ac::EffectConfig_t config;
        config.effect = ac::RuleFlagEffect_t{flag};
        config.scope = scope;
        config.persistence = ac::EffectPersistence_t::Continuous;
        return Add(std::move(config));
    }

    const ac::EffectConfig_t& RatingMod(ac::SocialRatingId rating, int amount,
                                        ac::EffectScope_t scope = ac::EffectScope_t::FactionGlobal)
    {
        ac::EffectConfig_t config;
        config.effect = ac::SocialRatingModifierEffect_t{rating, amount};
        config.scope = scope;
        config.persistence = ac::EffectPersistence_t::Continuous;
        return Add(std::move(config));
    }

private:
    std::deque<ac::EffectConfig_t> m_configs;
};

inline ac::ActiveEffect_t Active(const ac::EffectConfig_t& rConfig, std::string sourceId = "src",
                                 const ac::BaseManager* pOriginBase = nullptr)
{
    return ac::ActiveEffect_t{&rConfig, std::move(sourceId), pOriginBase};
}

inline ac::TileSelector_t BaseTileSelector()
{
    return ac::TileSelector_t{ac::TileSelectorKind::BaseTile, std::nullopt};
}

inline ac::TileSelector_t ImprovementSelector(std::string improvementId)
{
    return ac::TileSelector_t{ac::TileSelectorKind::HasImprovement, std::move(improvementId)};
}

inline ac::Condition_t TargetTileHas(std::string featureId)
{
    return ac::Condition_t{ac::ConditionKind::TargetTileHas, std::move(featureId)};
}

} // namespace actest
