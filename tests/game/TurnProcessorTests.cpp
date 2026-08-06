#include "GameFixtures.h"

#include "game/TurnProcessor.h"
#include "game/TurnStages.h"
#include "game/HookContext.h"
#include "game/Faction.h"
#include "game/GameState.h"
#include "game/GameSettings.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <vector>

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
    GameSettings settings;
    GameState gameState(std::make_unique<WorldMap>(3, 3), world.improvements, nullptr, settings,
                        *world.dataContext.moraleCalculator);

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

    GameSettings settings;
    GameState gameState(std::make_unique<WorldMap>(3, 3), fixtures.improvements, nullptr, settings,
                        *fixtures.dataContext.moraleCalculator);
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
    GameSettings settings;
    GameState gameState(std::make_unique<WorldMap>(3, 3), world.improvements, nullptr, settings,
                        *world.dataContext.moraleCalculator);

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
    GameSettings settings;
    GameState gameState(std::make_unique<WorldMap>(3, 3), world.improvements, nullptr, settings,
                        *world.dataContext.moraleCalculator);

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

    GameSettings settings;
    GameState gameState(std::make_unique<WorldMap>(3, 3), fixtures.improvements, nullptr, settings,
                        *fixtures.dataContext.moraleCalculator);
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
    GameSettings settings;
    GameState gameState(std::make_unique<WorldMap>(3, 3), world.improvements, nullptr, settings,
                        *world.dataContext.moraleCalculator);

    GlobalTurnStageRegistry_t global;
    global["OnlyContinue"] = std::make_unique<CountingGlobalStage>();

    TurnProcessor processor(std::move(global), PerFactionTurnStageRegistry_t{}, {"OnlyContinue"});
    CHECK_THROWS_AS(processor.Advance(gameState), std::logic_error);
}

TEST_CASE("TurnProcessor Reset recovers after a no-yield stage order throw",
          "[TurnProcessor]")
{
    actest::WorldFixture world;
    GameSettings settings;
    GameState gameState(std::make_unique<WorldMap>(3, 3), world.improvements, nullptr, settings,
                        *world.dataContext.moraleCalculator);

    class YieldAfterContinue : public GlobalTurnStage
    {
    public:
        YieldAfterContinue() : GlobalTurnStage(HookContext{}) {}
        int callCount = 0;

    protected:
        StageResult_t ExecuteImpl(GameState&) override
        {
            ++callCount;
            // First Advance: Continue so the cycle can complete without yield and throw.
            // After Reset, second Advance: Yield.
            return callCount == 1 ? StageResult_t::Continue : StageResult_t::Yield;
        }
    };

    GlobalTurnStageRegistry_t global;
    auto pStage = std::make_unique<YieldAfterContinue>();
    YieldAfterContinue& rStage = *pStage;
    global["Flip"] = std::move(pStage);

    TurnProcessor processor(std::move(global), PerFactionTurnStageRegistry_t{}, {"Flip"});
    CHECK_THROWS_AS(processor.Advance(gameState), std::logic_error);
    CHECK(rStage.callCount == 1);

    processor.Reset();
    processor.Advance(gameState);
    CHECK(rStage.callCount == 2);
}

TEST_CASE("TurnProcessor runs OnExit/post hooks when Execute throws",
          "[TurnProcessor]")
{
    actest::WorldFixture world;
    GameSettings settings;
    GameState gameState(std::make_unique<WorldMap>(3, 3), world.improvements, nullptr, settings,
                        *world.dataContext.moraleCalculator);

    class ThrowingStage : public GlobalTurnStage
    {
    public:
        ThrowingStage(HookContext hooks) : GlobalTurnStage(std::move(hooks)) {}

    protected:
        StageResult_t ExecuteImpl(GameState&) override
        {
            throw std::runtime_error("stage failed");
        }
    };

    int postCount = 0;
    HookContext hooks;
    Hook_t post;
    post.modId = "test";
    post.callback = [&]() { ++postCount; };
    hooks.AddPostHook(post);

    GlobalTurnStageRegistry_t global;
    global["Boom"] = std::make_unique<ThrowingStage>(std::move(hooks));

    TurnProcessor processor(std::move(global), PerFactionTurnStageRegistry_t{}, {"Boom"});
    CHECK_THROWS_AS(processor.Advance(gameState), std::runtime_error);
    CHECK(postCount == 1);

    // Not wedged mid-enter: Reset leaves a clean cursor (re-Advance throws again from the
    // stage body, which is defined recovery — not a stuck no-yield poison).
    processor.Reset();
    CHECK_THROWS_AS(processor.Advance(gameState), std::runtime_error);
    CHECK(postCount == 2);
}

