#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/base/BaseManager.h"
#include "game/buildings/BuildingManager.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticStatus.h"
#include "game/economy/EconomyManager.h"
#include "game/faction/Faction.h"
#include "game/map/TerritoryMap.h"
#include "game/map/WorldMap.h"
#include "game/movement/EvacuateTerritoryRules.h"
#include "game/production/ProductionRules.h"
#include "game/units/Military.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitManager.h"
#include "game/units/UpgradeCostCalculator.h"
#include "game/units/UpgradeMutations.h"
#include "game/units/UpgradeRules.h"
#include "game/visibility/VisibilityRules.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

const UnitDesign& RegisterDesign_(FactionFixture& rFixture, Faction& rFaction,
                                  const std::vector<std::string>& rComponentIds)
{
    std::vector<UnitSlotConfig_t> slots;
    std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
    int slotIndex = 0;
    for (const std::string& rId : rComponentIds)
    {
        const UnitComponentConfig_t* pComponent = rFixture.unitComponents.Find(rId);
        REQUIRE(pComponent != nullptr);
        UnitSlotConfig_t slot;
        slot.id = "slot_" + std::to_string(slotIndex++);
        slot.displayName = slot.id;
        slot.componentType = pComponent->type;
        slot.required = true;
        assigned[slot.id] = pComponent;
        slots.push_back(slot);
    }
    auto pDesign = std::make_unique<UnitDesign>(slots, assigned);
    const UnitDesign* pRaw = pDesign.get();
    REQUIRE(rFaction.GetMilitary().AddDesign(std::move(pDesign)));
    return *pRaw;
}

Unit& SpawnOnDesign_(FactionFixture& rFixture, Faction& rFaction, const UnitDesign& rDesign,
                     int x, int y)
{
    return rFaction.GetUnitManager().CreateUnit(rFixture.state.AllocateUnitId(), rDesign,
                                                rFixture.map.GetUnitPositions(),
                                                rFixture.At(x, y), UnitOrigin::Gift(nullptr));
}

} // namespace

TEST_CASE("upgrade_class sets must match", "[unit][upgrade]")
{
    FactionFixture fixture;
    Faction& rFaction = fixture.MakeFaction();

    const UnitDesign& rInfantry = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_weak_weapon", "test_armor"});
    const UnitDesign& rBetterGun = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_weapon", "test_armor"});
    const UnitDesign& rOtherChassis = RegisterDesign_(
        fixture, rFaction, {"test_slow_chassis", "test_weapon", "test_armor"});
    const UnitDesign& rArtillery = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_weapon", "test_armor", "bombard"});

    CHECK(IsValidUpgrade(rFaction, rInfantry, rBetterGun));
    CHECK_FALSE(IsValidUpgrade(rFaction, rInfantry, rOtherChassis));
    CHECK_FALSE(IsValidUpgrade(rFaction, rInfantry, rArtillery));
    CHECK(UpgradeTargets(rFaction, rInfantry).size() == 1);
    CHECK(UpgradeTargets(rFaction, rInfantry).front() == &rBetterGun);
}

TEST_CASE("Non-combat weapons only upgrade to the same weapon id", "[unit][upgrade]")
{
    FactionFixture fixture;
    Faction& rFaction = fixture.MakeFaction();

    const UnitDesign& rFormer = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_terraformer", "test_weak_armor"});
    const UnitDesign& rSameFormer = RegisterDesign_(
        fixture, rFaction,
        {"test_chassis", "test_terraformer", "test_armor"});
    const UnitDesign& rOtherModule = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_colony_pod", "test_weak_armor"});

    CHECK(IsValidUpgrade(rFaction, rFormer, rSameFormer));
    CHECK_FALSE(IsValidUpgrade(rFaction, rFormer, rOtherModule));
}

TEST_CASE("Weaker armor needs a strictly better combat weapon", "[unit][upgrade]")
{
    FactionFixture fixture;
    Faction& rFaction = fixture.MakeFaction();

    const UnitDesign& rArmored = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_weak_weapon", "test_armor"});
    const UnitDesign& rGlassCannon = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_weapon", "test_medium_armor"});
    const UnitDesign& rNoArmor = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_weapon", "test_weak_armor"});
    const UnitDesign& rWorse = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_weak_weapon", "test_medium_armor"});

    CHECK(IsValidUpgrade(rFaction, rArmored, rGlassCannon));
    CHECK_FALSE(IsValidUpgrade(rFaction, rArmored, rNoArmor));
    CHECK_FALSE(IsValidUpgrade(rFaction, rArmored, rWorse));
}

