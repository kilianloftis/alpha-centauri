// Unit detection channels: Conceal hides a unit until Detect covers every active channel.
// Orthogonal to tile fog — a lit tile can still hide cloaked / fungus-concealed units.

#include "GameFixtures.h"

#include "game/faction/UnitVisibility.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/map/TerritoryMap.h"
#include "game/units/Unit.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

TEST_CASE("Cloaked units are hidden on lit tiles until a cloak detector exists",
          "[visibility][detection][cloak]")
{
    actest::FactionFixture fixture;
    Faction& observer = fixture.MakeFaction();
    Faction& owner = fixture.MakeFaction();

    // Scout lights the tile; cloaked enemy stands on it.
    fixture.MakeUnit(observer, 4, 4, {"test_chassis"}); // vision 1
    Unit& cloaked = fixture.MakeUnit(owner, 5, 4, {"test_chassis", "Cloaking_Device"});
    observer.RebuildVisibility();

    REQUIRE(observer.GetVisibleMap().IsVisible(cloaked.GetTile()));
    CHECK(IsUnitVisibleTo(owner, cloaked, *fixture.ctx));
    CHECK_FALSE(IsUnitVisibleTo(observer, cloaked, *fixture.ctx));
}

TEST_CASE("Fungus grants terrain concealment; Sensor Detect pierces it within radius 2",
          "[visibility][detection][terrain]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& observer = fixture.MakeFaction();

    // Base claims Sensor territory; Sensor at (6,4) — base at (1,4) so base vision
    // does not cover the fungus unit at (8,4).
    fixture.MakeFactionBase(owner, 1, 4);
    fixture.ctx->AddImprovementWithEffects(fixture.At(6, 4), "Sensor");
    owner.RebuildVisibility();

    fixture.At(8, 4).SetHasFungus(true);
    Unit& hidden = fixture.MakeUnit(observer, 8, 4, {"test_chassis"});

    // Owner sees the tile via Sensor vision and Detects terrain within Chebyshev 2.
    REQUIRE(owner.GetVisibleMap().IsVisible(8, 4));
    CHECK(IsUnitVisibleTo(owner, hidden, *fixture.ctx));

    // Scout lights the fungus tile without terrain Detect.
    Faction& scoutFaction = fixture.MakeFaction();
    fixture.MakeUnit(scoutFaction, 7, 4, {"test_chassis"});
    scoutFaction.RebuildVisibility();
    REQUIRE(scoutFaction.GetVisibleMap().IsVisible(8, 4));
    CHECK_FALSE(IsUnitVisibleTo(scoutFaction, hidden, *fixture.ctx));
}

TEST_CASE("Sensor terrain Detect does not reach Chebyshev distance 3",
          "[visibility][detection][terrain]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& other = fixture.MakeFaction();

    // Sensor at (4,4); fungus unit at (7,4) is Chebyshev 3 — outside Detect radius 2.
    fixture.MakeFactionBase(owner, 1, 4);
    fixture.ctx->AddImprovementWithEffects(fixture.At(4, 4), "Sensor");
    // Scout lights (7,4); Sensor vision alone would not reach that far.
    fixture.MakeUnit(owner, 6, 4, {"test_chassis"});
    owner.RebuildVisibility();

    fixture.At(7, 4).SetHasFungus(true);
    Unit& far = fixture.MakeUnit(other, 7, 4, {"test_chassis"});

    REQUIRE(owner.GetVisibleMap().IsVisible(7, 4)); // scout vision
    CHECK_FALSE(IsUnitVisibleTo(owner, far, *fixture.ctx));
}

TEST_CASE("Sensor Detect requires territory ownership",
          "[visibility][detection][territory]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& other = fixture.MakeFaction();

    // No base → Sensor tile unowned → Detect applies to nobody.
    fixture.ctx->AddImprovementWithEffects(fixture.At(4, 4), "Sensor");
    fixture.At(4, 4).SetHasFungus(true);

    fixture.MakeUnit(other, 3, 4, {"test_chassis"});
    Unit& hidden = fixture.MakeUnit(owner, 4, 4, {"test_chassis"});
    other.RebuildVisibility();

    REQUIRE(other.GetVisibleMap().IsVisible(4, 4));
    REQUIRE(fixture.map.GetTerritory().GetOwner(4, 4) == k_NoFactionOwner);
    CHECK_FALSE(IsUnitVisibleTo(other, hidden, *fixture.ctx));
}

