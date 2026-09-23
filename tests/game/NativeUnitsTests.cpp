#include "GameFixtures.h"

#include "game/IConstructable.h"
#include "game/faction/Military.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/UnitManager.h"
#include "game/faction/UnitVisibility.h"
#include "game/units/EnsureNativeDesign.h"
#include "game/units/NativeDesign.h"
#include "game/units/NativeUnitRegistry.h"
#include "game/units/TransportRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"

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

TEST_CASE("Native unit registry loads the six shipping natives", "[native][gamedata]")
{
    const GameDataContext data = LoadNativeAwareData_();
    REQUIRE(data.nativeUnitRegistry);
    CHECK(data.nativeUnitRegistry->Find("Mind_Worm") != nullptr);
    CHECK(data.nativeUnitRegistry->Find("Isle_of_the_Deep") != nullptr);
    CHECK(data.nativeUnitRegistry->Find("Sea_Lurk") != nullptr);
    CHECK(data.nativeUnitRegistry->Find("Locusts_of_Chiron") != nullptr);
    CHECK(data.nativeUnitRegistry->Find("Spore_Launcher") != nullptr);
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
