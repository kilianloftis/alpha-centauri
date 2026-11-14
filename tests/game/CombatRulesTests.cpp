#include "GameFixtures.h"

#include "game/DifficultyConfigParser.h"
#include "game/GameRulesConfig.h"
#include "game/combat/AttackRules.h"
#include "game/combat/CollateralRules.h"
#include "game/combat/CombatResolver.h"
#include "game/combat/CombatRules.h"
#include "game/combat/MoraleRules.h"
#include "game/effects/TileEffectsContext.h"
#include "game/effects/WorldRulesConfigParser.h"
#include "game/faction/Faction.h"
#include "game/faction/FactionConfig.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/natives/NativeRules.h"
#include "game/units/Unit.h"

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

void FillLand_(WorldFixture& fixture)
{
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(100);
    }
}

void UseFixtureDifficulty_(FactionFixture& rFixtures, const char* difficultyId)
{
    *rFixtures.dataContext.difficultyConfig =
        DifficultyConfigParser::ParseConfig(FixturePath("difficulty/levels.json"));
    GameRulesConfig_t rules = rFixtures.settings.GetGameRules();
    rules.difficultyId = difficultyId;
    rFixtures.settings.SetGameRules(rules);
}

CombatStrength_t Strength_(Unit& rAttacker, Unit& rDefender, FactionFixture& rFixture)
{
    return ResolveCombatStrength(rAttacker, rDefender, rFixture.ctx, rFixture.morale(),
                                 CombatEngagement_t::Standard);
}

} // namespace

TEST_CASE("A tied round goes to the defender and only the attacker loses HP", "[combat]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& attacker = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon"});
    Unit& defender = fixture.MakeUnit(enemy, 9, 9, {"test_chassis"});

    const CombatRoundOutcome_t tie =
        ResolveCombatRound(attacker, defender, 5, 5);
    CHECK(tie.winner == CombatSide_t::Defender);
    CHECK(tie.attackerHpLoss == k_CombatRoundDamage);
    CHECK(tie.defenderHpLoss == 0);

    const CombatRoundOutcome_t win =
        ResolveCombatRound(attacker, defender, 6, 5);
    CHECK(win.winner == CombatSide_t::Attacker);
    CHECK(win.attackerHpLoss == 0);
    CHECK(win.defenderHpLoss == k_CombatRoundDamage);
    CHECK(attacker.GetCurrentHp() == 10);
    CHECK(defender.GetCurrentHp() == 10);
}

TEST_CASE("A strike spares a losing attacker and stops the defender at the floor",
          "[combat][bombard]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& attacker = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon"});
    Unit& defender = fixture.MakeUnit(enemy, 9, 9, {"test_chassis"});
    const int floor = 4;

    const CombatRoundOutcome_t miss =
        ResolveStrikeRound(attacker, defender, 0, 5, floor);
    CHECK(miss.winner == CombatSide_t::Defender);
    CHECK(miss.damage == k_CombatRoundDamage);
    CHECK(miss.attackerHpLoss == 0);

    auto hitAt = [&](int defenderHp)
    {
        defender.SetCurrentHp(defenderHp);
        return ResolveStrikeRound(attacker, defender, 5, 0, floor).defenderHpLoss;
    };
    CHECK(hitAt(10) == k_CombatRoundDamage);
    CHECK(hitAt(floor) == 0);
    CHECK(hitAt(floor - 2) == 0);
}

TEST_CASE("Planning collateral marks base occupants immune and changes no unit",
          "[combat][collateral]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& attacker =
        fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon", "test_collateral_2"});
    // The defender has already left the tile when collateral is planned.
    const Tile& rTile = fixture.At(9, 9);
    Unit& inOpen = fixture.MakeUnit(enemy, 9, 9, {"test_chassis"});

    const std::vector<CollateralHit_t> open = PlanStackCollateral(attacker, rTile);
    REQUIRE(open.size() == 1);
    CHECK(open.front().pOccupant == &inOpen);
    CHECK_FALSE(open.front().bImmune);
    CHECK(open.front().damage == 2);
    CHECK(inOpen.GetCurrentHp() == 10);

    fixture.ctx.AddOccupantWithEffects(fixture.At(9, 9), "Base");
    const std::vector<CollateralHit_t> inBase =
        PlanStackCollateral(attacker, rTile);
    REQUIRE(inBase.size() == 1);
    CHECK(inBase.front().bImmune);
    CHECK(inBase.front().damage == 0);
}

TEST_CASE("Only the survivor of a kill is offered promotion", "[combat][morale]")
{
    CombatResult_t result;
    CHECK_FALSE(PromotionCandidate(result).has_value());

    result.bDefenderDestroyed = true;
    CHECK(PromotionCandidate(result) == CombatSide_t::Attacker);

    result.bAttackerDestroyed = true;
    CHECK_FALSE(PromotionCandidate(result).has_value());

    result.bDefenderDestroyed = false;
    CHECK(PromotionCandidate(result) == CombatSide_t::Defender);
}

