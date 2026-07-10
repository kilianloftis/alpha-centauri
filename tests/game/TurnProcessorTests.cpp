#include "GameFixtures.h"

#include "game/TurnProcessor.h"
#include "game/TurnStages.h"
#include "game/GameState.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

using namespace ac;

namespace
{

// Minimal stubs to observe TurnProcessor's dispatch without pulling in real stage logic.
class CountingGlobalStage : public GlobalTurnStage
{
public:
    CountingGlobalStage() : GlobalTurnStage(nullptr) {}
    int callCount = 0;

protected:
    void ExecuteImpl(GameState&) override { ++callCount; }
};

class CountingPerFactionStage : public PerFactionTurnStage
{
public:
    CountingPerFactionStage() : PerFactionTurnStage(nullptr) {}
    int callCount = 0;

protected:
    void ExecuteImpl(GameState&, Faction&) override { ++callCount; }
};

} // namespace

TEST_CASE("TurnProcessor throws instead of silently skipping a stage id missing from both registries",
          "[TurnProcessor]")
{
    actest::WorldFixture world;
    GameState gameState(std::make_unique<WorldMap>(3, 3), world.improvements, nullptr);

    TurnProcessor processor(GlobalTurnStageRegistry_t{}, PerFactionTurnStageRegistry_t{},
                             {"NoSuchStage"});

    CHECK_THROWS_AS(processor.ProcessTurn(gameState), std::runtime_error);
}

TEST_CASE("TurnProcessor executes a global stage once and a per-faction stage once per faction",
          "[TurnProcessor]")
{
    actest::FactionFixture fixtures;
    fixtures.MakeFaction();
    fixtures.MakeFaction();

    GameState gameState(std::make_unique<WorldMap>(3, 3), fixtures.improvements, nullptr);
    gameState.AddFaction(std::move(fixtures.factions[0]));
    gameState.AddFaction(std::move(fixtures.factions[1]));

    GlobalTurnStageRegistry_t global;
    auto pGlobalStage = std::make_unique<CountingGlobalStage>();
    CountingGlobalStage& rGlobalStage = *pGlobalStage;
    global["GlobalOnce"] = std::move(pGlobalStage);

    PerFactionTurnStageRegistry_t perFaction;
    auto pPerFactionStage = std::make_unique<CountingPerFactionStage>();
    CountingPerFactionStage& rPerFactionStage = *pPerFactionStage;
    perFaction["PerFactionEach"] = std::move(pPerFactionStage);

    TurnProcessor processor(std::move(global), std::move(perFaction),
                             {"GlobalOnce", "PerFactionEach"});
    processor.ProcessTurn(gameState);

    CHECK(rGlobalStage.callCount == 1);
    CHECK(rPerFactionStage.callCount == 2);
}
