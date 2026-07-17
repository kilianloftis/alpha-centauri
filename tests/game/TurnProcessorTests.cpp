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

class CountingGlobalStage : public GlobalTurnStage
{
public:
    CountingGlobalStage() : GlobalTurnStage(HookContext{}) {}
    int callCount = 0;

protected:
    StageResult_t ExecuteImpl(GameState&) override
    {
        ++callCount;
        return StageResult_t::Continue;
    }
};

class CountingPerFactionStage : public PerFactionTurnStage
{
public:
    CountingPerFactionStage() : PerFactionTurnStage(HookContext{}) {}
    int callCount = 0;

protected:
    StageResult_t ExecuteImpl(GameState&, Faction&) override
    {
        ++callCount;
        return StageResult_t::Continue;
    }
};

class YieldOnceGlobalStage : public GlobalTurnStage
{
public:
    YieldOnceGlobalStage() : GlobalTurnStage(HookContext{}) {}
    int callCount = 0;

protected:
    StageResult_t ExecuteImpl(GameState&) override
    {
        ++callCount;
        if (callCount == 1)
        {
            return StageResult_t::Yield;
        }
        return StageResult_t::Continue;
    }
};

class AlwaysYieldStage : public GlobalTurnStage
{
public:
    AlwaysYieldStage() : GlobalTurnStage(HookContext{}) {}
    int callCount = 0;

protected:
    StageResult_t ExecuteImpl(GameState&) override
    {
        ++callCount;
        return StageResult_t::Yield;
    }
};

class YieldingPerFactionStage : public PerFactionTurnStage
{
public:
    YieldingPerFactionStage() : PerFactionTurnStage(HookContext{}) {}
    int callCount = 0;

protected:
    StageResult_t ExecuteImpl(GameState&, Faction&) override
    {
        ++callCount;
        if ((callCount % 2) == 1)
        {
            return StageResult_t::Yield;
        }
        return StageResult_t::Continue;
    }
};

} // namespace

TEST_CASE("TurnProcessor throws when constructed with an empty stage order",
          "[TurnProcessor]")
{
    CHECK_THROWS_AS(
        TurnProcessor(GlobalTurnStageRegistry_t{}, PerFactionTurnStageRegistry_t{}, {}),
        std::logic_error);
}

TEST_CASE("TurnProcessor throws instead of silently skipping a stage id missing from both registries",
          "[TurnProcessor]")
{
    actest::WorldFixture world;
    GameState gameState(std::make_unique<WorldMap>(3, 3), world.improvements, nullptr);

    TurnProcessor processor(GlobalTurnStageRegistry_t{}, PerFactionTurnStageRegistry_t{},
                             {"NoSuchStage"});

    CHECK_THROWS_AS(processor.Advance(gameState), std::runtime_error);
}

TEST_CASE("TurnProcessor executes stages until a yield, including per-faction stages",
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

    auto pStop = std::make_unique<AlwaysYieldStage>();
    AlwaysYieldStage& rStop = *pStop;
    global["Stop"] = std::move(pStop);

    PerFactionTurnStageRegistry_t perFaction;
    auto pPerFactionStage = std::make_unique<CountingPerFactionStage>();
    CountingPerFactionStage& rPerFactionStage = *pPerFactionStage;
    perFaction["PerFactionEach"] = std::move(pPerFactionStage);

    TurnProcessor processor(std::move(global), std::move(perFaction),
                             {"GlobalOnce", "PerFactionEach", "Stop"});
    processor.Advance(gameState);

    CHECK(rGlobalStage.callCount == 1);
    CHECK(rPerFactionStage.callCount == 2);
    CHECK(rStop.callCount == 1);
}

