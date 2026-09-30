// The world-event registry: the Cycle trigger, its parser, and the WorldEvents stage that
// advances the active set and fires each event's edge effects.

#include "GameFixtures.h"
#include "TempConfigFile.h"

#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/stages/WorldEvents.h"
#include "game/world-events/WorldEventConfig.h"
#include "game/world-events/WorldEventTracker.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <memory>
#include <string>

using namespace ac;
using namespace actest;
using Catch::Approx;
using Catch::Matchers::ContainsSubstring;

namespace
{

// A live session on its own 9x9 land map with a player and an AI faction.
struct WorldEventSession_
{
    FactionFixture fixtures;
    // Declared before the GameState: a tracker created from it holds a reference.
    WorldEventsConfig_t worldEvents;
    std::unique_ptr<GameState> pState;
    Faction* pPlayer = nullptr;
    Faction* pAi = nullptr;

    WorldEventSession_()
        : pState(MakeLandSession(fixtures, fixtures.improvements))
    {
        pPlayer = &AddSessionFaction(fixtures, *pState, fixtures.factionDefinition,
                                     /*bIsPlayerControlled=*/true);
        pAi = &AddSessionFaction(fixtures, *pState, fixtures.factionDefinition,
                                 /*bIsPlayerControlled=*/false);
    }

    BaseManager& MakeBase(Faction& rFaction, int x, int y)
    {
        return MakeSessionBase(fixtures, *pState, rFaction, x, y);
    }

    void RunWorldEvents(int yearsSinceFirstPlayable)
    {
        pState->SetMissionYear(GameState::k_FirstPlayableMissionYear + yearsSinceFirstPlayable);
        WorldEvents stage(HookContext("WorldEvents"));
        REQUIRE(stage.Execute(*pState) == StageResult_t::Continue);
    }
};

WorldEventsConfig_t ParseWorldEvents_(const std::string& rJson)
{
    const TempConfigFile file("world_events.json", rJson);
    return WorldEventsConfigParser{}.ParseConfig(file.Path());
}

} // namespace

TEST_CASE("A Cycle event is active for its duration in every cycle", "[world-events]")
{
    const WorldEventCycle_t perihelion{80, 20, 0};
    CHECK(IsWorldEventActive(perihelion, 0));
    CHECK(IsWorldEventActive(perihelion, 19));
    CHECK_FALSE(IsWorldEventActive(perihelion, 20));
    CHECK_FALSE(IsWorldEventActive(perihelion, 79));
    CHECK(IsWorldEventActive(perihelion, 80));

    const WorldEventCycle_t shifted{80, 20, 10};
    CHECK_FALSE(IsWorldEventActive(shifted, 0));
    CHECK(IsWorldEventActive(shifted, 10));
    CHECK_FALSE(IsWorldEventActive(shifted, 30));
}

TEST_CASE("WorldEvents fires on_start and on_end once on each edge", "[world-events][stage]")
{
    WorldEventSession_ session;
    session.worldEvents = ParseWorldEvents_(R"({ "events": [ {
        "id": "Pulse", "name": "Pulse",
        "trigger": { "kind": "Cycle", "cycle_years": 4, "duration_years": 2,
                     "start_year_offset": 0 },
        "effects": [],
        "on_start_effects": [ { "type": "GrantEnergy", "parameters": { "amount": 10 } } ],
        "on_end_effects": [ { "type": "GrantEnergy", "parameters": { "amount": 1 } } ]
    } ] })");
    session.pState->CreateWorldEvents(session.worldEvents);
    EconomyManager& rEconomy = session.pPlayer->GetEconomy();
    const int start = rEconomy.GetEnergy();

    session.RunWorldEvents(0);
    CHECK(rEconomy.GetEnergy() == start + 10);
    session.RunWorldEvents(1);
    CHECK(rEconomy.GetEnergy() == start + 10);
    session.RunWorldEvents(2);
    CHECK(rEconomy.GetEnergy() == start + 11);
    session.RunWorldEvents(3);
    CHECK(rEconomy.GetEnergy() == start + 11);
    session.RunWorldEvents(4);
    CHECK(rEconomy.GetEnergy() == start + 21);
    // Every faction is a subject of a world event's triggered lists.
    CHECK(session.pAi->GetEconomy().GetEnergy() > 0);
}