TEST_CASE("Sensor vision and terrain Detect follow a territory ownership change",
          "[visibility][fog][detection][territory]")
{
    actest::FactionFixture fixture;
    Faction& a = fixture.MakeFaction();
    Faction& b = fixture.MakeFaction();
    Faction& third = fixture.MakeFaction();

    // A claims Sensor at (5,4). Probe tile (5,6) is Chebyshev 2 from the Sensor but
    // outside both bases' vision-2 squares (A at (0,4), later B at (8,4)).
    fixture.MakeFactionBase(a, 0, 4);
    fixture.ctx->AddImprovementWithEffects(fixture.At(5, 4), "Sensor");
    a.RebuildVisibility();
    b.RebuildVisibility();

    fixture.At(5, 6).SetHasFungus(true);
    Unit& hidden = fixture.MakeUnit(third, 5, 6, {"test_chassis"});

    REQUIRE(fixture.map.GetTerritory().GetOwner(5, 4) == a.GetFactionId());
    REQUIRE(a.GetVisibleMap().IsVisible(5, 6));
    CHECK(IsUnitVisibleTo(a, hidden, *fixture.ctx));
    CHECK_FALSE(b.GetVisibleMap().IsVisible(5, 6));
    CHECK_FALSE(IsUnitVisibleTo(b, hidden, *fixture.ctx));

    // Closer base flips Sensor territory to B; vision and Detect retarget on rebuild.
    fixture.MakeFactionBase(b, 8, 4);
    REQUIRE(fixture.map.GetTerritory().GetOwner(5, 4) == b.GetFactionId());
    a.RebuildVisibility();
    b.RebuildVisibility();

    CHECK(b.GetVisibleMap().IsVisible(5, 6));
    CHECK(IsUnitVisibleTo(b, hidden, *fixture.ctx));
    CHECK_FALSE(a.GetVisibleMap().IsVisible(5, 6));
    CHECK_FALSE(IsUnitVisibleTo(a, hidden, *fixture.ctx));
}

TEST_CASE("Cloaked unit inside Sensor radius stays hidden (no cloak detector)",
          "[visibility][detection][cloak]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& other = fixture.MakeFaction();

    fixture.MakeFactionBase(owner, 1, 4);
    fixture.ctx->AddImprovementWithEffects(fixture.At(6, 4), "Sensor");
    owner.RebuildVisibility();

    Unit& cloaked = fixture.MakeUnit(other, 7, 4, {"test_chassis", "Cloaking_Device"});
    REQUIRE(owner.GetVisibleMap().IsVisible(cloaked.GetTile()));
    CHECK_FALSE(IsUnitVisibleTo(owner, cloaked, *fixture.ctx));
}

TEST_CASE("Contact reveal pierces cloak for the bumping faction only",
          "[visibility][detection][reveal]")
{
    actest::FactionFixture fixture;
    Faction& observer = fixture.MakeFaction();
    Faction& owner = fixture.MakeFaction();
    Faction& bystander = fixture.MakeFaction();

    fixture.MakeUnit(observer, 4, 4, {"test_chassis"});
    Unit& cloaked = fixture.MakeUnit(owner, 5, 4, {"test_chassis", "Cloaking_Device"});
    observer.RebuildVisibility();
    bystander.RebuildVisibility();

    REQUIRE_FALSE(IsUnitVisibleTo(observer, cloaked, *fixture.ctx));
    observer.GetRevealedUnits().Reveal(cloaked);
    CHECK(IsUnitVisibleTo(observer, cloaked, *fixture.ctx));
    CHECK_FALSE(IsUnitVisibleTo(bystander, cloaked, *fixture.ctx));
}

// WorldDisplay / WorldView both gate unit draw & pick on IsUnitVisibleTo — these cases
// are the contract those UI paths rely on for conceal vs contact reveal.
TEST_CASE("IsUnitVisibleTo is the UI gate for concealed vs contact-revealed units",
          "[visibility][detection][reveal][ui]")
{
    actest::FactionFixture fixture;
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    fixture.MakeUnit(player, 4, 4, {"test_chassis"});
    Unit& cloaked = fixture.MakeUnit(enemy, 5, 4, {"test_chassis", "Cloaking_Device"});
    Unit& plain = fixture.MakeUnit(enemy, 4, 5, {"test_chassis"});
    player.RebuildVisibility();

    REQUIRE(player.GetVisibleMap().IsVisible(cloaked.GetTile()));
    REQUIRE(player.GetVisibleMap().IsVisible(plain.GetTile()));

    // Concealed: hidden to the player. Visible hostile: shown.
    CHECK_FALSE(IsUnitVisibleTo(player, cloaked, *fixture.ctx));
    CHECK(IsUnitVisibleTo(player, plain, *fixture.ctx));

    player.GetRevealedUnits().Reveal(cloaked);
    CHECK(IsUnitVisibleTo(player, cloaked, *fixture.ctx));
}

