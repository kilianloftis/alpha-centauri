#include "GameFixtures.h"

#include "game/IConstructable.h"
#include "game/faction/Military.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/UnitManager.h"
#include "game/faction/UnitVisibility.h"
#include "game/units/EnsureNativeDesign.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/MovementConstants.h"
#include "game/units/NativeDesign.h"
#include "game/units/NativeUnitRegistry.h"
#include "game/units/TransportRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <string>

using namespace ac;
using namespace actest;

namespace
{

GameDataContext LoadNativeAwareData_()
{
    const std::filesystem::path repoRoot =
        std::filesystem::path(AC_TEST_FIXTURES_DIR) / ".." / "..";
    const std::filesystem::path previousDir = std::filesystem::current_path();
    std::filesystem::current_path(repoRoot);
    GameDataContext data = LoadGameData();
    std::filesystem::current_path(previousDir);
    return data;
}

} // namespace

TEST_CASE("Native unit registry loads the shipping natives", "[native][gamedata]")
{
    const GameDataContext data = LoadNativeAwareData_();
    REQUIRE(data.nativeUnitRegistry);
    CHECK(data.nativeUnitRegistry->Find("Mind_Worm") != nullptr);
    CHECK(data.nativeUnitRegistry->Find("Isle_of_the_Deep") != nullptr);
    CHECK(data.nativeUnitRegistry->Find("Sea_Lurk") != nullptr);
    CHECK(data.nativeUnitRegistry->Find("Locusts_of_Chiron") != nullptr);
    CHECK(data.nativeUnitRegistry->Find("Spore_Launcher") != nullptr);
    CHECK(data.nativeUnitRegistry->Find("Fungal_Tower") != nullptr);
    CHECK(data.nativeUnitRegistry->Find("Alien_Artifact") != nullptr);
}

TEST_CASE("EnsureNativeDesign fields Mind Worm as land psi combat", "[native]")
{
    FactionFixture fixture;
    // Point the fixture context at shipping natives for EnsureNativeDesign.
    fixture.dataContext.nativeUnitRegistry = std::make_unique<NativeUnitRegistry>();
    const std::filesystem::path repoRoot =
        std::filesystem::path(AC_TEST_FIXTURES_DIR) / ".." / "..";
    fixture.dataContext.nativeUnitRegistry->Load(
        (repoRoot / "config" / "native_units.json").string());

    Faction& rFaction = fixture.MakeFaction();
    const NativeDesign* pDesign = EnsureNativeDesign(rFaction, fixture.dataContext, "Mind_Worm");
    REQUIRE(pDesign);
    CHECK(pDesign->GetDomain() == UnitDomain_t::Land);
    CHECK(pDesign->GetMovementPoints() == 1);
    CHECK(pDesign->IsCombatUnit());
    CHECK(ResolveFlag(*pDesign, RuleFlagId_t::ForcesPsiCombat));
    CHECK_FALSE(pDesign->UsesFuel());
    CHECK_FALSE(pDesign->HasComponent("Mind_Worm"));

    Unit& rUnit = rFaction.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, *pDesign, fixture.map.GetUnitPositions(), fixture.At(2, 2));
    CHECK(rUnit.GetDomain() == UnitDomain_t::Land);
    CHECK(ResolveStat(rUnit, StatId_t::Movement) == 1);

    const IConstructable* pItem = dynamic_cast<const IConstructable*>(
        rFaction.GetMilitary().GetDesign("Mind_Worm"));
    REQUIRE(pItem);
    CHECK(pItem->GetConstructableKind() == ConstructableKind_t::Unit);
    CHECK(pItem->GetName() == "Mind Worm");
}

TEST_CASE("Locusts of Chiron are air without fuel", "[native]")
{
    FactionFixture fixture;
    fixture.dataContext.nativeUnitRegistry = std::make_unique<NativeUnitRegistry>();
    const std::filesystem::path repoRoot =
        std::filesystem::path(AC_TEST_FIXTURES_DIR) / ".." / "..";
    fixture.dataContext.nativeUnitRegistry->Load(
        (repoRoot / "config" / "native_units.json").string());

    Faction& rFaction = fixture.MakeFaction();
    const NativeDesign* pDesign =
        EnsureNativeDesign(rFaction, fixture.dataContext, "Locusts_of_Chiron");
    REQUIRE(pDesign);
    CHECK(pDesign->GetDomain() == UnitDomain_t::Air);
    CHECK(pDesign->GetMovementPoints() == 8);
    CHECK_FALSE(pDesign->UsesFuel());
    CHECK(pDesign->MaxFuel() == 0);
}

