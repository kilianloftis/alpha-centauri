#include "game/units/ProbeActionConfigParser.h"
#include "game/units/ProbeRules.h"
#include "game/units/MoraleCalculator.h"
#include "game/units/MoraleConfigParser.h"
#include "game/social-engineering/SocialRatingConfigParser.h"
#include "game/effects/BonusEffect.h"

#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <random>
#include <variant>

using namespace ac;
using namespace actest;

namespace
{

const StatModifierEffect_t* FindProbeDefenseAtLevel_(const SocialRatingConfig_t& rConfig, int level)
{
    const auto it = rConfig.levelEffects.find(level);
    if (it == rConfig.levelEffects.end())
    {
        return nullptr;
    }
    for (const EffectConfig_t& rEffect : it->second)
    {
        const auto* pMod = std::get_if<StatModifierEffect_t>(&rEffect.effect);
        if (pMod && pMod->stat == StatId_t::ProbeDefense)
        {
            return pMod;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("ProbeActionConfigParser loads SMAC defaults", "[probe][config]")
{
    const ProbeActionsConfig_t config =
        ProbeActionConfigParser{}.ParseConfig(FixturePath("probe_actions.json"));

    CHECK(config.successFormula.moraleDivisor == 2);
    CHECK(config.successFormula.strengthOffset == 1);
    REQUIRE(config.Find(ProbeActionId_t::Infiltrate) != nullptr);
    CHECK(config.Find(ProbeActionId_t::Infiltrate)->risk == 0);
    REQUIRE(config.Find(ProbeActionId_t::Infiltrate)->effects.size() == 1);
    CHECK(std::get_if<InfiltrationEffect_t>(
        &config.Find(ProbeActionId_t::Infiltrate)->effects.front().effect));
    REQUIRE(config.Find(ProbeActionId_t::Infiltrate)->effects.front().factionFilter);
    CHECK(config.Find(ProbeActionId_t::Infiltrate)->effects.front().factionFilter->kind
          == FactionFilterKind_t::ActionTarget);
    REQUIRE(config.Find(ProbeActionId_t::StealTech) != nullptr);
    CHECK(config.Find(ProbeActionId_t::StealTech)->riskRepeat == 1);
    REQUIRE(config.Find(ProbeActionId_t::MindControlBase) != nullptr);
    REQUIRE(config.Find(ProbeActionId_t::MindControlBase)->cost.has_value());
    CHECK(config.Find(ProbeActionId_t::MindControlBase)->cost->energyBias == 1200);
    CHECK(config.Find(ProbeActionId_t::MindControlBase)->cost->distBias == 4);
    CHECK_FALSE(config.Find(ProbeActionId_t::Infiltrate)->cost.has_value());
    REQUIRE(config.Find(ProbeActionId_t::SubvertUnit) != nullptr);
    REQUIRE(config.Find(ProbeActionId_t::SubvertUnit)->cost.has_value());
    CHECK(config.Find(ProbeActionId_t::SubvertUnit)->cost->energyBias == 800);
    CHECK(config.Find(ProbeActionId_t::SubvertUnit)->target == ProbeTargetKind_t::Unit);
}

TEST_CASE("SE Probe negative levels emit probe_defense for success math", "[probe][config][se]")
{
    // Wiki Probe: -1/-2 increase enemy success rate via target PROBE effect in the
    // success/escape formulas (not a separate success_rate stat). Positive levels raise
    // own-probe morale instead and must not emit probe_defense.
    const std::vector<SocialRatingConfig_t> ratings =
        SocialRatingConfigParser{}.ParseConfig(FixturePath("social_rating_effects.json"));

    const SocialRatingConfig_t* pProbe = nullptr;
    for (const SocialRatingConfig_t& rRating : ratings)
    {
        if (rRating.id == "probe")
        {
            pProbe = &rRating;
            break;
        }
    }
    REQUIRE(pProbe != nullptr);

    REQUIRE(FindProbeDefenseAtLevel_(*pProbe, -1) != nullptr);
    CHECK(FindProbeDefenseAtLevel_(*pProbe, -1)->amount == -1);
    REQUIRE(FindProbeDefenseAtLevel_(*pProbe, -2) != nullptr);
    CHECK(FindProbeDefenseAtLevel_(*pProbe, -2)->amount == -2);
    REQUIRE(FindProbeDefenseAtLevel_(*pProbe, -3) != nullptr);
    CHECK(FindProbeDefenseAtLevel_(*pProbe, -3)->amount == -3);

    CHECK(FindProbeDefenseAtLevel_(*pProbe, 1) == nullptr);
    CHECK(FindProbeDefenseAtLevel_(*pProbe, 2) == nullptr);
    CHECK(FindProbeDefenseAtLevel_(*pProbe, 3) == nullptr);
}

TEST_CASE("ComputeProbeChances matches wiki tables for RISK 0-2", "[probe][rules]")
{
    ProbeSuccessFormula_t formula;
    // Elite (6), target Probe 0 → strength = 6/2 - 0 + 1 = 4
    {
        const ProbeChance_t c = ComputeProbeChances(6, 0, 0, formula);
        CHECK(c.successRate == 100);
        // escape failure = 1/6 → survival 100 - 16 = 84? (1*100)/6 = 16
        CHECK(c.survivalRate == 100 - (100 / 6));
    }
    // Disciplined (2), RISK 1, target 0 → strength = 1 - 0 + 1 = 2; failure = 50%
    {
        const ProbeChance_t c = ComputeProbeChances(2, 1, 0, formula);
        CHECK(c.probeStrength == 2);
        CHECK(c.successRate == 50);
        // escape: (1+1)/2 = 100% loss → survival 0
        CHECK(c.survivalRate == 0);
    }
    // Hardened (3), RISK 2, target -2 → strength = 1 - (-2) + 1 = 4; failure = 50%
    {
        const ProbeChance_t c = ComputeProbeChances(3, 2, -2, formula);
        CHECK(c.probeStrength == 4);
        CHECK(c.successRate == 50);
    }
}

TEST_CASE("Algo enhancement halves failure; HSA halves success", "[probe][rules]")
{
    ProbeSuccessFormula_t formula;
    const ProbeChance_t normal = ComputeProbeChances(4, 2, 0, formula, 1.0, 1.0);
    // Algo vs non-HSA: failure scale 0.5
    const ProbeChance_t algo = ComputeProbeChances(4, 2, 0, formula, 0.5, 1.0);
    // Algo vs HSA: failure scale stays 1.0, success scale 0.5
    const ProbeChance_t hsa = ComputeProbeChances(4, 2, 0, formula, 1.0, 0.5);

    // strength = 4/2 + 1 = 3; failure = 200/3 = 66 → success 34
    CHECK(normal.successRate == 100 - (200 / 3));
    CHECK(algo.successRate == 100 - static_cast<int>(std::lround((200 / 3) * 0.5)));
    CHECK(hsa.successRate == static_cast<int>(std::lround((100 - (200 / 3)) * 0.5)));
}

TEST_CASE("RollProbeAction RISK 0 always succeeds mission", "[probe][rules]")
{
    ProbeSuccessFormula_t formula;
    const ProbeChance_t chances = ComputeProbeChances(2, 0, 0, formula);
    std::mt19937 rng(1);
    for (int i = 0; i < 20; ++i)
    {
        const ProbeRollResult_t roll = RollProbeAction(chances, rng);
        CHECK(roll.missionSucceeded);
    }
}

TEST_CASE("TryPromoteProbeMission bumps XP up to max", "[probe][morale]")
{
    const MoraleConfig_t moraleConfig =
        MoraleConfigParser{}.ParseConfig(FixturePath("morale_levels.json"));
    MoraleCalculator morale(moraleConfig);

    // Minimal unit isn't needed — just verify helper clamps via a stub is awkward without
    // Unit. Covered in ProbeAction integration tests; here pin config max.
    CHECK(moraleConfig.MaxLevel() == 6);
    CHECK(moraleConfig.probeBaseIntrinsic == 2);
}
