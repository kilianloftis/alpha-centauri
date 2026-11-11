#include "GameFixtures.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/PlayerInteraction.h"
#include "game/PlayerInteractionQueue.h"
#include "game/economy/EconomyManager.h"
#include "game/effects/TriggeredEffect.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementIds.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/UnitManager.h"
#include "game/supply/SupplyPodConfig.h"
#include "game/supply/SupplyPodIndex.h"
#include "game/supply/SupplyPodMutations.h"
#include "game/supply/SupplyPodRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"
#include "game/worldgen/SupplyPodGeneration.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <random>
#include <ranges>
#include <string>
#include <variant>

using namespace ac;
using namespace actest;

namespace
{

SupplyPodEventGroupConfig_t EmptyEventGroup_(const std::string& rId, UnitDomain_t domain,
                                             const std::string& rEmptyScript)
{
    SupplyPodEventGroupConfig_t eventGroup;
    eventGroup.id = rId;
    eventGroup.placement.domain = domain;
    eventGroup.emptyNotice.script = rEmptyScript;
    return eventGroup;
}

SupplyPodConfig_t OneEventConfig_(const SupplyPodEventConfig_t& rEvent, UnitDomain_t domain)
{
    SupplyPodEventConfig_t event = rEvent;
    if (event.notice.script.empty())
    {
        event.notice.script = event.id;
    }

    SupplyPodEventGroupConfig_t land = EmptyEventGroup_("Land", UnitDomain_t::Land, "Empty");
    SupplyPodEventGroupConfig_t sea = EmptyEventGroup_("Sea", UnitDomain_t::Sea, "Empty_Sea");
    if (domain == UnitDomain_t::Land)
    {
        land.events = {std::move(event)};
    }
    else
    {
        sea.events = {std::move(event)};
    }

    SupplyPodConfig_t config;
    config.eventGroups = {std::move(land), std::move(sea)};
    return config;
}

void InstallConfig_(FactionFixture& rFixtures, SupplyPodConfig_t config)
{
    rFixtures.dataContext.supplyPodConfig =
        std::make_unique<SupplyPodConfig_t>(std::move(config));
}

void PlacePod_(FactionFixture& rFixtures, Tile& rTile, const std::string& rEventGroupId)
{
    rTile.AddTerrainFeature(
        rFixtures.improvements.Get(std::string(ImprovementIds::k_SupplyPod)));
    rFixtures.state.GetSupplyPods().Stamp(rTile, rEventGroupId);
}

bool FrontIsSupplyPodNotice_(GameState& rState, const std::string& rNoticeKey)
{
    const QueuedInteraction_t* pFront = rState.GetPlayerInteractions().Front();
    if (!pFront)
    {
        return false;
    }
    const auto* pNotice = std::get_if<SupplyPodNoticeInteraction_t>(&pFront->payload);
    return pNotice && pNotice->noticeKey == rNoticeKey;
}

TriggeredEffectConfig_t Effect_(TriggeredEffectVariant_t effect)
{
    TriggeredEffectConfig_t config;
    config.effect = std::move(effect);
    return config;
}

} // namespace

TEST_CASE("Supply pods coexist with landmarks and bonuses by mutual excludes",
          "[supply][placement]")
{
    FactionFixture fixtures;
    FillLand(fixtures.map);
    Tile& rTile = fixtures.At(8, 8);
    const ImprovementConfig_t& rPod =
        fixtures.improvements.Get(std::string(ImprovementIds::k_SupplyPod));
    const ImprovementConfig_t& rMonolith =
        fixtures.improvements.Get(std::string(ImprovementIds::k_Monolith));

    CHECK(CanBuildImprovement(rTile, rPod, {}, {}));
    rTile.AddTerrainFeature(rPod);
    CHECK_FALSE(CanBuildImprovement(rTile, rMonolith, {}, {}));

    rTile.RemoveTerrainFeature(rPod.id);
    rTile.AddTerrainFeature(rMonolith);
    CHECK_FALSE(CanBuildImprovement(rTile, rPod, {}, {}));
}

TEST_CASE("Global supply-pod placement stamps event_group ids", "[supply][placement]")
{
    FactionFixture fixtures;
    FillLand(fixtures.map);
    SupplyPodConfig_t config;
    SupplyPodEventGroupConfig_t land = EmptyEventGroup_("Land", UnitDomain_t::Land, "Empty");
    land.placement.globalFraction = 1.0;
    SupplyPodEventGroupConfig_t sea = EmptyEventGroup_("Sea", UnitDomain_t::Sea, "Empty_Sea");
    config.eventGroups = {std::move(land), std::move(sea)};
    std::mt19937 rng(1);
    SupplyPodIndex index;

    const int scattered =
        PlaceGlobalSupplyPods(fixtures.map, config, fixtures.improvements, index, rng);
    CHECK(scattered > 0);
    int stamped = 0;
    for (const auto& pTile : fixtures.map.GetTiles())
    {
        if (!pTile || !TileHasSupplyPod(*pTile))
        {
            continue;
        }
        const std::string* pId = index.Find(*pTile);
        REQUIRE(pId != nullptr);
        CHECK(*pId == "Land");
        ++stamped;
    }
    CHECK(stamped == scattered);
}