TEST_CASE("Non-combatants are left unprotected only when no combatant shares their tile",
          "[combat][collateral]")
{
    std::vector<EffectConfig_t> worldRules =
        WorldRulesConfigParser::ParseConfig(FixturePath("world_rules.json"));
    FactionFixture fixture(k_TestMapWidth, k_TestMapHeight, std::move(worldRules));
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& attacker =
        fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon", "test_collateral_1"});
    // The defender has already left the tile when collateral is planned.
    const Tile& rTile = fixture.At(9, 9);
    Unit& probe = fixture.MakeUnit(enemy, 9, 9, {"test_chassis", "Probe_Team"});

    const std::vector<CollateralHit_t> alone = PlanStackCollateral(attacker, rTile);
    CHECK(NonCombatantsLeftUnprotected(attacker, rTile, alone)
          == std::vector<Unit*>{&probe});

    fixture.MakeUnit(enemy, 9, 9, {"test_chassis"});
    const std::vector<CollateralHit_t> guarded =
        PlanStackCollateral(attacker, rTile);
    CHECK(NonCombatantsLeftUnprotected(attacker, rTile, guarded).empty());
}

TEST_CASE("Without the world rule, collateral leaves no non-combatant unprotected",
          "[combat][collateral]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& attacker =
        fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon", "test_collateral_1"});
    // The defender has already left the tile when collateral is planned.
    const Tile& rTile = fixture.At(9, 9);
    fixture.MakeUnit(enemy, 9, 9, {"test_chassis", "Probe_Team"});

    const std::vector<CollateralHit_t> plan = PlanStackCollateral(attacker, rTile);
    CHECK(NonCombatantsLeftUnprotected(attacker, rTile, plan).empty());
}

TEST_CASE("Equal conventional units reduce combat odds to 1 to 1", "[combat][odds]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& attacker = fixture.MakeUnit(player, 8, 8, {"test_chassis"});
    Unit& defender = fixture.MakeUnit(enemy, 9, 9, {"test_chassis"});

    const CombatOdds_t odds =
        ComputeCombatOdds(attacker, defender, CombatEngagement_t::Standard);
    CHECK(odds.attackerOdds == 1);
    CHECK(odds.defenderOdds == 1);
}

TEST_CASE("A 4-attack unit is 4 to 1 against 1-defense at full HP", "[combat][odds]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& attacker = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon"});
    Unit& defender = fixture.MakeUnit(enemy, 9, 9, {"test_chassis", "test_weak_armor"});

    const CombatOdds_t odds =
        ComputeCombatOdds(attacker, defender, CombatEngagement_t::Standard);
    CHECK(odds.attackerOdds == 4);
    CHECK(odds.defenderOdds == 1);
}

TEST_CASE("A damaged defender changes only its own hits in the odds", "[combat][odds]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& attacker =
        fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weak_weapon"});
    Unit& defender =
        fixture.MakeUnit(enemy, 9, 9, {"test_chassis", "test_weak_armor"});
    defender.SetCurrentHp(5);

    const CombatOdds_t odds =
        ComputeCombatOdds(attacker, defender, CombatEngagement_t::Standard);
    CHECK(odds.attackerOdds == 2);
    CHECK(odds.defenderOdds == 1);
}

TEST_CASE("Psi odds use hits-to-kill so a double-HP double-firepower attacker is 1 to 1",
          "[combat][odds][psi]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& attacker = fixture.MakeUnit(
        player, 8, 8, {"test_chassis", "test_weapon", "test_psi", "test_psi_tier_2",
                       "test_extra_hp_10"});
    Unit& defender = fixture.MakeUnit(enemy, 9, 9, {"test_chassis", "test_armor"});

    REQUIRE(attacker.GetCurrentHp() == 20);
    REQUIRE(defender.GetCurrentHp() == 10);
    const CombatOdds_t odds =
        ComputeCombatOdds(attacker, defender, CombatEngagement_t::Standard);
    CHECK(odds.attackerOdds == 1);
    CHECK(odds.defenderOdds == 1);
}

TEST_CASE("SelectDefender picks the candidate with the better chance, ties keep order",
          "[combat][odds]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& attacker = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon"});
    Unit& unarmored = fixture.MakeUnit(enemy, 9, 9, {"test_chassis"});
    Unit& armored = fixture.MakeUnit(enemy, 9, 9, {"test_chassis", "test_armor"});
    Unit& fullHp = fixture.MakeUnit(enemy, 10, 10, {"test_chassis", "test_armor"});
    Unit& hurt = fixture.MakeUnit(enemy, 11, 11, {"test_chassis", "test_armor"});
    hurt.SetCurrentHp(5);
    Unit& first = fixture.MakeUnit(enemy, 12, 12, {"test_chassis"});
    Unit& second = fixture.MakeUnit(enemy, 13, 13, {"test_chassis"});

    CHECK(SelectDefender(attacker, {&unarmored, &armored}) == &armored);
    CHECK(SelectDefender(attacker, {&armored, &unarmored}) == &armored);
    CHECK(SelectDefender(attacker, {&hurt, &fullHp}) == &fullHp);
    CHECK(SelectDefender(attacker, {&first, &second}) == &first);
    CHECK(SelectDefender(attacker, {&second, &first}) == &second);
    CHECK(SelectDefender(attacker, {}) == nullptr);
}

