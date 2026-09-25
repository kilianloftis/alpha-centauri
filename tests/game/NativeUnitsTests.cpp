#include "GameFixtures.h"

#include "game/IConstructable.h"
#include "game/faction/Military.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/UnitManager.h"
#include "game/faction/UnitVisibility.h"
#include "game/units/EnsureNativeDesign.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/NativeDesign.h"
#include "game/units/NativeUnitConfigParser.h"
#include "game/units/NativeUnitRegistry.h"
#include "game/units/TransportRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"

#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <memory>
#include <string>

using namespace ac;
using namespace actest;

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
    CHECK(pDesign->IsCombatUnit());
    CHECK(ResolveFlag(*pDesign, RuleFlagId_t::ForcesPsiCombat));
    CHECK_FALSE(pDesign->UsesFuel());
    CHECK_FALSE(pDesign->HasComponent("Mind_Worm"));

    Unit& rUnit = rFaction.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, *pDesign, fixture.map.GetUnitPositions(), fixture.At(2, 2));
    CHECK(rUnit.GetDomain() == UnitDomain_t::Land);

    const IConstructable* pItem = dynamic_cast<const IConstructable*>(
        rFaction.GetMilitary().GetDesign("Mind_Worm"));
    REQUIRE(pItem);
    CHECK(pItem->GetConstructableKind() == ConstructableKind_t::Unit);
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
    CHECK_FALSE(pDesign->UsesFuel());
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

    Tile& rRoad = fixture.At(7, 4);
    rRoad.SetElevation(100);
    rRoad.AddImprovement(fixture.improvements.Get("Road"));

    fixture.At(4, 4).SetElevation(100);
    fixture.At(4, 5).SetElevation(100);
    Unit& rWorm = rFaction.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, *pWorm, fixture.map.GetUnitPositions(), fixture.At(4, 4));
    Unit& rSpore = rFaction.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, *pSpore, fixture.map.GetUnitPositions(), fixture.At(4, 5));

    const MoveCostCalculator calc(fixture.improvements);
    for (const Unit* pUnit : {&rWorm, &rSpore})
    {
        const auto costs = calc.ForUnit(*pUnit, fixture.map);
        const EntryTerms_t roadTerms = costs.EntryTerms(rRoad);
        const EntryTerms_t fungusTerms = costs.EntryTerms(rFungus);
        CHECK(fungusTerms.costFragments == roadTerms.costFragments);
        CHECK_FALSE(fungusTerms.bRequiresFullCost);
        CHECK_FALSE(fungusTerms.bEndsTurn);

        const EntryTerms_t rockyTerms = costs.EntryTerms(rRockyFungus);
        CHECK(rockyTerms.costFragments == fungusTerms.costFragments);
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
    const EntryTerms_t shipOpen = calc.ForUnit(rShip, fixture.map).EntryTerms(rOpenSea);
    const EntryTerms_t shipFungus = calc.ForUnit(rShip, fixture.map).EntryTerms(rSeaFungus);
    CHECK(shipFungus.costFragments > shipOpen.costFragments);

    for (const Unit* pUnit : {&rIsle, &rLurk})
    {
        const auto costs = calc.ForUnit(*pUnit, fixture.map);
        const EntryTerms_t openTerms = costs.EntryTerms(rOpenSea);
        const EntryTerms_t fungusTerms = costs.EntryTerms(rSeaFungus);
        CHECK(fungusTerms.costFragments == openTerms.costFragments);
        CHECK_FALSE(fungusTerms.bRequiresFullCost);
        CHECK_FALSE(fungusTerms.bEndsTurn);
    }
}

