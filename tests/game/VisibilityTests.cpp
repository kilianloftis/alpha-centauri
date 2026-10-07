// Fog of war: Faction owns FactionExploredMap (permanent memory) and FactionVisibleMap
// (current vision), rebuilt from unit Vision and base sight whenever sources change.

#include "GameFixtures.h"

#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/faction/UnitManager.h"
#include "game/faction/VisibilityRules.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>

using namespace ac;

TEST_CASE("Unit vision reveals a Chebyshev disk including diagonals", "[visibility][fog]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    fixture.MakeUnit(faction, 8, 8, {"test_chassis"}); // vision 1

    const FactionVisibleMap& rVisible = faction.GetVisibleMap();
    const FactionExploredMap& rExplored = faction.GetExploredMap();

    CHECK(rVisible.IsVisible(8, 8));
    CHECK(rExplored.IsExplored(8, 8));
    CHECK(rVisible.IsVisible(9, 9));
    CHECK(rVisible.IsVisible(7, 9));
    CHECK(rVisible.IsVisible(7, 7));
    CHECK(rVisible.IsVisible(9, 7));
    // Vision 1 includes diagonals (Chebyshev distance 1).
    CHECK(rVisible.IsVisible(8, 10));
    CHECK(rVisible.IsVisible(8, 6));
    CHECK(rExplored.IsExplored(8, 10));

    // Chebyshev distance 2 is outside vision 1.
    CHECK_FALSE(rVisible.IsVisible(10, 10));
    CHECK_FALSE(rExplored.IsExplored(10, 10));
    CHECK_FALSE(rVisible.IsVisible(8, 12));
}

TEST_CASE("Moving a unit expands explored memory and updates current visibility",
          "[visibility][fog]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});

    const FactionVisibleMap& rVisible = faction.GetVisibleMap();
    const FactionExploredMap& rExplored = faction.GetExploredMap();
    REQUIRE(rExplored.IsExplored(8, 8));
    REQUIRE(rVisible.IsVisible(8, 8));
    REQUIRE_FALSE(rExplored.IsExplored(10, 10));

    fixture.MoveUnit(unit, 9, 9);

    // Old tile stays explored (memory) and remains visible while still in the
    // vision-1 square around the new position.
    CHECK(rExplored.IsExplored(8, 8));
    CHECK(rVisible.IsVisible(8, 8));

    // New frontier is revealed.
    CHECK(rVisible.IsVisible(10, 10));
    CHECK(rExplored.IsExplored(10, 10));

    fixture.MoveUnit(unit, 10, 10);

    // Origin is now outside the Chebyshev vision-1 square and fogged, but still explored.
    CHECK(rExplored.IsExplored(8, 8));
    CHECK_FALSE(rVisible.IsVisible(8, 8));
    CHECK(rVisible.IsVisible(10, 10));
}

TEST_CASE("Destroying a unit drops current visibility but keeps explored tiles",
          "[visibility][fog]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});

    const FactionVisibleMap& rVisible = faction.GetVisibleMap();
    const FactionExploredMap& rExplored = faction.GetExploredMap();
    REQUIRE(rVisible.IsVisible(8, 8));
    REQUIRE(rExplored.IsExplored(9, 9));

    faction.GetUnitManager().DestroyUnit(unit);

    CHECK_FALSE(rVisible.IsVisible(8, 8));
    CHECK(rExplored.IsExplored(8, 8));
    CHECK(rExplored.IsExplored(9, 9));
}

TEST_CASE("A base reveals a Chebyshev radius-2 vision square", "[visibility][fog]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    fixture.MakeFactionBase(faction, 8, 8);

    const FactionVisibleMap& rVisible = faction.GetVisibleMap();
    CHECK(rVisible.IsVisible(8, 8));
    CHECK(rVisible.IsVisible(9, 9));
    CHECK(rVisible.IsVisible(10, 10));
    CHECK(rVisible.IsVisible(8, 12));
    CHECK_FALSE(rVisible.IsVisible(11, 11));
}

TEST_CASE("A Sensor reveals Chebyshev radius-2 for its territory owner only",
          "[visibility][fog][territory]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& other = fixture.MakeFaction();
    // Center the base so its vision-2 square does not reach the Sensor; Sensor alone
    // must supply the far-ring checks below.
    fixture.MakeFactionBase(owner, 8, 8);
    fixture.ctx->AddOccupantWithEffects(fixture.At(11, 5), "Sensor");
    owner.RebuildVisibility();
    other.RebuildVisibility();

    const FactionId_t ownerId = owner.GetFactionId();
    REQUIRE(fixture.map.GetTerritory().GetOwner(11, 5) == ownerId);

    CHECK(owner.GetVisibleMap().IsVisible(11, 5));
    CHECK(owner.GetVisibleMap().IsVisible(12, 4)); // Chebyshev 1 from Sensor
    CHECK(owner.GetVisibleMap().IsVisible(13, 7)); // Chebyshev 2
    CHECK(owner.GetVisibleMap().IsVisible(14, 6));
    // Chebyshev 3 from Sensor, and outside base vision-2 from (4,4).
    CHECK_FALSE(owner.GetVisibleMap().IsVisible(5, 11));
    CHECK(owner.GetExploredMap().IsExplored(13, 7));

    CHECK_FALSE(other.GetVisibleMap().IsVisible(11, 5));
    CHECK_FALSE(other.GetVisibleMap().IsVisible(13, 7));
}

