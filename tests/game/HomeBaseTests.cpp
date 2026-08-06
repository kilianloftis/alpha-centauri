// Home-base state model: each BaseManager owns a HomeBaseIndex; units hold HomeBaseClaim
// handles. Unlike WorkedTileClaim there is no exclusivity — many units may share one home.
// Destroying the base orphans outstanding claims so units never keep a dangling pointer.

#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/HomeBaseIndex.h"
#include "game/units/Unit.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using namespace actest;

TEST_CASE("Unit creation registers in the home base index", "[unit][home]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    Unit& unit = fixture.MakeUnit(faction, 5, 4, {"test_chassis"}, &base);

    CHECK(unit.GetHomeBase() == &base);
    REQUIRE(base.GetHomeUnits().GetUnits().size() == 1);
    CHECK(base.GetHomeUnits().GetUnits().front() == &unit);
}

TEST_CASE("Multiple units may share one home base", "[unit][home]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    Unit& a = fixture.MakeUnit(faction, 5, 4, {"test_chassis"}, &base);
    Unit& b = fixture.MakeUnit(faction, 6, 4, {"test_chassis"}, &base);

    CHECK(a.GetHomeBase() == &base);
    CHECK(b.GetHomeBase() == &base);
    CHECK(base.GetHomeUnits().GetUnits().size() == 2);
}

TEST_CASE("Destroying a unit releases its home-base claim", "[unit][home]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    Unit& unit = fixture.MakeUnit(faction, 5, 4, {"test_chassis"}, &base);
    REQUIRE(base.GetHomeUnits().GetUnits().size() == 1);

    faction.GetUnitManager().DestroyUnit(unit);
    CHECK(base.GetHomeUnits().GetUnits().empty());
}

TEST_CASE("SetHomeBase transfers the claim between bases", "[unit][home]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& baseA = fixture.MakeFactionBase(faction, 2, 2);
    BaseManager& baseB = fixture.MakeFactionBase(faction, 6, 6);

    Unit& unit = fixture.MakeUnit(faction, 3, 2, {"test_chassis"}, &baseA);
    REQUIRE(baseA.GetHomeUnits().GetUnits().size() == 1);
    REQUIRE(baseB.GetHomeUnits().GetUnits().empty());

    unit.SetHomeBase(&baseB);
    CHECK(unit.GetHomeBase() == &baseB);
    CHECK(baseA.GetHomeUnits().GetUnits().empty());
    REQUIRE(baseB.GetHomeUnits().GetUnits().size() == 1);
    CHECK(baseB.GetHomeUnits().GetUnits().front() == &unit);

    unit.SetHomeBase(nullptr);
    CHECK(unit.GetHomeBase() == nullptr);
    CHECK(baseB.GetHomeUnits().GetUnits().empty());
}

TEST_CASE("Destroying a base orphans home-base claims", "[unit][home]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);
    const BaseId_t baseId = base.GetBaseId();

    Unit& unit = fixture.MakeUnit(faction, 5, 4, {"test_chassis"}, &base);
    REQUIRE(unit.GetHomeBase() == &base);

    REQUIRE(faction.ExtractBase(baseId).has_value());
    CHECK(unit.GetHomeBase() == nullptr);
}

TEST_CASE("Transferring a base drops the previous owner's home claims", "[unit][home][transfer]")
{
    FactionFixture fixture;
    Faction& giver = fixture.MakeFaction();
    Faction& receiver = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(giver, 4, 4);

    Unit& unit = fixture.MakeUnit(giver, 5, 4, {"test_chassis"}, &base);
    REQUIRE(unit.GetHomeBase() == &base);

    // Ownership change ≠ destroy — the HomeBaseIndex moves with the BaseManager object rather
    // than the faction — but a claim held by a unit the new owner does not own is a *foreign*
    // claim, and the transfer protocol treats those as invalid. Leaving it would let the captor
    // collect the loser's supply-crawler yield (ResourceManager::ComputeWorked_ walks the home
    // index with no faction check) and list the loser's units in its own SupportDisplay.
    giver.TransferBaseTo(base.GetBaseId(), receiver);

    CHECK(&base.GetFaction() == &receiver);
    CHECK(&unit.GetFaction() == &giver);
    // The unit itself survives untouched; only the home link is severed.
    CHECK(unit.GetHomeBase() == nullptr);
    CHECK(base.GetHomeUnits().GetUnits().empty());
}

TEST_CASE("Transferring a base keeps home claims held by the receiver's own units",
          "[unit][home][transfer]")
{
    FactionFixture fixture;
    Faction& giver = fixture.MakeFaction();
    Faction& receiver = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(giver, 4, 4);
    BaseManager* pBaseAddress = &base;

    // A receiver-owned unit already homed at the base being handed over: not foreign, so the
    // claim must survive the transfer (the foreign-claim rule is about ownership, not about
    // transfer clearing the index wholesale).
    Unit& receiverUnit = fixture.MakeUnit(receiver, 5, 4, {"test_chassis"}, nullptr);
    receiverUnit.SetHomeBase(&base);
    REQUIRE(receiverUnit.GetHomeBase() == &base);

    giver.TransferBaseTo(base.GetBaseId(), receiver);

    CHECK(&base.GetFaction() == &receiver);
    CHECK(receiverUnit.GetHomeBase() == pBaseAddress);
    REQUIRE(base.GetHomeUnits().GetUnits().size() == 1);
    CHECK(base.GetHomeUnits().GetUnits().front() == &receiverUnit);
}