TEST_CASE("Opening a supply pod removes it and grants energy from the fixture table",
          "[supply][open]")
{
    FactionFixture fixtures;
    FillLand(fixtures.map);
    Faction& rPlayer = AddSessionFaction(fixtures, fixtures.factionDefinition, true);
    Unit& rUnit = fixtures.MakeUnit(rPlayer, 8, 8);
    Tile& rTile = fixtures.At(8, 8);
    PlacePod_(fixtures, rTile, "Land");

    const int energyBefore = rPlayer.GetEconomy().GetEnergy();
    const SupplyPodOpenResult_t result =
        OpenSupplyPod(fixtures.state, rUnit, fixtures.state.GetRng());

    CHECK(result.bOpened);
    CHECK(result.eventId == "OnlyEnergy");
    CHECK_FALSE(TileHasSupplyPod(rTile));
    CHECK(fixtures.state.GetSupplyPods().Find(rTile) == nullptr);
    CHECK(rPlayer.GetEconomy().GetEnergy() > energyBefore);
    CHECK(FrontIsSupplyPodNotice_(fixtures.state, "OnlyEnergy"));
}

TEST_CASE("Air units cannot open supply pods", "[supply][open]")
{
    FactionFixture fixtures;
    FillLand(fixtures.map);
    Faction& rPlayer = AddSessionFaction(fixtures, fixtures.factionDefinition, true);
    Unit& rAir = fixtures.MakeUnit(rPlayer, 8, 8, {"test_flight_chassis"});
    Tile& rTile = fixtures.At(8, 8);
    PlacePod_(fixtures, rTile, "Land");

    CHECK(rAir.GetDomain() == UnitDomain_t::Air);
    CHECK_FALSE(CanOpenSupplyPod(rAir));
    const SupplyPodOpenResult_t result =
        OpenSupplyPod(fixtures.state, rAir, fixtures.state.GetRng());
    CHECK_FALSE(result.bOpened);
    CHECK(TileHasSupplyPod(rTile));
    CHECK(fixtures.state.GetPlayerInteractions().Empty());
}

TEST_CASE("PickEvent returns Empty when no event is eligible", "[supply][rules][empty]")
{
    SupplyPodEventGroupConfig_t eventGroup =
        EmptyEventGroup_("Sea", UnitDomain_t::Sea, "Empty_Sea");
    SupplyPodEventConfig_t gated;
    gated.id = "Later";
    gated.weight = 1;
    gated.filters.minTurn = 10;
    eventGroup.events = {std::move(gated)};
    std::mt19937 rng(1);

    const SupplyPodEventPick_t pick = PickEvent(eventGroup, /*turn=*/0, rng);
    CHECK(pick.eventId == k_SupplyPodEmptyEventId);
    CHECK(pick.pEvent == nullptr);
    REQUIRE(pick.pNotice != nullptr);
    CHECK(pick.pNotice->script == "Empty_Sea");
}

TEST_CASE("Open uses the stamped event_group, not the tile surface", "[supply][open][stamp]")
{
    FactionFixture fixtures;
    FillLand(fixtures.map);
    // Water tile stamped Land — open must use Land's empty table when Land has no events.
    SupplyPodConfig_t config;
    config.eventGroups = {EmptyEventGroup_("Land", UnitDomain_t::Land, "Empty"),
                          EmptyEventGroup_("Sea", UnitDomain_t::Sea, "Empty_Sea")};
    InstallConfig_(fixtures, std::move(config));

    Faction& rPlayer = AddSessionFaction(fixtures, fixtures.factionDefinition, true);
    Unit& rUnit = fixtures.MakeUnit(rPlayer, 8, 8);
    Tile& rTile = fixtures.At(8, 8);
    rTile.SetElevation(-100);
    REQUIRE(rTile.IsWater());
    PlacePod_(fixtures, rTile, "Land");

    const SupplyPodOpenResult_t result =
        OpenSupplyPod(fixtures.state, rUnit, fixtures.state.GetRng());
    CHECK(result.bOpened);
    CHECK(result.eventId == k_SupplyPodEmptyEventId);
    CHECK(result.noticeKey == "Empty");
    CHECK_FALSE(TileHasSupplyPod(rTile));
    CHECK(FrontIsSupplyPodNotice_(fixtures.state, "Empty"));
}

