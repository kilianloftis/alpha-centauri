// Away-from-home drones and garrison police (both tuned by Police SE, separate calculators).

#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/SocialEngineeringManager.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/AwayFromHomeDrones.h"
#include "game/faction/base/population/GarrisonPolice.h"
#include "game/map/TerritoryMap.h"
#include "game/units/Unit.h"

#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

void RebuildTerritory_(FactionFixture& rFixture)
{
    std::vector<const BaseManager*> bases;
    for (const auto& pFaction : rFixture.factions)
    {
        for (const BaseManager& rBase : pFaction->Bases())
        {
            bases.push_back(&rBase);
        }
    }
    rFixture.map.GetTerritory().Rebuild(rFixture.map, bases);
}

void SetPolice_(Faction& rFaction, FactionFixture& rFixture, const char* policyId)
{
    rFaction.GetSocialEngineering().SetActivePolicy(rFixture.socialPolicies().Get(policyId));
}

int CombinedPoliceDroneDelta_(const BaseManager& rBase)
{
    return ComputeAwayFromHomeDrones(rBase) - ComputeGarrisonPoliceSuppression(rBase);
}

} // namespace

TEST_CASE("Combat unit away from home adds drones at home base; former does not",
          "[police][away]")
{
    // Map larger than land claim radius 7 so units can leave owner territory.
    FactionFixture fixture(25, 25);
    Faction& faction = fixture.MakeFaction();
    BaseManager& home = fixture.MakeFactionBase(faction, 4, 4);
    BaseManager& other = fixture.MakeFactionBase(faction, 4, 0);
    RebuildTerritory_(fixture);

    SetPolice_(faction, fixture, "police_minus4_policy");
    REQUIRE(home.GetEffectiveSocialRating(SocialRatingId_t::Police) == -4);

    Unit& combat = fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_weapon"}, &home);
    Unit& former = fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_terraformer"}, &home);
    CHECK(combat.IsCombatUnit());
    CHECK_FALSE(former.IsCombatUnit());
    CHECK(combat.GetStat(StatId_t::AwayFromHomeDrones) == 1);
    CHECK(former.GetStat(StatId_t::AwayFromHomeDrones) == 0);

    fixture.MoveUnit(combat, 20, 20);
    fixture.MoveUnit(former, 21, 20);
    REQUIRE(fixture.map.GetTerritory().GetOwner(combat.GetTile()) != faction.GetFactionId());

    CHECK(ComputeAwayFromHomeDrones(home) == 1);
    CHECK(CombinedPoliceDroneDelta_(home) == 1);
    CHECK(ComputeAwayFromHomeDrones(other) == 0);
}

TEST_CASE("Police -5 doubles away-from-home drones; no flat SE drones", "[police][away]")
{
    FactionFixture fixture(25, 25);
    Faction& faction = fixture.MakeFaction();
    BaseManager& home = fixture.MakeFactionBase(faction, 4, 4);
    RebuildTerritory_(fixture);

    SetPolice_(faction, fixture, "police_minus5_policy");
    REQUIRE(home.GetEffectiveSocialRating(SocialRatingId_t::Police) == -5);
    CHECK(home.GetDroneModifier() == 0);

    Unit& combat = fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_weapon"}, &home);
    fixture.MoveUnit(combat, 20, 20);
    REQUIRE(fixture.map.GetTerritory().GetOwner(combat.GetTile()) != faction.GetFactionId());

    CHECK(ComputeAwayFromHomeDrones(home) == 2);
    CHECK(ComputeGarrisonPoliceSuppression(home) == 0);
    CHECK(CombinedPoliceDroneDelta_(home) == 2);
}

TEST_CASE("Police -3 MaxClamp 1; -2 ignores away units", "[police][away]")
{
    FactionFixture fixture(25, 25);
    Faction& faction = fixture.MakeFaction();
    BaseManager& home = fixture.MakeFactionBase(faction, 4, 4);
    RebuildTerritory_(fixture);

    SetPolice_(faction, fixture, "police_minus3_policy");
    REQUIRE(home.GetEffectiveSocialRating(SocialRatingId_t::Police) == -3);

    for (int i = 0; i < 5; ++i)
    {
        Unit& u = fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_weapon"}, &home);
        fixture.MoveUnit(u, 20 + i, 20);
    }
    CHECK(ComputeAwayFromHomeDrones(home) == 1);

    SetPolice_(faction, fixture, "police_minus2_policy");
    REQUIRE(home.GetEffectiveSocialRating(SocialRatingId_t::Police) == -2);
    CHECK(ComputeAwayFromHomeDrones(home) == 0);
}