TEST_CASE("TurnProcessor unbound replace hook does not skip ExecuteImpl",
          "[TurnProcessor]")
{
    actest::WorldFixture world;
    GameSettings settings;
    GameState gameState(std::make_unique<WorldMap>(3, 3), world.improvements, nullptr, settings,
                        *world.dataContext.moraleCalculator);

    HookContext hooks;
    Hook_t replace;
    replace.modId = "mod";
    // no callback
    hooks.AddReplaceHook(replace);

    class HookedCountingStage : public GlobalTurnStage
    {
    public:
        HookedCountingStage(HookContext h) : GlobalTurnStage(std::move(h)) {}
        int callCount = 0;

    protected:
        StageResult_t ExecuteImpl(GameState&) override
        {
            ++callCount;
            return StageResult_t::Continue;
        }
    };

    GlobalTurnStageRegistry_t global;
    auto pStage = std::make_unique<HookedCountingStage>(std::move(hooks));
    HookedCountingStage& rStage = *pStage;
    global["Hooked"] = std::move(pStage);
    auto pStop = std::make_unique<AlwaysYieldStage>();
    global["Stop"] = std::move(pStop);

    TurnProcessor processor(std::move(global), PerFactionTurnStageRegistry_t{},
                            {"Hooked", "Stop"});
    processor.Advance(gameState);
    CHECK(rStage.callCount == 1);
}

TEST_CASE("TurnProcessor per-faction resume does not skip later factions by id order",
          "[TurnProcessor]")
{
    actest::FactionFixture fixtures;
    // Allocate so the first living faction can have a higher id than the second insertion
    // would under a naive "< resumeId" skip — use GameState allocators after construct.
    GameSettings settings;
    GameState gameState(std::make_unique<WorldMap>(3, 3), fixtures.improvements, nullptr, settings,
                        *fixtures.dataContext.moraleCalculator);

    // Burn a low id so the first added faction is not id 1 contiguous-only assumption.
    (void)gameState.AllocateFactionId();
    (void)gameState.AllocateFactionId();

    auto pA = std::make_unique<Faction>(
        gameState.AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext);
    auto pB = std::make_unique<Faction>(
        gameState.AllocateFactionId(), false, fixtures.factionDefinition, fixtures.dataContext);
    gameState.AddFaction(std::move(pA));
    gameState.AddFaction(std::move(pB));

    class YieldFirstFactionOnce : public PerFactionTurnStage
    {
    public:
        YieldFirstFactionOnce() : PerFactionTurnStage(HookContext{}) {}
        int callCount = 0;
        std::vector<FactionId_t> order;

    protected:
        StageResult_t ExecuteImpl(GameState&, Faction& rFaction) override
        {
            order.push_back(rFaction.GetFactionId());
            ++callCount;
            // Yield once on the first call for each "wave" pattern: first visit yields.
            if (callCount == 1)
            {
                return StageResult_t::Yield;
            }
            return StageResult_t::Continue;
        }
    };

    PerFactionTurnStageRegistry_t perFaction;
    auto pStage = std::make_unique<YieldFirstFactionOnce>();
    YieldFirstFactionOnce& rStage = *pStage;
    perFaction["YieldFirst"] = std::move(pStage);

    GlobalTurnStageRegistry_t global;
    auto pStop = std::make_unique<AlwaysYieldStage>();
    global["Stop"] = std::move(pStop);

    TurnProcessor processor(std::move(global), std::move(perFaction), {"YieldFirst", "Stop"});

    processor.Advance(gameState);
    REQUIRE(rStage.callCount == 1);
    REQUIRE(rStage.order.size() == 1);

    processor.Advance(gameState); // resume first (Continue), then second (Continue), then Stop
    CHECK(rStage.callCount == 3);
    REQUIRE(rStage.order.size() == 3);
    CHECK(rStage.order[0] == rStage.order[1]); // same faction resumed
    CHECK(rStage.order[2] != rStage.order[0]); // second faction also processed
}
