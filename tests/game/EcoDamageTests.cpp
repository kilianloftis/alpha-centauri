// Eco-damage score: the per-base inputs BaseEcology assembles, the Lua formula the calculator
// evaluates, and the config parser. Score tests swap the fixture formula for one that reports a
// single input, so each case pins exactly the quantity it names.

#include "GameFixtures.h"
#include "TempConfigFile.h"

#include "game/GameState.h"
#include "game/atrocities/AtrocityLedger.h"
#include "game/ecology/EcoDamageCalculator.h"
#include "game/ecology/EcoDamageConfig.h"
#include "game/ecology/EcologyLedger.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/map/Tile.h"
#include "game/units/Unit.h"
#include "lib/LuaRuntime.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <memory>
#include <string>

using namespace ac;
using namespace actest;
using Catch::Matchers::ContainsSubstring;

namespace
{

// One faction and a size-1 base at (4, 4) on the fixture map with its worker idle, so every
// worked tile is one the test assigned.
struct EcoFixture_
{
    FactionFixture fixtures;
    Faction* pFaction = nullptr;
    BaseManager* pBase = nullptr;

    explicit EcoFixture_(std::vector<EffectConfig_t> worldRules = {})
        : fixtures(actest::k_TestMapWidth, actest::k_TestMapHeight, std::move(worldRules))
    {
        pFaction = &fixtures.MakeFaction();
        pBase = &MakeBase(8, 8);
    }

    BaseManager& MakeBase(int x, int y)
    {
        BaseManager& rBase = fixtures.MakeFactionBase(*pFaction, x, y, /*pop*/ 1);
        rBase.GetWorkerAssignments().UnassignAll();
        return rBase;
    }

    // Set before the first score read: the formula is not a memo input.
    void Formula(const char* pFormula)
    {
        fixtures.dataContext.ecoDamageConfig->damageFormula = pFormula;
    }

    void Improve(int x, int y, const char* pId)
    {
        fixtures.ctx->AddOccupantWithEffects(fixtures.At(x, y), pId);
    }

    void Work(BaseManager& rBase, int x, int y)
    {
        REQUIRE(rBase.UserAssignBestAvailableWorker(&fixtures.At(x, y)));
    }

    EcologyLedger& Ecology() { return fixtures.pBindState->GetEcologyLedger(); }
};

EcoDamageConfig_t LoadFixtureConfig_(LuaRuntime& rLua)
{
    return EcoDamageConfigParser{}.ParseConfig(FixturePath("eco_damage.json"),
                                               FixturePath("eco_damage.lua"), rLua);
}

// Inputs that make the result equal the damage factor: techs x eco_scale / 300 == 1.
EcoDamageInputs_t FactorInputs_()
{
    EcoDamageInputs_t inputs;
    inputs.techs = 300;
    inputs.ecoScale = 1.0;
    return inputs;
}

} // namespace

TEST_CASE("A worked borehole contributes 18, an idle one 9", "[ecology][terraform]")
{
    SECTION("idle")
    {
        EcoFixture_ eco;
        eco.Formula("terraform_raw");
        eco.Improve(9, 9, "ThermalBorehole");
        CHECK(eco.pBase->GetEcologicalDamage() == 9);
    }
    SECTION("worked")
    {
        EcoFixture_ eco;
        eco.Formula("terraform_raw");
        eco.Improve(9, 9, "ThermalBorehole");
        eco.Work(*eco.pBase, 9, 9);
        CHECK(eco.pBase->GetEcologicalDamage() == 18);
    }
}

TEST_CASE("Reassigning a worker onto a borehole invalidates the score", "[ecology][memo]")
{
    EcoFixture_ eco;
    eco.Formula("terraform_raw");
    eco.Improve(9, 9, "ThermalBorehole");
    CHECK(eco.pBase->GetEcologicalDamage() == 9);
    eco.Work(*eco.pBase, 9, 9);
    CHECK(eco.pBase->GetEcologicalDamage() == 18);
}

TEST_CASE("A kelp farm contributes 1 whether or not it is worked", "[ecology][terraform]")
{
    EcoFixture_ eco;
    eco.Formula("terraform_raw");
    eco.fixtures.At(9, 9).SetElevation(-100);
    eco.Improve(9, 9, "KelpFarm");
    CHECK(eco.pBase->GetEcologicalDamage() == 1);
    eco.Work(*eco.pBase, 9, 9);
    CHECK(eco.pBase->GetEcologicalDamage() == 1);
}