TEST_CASE("Police 0: one garrison combat unit suppresses; second does not; adjacent ignored",
          "[police][garrison]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);
    RebuildTerritory_(fixture);

    REQUIRE(base.GetEffectiveSocialRating(SocialRatingId_t::Police) == 0);

    Unit& first = fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_weapon"}, &base);
    CHECK(first.GetStat(StatId_t::PoliceEffectiveness) == 1);
    CHECK(ComputeGarrisonPoliceSuppression(base) == 1);
    CHECK(CombinedPoliceDroneDelta_(base) == -1);

    fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_weapon"}, &base);
    CHECK(ComputeGarrisonPoliceSuppression(base) == 1);

    Unit& adjacent = fixture.MakeUnit(faction, 5, 4, {"test_chassis", "test_weapon"}, &base);
    CHECK(adjacent.GetTile().GetX() == 5);
    CHECK(ComputeGarrisonPoliceSuppression(base) == 1);
}

TEST_CASE("Garrison police fills slots by effectiveness, not spawn order", "[police][garrison]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    REQUIRE(base.GetEffectiveSocialRating(SocialRatingId_t::Police) == 0); // max_police 1

    // Weaker unit first on the tile; NLM is stronger (2). With one slot, prefer NLM.
    Unit& stock = fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_weapon"}, &base);
    Unit& nlm = fixture.MakeUnit(
        faction, 4, 4, {"test_chassis", "test_weapon", "Non_Lethal_Methods"}, &base);
    REQUIRE(stock.GetStat(StatId_t::PoliceEffectiveness) == 1);
    REQUIRE(nlm.GetStat(StatId_t::PoliceEffectiveness) == 2);
    CHECK(ComputeGarrisonPoliceSuppression(base) == 2);
}

TEST_CASE("Police 3 + Non_Lethal_Methods suppresses 3, not 4", "[police][garrison]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    SetPolice_(faction, fixture, "police_plus3_policy");
    REQUIRE(base.GetEffectiveSocialRating(SocialRatingId_t::Police) == 3);

    Unit& stock = fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_weapon"}, &base);
    CHECK(stock.GetStat(StatId_t::PoliceEffectiveness) == 2);
    CHECK(ComputeGarrisonPoliceSuppression(base) == 2);

    faction.GetUnitManager().DestroyUnit(stock);
    Unit& nlm = fixture.MakeUnit(
        faction, 4, 4, {"test_chassis", "test_weapon", "Non_Lethal_Methods"}, &base);
    CHECK(nlm.GetStat(StatId_t::PoliceEffectiveness) == 3);
    CHECK(ComputeGarrisonPoliceSuppression(base) == 3);

    // Over-cap: NLM (3) preferred over stock (2); max_police 3.
    fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_weapon"}, &base);
    fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_weapon"}, &base);
    fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_weapon"}, &base);
    CHECK(ComputeGarrisonPoliceSuppression(base) == 3 + 2 + 2);
}

TEST_CASE("Embarked combat unit on base tile is not police", "[police][garrison]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    Unit& transport =
        fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_transport"}, &base);
    Unit& combat = fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_weapon"}, &base);
    combat.EmbarkInto(transport);
    REQUIRE(combat.IsEmbarked());

    CHECK(ComputeGarrisonPoliceSuppression(base) == 0);
    CHECK(CombinedPoliceDroneDelta_(base) == 0);
}

TEST_CASE("Facility drones are independent of police delta", "[police][drones]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    CHECK(base.GetDroneModifier() == 0);
    CHECK(CombinedPoliceDroneDelta_(base) == 0);

    base.GetBuildingManager().AddBuilding("drone_hall");
    CHECK(base.GetDroneModifier() == 2);
    CHECK(CombinedPoliceDroneDelta_(base) == 0);
}