TEST_CASE("Mind Worms and Spore Launchers treat fungus as roads", "[native][movement]")
{
    FactionFixture fixture;
    fixture.dataContext.nativeUnitRegistry = std::make_unique<NativeUnitRegistry>();
    const std::filesystem::path repoRoot =
        std::filesystem::path(AC_TEST_FIXTURES_DIR) / ".." / "..";
    fixture.dataContext.nativeUnitRegistry->Load(
        (repoRoot / "config" / "native_units.json").string());

    Faction& rFaction = fixture.MakeFaction();
    const NativeDesign* pWorm =
        EnsureNativeDesign(rFaction, fixture.dataContext, "Mind_Worm");
    const NativeDesign* pSpore =
        EnsureNativeDesign(rFaction, fixture.dataContext, "Spore_Launcher");
    REQUIRE(pWorm);
    REQUIRE(pSpore);

    Tile& rFungus = fixture.At(5, 4);
    rFungus.SetElevation(100);
    rFungus.SetHasFungus(true);
    Tile& rRockyFungus = fixture.At(6, 4);
    rRockyFungus.SetElevation(100);
    rRockyFungus.SetRockiness(Rockiness_t::Rocky);
    rRockyFungus.SetHasFungus(true);

    fixture.At(4, 4).SetElevation(100);
    fixture.At(4, 5).SetElevation(100);
    Unit& rWorm = rFaction.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, *pWorm, fixture.map.GetUnitPositions(), fixture.At(4, 4));
    Unit& rSpore = rFaction.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, *pSpore, fixture.map.GetUnitPositions(), fixture.At(4, 5));

    const MoveCostCalculator calc(fixture.improvements);
    constexpr int k_point = MovementConstants_t::k_moveFragmentsPerPoint;
    for (const Unit* pUnit : {&rWorm, &rSpore})
    {
        const auto costs = calc.ForUnit(*pUnit, fixture.map);
        const EntryTerms_t fungusTerms = costs.EntryTerms(rFungus);
        CHECK(fungusTerms.costFragments == k_point / 3);
        CHECK_FALSE(fungusTerms.bRequiresFullCost);
        CHECK_FALSE(fungusTerms.bEndsTurn);

        const EntryTerms_t rockyTerms = costs.EntryTerms(rRockyFungus);
        CHECK(rockyTerms.costFragments == k_point / 3);
        CHECK_FALSE(rockyTerms.bRequiresFullCost);
        CHECK_FALSE(rockyTerms.bEndsTurn);
    }
}

TEST_CASE("Isle of the Deep and Sea Lurk treat fungus as a normal tile", "[native][movement]")
{
    FactionFixture fixture;
    fixture.dataContext.nativeUnitRegistry = std::make_unique<NativeUnitRegistry>();
    const std::filesystem::path repoRoot =
        std::filesystem::path(AC_TEST_FIXTURES_DIR) / ".." / "..";
    fixture.dataContext.nativeUnitRegistry->Load(
        (repoRoot / "config" / "native_units.json").string());

    Faction& rFaction = fixture.MakeFaction();
    const NativeDesign* pIsle =
        EnsureNativeDesign(rFaction, fixture.dataContext, "Isle_of_the_Deep");
    const NativeDesign* pLurk =
        EnsureNativeDesign(rFaction, fixture.dataContext, "Sea_Lurk");
    REQUIRE(pIsle);
    REQUIRE(pLurk);

    Tile& rOpenSea = fixture.At(6, 4);
    rOpenSea.SetElevation(-100);
    Tile& rSeaFungus = fixture.At(7, 4);
    rSeaFungus.SetElevation(-100);
    rSeaFungus.SetHasFungus(true);

    fixture.At(5, 4).SetElevation(-100);
    fixture.At(5, 5).SetElevation(-100);
    fixture.At(5, 6).SetElevation(-100);
    Unit& rIsle = rFaction.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, *pIsle, fixture.map.GetUnitPositions(), fixture.At(5, 4));
    Unit& rLurk = rFaction.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, *pLurk, fixture.map.GetUnitPositions(), fixture.At(5, 5));
    Unit& rShip = fixture.MakeUnit(rFaction, 5, 6, {"test_sea_chassis"});

    const MoveCostCalculator calc(fixture.improvements);
    constexpr int k_point = MovementConstants_t::k_moveFragmentsPerPoint;
    const EntryTerms_t shipTerms = calc.ForUnit(rShip, fixture.map).EntryTerms(rSeaFungus);
    CHECK(shipTerms.costFragments == 3 * k_point);
    CHECK(shipTerms.bRequiresFullCost);
    CHECK(shipTerms.bEndsTurn);

    for (const Unit* pUnit : {&rIsle, &rLurk})
    {
        const auto costs = calc.ForUnit(*pUnit, fixture.map);
        const EntryTerms_t openTerms = costs.EntryTerms(rOpenSea);
        const EntryTerms_t fungusTerms = costs.EntryTerms(rSeaFungus);
        CHECK(fungusTerms.costFragments == openTerms.costFragments);
        CHECK(fungusTerms.costFragments == k_point);
        CHECK_FALSE(fungusTerms.bRequiresFullCost);
        CHECK_FALSE(fungusTerms.bEndsTurn);
    }
}