TEST_CASE("A forest subtracts 1", "[ecology][terraform]")
{
    EcoFixture_ eco;
    eco.Formula("terraform_raw");
    eco.Improve(9, 9, "ThermalBorehole");
    eco.Improve(7, 7, "Forest");
    CHECK(eco.pBase->GetEcologicalDamage() == 8);
}

TEST_CASE("A crawled borehole counts its unworked weight only", "[ecology][terraform][supply]")
{
    SECTION("crawler homed at this base")
    {
        EcoFixture_ eco;
        eco.Formula("terraform_raw");
        eco.Improve(9, 9, "ThermalBorehole");
        Unit& crawler = eco.fixtures.MakeUnit(*eco.pFaction, 9, 9,
                                              {"test_chassis", "test_supply_crawler"},
                                              eco.pBase);
        REQUIRE(crawler.TryStartSupplyCrawl(StatId_t::Minerals));
        CHECK(eco.pBase->GetEcologicalDamage() == 9);
    }
    SECTION("crawler homed at a neighbouring base")
    {
        EcoFixture_ eco;
        eco.Formula("terraform_raw");
        BaseManager& rNeighbour = eco.MakeBase(12, 12);
        eco.Improve(10, 10, "ThermalBorehole");
        Unit& crawler = eco.fixtures.MakeUnit(*eco.pFaction, 10, 10,
                                              {"test_chassis", "test_supply_crawler"},
                                              &rNeighbour);
        REQUIRE(crawler.TryStartSupplyCrawl(StatId_t::Minerals));
        CHECK(eco.pBase->GetEcologicalDamage() == 9);
    }
}

TEST_CASE("Improvements on one tile stack their weights", "[ecology][terraform]")
{
    EcoFixture_ eco;
    eco.Formula("terraform_raw");
    eco.Improve(9, 9, "Road");
    eco.Improve(9, 9, "Mine");
    eco.Work(*eco.pBase, 9, 9);
    CHECK(eco.pBase->GetEcologicalDamage() == 4);

    eco.Improve(9, 9, "MagTube");
    CHECK(eco.pBase->GetEcologicalDamage() == 6);
}

TEST_CASE("Sensors, bunkers and airbases contribute nothing", "[ecology][terraform]")
{
    EcoFixture_ eco;
    eco.Formula("terraform_raw");
    eco.Improve(9, 9, "Sensor");
    eco.Improve(7, 7, "Bunker");
    eco.Improve(7, 9, "Airbase");
    eco.Work(*eco.pBase, 9, 9);
    CHECK(eco.pBase->GetEcologicalDamage() == 0);
}

TEST_CASE("A sea base adds 1 from its own tile", "[ecology][terraform]")
{
    EcoFixture_ eco;
    eco.Formula("terraform_raw");
    CHECK(eco.pBase->GetEcologicalDamage() == 0);

    eco.fixtures.At(8, 4).SetElevation(-100);
    BaseManager& rSeaBase = eco.MakeBase(8, 4);
    CHECK(rSeaBase.GetEcologicalDamage() == 1);
}

TEST_CASE("Tree Farm halves the terraform term and Hybrid Forest zeroes it", "[ecology]")
{
    SECTION("Tree Farm")
    {
        EcoFixture_ eco;
        eco.Formula("math.floor(terraform_scale * 100)");
        eco.pBase->GetBuildingManager().AddBuilding("test_tree_farm");
        CHECK(eco.pBase->GetEcologicalDamage() == 50);
    }
    SECTION("Hybrid Forest")
    {
        EcoFixture_ eco;
        eco.Formula("math.floor(terraform_scale * 100)");
        eco.pBase->GetBuildingManager().AddBuilding("test_hybrid_forest");
        CHECK(eco.pBase->GetEcologicalDamage() == 0);
    }
}

TEST_CASE("Good facilities raise EcoDamageReduction at their base only", "[ecology]")
{
    EcoFixture_ eco;
    eco.Formula("damage_reduction");
    BaseManager& rOther = eco.MakeBase(8, 16);
    eco.pBase->GetBuildingManager().AddBuilding("test_eco_preserve");
    eco.pBase->GetBuildingManager().AddBuilding("test_nanoreplicator");
    CHECK(eco.pBase->GetEcologicalDamage() == 2);
    CHECK(rOther.GetEcologicalDamage() == 0);
}