TEST_CASE("TurnProcessor re-enters a yielding stage on the next Advance",
          "[TurnProcessor]")
{
    actest::WorldFixture world;
    GameState gameState(std::make_unique<WorldMap>(3, 3), world.improvements, nullptr);

    GlobalTurnStageRegistry_t global;
    auto pStage = std::make_unique<YieldOnceGlobalStage>();
    YieldOnceGlobalStage& rStage = *pStage;
    global["YieldOnce"] = std::move(pStage);

    auto pAfter = std::make_unique<CountingGlobalStage>();
    CountingGlobalStage& rAfter = *pAfter;
    global["After"] = std::move(pAfter);

    auto pStop = std::make_unique<AlwaysYieldStage>();
    AlwaysYieldStage& rStop = *pStop;
    global["Stop"] = std::move(pStop);

    TurnProcessor processor(std::move(global), PerFactionTurnStageRegistry_t{},
                             {"YieldOnce", "After", "Stop"});

    processor.Advance(gameState);
    CHECK(rStage.callCount == 1);
    CHECK(rAfter.callCount == 0);
    CHECK(rStop.callCount == 0);

    processor.Advance(gameState);
    CHECK(rStage.callCount == 2);
    CHECK(rAfter.callCount == 1);
    CHECK(rStop.callCount == 1);
}

TEST_CASE("TurnProcessor wraps to the start of the stage order after the last stage",
          "[TurnProcessor]")
{
    actest::WorldFixture world;
    GameState gameState(std::make_unique<WorldMap>(3, 3), world.improvements, nullptr);

    // Yields once, then Continues and resets so the next cycle yields again.
    class YieldEachCycleStage : public GlobalTurnStage
    {
    public:
        YieldEachCycleStage() : GlobalTurnStage(HookContext{}) {}
        int callCount = 0;
        bool bYieldNext = true;

    protected:
        StageResult_t ExecuteImpl(GameState&) override
        {
            ++callCount;
            if (bYieldNext)
            {
                bYieldNext = false;
                return StageResult_t::Yield;
            }
            bYieldNext = true;
            return StageResult_t::Continue;
        }
    };

    GlobalTurnStageRegistry_t global;
    auto pStage = std::make_unique<YieldEachCycleStage>();
    YieldEachCycleStage& rStage = *pStage;
    global["Cycle"] = std::move(pStage);

    auto pAfter = std::make_unique<CountingGlobalStage>();
    CountingGlobalStage& rAfter = *pAfter;
    global["After"] = std::move(pAfter);

    TurnProcessor processor(std::move(global), PerFactionTurnStageRegistry_t{},
                             {"Cycle", "After"});

    processor.Advance(gameState);
    CHECK(rStage.callCount == 1);
    CHECK(rAfter.callCount == 0);

    // Resume: Cycle continues, After runs, wrap, Cycle yields again (third call).
    processor.Advance(gameState);
    CHECK(rStage.callCount == 3);
    CHECK(rAfter.callCount == 1);
}

TEST_CASE("TurnProcessor resumes the same faction after a per-faction stage yields",
          "[TurnProcessor]")
{
    actest::FactionFixture fixtures;
    fixtures.MakeFaction();
    fixtures.MakeFaction();

    GameState gameState(std::make_unique<WorldMap>(3, 3), fixtures.improvements, nullptr);
    gameState.AddFaction(std::move(fixtures.factions[0]));
    gameState.AddFaction(std::move(fixtures.factions[1]));

    PerFactionTurnStageRegistry_t perFaction;
    auto pStage = std::make_unique<YieldingPerFactionStage>();
    YieldingPerFactionStage& rStage = *pStage;
    perFaction["YieldEach"] = std::move(pStage);

    GlobalTurnStageRegistry_t global;
    auto pStop = std::make_unique<AlwaysYieldStage>();
    global["Stop"] = std::move(pStop);

    TurnProcessor processor(std::move(global), std::move(perFaction), {"YieldEach", "Stop"});

    processor.Advance(gameState);
    CHECK(rStage.callCount == 1);

    processor.Advance(gameState);
    CHECK(rStage.callCount == 3);

    // Faction 1 finished; Stop yields.
    processor.Advance(gameState);
    CHECK(rStage.callCount == 4);
}

TEST_CASE("TurnProcessor throws if the stage order has no yielding stage",
          "[TurnProcessor]")
{
    actest::WorldFixture world;
    GameState gameState(std::make_unique<WorldMap>(3, 3), world.improvements, nullptr);

    GlobalTurnStageRegistry_t global;
    global["OnlyContinue"] = std::make_unique<CountingGlobalStage>();

    TurnProcessor processor(std::move(global), PerFactionTurnStageRegistry_t{}, {"OnlyContinue"});
    CHECK_THROWS_AS(processor.Advance(gameState), std::logic_error);
}