TEST_CASE("FactionUnits Conceal from a cloaking project hides live units",
          "[visibility][detection][cloak][FactionUnits]")
{
    actest::FactionFixture fixture;
    Faction& observer = fixture.MakeFaction();
    Faction& owner = fixture.MakeFaction();

    BaseManager& base = fixture.MakeFactionBase(owner, 1, 4);
    fixture.MakeUnit(observer, 4, 4, {"test_chassis"});
    Unit& subject = fixture.MakeUnit(owner, 5, 4, {"test_chassis"}, &base);
    observer.RebuildVisibility();

    REQUIRE(observer.GetVisibleMap().IsVisible(subject.GetTile()));
    CHECK(IsUnitVisibleTo(observer, subject, *fixture.ctx));

    base.GetBuildingManager().AddBuilding("cloaking_project");
    CHECK_FALSE(IsUnitVisibleTo(observer, subject, *fixture.ctx));
}

TEST_CASE("Conditional Conceal applies only when TargetTileHas is satisfied",
          "[visibility][detection][cloak][condition]")
{
    actest::FactionFixture fixture;
    Faction& observer = fixture.MakeFaction();
    Faction& owner = fixture.MakeFaction();

    fixture.MakeUnit(observer, 4, 4, {"test_chassis"});
    Unit& subject = fixture.MakeUnit(owner, 5, 4, {"test_chassis", "deep_pressure_hull"});
    observer.RebuildVisibility();

    REQUIRE(observer.GetVisibleMap().IsVisible(subject.GetTile()));
    // Clear tile: condition fails → no active Conceal → visible.
    CHECK(IsUnitVisibleTo(observer, subject, *fixture.ctx));

    fixture.At(5, 4).SetHasFungus(true);
    CHECK_FALSE(IsUnitVisibleTo(observer, subject, *fixture.ctx));
}

TEST_CASE("Conditional Detect applies only when TargetTileHas is satisfied",
          "[visibility][detection][cloak][condition]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& other = fixture.MakeFaction();

    // Detector at (6,4) pierces cloak only on River tiles within radius 2.
    fixture.MakeFactionBase(owner, 1, 4);
    fixture.ctx->AddImprovementWithEffects(fixture.At(6, 4), "conditional_cloak_detector");
    fixture.MakeUnit(owner, 6, 4, {"test_chassis"}); // light (7,4) and surrounds
    owner.RebuildVisibility();

    Unit& cloaked = fixture.MakeUnit(other, 7, 4, {"test_chassis", "Cloaking_Device"});
    REQUIRE(owner.GetVisibleMap().IsVisible(7, 4));
    // Cloak active; Detect condition fails off-river → still hidden.
    CHECK_FALSE(IsUnitVisibleTo(owner, cloaked, *fixture.ctx));

    fixture.At(7, 4).SetHasRiver(true);
    // Same unit, same detector: condition now met → cloak pierced.
    CHECK(IsUnitVisibleTo(owner, cloaked, *fixture.ctx));
}

TEST_CASE("Sensor terrain Detect wraps horizontally across the map seam",
          "[visibility][detection][wrap]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& other = fixture.MakeFaction();
    const int width = fixture.map.GetWidth();

    fixture.MakeFactionBase(owner, 4, 4);
    fixture.ctx->AddImprovementWithEffects(fixture.At(0, 4), "Sensor");
    owner.RebuildVisibility();

    fixture.At(width - 1, 4).SetHasFungus(true);
    Unit& hiddenNear = fixture.MakeUnit(other, width - 1, 4, {"test_chassis"});
    fixture.At(width - 3, 4).SetHasFungus(true);
    Unit& hiddenFar = fixture.MakeUnit(other, width - 3, 4, {"test_chassis"});

    REQUIRE(owner.GetVisibleMap().IsVisible(width - 1, 4));
    CHECK(IsUnitVisibleTo(owner, hiddenNear, *fixture.ctx));

    // Light the far tile with a scout so concealment (not fog) is what hides it.
    // Chebyshev 3 across the wrap — Detect radius 2 does not reach.
    fixture.MakeUnit(owner, width - 3, 5, {"test_chassis"});
    owner.RebuildVisibility();
    REQUIRE(owner.GetVisibleMap().IsVisible(width - 3, 4));
    CHECK_FALSE(IsUnitVisibleTo(owner, hiddenFar, *fixture.ctx));
}