TEST_CASE("The faction-wide clean-minerals cap applies in full at every base", "[ecology][cap]")
{
    EcoFixture_ eco;
    eco.Formula("clean_minerals + fungal_blooms + clean_mineral_grants");
    BaseManager& rOther = eco.MakeBase(8, 16);
    CHECK(eco.pBase->GetEcologicalDamage() == 16);
    CHECK(rOther.GetEcologicalDamage() == 16);

    // A bloom moves the score without touching any effect pool.
    eco.Ecology().RecordFungalBloom(eco.pFaction->GetFactionId());
    eco.Ecology().GrantCleanMinerals(eco.pFaction->GetFactionId(), 1);
    CHECK(eco.pBase->GetEcologicalDamage() == 18);
    CHECK(rOther.GetEcologicalDamage() == 18);
}

TEST_CASE("Orbital minerals raise production but not the minerals ecology charges",
          "[ecology][minerals]")
{
    EcoFixture_ eco;
    eco.Formula("minerals + mineral_offset");
    BaseManager& rOther = eco.MakeBase(8, 16);
    const int mineralsBefore = eco.pBase->GetMineralProduction();
    const int chargedBefore = eco.pBase->GetEcologicalDamage();
    CHECK(chargedBefore == mineralsBefore);

    // Two copies at the other base: AllOwnerBases reaches this one too.
    rOther.GetBuildingManager().AddBuilding("test_orbital_mining");
    rOther.GetBuildingManager().AddBuilding("test_orbital_mining");
    CHECK(eco.pBase->GetMineralProduction() == mineralsBefore + 2);
    CHECK(eco.pBase->GetEcologicalDamage() == chargedBefore);
}

TEST_CASE("A dirty facility charges ecology through a positive mineral offset",
          "[ecology][minerals]")
{
    EcoFixture_ eco;
    eco.Formula("mineral_offset");
    eco.pBase->GetBuildingManager().AddBuilding("test_dirty_facility");
    CHECK(eco.pBase->GetEcologicalDamage() == 2);
}

TEST_CASE("Virtual minerals sum counted atrocities and the ecology ledger", "[ecology][virtual]")
{
    EcoFixture_ eco;
    eco.Formula("virtual_minerals");
    const FactionId_t perpetrator = eco.pFaction->GetFactionId();
    AtrocityLedger& rAtrocities = eco.fixtures.pBindState->GetAtrocityLedger();
    CHECK(eco.pBase->GetEcologicalDamage() == 0);

    AtrocityRecord_t major;
    major.perpetrator = perpetrator;
    major.severity = AtrocitySeverityId_t::Major;
    major.bCharterInForce = true;
    major.bCounted = true;
    rAtrocities.Record(major);
    // Mid-turn: the ledger revision alone has to invalidate the score.
    CHECK(eco.pBase->GetEcologicalDamage() == 3);

    AtrocityRecord_t simple = major;
    simple.severity = AtrocitySeverityId_t::Simple;
    rAtrocities.Record(simple);
    AtrocityRecord_t excused = major;
    excused.bCharterInForce = false;
    excused.bCounted = false;
    rAtrocities.Record(excused);
    CHECK(eco.pBase->GetEcologicalDamage() == 3);

    eco.Ecology().AddVirtualMinerals(perpetrator, 5);
    CHECK(eco.pBase->GetEcologicalDamage() == 8);
}

TEST_CASE("AddVirtualMinerals is ungated by the Charter and records no atrocity",
          "[ecology][virtual]")
{
    for (const bool bCharterInForce : {true, false})
    {
        std::vector<EffectConfig_t> worldRules;
        if (bCharterInForce)
        {
            EffectConfig_t charter;
            charter.scope = EffectScope_t::WorldGlobal;
            charter.effect = RuleFlagEffect_t{RuleFlagId_t::AtrocitiesForbidden, std::nullopt};
            worldRules.push_back(charter);
        }
        EcoFixture_ eco(worldRules);
        REQUIRE(ResolveFlag(*eco.pFaction, RuleFlagId_t::AtrocitiesForbidden) == bCharterInForce);

        TriggeredEffectConfig_t tectonic;
        tectonic.effect = AddVirtualMineralsEffect_t{5};
        TriggeredEffectContext_t context(*eco.fixtures.pBindState, *eco.pFaction);
        ApplyTriggeredEffects(std::vector<TriggeredEffectConfig_t>{tectonic}, context);

        CHECK(eco.Ecology().VirtualMinerals(eco.pFaction->GetFactionId()) == 5);
        CHECK(eco.fixtures.pBindState->GetAtrocityLedger().Records().empty());
    }
}