TEST_CASE("TryUpgradeUnit spends energy and rebinds the design", "[unit][upgrade]")
{
    FactionFixture fixture;
    Faction& rFaction = fixture.MakeFaction();

    const UnitDesign& rOld = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_weak_weapon", "test_costly_armor"});
    const UnitDesign& rNew = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_costly_weapon", "test_costly_armor"});
    rFaction.GetMilitary().RecordBuiltComponents(rOld);
    rFaction.GetMilitary().RecordBuiltComponents(rNew);

    Unit& rUnit = SpawnOnDesign_(fixture, rFaction, rOld, 8, 8);
    REQUIRE(CanOfferUpgrade(rUnit));

    const std::optional<int> quote = QuoteUpgradeEnergy(rFaction, rOld, rNew);
    REQUIRE(quote.has_value());
    REQUIRE(*quote > 0);
    rFaction.GetEconomy().AddEnergy(*quote);

    const int energyBefore = rFaction.GetEconomy().GetEnergy();
    REQUIRE(TryUpgradeUnit(rUnit, rNew));
    CHECK(&rUnit.GetDesign() == &rNew);
    CHECK(rFaction.GetEconomy().GetEnergy() == energyBefore - *quote);
}

TEST_CASE("Insufficient energy leaves the unit unchanged", "[unit][upgrade]")
{
    FactionFixture fixture;
    Faction& rFaction = fixture.MakeFaction();
    // Drain the starting treasury.
    rFaction.GetEconomy().SpendEnergy(rFaction.GetEconomy().GetEnergy());

    const UnitDesign& rOld = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_weak_weapon", "test_costly_armor"});
    const UnitDesign& rNew = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_costly_weapon", "test_costly_armor"});
    rFaction.GetMilitary().RecordBuiltComponents(rOld);
    rFaction.GetMilitary().RecordBuiltComponents(rNew);
    Unit& rUnit = SpawnOnDesign_(fixture, rFaction, rOld, 8, 8);

    const std::optional<int> quote = QuoteUpgradeEnergy(rFaction, rOld, rNew);
    REQUIRE(quote.has_value());
    REQUIRE(*quote > 0);
    CHECK_FALSE(TryUpgradeUnit(rUnit, rNew));
    CHECK(&rUnit.GetDesign() == &rOld);
}

TEST_CASE("Spent moves block upgrade offers", "[unit][upgrade]")
{
    FactionFixture fixture;
    Faction& rFaction = fixture.MakeFaction();
    const UnitDesign& rDesign = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_weapon", "test_armor"});
    Unit& rUnit = SpawnOnDesign_(fixture, rFaction, rDesign, 8, 8);
    rUnit.SpendRemainingMoveFragments();
    CHECK_FALSE(CanOfferUpgrade(rUnit));
}

TEST_CASE("Unprototyped targets use the surcharge cost", "[unit][upgrade]")
{
    FactionFixture fixture;
    Faction& rFaction = fixture.MakeFaction();

    const UnitDesign& rOld = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_weak_weapon", "test_costly_armor"});
    // Unique costly weapon keeps the target unprototyped until its components are recorded.
    const UnitDesign& rNew = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_costly_weapon", "test_costly_armor"});

    REQUIRE(IsPrototype(rFaction, rNew));
    const std::optional<int> unprototypedCost = QuoteUpgradeEnergy(rFaction, rOld, rNew);
    REQUIRE(unprototypedCost.has_value());

    rFaction.GetMilitary().RecordBuiltComponents(rNew);
    REQUIRE_FALSE(IsPrototype(rFaction, rNew));
    const std::optional<int> prototypedCost = QuoteUpgradeEnergy(rFaction, rOld, rNew);
    REQUIRE(prototypedCost.has_value());
    CHECK(*unprototypedCost > *prototypedCost);
}

TEST_CASE("Nano Factory UpgradeCostScale halves upgrade energy", "[unit][upgrade]")
{
    FactionFixture fixture;
    Faction& rFaction = fixture.MakeFaction();

    const UnitDesign& rOld = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_weak_weapon", "test_costly_armor"});
    const UnitDesign& rNew = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_costly_weapon", "test_costly_armor"});
    rFaction.GetMilitary().RecordBuiltComponents(rOld);
    rFaction.GetMilitary().RecordBuiltComponents(rNew);

    const std::optional<int> fullCost = QuoteUpgradeEnergy(rFaction, rOld, rNew);
    REQUIRE(fullCost.has_value());
    REQUIRE(*fullCost > 0);

    BaseManager& rBase = fixture.MakeFactionBase(rFaction, 8, 4);
    rBase.GetBuildingManager().AddBuilding("Nano_Factory");

    const std::optional<int> halvedCost = QuoteUpgradeEnergy(rFaction, rOld, rNew);
    REQUIRE(halvedCost.has_value());
    CHECK(*halvedCost == *fullCost / 2);
}

TEST_CASE("High_Morale presence adjusts XP on upgrade", "[unit][upgrade]")
{
    FactionFixture fixture;
    Faction& rFaction = fixture.MakeFaction();
    rFaction.GetEconomy().AddEnergy(500);

    const UnitDesign& rOld = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_weak_weapon", "test_armor"});
    const UnitDesign& rNew = RegisterDesign_(
        fixture, rFaction, {"test_chassis", "test_weapon", "test_armor", "High_Morale"});
    Unit& rUnit = SpawnOnDesign_(fixture, rFaction, rOld, 8, 8);
    const int xpBefore = rUnit.GetXp();

    REQUIRE(TryUpgradeUnit(rUnit, rNew));
    CHECK(rUnit.GetXp() == xpBefore + 1);
}