TEST_CASE("Unit vision wraps horizontally across the map seam", "[visibility][fog][wrap]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    const int width = fixture.map.GetWidth();
    fixture.MakeUnit(faction, 0, 4, {"test_chassis"}); // vision 1

    const FactionVisibleMap& rVisible = faction.GetVisibleMap();
    const FactionExploredMap& rExplored = faction.GetExploredMap();

    CHECK(rVisible.IsVisible(0, 4));
    CHECK(rVisible.IsVisible(1, 5));
    // West neighbor is the east edge.
    CHECK(rVisible.IsVisible(width - 2, 4));
    CHECK(rExplored.IsExplored(width - 2, 4));
    CHECK(rVisible.IsVisible(width - 1, 5)); // diagonal across the seam
    CHECK(rVisible.IsVisible(width - 1, 3));

    // Chebyshev 2 across the wrap (and east) stays outside vision 1.
    CHECK_FALSE(rVisible.IsVisible(width - 4, 4));
    CHECK_FALSE(rVisible.IsVisible(2, 6));
}

TEST_CASE("Base vision wraps horizontally across the map seam", "[visibility][fog][wrap]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    const int width = fixture.map.GetWidth();
    fixture.MakeFactionBase(faction, 0, 4); // vision 2

    const FactionVisibleMap& rVisible = faction.GetVisibleMap();
    CHECK(rVisible.IsVisible(0, 4));
    CHECK(rVisible.IsVisible(2, 6));
    CHECK(rVisible.IsVisible(width - 2, 4));
    CHECK(rVisible.IsVisible(width - 4, 4)); // Chebyshev 2 west via wrap
    CHECK(rVisible.IsVisible(width - 3, 5));

    CHECK_FALSE(rVisible.IsVisible(3, 7));         // Chebyshev 3 east
    CHECK_FALSE(rVisible.IsVisible(width - 6, 4)); // Chebyshev 3 west via wrap
}

TEST_CASE("Sensor vision wraps horizontally across the map seam",
          "[visibility][fog][wrap][territory]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    const int width = fixture.map.GetWidth();
    // Base far enough that only the Sensor supplies seam vision.
    fixture.MakeFactionBase(owner, 4, 8);
    fixture.ctx->AddOccupantWithEffects(fixture.At(0, 4), "Sensor");
    owner.RebuildVisibility();

    REQUIRE(fixture.map.GetTerritory().GetOwner(0, 4) == owner.GetFactionId());
    CHECK(owner.GetVisibleMap().IsVisible(0, 4));
    CHECK(owner.GetVisibleMap().IsVisible(width - 2, 4));
    CHECK(owner.GetVisibleMap().IsVisible(width - 4, 4));
    // Chebyshev 3 from the Sensor across the wrap — outside its vision-2 square.
    CHECK_FALSE(owner.GetVisibleMap().IsVisible(width - 6, 4));
}

TEST_CASE("ApplyRemoveShroud marks the entire map explored", "[visibility][fog][shroud]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    fixture.MakeUnit(faction, 8, 8, {"test_chassis"});

    REQUIRE_FALSE(faction.GetExploredMap().IsExplored(8, 0));
    ApplyRemoveShroud(faction);
    CHECK(faction.GetExploredMap().IsExplored(8, 0));
    CHECK(faction.GetExploredMap().IsExplored(8, 16));
    // Fog of war is unchanged: far tiles stay invisible.
    CHECK_FALSE(faction.GetVisibleMap().IsVisible(8, 0));
}

TEST_CASE("ApplyRemoveFog makes every tile currently visible", "[visibility][fog]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    fixture.MakeUnit(faction, 8, 8, {"test_chassis"});

    REQUIRE_FALSE(faction.GetVisibleMap().IsVisible(8, 0));
    ApplyRemoveFog(faction);
    CHECK(faction.IsFogRemoved());
    CHECK(faction.GetVisibleMap().IsVisible(8, 0));
    CHECK(faction.GetVisibleMap().IsVisible(8, 16));

    // Sticky fog removal survives a vision rebuild.
    faction.RebuildVisibility();
    CHECK(faction.GetVisibleMap().IsVisible(8, 0));
}

