#include "GameFixtures.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <stdexcept>
#include <thread>

namespace
{

std::unique_ptr<ac::GameState> MakeState_(actest::FactionFixture& rFixture)
{
    auto pMap = std::make_unique<ac::WorldMap>(2, 1, actest::TestMapRules());
    return std::make_unique<ac::GameState>(std::move(pMap), rFixture.dataContext,
                                           rFixture.settings, actest::k_TestRngSeed);
}

} // namespace

TEST_CASE("With no scope there is no current game state", "[game][current-game-state]")
{
    CHECK(ac::FindCurrentGameState() == nullptr);
    CHECK_THROWS_AS(ac::CurrentGameState(), std::logic_error);
}

TEST_CASE("A scope makes its game state current", "[game][current-game-state]")
{
    actest::FactionFixture fixture;
    const std::unique_ptr<ac::GameState> pState = MakeState_(fixture);

    const ac::GameStateScope scope(*pState);

    CHECK(ac::FindCurrentGameState() == pState.get());
    CHECK(&ac::CurrentGameState() == pState.get());
}

TEST_CASE("A nested scope restores the outer game state when it ends",
          "[game][current-game-state]")
{
    actest::FactionFixture fixture;
    const std::unique_ptr<ac::GameState> pOuter = MakeState_(fixture);
    const std::unique_ptr<ac::GameState> pInner = MakeState_(fixture);

    const ac::GameStateScope outer(*pOuter);
    {
        const ac::GameStateScope inner(*pInner);
        CHECK(ac::FindCurrentGameState() == pInner.get());
    }
    CHECK(ac::FindCurrentGameState() == pOuter.get());
}

TEST_CASE("A scope on one thread is not visible on another", "[game][current-game-state]")
{
    actest::FactionFixture fixture;
    const std::unique_ptr<ac::GameState> pState = MakeState_(fixture);
    const ac::GameStateScope scope(*pState);

    const ac::GameState* pSeenOnOtherThread = pState.get();
    std::thread other([&pSeenOnOtherThread]() { pSeenOnOtherThread = ac::FindCurrentGameState(); });
    other.join();

    CHECK(pSeenOnOtherThread == nullptr);
}

TEST_CASE("AddFaction rejects a faction built against another map", "[game][current-game-state]")
{
    actest::FactionFixture fixture;
    ac::WorldMap otherMap(2, 1, actest::TestMapRules());
    auto pFaction = std::make_unique<ac::Faction>(
        fixture.state.AllocateFactionId(), /*bIsPlayerControlled=*/true, fixture.factionDefinition,
        fixture.dataContext, otherMap, fixture.settings, actest::k_TestFactionSeed);

    CHECK_THROWS_AS(fixture.state.AddFaction(std::move(pFaction)), std::invalid_argument);
    CHECK(fixture.state.Factions().empty());
}