TEST_CASE("A native is wild only while a native-life faction owns it", "[natives]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    FactionConfig_t wildDefinition = fixture.factionDefinition;
    wildDefinition.id = "wild_life";
    wildDefinition.identity.species = FactionSpecies_t::NativeLife;
    Faction& wild = fixture.MakeFaction(wildDefinition);

    const Unit& wildWorm = fixture.MakeUnit(wild, 8, 8, {"test_chassis", "native_life_chassis"});
    const Unit& bredWorm =
        fixture.MakeUnit(player, 10, 10, {"test_chassis", "native_life_chassis"});
    const Unit& wildMachine = fixture.MakeUnit(wild, 12, 12, {"test_chassis"});

    CHECK(IsWildNative(wildWorm));
    CHECK_FALSE(IsWildNative(bredWorm));
    CHECK_FALSE(IsWildNative(wildMachine));
}

TEST_CASE("Citizen difficulty scales Attack in human vs AI fights", "[combat][difficulty]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& ai = fixture.MakeFaction();
    Unit& playerUnit = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon"});
    Unit& aiUnit = fixture.MakeUnit(ai, 9, 9, {"test_chassis", "test_weapon"});
    // Baseline morale so AddPercent is 0 and integer handicap scales divide cleanly.
    playerUnit.SetXp(fixture.morale().GetConfig().baselineIndex);
    aiUnit.SetXp(fixture.morale().GetConfig().baselineIndex);

    UseFixtureDifficulty_(fixture, "librarian");
    const int basePlayerAttack = Strength_(playerUnit, aiUnit, fixture).attackStrength;
    const int baseAiAttack = Strength_(aiUnit, playerUnit, fixture).attackStrength;

    UseFixtureDifficulty_(fixture, "citizen");
    CHECK(Strength_(playerUnit, aiUnit, fixture).attackStrength == basePlayerAttack * 2);
    CHECK(Strength_(aiUnit, playerUnit, fixture).attackStrength == baseAiAttack / 4);
}

TEST_CASE("Talent difficulty cuts only Planet Attack against the player", "[combat][difficulty]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& ai = fixture.MakeFaction();
    Faction& planet = AddNativeLifeFaction(fixture);
    Unit& playerUnit = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon"});
    Unit& aiUnit = fixture.MakeUnit(ai, 9, 9, {"test_chassis", "test_weapon"});
    Unit& planetUnit = fixture.MakeUnit(planet, 10, 10, {"test_chassis", "test_weapon"});
    playerUnit.SetXp(fixture.morale().GetConfig().baselineIndex);
    aiUnit.SetXp(fixture.morale().GetConfig().baselineIndex);
    planetUnit.SetXp(fixture.morale().GetConfig().baselineIndex);

    UseFixtureDifficulty_(fixture, "librarian");
    const int baseAiAttack = Strength_(aiUnit, playerUnit, fixture).attackStrength;
    const int basePlanetAttack = Strength_(planetUnit, playerUnit, fixture).attackStrength;
    const int basePlayerAttack = Strength_(playerUnit, aiUnit, fixture).attackStrength;

    UseFixtureDifficulty_(fixture, "talent");
    CHECK(Strength_(aiUnit, playerUnit, fixture).attackStrength == baseAiAttack);
    CHECK(Strength_(planetUnit, playerUnit, fixture).attackStrength == basePlanetAttack * 3 / 4);
    CHECK(Strength_(playerUnit, aiUnit, fixture).attackStrength == basePlayerAttack);
}

TEST_CASE("Citizen difficulty leaves human vs human Attack unchanged", "[combat][difficulty]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    fixture.extraDefinitions.push_back(
        std::make_unique<FactionConfig_t>(fixture.factionDefinition));
    FactionConfig_t& rOtherDef = *fixture.extraDefinitions.back();
    rOtherDef.id = "other_player";
    Faction& otherPlayer = AddSessionFaction(fixture, rOtherDef, true);
    Unit& a = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon"});
    Unit& b = fixture.MakeUnit(otherPlayer, 9, 9, {"test_chassis", "test_weapon"});
    a.SetXp(fixture.morale().GetConfig().baselineIndex);
    b.SetXp(fixture.morale().GetConfig().baselineIndex);

    UseFixtureDifficulty_(fixture, "librarian");
    const int baseAttack = Strength_(a, b, fixture).attackStrength;

    UseFixtureDifficulty_(fixture, "citizen");
    CHECK(Strength_(a, b, fixture).attackStrength == baseAttack);
}