TEST_CASE("Fungal Tower is immobile land psi with 50 percent defense", "[native]")
{
    FactionFixture fixture;
    fixture.dataContext.nativeUnitRegistry = std::make_unique<NativeUnitRegistry>();
    const std::filesystem::path repoRoot =
        std::filesystem::path(AC_TEST_FIXTURES_DIR) / ".." / "..";
    fixture.dataContext.nativeUnitRegistry->Load(
        (repoRoot / "config" / "native_units.json").string());

    Faction& rFaction = fixture.MakeFaction();
    const NativeDesign* pDesign =
        EnsureNativeDesign(rFaction, fixture.dataContext, "Fungal_Tower");
    REQUIRE(pDesign);
    CHECK(pDesign->GetDomain() == UnitDomain_t::Land);
    CHECK(pDesign->GetMovementPoints() == 0);
    CHECK(pDesign->IsCombatUnit());
    CHECK(ResolveFlag(*pDesign, RuleFlagId_t::ForcesPsiCombat));
    CHECK(ResolveFlag(*pDesign, RuleFlagId_t::VisibleInFog));

    Unit& rTower = rFaction.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, *pDesign, fixture.map.GetUnitPositions(), fixture.At(2, 2));
    CHECK(ResolveStat(rTower, StatId_t::Movement) == 0);
    CHECK(ResolveMultiplicativeStat(rTower, StatId_t::Defense, 1.0) == Catch::Approx(1.5));
}

TEST_CASE("Fungal Tower stays visible in fog once its tile is explored", "[native][visibility]")
{
    FactionFixture fixture;
    fixture.dataContext.nativeUnitRegistry = std::make_unique<NativeUnitRegistry>();
    const std::filesystem::path repoRoot =
        std::filesystem::path(AC_TEST_FIXTURES_DIR) / ".." / "..";
    fixture.dataContext.nativeUnitRegistry->Load(
        (repoRoot / "config" / "native_units.json").string());

    Faction& rOwner = fixture.MakeFaction();
    Faction& rObserver = fixture.MakeFaction();
    const NativeDesign* pTowerDesign =
        EnsureNativeDesign(rOwner, fixture.dataContext, "Fungal_Tower");
    const NativeDesign* pWormDesign =
        EnsureNativeDesign(rOwner, fixture.dataContext, "Mind_Worm");
    REQUIRE(pTowerDesign);
    REQUIRE(pWormDesign);

    Tile& rTowerTile = fixture.At(4, 4);
    Tile& rWormTile = fixture.At(4, 5);
    Unit& rTower = rOwner.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, *pTowerDesign, fixture.map.GetUnitPositions(), rTowerTile);
    Unit& rWorm = rOwner.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, *pWormDesign, fixture.map.GetUnitPositions(), rWormTile);

    rObserver.RebuildVisibility();
    REQUIRE_FALSE(rObserver.GetExploredMap().IsExplored(rTowerTile));
    REQUIRE_FALSE(rObserver.GetVisibleMap().IsVisible(rTowerTile));
    CHECK_FALSE(IsUnitVisibleTo(rObserver, rTower, *fixture.ctx));

    rObserver.GetExploredMap().Mark(rTowerTile);
    rObserver.GetExploredMap().Mark(rWormTile);
    rObserver.RebuildVisibility();
    REQUIRE(rObserver.GetExploredMap().IsExplored(rTowerTile));
    REQUIRE_FALSE(rObserver.GetVisibleMap().IsVisible(rTowerTile));
    REQUIRE_FALSE(rObserver.GetVisibleMap().IsVisible(rWormTile));
    CHECK(IsUnitVisibleTo(rObserver, rTower, *fixture.ctx));
    CHECK_FALSE(IsUnitVisibleTo(rObserver, rWorm, *fixture.ctx));
}