TEST_CASE("No eligible events opens as Empty and removes the pod", "[supply][open][empty]")
{
    FactionFixture fixtures;
    FillLand(fixtures.map);
    SupplyPodEventConfig_t event;
    event.id = "LandEnergy";
    event.weight = 1;
    event.effects = {Effect_(GrantEnergyEffect_t{.amount = 5})};
    InstallConfig_(fixtures, OneEventConfig_(event, UnitDomain_t::Land));

    Faction& rPlayer = AddSessionFaction(fixtures, fixtures.factionDefinition, true);
    Unit& rUnit = fixtures.MakeUnit(rPlayer, 8, 8);
    Tile& rTile = fixtures.At(8, 8);
    rTile.SetElevation(-100);
    REQUIRE(rTile.IsWater());
    PlacePod_(fixtures, rTile, "Sea");

    const SupplyPodOpenResult_t result =
        OpenSupplyPod(fixtures.state, rUnit, fixtures.state.GetRng());
    CHECK(result.bOpened);
    CHECK(result.eventId == k_SupplyPodEmptyEventId);
    CHECK(result.noticeKey == "Empty_Sea");
    CHECK_FALSE(TileHasSupplyPod(rTile));
    CHECK(FrontIsSupplyPodNotice_(fixtures.state, "Empty_Sea"));
}

TEST_CASE("ResourceBonus places a tile bonus and removes the pod", "[supply][outcome]")
{
    FactionFixture fixtures;
    FillLand(fixtures.map);
    SupplyPodEventConfig_t event;
    event.id = "ResourceBonus";
    event.weight = 1;
    AddRandomOccupantEffect_t add;
    add.improvementIds = {"Nutrients", "Minerals", "Energy"};
    event.effects = {Effect_(std::move(add))};
    InstallConfig_(fixtures, OneEventConfig_(event, UnitDomain_t::Land));

    Faction& rPlayer = AddSessionFaction(fixtures, fixtures.factionDefinition, true);
    Unit& rUnit = fixtures.MakeUnit(rPlayer, 8, 8);
    Tile& rTile = fixtures.At(8, 8);
    PlacePod_(fixtures, rTile, "Land");

    const SupplyPodOpenResult_t result =
        OpenSupplyPod(fixtures.state, rUnit, fixtures.state.GetRng());
    CHECK(result.bOpened);
    CHECK_FALSE(TileHasSupplyPod(rTile));
    const bool bHasBonus = rTile.HasTerrainFeature("Nutrients") || rTile.HasTerrainFeature("Minerals")
                           || rTile.HasTerrainFeature("Energy");
    CHECK(bHasBonus);
    CHECK(FrontIsSupplyPodNotice_(fixtures.state, "ResourceBonus"));
}

TEST_CASE("PlaceMonolith stamps a Monolith", "[supply][outcome]")
{
    FactionFixture fixtures;
    FillLand(fixtures.map);
    SupplyPodEventConfig_t event;
    event.id = "PlaceMonolith";
    event.weight = 1;
    event.effects = {Effect_(AddOccupantEffect_t{"Monolith"})};
    InstallConfig_(fixtures, OneEventConfig_(event, UnitDomain_t::Land));

    Faction& rPlayer = AddSessionFaction(fixtures, fixtures.factionDefinition, true);
    Unit& rUnit = fixtures.MakeUnit(rPlayer, 8, 8);
    Tile& rTile = fixtures.At(8, 8);
    PlacePod_(fixtures, rTile, "Land");

    CHECK(OpenSupplyPod(fixtures.state, rUnit, fixtures.state.GetRng()).bOpened);
    CHECK(rTile.HasTerrainFeature(ImprovementIds::k_Monolith));
}

TEST_CASE("MapReveal marks explored tiles around the pod", "[supply][outcome]")
{
    FactionFixture fixtures;
    FillLand(fixtures.map);
    SupplyPodEventConfig_t event;
    event.id = "MapReveal";
    event.weight = 1;
    event.effects = {Effect_(RevealMapEffect_t{2})};
    InstallConfig_(fixtures, OneEventConfig_(event, UnitDomain_t::Land));

    Faction& rPlayer = AddSessionFaction(fixtures, fixtures.factionDefinition, true);
    Unit& rUnit = fixtures.MakeUnit(rPlayer, 8, 8);
    Tile& rTile = fixtures.At(8, 8);
    PlacePod_(fixtures, rTile, "Land");

    CHECK(OpenSupplyPod(fixtures.state, rUnit, fixtures.state.GetRng()).bOpened);
    CHECK(rPlayer.GetExploredMap().IsExplored(rTile));
    CHECK(rPlayer.GetExploredMap().IsExplored(fixtures.At(10, 8)));
}

TEST_CASE("DestroyVisitor destroys the opener", "[supply][outcome]")
{
    FactionFixture fixtures;
    FillLand(fixtures.map);
    SupplyPodEventConfig_t event;
    event.id = "DestroyVisitor";
    event.weight = 1;
    event.effects = {Effect_(DestroyUnitEffect_t{})};
    InstallConfig_(fixtures, OneEventConfig_(event, UnitDomain_t::Land));

    Faction& rPlayer = AddSessionFaction(fixtures, fixtures.factionDefinition, true);
    Unit& rUnit = fixtures.MakeUnit(rPlayer, 8, 8);
    PlacePod_(fixtures, fixtures.At(8, 8), "Land");

    const SupplyPodOpenResult_t result =
        OpenSupplyPod(fixtures.state, rUnit, fixtures.state.GetRng());
    CHECK(result.bOpened);
    CHECK(result.bVisitorDestroyed);
    CHECK(std::ranges::distance(rPlayer.GetUnitManager().Units()) == 0);
}