TEST_CASE("Visibility remove_shroud explores the map on rebuild", "[visibility][fog][shroud]")
{
    actest::FactionFixture fixture;
    VisibilityConfig_t visibility;
    visibility.removeShroud = true;
    // Settings are a Faction constructor dependency, so configure them before minting one.
    fixture.settings.SetVisibility(visibility);

    Faction& faction = fixture.MakeFaction();
    faction.RebuildVisibility();

    CHECK(faction.GetExploredMap().IsExplored(8, 0));
    CHECK(faction.GetExploredMap().IsExplored(8, 16));
}

TEST_CASE("Visibility remove_fog applies to the player faction only", "[visibility][fog]")
{
    actest::FactionFixture fixture;
    VisibilityConfig_t visibility;
    visibility.removeFog = true;
    fixture.settings.SetVisibility(visibility);

    Faction& player = fixture.MakeFaction();
    Faction& ai = fixture.MakeFaction();
    player.RebuildVisibility();
    ai.RebuildVisibility();

    CHECK(player.GetVisibleMap().IsVisible(8, 0));
    CHECK_FALSE(ai.GetVisibleMap().IsVisible(8, 0));
}

TEST_CASE("OnVisibilityChanged toggles fog live without rebuild call sites", "[visibility][fog]")
{
    actest::FactionFixture fixture;
    GameSettings settings;

    auto pMap = std::make_unique<WorldMap>(actest::k_TestMapWidth, actest::k_TestMapHeight, actest::TestMapRules());
    GameState gameState(std::move(pMap), fixture.dataContext, settings, actest::k_TestRngSeed);

    auto pFaction = std::make_unique<Faction>(
        gameState.AllocateFactionId(), true, fixture.factionDefinition,
        fixture.dataContext, gameState.GetWorldMap(), settings, actest::k_TestFactionSeed);
    Faction& player = gameState.AddFaction(std::move(pFaction));
    player.RebuildVisibility();
    REQUIRE_FALSE(player.GetVisibleMap().IsVisible(8, 0));

    VisibilityConfig_t visibility;
    visibility.removeFog = true;
    settings.SetVisibility(visibility);
    CHECK(player.GetVisibleMap().IsVisible(8, 0));

    visibility.removeFog = false;
    settings.SetVisibility(visibility);
    CHECK_FALSE(player.GetVisibleMap().IsVisible(8, 0));
}

TEST_CASE("A vision improvement's sight radius is resolved once, at config load",
          "[visibility][fog]")
{
    // The radius was re-derived per improvement per tile on every rebuild, allocating a vector
    // and running the stat resolver for data that cannot change after load.
    actest::FactionFixture fixture;
    const ImprovementConfig_t& rSensor = fixture.improvements.Get("Sensor");
    CHECK(rSensor.visionRadius == 2);

    // A non-vision improvement resolves to zero rather than to "some radius".
    const ImprovementConfig_t& rFarm = fixture.improvements.Get("Farm");
    CHECK(rFarm.visionRadius == 0);
}

TEST_CASE("Visibility rebuilds are coalesced inside a deferral scope", "[visibility][fog]")
{
    // A rebuild re-reveals from every source, walks every tile, and drives a first-contact
    // sweep over every other faction. Bursts of unit events must not pay for it per event.
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    int rebuilds = 0;
    faction.OnVisibilityRebuilt.Connect([&rebuilds](Faction&) { ++rebuilds; });

    {
        Faction::VisibilityRebuildScope scope = faction.DeferVisibilityRebuild();
        faction.RebuildVisibility();
        faction.RebuildVisibility();
        faction.RebuildVisibility();
        CHECK(rebuilds == 0);
    }
    CHECK(rebuilds == 1);

    // A scope that requested nothing rebuilds nothing.
    {
        Faction::VisibilityRebuildScope scope = faction.DeferVisibilityRebuild();
    }
    CHECK(rebuilds == 1);

    // Outside a scope, a rebuild is immediate.
    faction.RebuildVisibility();
    CHECK(rebuilds == 2);
}

TEST_CASE("Sinking a loaded transport rebuilds visibility once", "[visibility][fog]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    // A transport over open water carrying two land units: neither passenger survives the
    // carrier's loss, so DestroyUnit recurses into both.
    fixture.At(8, 8).SetElevation(-100);
    Unit& rTransport =
        fixture.MakeUnit(faction, 8, 8, {"test_sea_chassis", "test_transport_2"});
    Unit& rFirst = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    Unit& rSecond = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    rFirst.EmbarkInto(rTransport);
    rSecond.EmbarkInto(rTransport);
    REQUIRE(rTransport.GetCargo().size() == 2);

    int rebuilds = 0;
    faction.OnVisibilityRebuilt.Connect([&rebuilds](Faction&) { ++rebuilds; });

    faction.GetUnitManager().DestroyUnit(rTransport);
    CHECK(rebuilds == 1);
}
