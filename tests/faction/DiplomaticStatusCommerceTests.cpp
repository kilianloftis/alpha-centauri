#include "DiplomacyFixture.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/EffectEnums.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <stdexcept>

using namespace ac;
using namespace actest;

TEST_CASE("A faction's commerce rate follows its status with each partner",
          "[diplomacy][status][commerce]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    game.Set(*game.pA, *game.pC, DiplomaticStatus_t::Treaty);
    game.Set(*game.pA, *game.pC, DiplomaticStatus_t::Pact);

    const auto rateToward = [&](const Faction* pPartner)
    {
        EffectContext_t ctx;
        ctx.pFaction = game.pA;
        ctx.pPartner = pPartner;
        return ResolveFactionStat(game.pA->GetActiveEffects(), StatId_t::CommerceRate, 1.0, &ctx);
    };

    CHECK(rateToward(game.pB) == 0.5);
    CHECK(rateToward(game.pC) == 1.0);
    // Pair effects never reach a resolve that names no partner.
    CHECK(rateToward(nullptr) == 1.0);
    CHECK(ResolveFactionStat(game.pA->GetActiveEffects(), StatId_t::CommerceRate, 1.0) == 1.0);

    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Vendetta);
    CHECK(rateToward(game.pB) == 0.0);

    Faction& rLate = AddSessionFaction(game.fixtures, *game.pState,
                                       game.fixtures.factionDefinition, false);
    CHECK(rateToward(&rLate) == 0.0);
}

TEST_CASE("FactionPair scope belongs to diplomatic statuses alone", "[diplomacy][status][effects]")
{
    const nlohmann::json container = nlohmann::json::parse(R"({
      "effects": [{ "type": "StatModifier", "scope": "FactionPair",
                    "parameters": { "stat": "commerce_rate", "amount": 2, "op": "MultiplyGeometric" } }]
    })");
    CHECK_THROWS_AS(
        EffectConfigParser::ParseEffects(container, EffectSourceKind_t::Building, "test_building"),
        std::runtime_error);
    CHECK_NOTHROW(EffectConfigParser::ParseEffects(container, EffectSourceKind_t::DiplomaticStatus,
                                                   "statuses.Treaty"));
}