TEST_CASE("Vision upgrade establishes first contact", "[unit][upgrade][first-contact]")
{
    FactionFixture fixture;
    FillLand(fixture.map);
    Faction& rScout = fixture.MakeFaction();
    Faction& rOther = fixture.MakeFaction();
    rScout.GetEconomy().AddEnergy(500);

    const UnitDesign& rNearSight = RegisterDesign_(
        fixture, rScout, {"test_chassis", "test_weapon", "test_armor"});
    const UnitDesign& rFarSight = RegisterDesign_(
        fixture, rScout, {"test_chassis", "test_weapon", "test_armor", "extra_vision"});
    Unit& rUnit = SpawnOnDesign_(fixture, rScout, rNearSight, 8, 8);
    fixture.MakeUnit(rOther, 14, 14, {"test_chassis"});

    DiplomacyLedger& rLedger = fixture.state.GetDiplomacyLedger();
    rLedger.SetKnown(rScout.GetFactionId(), rOther.GetFactionId(), false);
    REQUIRE_FALSE(rLedger.AreKnown(rScout.GetFactionId(), rOther.GetFactionId()));
    REQUIRE_FALSE(rScout.GetVisibleMap().IsVisible(fixture.At(14, 14)));

    REQUIRE(TryUpgradeUnit(rUnit, rFarSight));
    CHECK(rScout.GetVisibleMap().IsVisible(fixture.At(14, 14)));
    CHECK(rLedger.AreKnown(rScout.GetFactionId(), rOther.GetFactionId()));
}

TEST_CASE("Dropping Covert on upgrade meets a watching faction",
          "[unit][upgrade][first-contact][covert]")
{
    FactionFixture fixture;
    FillLand(fixture.map);
    Faction& rWatcher = fixture.MakeFaction();
    Faction& rGuest = fixture.MakeFaction();
    rGuest.GetEconomy().AddEnergy(500);

    fixture.MakeFactionBase(rWatcher, 8, 8);
    const UnitDesign& rCovert = RegisterDesign_(
        fixture, rGuest, {"test_chassis", "test_weapon", "test_armor", "covert"});
    const UnitDesign& rOpen = RegisterDesign_(
        fixture, rGuest, {"test_chassis", "test_weapon", "test_armor"});
    Unit& rUnit = SpawnOnDesign_(fixture, rGuest, rCovert, 10, 10);

    DiplomacyLedger& rLedger = fixture.state.GetDiplomacyLedger();
    rLedger.SetKnown(rWatcher.GetFactionId(), rGuest.GetFactionId(), false);
    REQUIRE(rWatcher.GetVisibleMap().IsVisible(rUnit.GetTile()));
    REQUIRE_FALSE(IsOwnerKnownTo(rWatcher.GetFactionId(), rUnit));
    REQUIRE_FALSE(rLedger.AreKnown(rWatcher.GetFactionId(), rGuest.GetFactionId()));

    REQUIRE(TryUpgradeUnit(rUnit, rOpen));
    CHECK(IsOwnerKnownTo(rWatcher.GetFactionId(), rUnit));
    CHECK(rLedger.AreKnown(rWatcher.GetFactionId(), rGuest.GetFactionId()));
}

TEST_CASE("Dropping Covert on foreign territory stays while EvacuateUnitsOnTerritoryChange is off",
          "[unit][upgrade][evacuate][covert]")
{
    FactionFixture fixture;
    FillLand(fixture.map);
    Faction& rGuest = fixture.MakeFaction();
    Faction& rHost = fixture.MakeFaction();
    rGuest.GetEconomy().AddEnergy(500);

    fixture.MakeFactionBase(rGuest, 5, 5);
    fixture.MakeFactionBase(rHost, 11, 11);
    REQUIRE(fixture.map.GetTerritory().GetOwner(10, 10) == rHost.GetFactionId());

    DiplomacyLedger& rLedger = fixture.state.GetDiplomacyLedger();
    rLedger.SetStatus(rGuest.GetFactionId(), rHost.GetFactionId(), DiplomaticStatus_t::Treaty);

    const UnitDesign& rCovert = RegisterDesign_(
        fixture, rGuest, {"test_chassis", "test_weapon", "test_armor", "covert"});
    const UnitDesign& rOpen = RegisterDesign_(
        fixture, rGuest, {"test_chassis", "test_weapon", "test_armor"});
    Unit& rUnit = SpawnOnDesign_(fixture, rGuest, rCovert, 10, 10);
    REQUIRE_FALSE(EvacuateUnitsOnTerritoryChange());
    REQUIRE(MustVacateCurrentTerritory(rUnit) == false);
    REQUIRE(TryUpgradeUnit(rUnit, rOpen));
    // Identity is known and the tile is illegal, but shipping leaves the unit in place.
    CHECK(MustVacateCurrentTerritory(rUnit));
    CHECK(&rUnit.GetTile() == &fixture.At(10, 10));
}