TEST_CASE("Alien Artifact is non-combat", "[native]")
{
    FactionFixture fixture;
    fixture.dataContext.nativeUnitRegistry = std::make_unique<NativeUnitRegistry>();
    const std::filesystem::path repoRoot =
        std::filesystem::path(AC_TEST_FIXTURES_DIR) / ".." / "..";
    fixture.dataContext.nativeUnitRegistry->Load(
        (repoRoot / "config" / "native_units.json").string());

    Faction& rFaction = fixture.MakeFaction();
    const NativeDesign* pDesign =
        EnsureNativeDesign(rFaction, fixture.dataContext, "Alien_Artifact");
    REQUIRE(pDesign);
    CHECK_FALSE(pDesign->IsCombatUnit());
    CHECK_FALSE(ResolveFlag(*pDesign, RuleFlagId_t::ForcesPsiCombat));
}

TEST_CASE("Isle of the Deep cargo capacity scales with IntrinsicXp", "[native][cargo]")
{
    FactionFixture fixture;
    fixture.dataContext.nativeUnitRegistry = std::make_unique<NativeUnitRegistry>();
    const std::filesystem::path repoRoot =
        std::filesystem::path(AC_TEST_FIXTURES_DIR) / ".." / "..";
    fixture.dataContext.nativeUnitRegistry->Load(
        (repoRoot / "config" / "native_units.json").string());

    Faction& rFaction = fixture.MakeFaction();
    const NativeDesign* pDesign =
        EnsureNativeDesign(rFaction, fixture.dataContext, "Isle_of_the_Deep");
    REQUIRE(pDesign);
    CHECK(pDesign->GetDomain() == UnitDomain_t::Sea);
    // IDesign-only resolve drops IntrinsicXp → capacity 0.
    CHECK(ResolveStat(*pDesign, StatId_t::CargoCapacity) == 0);

    fixture.At(3, 3).SetElevation(-100);
    Unit& rIsle = rFaction.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, *pDesign, fixture.map.GetUnitPositions(), fixture.At(3, 3));

    rIsle.SetXp(1);
    CHECK(ResolveStat(rIsle, StatId_t::CargoCapacity) == 1);
    CHECK(FreeCargoSlots(rIsle) == 1);

    rIsle.SetXp(3);
    CHECK(ResolveStat(rIsle, StatId_t::CargoCapacity) == 3);
    CHECK(FreeCargoSlots(rIsle) == 3);

    rIsle.SetXp(6);
    CHECK(ResolveStat(rIsle, StatId_t::CargoCapacity) == 6);
}

TEST_CASE("Sea Lurk is concealed on Water via deep_pressure", "[native][visibility]")
{
    FactionFixture fixture;
    fixture.dataContext.nativeUnitRegistry = std::make_unique<NativeUnitRegistry>();
    const std::filesystem::path repoRoot =
        std::filesystem::path(AC_TEST_FIXTURES_DIR) / ".." / "..";
    fixture.dataContext.nativeUnitRegistry->Load(
        (repoRoot / "config" / "native_units.json").string());

    Faction& observer = fixture.MakeFaction();
    Faction& owner = fixture.MakeFaction();
    fixture.MakeUnit(observer, 4, 4, {"test_chassis"});

    fixture.At(5, 4).SetElevation(-100);
    const NativeDesign* pDesign = EnsureNativeDesign(owner, fixture.dataContext, "Sea_Lurk");
    REQUIRE(pDesign);
    Unit& subject = owner.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, *pDesign, fixture.map.GetUnitPositions(), fixture.At(5, 4));
    observer.RebuildVisibility();

    REQUIRE(observer.GetVisibleMap().IsVisible(subject.GetTile()));
    CHECK_FALSE(IsUnitVisibleTo(observer, subject, *fixture.ctx));

    fixture.At(5, 4).SetElevation(100);
    CHECK(IsUnitVisibleTo(observer, subject, *fixture.ctx));
}
