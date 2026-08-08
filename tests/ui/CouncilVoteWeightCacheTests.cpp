// The council panels' vote-weight cache. Package 15 commit B: both panels resolved
// CouncilVotes stat modifiers once per member per paint (the info panel twice), so the cache
// is what stands between the council screen and a full effect-pool copy every frame.

#include "GameFixtures.h"

#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/CouncilRulesConfigParser.h"
#include "game/council/PlanetaryCouncil.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "ui/council/CouncilVoteWeightCache.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

using namespace ac;
using namespace actest;

namespace
{

struct CouncilFixture_
{
    FactionFixture fixtures;
    GameSettings settings;
    CouncilProposalRegistry councilRegistry;
    CouncilRulesConfig_t councilRules;
    std::unique_ptr<GameState> pState;
    Faction* pA = nullptr;
    Faction* pB = nullptr;

    CouncilFixture_()
    {
        councilRegistry.Load(FixturePath("council/proposals.json"));
        councilRules = CouncilRulesConfigParser{}.ParseConfig(FixturePath("council/rules.json"));

        auto pMap = std::make_unique<WorldMap>(9, 9);
        for (const auto& pTile : pMap->GetTiles())
        {
            pTile->SetElevation(100);
        }
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
            *fixtures.dataContext.moraleCalculator, k_TestRngSeed);

        pA = &pState->AddFaction(std::make_unique<Faction>(
            pState->AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
            pState->GetWorldMap(), settings, k_TestFactionSeed));
        pB = &pState->AddFaction(std::make_unique<Faction>(
            pState->AllocateFactionId(), false, fixtures.factionDefinition, fixtures.dataContext,
            pState->GetWorldMap(), settings, k_TestFactionSeed));

        fixtures.MakeFactionBase(*pA, 2, 2);
        fixtures.MakeFactionBase(*pB, 6, 2);

        pState->CreatePlanetaryCouncil(councilRegistry, councilRules);
        pState->SetMissionYear(2100);
    }

    PlanetaryCouncil& Council() { return *pState->GetPlanetaryCouncil(); }
};

} // namespace

TEST_CASE("A cached vote weight equals the value it replaced", "[ui][council]")
{
    CouncilFixture_ fixture;
    CouncilVoteWeightCache cache;

    const int live =
        fixture.Council().ComputeVoteWeight(*fixture.pA, CouncilVoteWeight_t::Representative);
    CHECK(cache.Get(fixture.Council(), *fixture.pA, CouncilVoteWeight_t::Representative) == live);
    CHECK(cache.GetComputeCount() == 1);

    // Repeated reads of an unchanged council are the per-frame case: same answer, no work.
    for (int i = 0; i < 10; ++i)
    {
        CHECK(cache.Get(fixture.Council(), *fixture.pA, CouncilVoteWeight_t::Representative)
              == live);
    }
    CHECK(cache.GetComputeCount() == 1);
}

TEST_CASE("Each faction gets its own cached weight", "[ui][council]")
{
    // One entry per faction: a single-slot cache would hand faction B the weight computed for
    // faction A, which is exactly what the panels ask for in a loop over members.
    CouncilFixture_ fixture;
    CouncilVoteWeightCache cache;

    // Give A a second base so population-weighted votes differ between the two.
    fixture.fixtures.MakeFactionBase(*fixture.pA, 4, 4);

    const int liveA =
        fixture.Council().ComputeVoteWeight(*fixture.pA, CouncilVoteWeight_t::Population);
    const int liveB =
        fixture.Council().ComputeVoteWeight(*fixture.pB, CouncilVoteWeight_t::Population);
    REQUIRE(liveA != liveB);

    // Alternating between members is what the panels do; a single-slot cache would answer
    // correctly but recompute on every read.
    for (int i = 0; i < 5; ++i)
    {
        CHECK(cache.Get(fixture.Council(), *fixture.pA, CouncilVoteWeight_t::Population) == liveA);
        CHECK(cache.Get(fixture.Council(), *fixture.pB, CouncilVoteWeight_t::Population) == liveB);
    }
    CHECK(cache.GetComputeCount() == 2);
}

TEST_CASE("A cached weight follows a population change", "[ui][council]")
{
    // AddPop is the narrowest mutation that changes a population-weighted vote: no new base,
    // no new building. It also moves the faction's effect-pool version (pops contribute
    // effects), so this does not isolate the cache's population field — see the header.
    CouncilFixture_ fixture;
    CouncilVoteWeightCache cache;
    BaseManager& rBase = *fixture.pA->Bases().begin();

    const int before =
        cache.Get(fixture.Council(), *fixture.pA, CouncilVoteWeight_t::Population);

    rBase.GetPopulation().AddPop();
    const int live =
        fixture.Council().ComputeVoteWeight(*fixture.pA, CouncilVoteWeight_t::Population);
    REQUIRE(live != before);

    CHECK(cache.Get(fixture.Council(), *fixture.pA, CouncilVoteWeight_t::Population) == live);
}

TEST_CASE("A cached weight is not reused across weighting modes", "[ui][council]")
{
    CouncilFixture_ fixture;
    CouncilVoteWeightCache cache;
    fixture.fixtures.MakeFactionBase(*fixture.pA, 4, 4);

    const int representative =
        fixture.Council().ComputeVoteWeight(*fixture.pA, CouncilVoteWeight_t::Representative);
    const int population =
        fixture.Council().ComputeVoteWeight(*fixture.pA, CouncilVoteWeight_t::Population);
    REQUIRE(representative != population);

    CHECK(cache.Get(fixture.Council(), *fixture.pA, CouncilVoteWeight_t::Representative)
          == representative);
    CHECK(cache.Get(fixture.Council(), *fixture.pA, CouncilVoteWeight_t::Population)
          == population);
    CHECK(cache.Get(fixture.Council(), *fixture.pA, CouncilVoteWeight_t::Representative)
          == representative);
}