TEST_CASE("Fungal Tower is land psi combat visible in fog", "[native]")
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
    CHECK(pDesign->IsCombatUnit());
    CHECK(ResolveFlag(*pDesign, RuleFlagId_t::ForcesPsiCombat));
    CHECK(ResolveFlag(*pDesign, RuleFlagId_t::VisibleInFog));
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

    fixture.At(3, 3).SetElevation(-100);
    Unit& rIsle = rFaction.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, *pDesign, fixture.map.GetUnitPositions(), fixture.At(3, 3));

    rIsle.SetXp(1);
    const int atOne = ResolveStat(rIsle, StatId_t::CargoCapacity);
    CHECK(FreeCargoSlots(rIsle) == atOne);

    rIsle.SetXp(3);
    const int atThree = ResolveStat(rIsle, StatId_t::CargoCapacity);
    CHECK(atThree > atOne);
    CHECK(FreeCargoSlots(rIsle) == atThree);

    rIsle.SetXp(6);
    const int atSix = ResolveStat(rIsle, StatId_t::CargoCapacity);
    CHECK(atSix > atThree);
    CHECK(FreeCargoSlots(rIsle) == atSix);
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

namespace
{

void LoadShippingNatives_(FactionFixture& rFixture)
{
    rFixture.dataContext.nativeUnitRegistry = std::make_unique<NativeUnitRegistry>();
    const std::filesystem::path repoRoot =
        std::filesystem::path(AC_TEST_FIXTURES_DIR) / ".." / "..";
    rFixture.dataContext.nativeUnitRegistry->Load(
        (repoRoot / "config" / "native_units.json").string());
}

const NativeDesign& RequireNative_(Faction& rFaction, const GameDataContext& rData,
                                   const std::string& rId)
{
    const NativeDesign* pDesign = EnsureNativeDesign(rFaction, rData, rId);
    REQUIRE(pDesign);
    return *pDesign;
}

} // namespace

TEST_CASE("native_life is the RuleFlag on the design", "[native][parser]")
{
    NativeUnitConfigParser parser;
    const nlohmann::json marked = nlohmann::json::parse(R"({
        "id": "Marked",
        "name": "Marked",
        "domain": "land",
        "effects": [
            { "type": "RuleFlag", "scope": "ThisUnit",
              "parameters": { "flag": "native_life" } }
        ]
    })");
    const NativeDesign markedDesign(parser.ParseNativeUnitConfig(marked));
    CHECK(markedDesign.IsNativeLife());

    const nlohmann::json plain = nlohmann::json::parse(R"({
        "id": "Plain",
        "name": "Plain",
        "domain": "land"
    })");
    const NativeDesign plainDesign(parser.ParseNativeUnitConfig(plain));
    CHECK_FALSE(plainDesign.IsNativeLife());
}

TEST_CASE("Command Center and Aerospace Complex do not raise native starting XP",
          "[native][xp]")
{
    FactionFixture fixture;
    LoadShippingNatives_(fixture);
    Faction& rFaction = fixture.MakeFaction();
    BaseManager& rBase = fixture.MakeFactionBase(rFaction, 2, 2);
    rBase.GetBuildingManager().AddBuilding("Command_Center");
    rBase.GetBuildingManager().AddBuilding("Aerospace_Complex");

    const NativeDesign& rWorm = RequireNative_(rFaction, fixture.dataContext, "Mind_Worm");
    const NativeDesign& rLocust =
        RequireNative_(rFaction, fixture.dataContext, "Locusts_of_Chiron");
    CHECK(rWorm.IsNativeLife());
    CHECK(rLocust.IsNativeLife());

    BaseManager& rBare = fixture.MakeFactionBase(rFaction, 7, 7);

    Unit& rWormUnit = rFaction.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, rWorm, fixture.map.GetUnitPositions(), fixture.At(4, 4),
        &rBase, &rBase);
    Unit& rWormBare = rFaction.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, rWorm, fixture.map.GetUnitPositions(), fixture.At(4, 5),
        &rBare, &rBare);
    Unit& rLocustUnit = rFaction.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, rLocust, fixture.map.GetUnitPositions(), fixture.At(5, 4),
        &rBase, &rBase);
    Unit& rLocustBare = rFaction.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, rLocust, fixture.map.GetUnitPositions(), fixture.At(5, 5),
        &rBare, &rBare);
    Unit& rLand = fixture.MakeUnit(rFaction, 6, 4, {"test_chassis"}, &rBase, &rBase);
    Unit& rLandBare = fixture.MakeUnit(rFaction, 6, 5, {"test_chassis"}, &rBare, &rBare);

    CHECK(rWormUnit.GetXp() == rWormBare.GetXp());
    CHECK(rLocustUnit.GetXp() == rLocustBare.GetXp());
    CHECK_FALSE(rLand.GetDesign().IsNativeLife());
    CHECK(rLand.GetXp() > rLandBare.GetXp());
}