TEST_CASE("The score needs the faction bound to a session", "[ecology]")
{
    BaseFixture fixture;
    BaseManager& rBase = fixture.MakeBase(8, 8);
    CHECK_THROWS_AS(rBase.GetEcologicalDamage(), std::logic_error);
}

TEST_CASE("Eco damage formula: a base under its cap scores 0", "[ecology][formula]")
{
    LuaRuntime lua;
    const EcoDamageConfig_t config = LoadFixtureConfig_(lua);
    const EcoDamageCalculator calculator(config, lua);

    EcoDamageInputs_t inputs = FactorInputs_();
    inputs.terraformRaw = 8;
    inputs.minerals = 10;
    inputs.cleanMinerals = 16;
    CHECK(calculator.Calculate(inputs) == 0);
}

TEST_CASE("Eco damage formula: blooms and grants raise the cap", "[ecology][formula]")
{
    LuaRuntime lua;
    const EcoDamageConfig_t config = LoadFixtureConfig_(lua);
    const EcoDamageCalculator calculator(config, lua);

    EcoDamageInputs_t inputs = FactorInputs_();
    inputs.minerals = 20;
    inputs.cleanMinerals = 16;
    CHECK(calculator.Calculate(inputs) == 4);
    inputs.fungalBlooms = 2;
    inputs.cleanMineralGrants = 1;
    CHECK(calculator.Calculate(inputs) == 1);
}

TEST_CASE("Eco damage formula: the worked example scales with difficulty", "[ecology][formula]")
{
    LuaRuntime lua;
    const EcoDamageConfig_t config = LoadFixtureConfig_(lua);
    const EcoDamageCalculator calculator(config, lua);

    // Terraform 18 against a cap of 16 leaves 2, plus 20 minerals: a damage factor of 22.
    EcoDamageInputs_t inputs;
    inputs.terraformRaw = 144;
    inputs.minerals = 20;
    inputs.cleanMinerals = 16;
    inputs.techs = 30;

    inputs.ecoScale = 3.0; // Librarian
    CHECK(calculator.Calculate(inputs) == 6);
    inputs.ecoScale = 5.0; // Transcend
    CHECK(calculator.Calculate(inputs) == 11);
}

TEST_CASE("Eco damage formula: good facilities divide only the mineral term",
          "[ecology][formula]")
{
    LuaRuntime lua;
    const EcoDamageConfig_t config = LoadFixtureConfig_(lua);
    const EcoDamageCalculator calculator(config, lua);

    // Terraform 20 against a cap of 16 leaves 4 undivided; 40 minerals halve to 20.
    EcoDamageInputs_t inputs = FactorInputs_();
    inputs.terraformRaw = 160;
    inputs.minerals = 40;
    inputs.cleanMinerals = 16;
    CHECK(calculator.Calculate(inputs) == 44);
    inputs.damageReduction = 1;
    CHECK(calculator.Calculate(inputs) == 24);
}

TEST_CASE("Eco damage formula: virtual minerals answer to good facilities like real ones",
          "[ecology][formula][virtual]")
{
    LuaRuntime lua;
    const EcoDamageConfig_t config = LoadFixtureConfig_(lua);
    const EcoDamageCalculator calculator(config, lua);

    EcoDamageInputs_t inputs = FactorInputs_();
    inputs.virtualMinerals = 10;
    CHECK(calculator.Calculate(inputs) == 10);
    inputs.damageReduction = 1;
    CHECK(calculator.Calculate(inputs) == 5);
}