TEST_CASE("An active event's effects reach every faction and leave with it", "[world-events]")
{
    WorldEventSession_ session;
    session.pState->CreateWorldEvents(*session.fixtures.dataContext.worldEventsConfig);
    const BaseManager& rPlayerBase = session.MakeBase(*session.pPlayer, 2, 2);
    const BaseManager& rAiBase = session.MakeBase(*session.pAi, 6, 6);
    const auto scale = [](const BaseManager& rBase) {
        return ResolveBaseStat(rBase.GetBaseEffects(), StatId_t::EcologicalDamage,
                               SeedFor(StatId_t::EcologicalDamage));
    };
    const double playerBefore = scale(rPlayerBase);
    const double aiBefore = scale(rAiBase);

    session.RunWorldEvents(0);
    CHECK(scale(rPlayerBase) == Approx(2.0 * playerBefore));
    CHECK(scale(rAiBase) == Approx(2.0 * aiBefore));

    session.RunWorldEvents(20);
    CHECK(scale(rPlayerBase) == Approx(playerBefore));
    CHECK(scale(rAiBase) == Approx(aiBefore));
}

TEST_CASE("WorldEventsConfigParser rejects bad configs", "[world-events][parser]")
{
    const auto event = [](const std::string& rTrigger) {
        return R"({ "events": [ { "id": "E", "name": "E", "trigger": )" + rTrigger
               + R"(, "effects": [], "on_start_effects": [], "on_end_effects": [] } ] })";
    };
    CHECK_NOTHROW(ParseWorldEvents_(event(
        R"({ "kind": "Cycle", "cycle_years": 8, "duration_years": 2, "start_year_offset": 0 })")));
    CHECK_THROWS_WITH(ParseWorldEvents_(event(
        R"({ "kind": "Random", "cycle_years": 8, "duration_years": 2, "start_year_offset": 0 })")),
        ContainsSubstring("Cycle"));
    CHECK_THROWS_WITH(ParseWorldEvents_(event(
        R"({ "kind": "Cycle", "cycle_years": 8, "duration_years": 9, "start_year_offset": 0 })")),
        ContainsSubstring("duration_years"));
    CHECK_THROWS_WITH(ParseWorldEvents_(event(
        R"({ "kind": "Cycle", "cycle_years": 0, "duration_years": 0, "start_year_offset": 0 })")),
        ContainsSubstring("cycle_years"));
    CHECK_THROWS_WITH(ParseWorldEvents_(event(
        R"({ "kind": "Cycle", "cycle_years": 8, "duration_years": 2 })")),
        ContainsSubstring("start_year_offset"));
    CHECK_THROWS_WITH(ParseWorldEvents_(R"({ "events": [ { "id": "E", "name": "E",
        "trigger": { "kind": "Cycle", "cycle_years": 8, "duration_years": 2,
                     "start_year_offset": 0 },
        "effects": [] , "on_start_effects": [] } ] })"),
        ContainsSubstring("on_end_effects"));
    CHECK_THROWS_WITH(ParseWorldEvents_(R"({ "events": [
        { "id": "E", "name": "E", "trigger": { "kind": "Cycle", "cycle_years": 8,
          "duration_years": 2, "start_year_offset": 0 },
          "effects": [], "on_start_effects": [], "on_end_effects": [] },
        { "id": "E", "name": "E", "trigger": { "kind": "Cycle", "cycle_years": 8,
          "duration_years": 2, "start_year_offset": 0 },
          "effects": [], "on_start_effects": [], "on_end_effects": [] } ] })"),
        ContainsSubstring("duplicate"));
}