TEST_CASE("Centauri Preserve grants +1 starting XP only to native life", "[native][xp]")
{
    NativeUnitConfigParser parser;
    const NativeDesign quietDesign(parser.ParseNativeUnitConfig(nlohmann::json::parse(R"({
        "id": "Quiet_Form",
        "name": "Quiet Form",
        "domain": "land",
        "effects": [
            { "type": "RuleFlag", "scope": "ThisUnit",
              "parameters": { "flag": "native_life" } }
        ]
    })")));
    NativeUnitConfig_t plainConfig;
    plainConfig.id = "Plain_Form";
    plainConfig.name = "Plain Form";
    plainConfig.domain = UnitDomain_t::Land;
    NativeDesign plainDesign(plainConfig);

    FactionFixture fixture;
    LoadShippingNatives_(fixture);
    Faction& rFaction = fixture.MakeFaction();
    BaseManager& rBase = fixture.MakeFactionBase(rFaction, 2, 2);
    rBase.GetBuildingManager().AddBuilding("Centauri_Preserve");

    const NativeDesign& rWorm = RequireNative_(rFaction, fixture.dataContext, "Mind_Worm");

    Unit& rWormUnit = rFaction.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, rWorm, fixture.map.GetUnitPositions(), fixture.At(4, 4),
        &rBase, &rBase);
    Unit& rQuiet = rFaction.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, quietDesign, fixture.map.GetUnitPositions(), fixture.At(5, 4),
        &rBase, &rBase);
    Unit& rPlain = rFaction.GetUnitManager().CreateUnit(
        fixture.nextUnitId++, plainDesign, fixture.map.GetUnitPositions(), fixture.At(6, 4),
        &rBase, &rBase);
    Unit& rLand = fixture.MakeUnit(rFaction, 7, 4, {"test_chassis"}, &rBase, &rBase);

    CHECK(rWorm.IsNativeLife());
    CHECK(ResolveFlag(rWorm, RuleFlagId_t::ForcesPsiCombat));
    CHECK(rWormUnit.GetXp() == 2);
    CHECK(quietDesign.IsNativeLife());
    CHECK_FALSE(ResolveFlag(quietDesign, RuleFlagId_t::ForcesPsiCombat));
    CHECK(rQuiet.GetXp() == 2);
    CHECK_FALSE(plainDesign.IsNativeLife());
    CHECK(rPlain.GetXp() == 1);
    CHECK_FALSE(rLand.GetDesign().IsNativeLife());
    CHECK(rLand.GetXp() == 2);
}

TEST_CASE("a composed unit with native_life takes lifecycle train bonuses", "[native][xp]")
{
    FactionFixture fixture;
    Faction& rFaction = fixture.MakeFaction();
    BaseManager& rCommand = fixture.MakeFactionBase(rFaction, 2, 2);
    BaseManager& rPreserve = fixture.MakeFactionBase(rFaction, 3, 2);
    rCommand.GetBuildingManager().AddBuilding("Command_Center");
    rPreserve.GetBuildingManager().AddBuilding("Centauri_Preserve");

    Unit& rAtCommand =
        fixture.MakeUnit(rFaction, 4, 4, {"native_life_chassis"}, &rCommand, &rCommand);
    Unit& rAtPreserve =
        fixture.MakeUnit(rFaction, 5, 4, {"native_life_chassis"}, &rPreserve, &rPreserve);

    CHECK(rAtCommand.GetDesign().IsNativeLife());
    CHECK(rAtCommand.GetXp() == 2);
    CHECK(rAtPreserve.GetDesign().IsNativeLife());
    CHECK(rAtPreserve.GetXp() == 2);
}