TEST_CASE("Eco damage formula: the mineral offset is signed", "[ecology][formula][minerals]")
{
    LuaRuntime lua;
    const EcoDamageConfig_t config = LoadFixtureConfig_(lua);
    const EcoDamageCalculator calculator(config, lua);

    EcoDamageInputs_t inputs = FactorInputs_();
    inputs.minerals = 20;
    CHECK(calculator.Calculate(inputs) == 20);
    inputs.mineralOffset = 2;
    CHECK(calculator.Calculate(inputs) == 22);
    inputs.mineralOffset = -2;
    CHECK(calculator.Calculate(inputs) == 18);
}

TEST_CASE("Eco damage formula: terraform scale applies before the cap", "[ecology][formula]")
{
    LuaRuntime lua;
    const EcoDamageConfig_t config = LoadFixtureConfig_(lua);
    const EcoDamageCalculator calculator(config, lua);

    EcoDamageInputs_t inputs = FactorInputs_();
    inputs.terraformRaw = 160;
    CHECK(calculator.Calculate(inputs) == 20);
    inputs.terraformScale = 0.5;
    CHECK(calculator.Calculate(inputs) == 10);
    inputs.terraformScale = 0.0;
    CHECK(calculator.Calculate(inputs) == 0);
}

TEST_CASE("Eco damage calculator rejects a negative result", "[ecology][formula]")
{
    LuaRuntime lua;
    EcoDamageConfig_t config = LoadFixtureConfig_(lua);
    config.damageFormula = "-1";
    const EcoDamageCalculator calculator(config, lua);
    CHECK_THROWS_WITH(calculator.Calculate(EcoDamageInputs_t{}), ContainsSubstring("negative"));
}

TEST_CASE("EcoDamageConfigParser reads the fixture", "[ecology][parser]")
{
    LuaRuntime lua;
    const EcoDamageConfig_t config = LoadFixtureConfig_(lua);
    CHECK(config.maxChancePercent == 100);
    CHECK(config.effects.size() == 1);
    REQUIRE(config.onPopEffects.size() == 1);
    CHECK(std::holds_alternative<FungalBloomEffect_t>(config.onPopEffects[0].effect));
    CHECK(config.damageFormula == "eco_damage_formula()");
}

TEST_CASE("EcoDamageConfigParser rejects bad configs", "[ecology][parser]")
{
    LuaRuntime lua;
    const auto parse = [&lua](const std::string& rJson) {
        const TempConfigFile file("eco_damage.json", rJson);
        return EcoDamageConfigParser{}.ParseConfig(file.Path(), FixturePath("eco_damage.lua"),
                                                   lua);
    };
    const std::string popOk =
        R"("fungal_pop": { "max_chance_percent": 100, "on_pop_effects": [] })";

    CHECK_THROWS_WITH(parse("{ " + popOk + " }"), ContainsSubstring("effects"));
    CHECK_THROWS_WITH(parse(R"({ "effects": [] })"), ContainsSubstring("fungal_pop"));
    CHECK_THROWS_WITH(parse(R"({ "effects": [], "fungal_pop": { "on_pop_effects": [] } })"),
                      ContainsSubstring("max_chance_percent"));
    CHECK_THROWS_WITH(
        parse(R"({ "effects": [], "fungal_pop": { "max_chance_percent": 100 } })"),
        ContainsSubstring("on_pop_effects"));
    CHECK_THROWS_WITH(parse(R"({ "effects": [], "fungal_pop": { "max_chance_percent": 101,
                                 "on_pop_effects": [] } })"),
                      ContainsSubstring("max_chance_percent"));
    CHECK_THROWS_WITH(parse(R"({ "effects": [], "extra": 1, )" + popOk + " }"),
                      ContainsSubstring("extra"));
    CHECK_THROWS_WITH(parse(R"({ "effects": [], "fungal_pop": { "max_chance_percent": 100,
                                 "on_pop_effects": [ { "type": "StatModifier",
                                   "scope": "FactionGlobal",
                                   "parameters": { "stat": "minerals", "amount": 1,
                                                   "op": "Add" } } ] } })"),
                      ContainsSubstring("continuous"));
}

TEST_CASE("EcoDamageConfigParser requires the script to name damage_formula", "[ecology][parser]")
{
    LuaRuntime lua;
    const TempConfigFile script("eco_damage.lua", "return { }");
    CHECK_THROWS_WITH(EcoDamageConfigParser{}.ParseConfig(FixturePath("eco_damage.json"),
                                                          script.Path(), lua),
                      ContainsSubstring("damage_formula"));
}
