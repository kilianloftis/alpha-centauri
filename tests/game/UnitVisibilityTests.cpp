// Unit detection channels: Conceal hides a unit until Detect covers every active channel.
// Orthogonal to tile fog — a lit tile can still hide cloaked / fungus-concealed units.

#include "GameFixtures.h"

#include "game/faction/UnitVisibility.h"
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
