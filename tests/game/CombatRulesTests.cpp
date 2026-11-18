#include "GameFixtures.h"

#include "game/DifficultyConfigParser.h"
#include "game/GameRulesConfig.h"
#include "game/combat/AttackRules.h"
#include "game/combat/CollateralRules.h"
#include "game/combat/CombatResolver.h"
#include "game/combat/CombatRules.h"
#include "game/combat/MoraleRules.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/TileEffectsContext.h"
#include "game/effects/WorldRulesConfigParser.h"
#include "game/faction/Faction.h"
#include "game/faction/FactionConfig.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/movement/MovementConstants.h"
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
    return ResolveCombatStrength(rAttacker, rDefender, rFixture.morale(),
                                 CombatEngagement_t::Standard);
}

} // namespace

TEST_CASE("A tied round goes to the defender and only the attacker loses HP", "[combat]")
{
    CombatStrength_t strength;
    strength.attackerRoundDamage = 2;
    strength.defenderRoundDamage = 3;

    const CombatRoundOutcome_t tie = ResolveCombatRound(strength, 5, 5);
    CHECK(tie.winner == CombatSide_t::Defender);
    CHECK(tie.attackerHpLoss == 2);
    CHECK(tie.defenderHpLoss == 0);

    const CombatRoundOutcome_t win = ResolveCombatRound(strength, 6, 5);
    CHECK(win.winner == CombatSide_t::Attacker);
    CHECK(win.attackerHpLoss == 0);
    CHECK(win.defenderHpLoss == 3);
}

TEST_CASE("A strike spares a losing attacker and stops the defender at the floor",
          "[combat][bombard]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& enemy = fixture.MakeFaction();
    Unit& defender = fixture.MakeUnit(enemy, 9, 9, {"test_chassis"});
    CombatStrength_t strength;
    strength.attackerRoundDamage = 2;
    strength.defenderRoundDamage = 3;
    const int floor = 4;

    const CombatRoundOutcome_t miss = ResolveStrikeRound(strength, defender, 0, 5, floor);
    CHECK(miss.winner == CombatSide_t::Defender);
    CHECK(miss.damage == 2);
    CHECK(miss.attackerHpLoss == 0);

    auto hitAt = [&](int defenderHp)
    {
        defender.SetCurrentHp(defenderHp);
        return ResolveStrikeRound(strength, defender, 5, 0, floor).defenderHpLoss;
    };
    CHECK(hitAt(10) == 3);
    CHECK(hitAt(floor + 1) == 1);
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

TEST_CASE("Psi odds use hits-to-kill, so double HP and firepower keep the strength ratio",
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
    // Both need ten hits, so the odds are the land psi strengths, 3 to 2.
    CHECK(odds.attackerOdds == 3);
    CHECK(odds.defenderOdds == 2);
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

TEST_CASE("Disciplined armor 1 defends at 256 and Green armor 1 at 224", "[combat][strength]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    UseFixtureDifficulty_(fixture, "librarian");
    Unit& attacker = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon"});
    Unit& defender = fixture.MakeUnit(enemy, 9, 9, {"test_chassis", "test_weak_armor"});

    defender.SetXp(2);
    CHECK(Strength_(attacker, defender, fixture).defenseStrength == 256);
    defender.SetXp(1);
    CHECK(Strength_(attacker, defender, fixture).defenseStrength == 224);
}

TEST_CASE("A fractional strength rounds down once", "[combat][strength]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    UseFixtureDifficulty_(fixture, "librarian");
    Unit& attacker =
        fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon", "test_tenth_attack"});
    Unit& defender = fixture.MakeUnit(enemy, 9, 9, {"test_chassis", "test_weak_armor"});
    attacker.SetXp(1);

    // 4 x 1.1 x Green 0.875 x 256 = 985.6.
    CHECK(Strength_(attacker, defender, fixture).attackStrength == 985);
}

TEST_CASE("FloorResolvedStat keeps a product that is whole in decimal", "[combat][strength]")
{
    CHECK(FloorResolvedStat(0.29 * 100.0) == 29);
    CHECK(FloorResolvedStat(3.75) == 3);
    CHECK(FloorResolvedStat(0.0) == 0);
}

TEST_CASE("Partial move scales the whole-number strength before the difficulty handicap",
          "[combat][strength][difficulty]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& ai = fixture.MakeFaction();
    Unit& attacker = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weak_weapon"});
    Unit& defender = fixture.MakeUnit(ai, 9, 9, {"test_chassis", "test_weak_armor"});
    attacker.SetXp(1);
    attacker.SetMoveFragmentsRemaining(MovementConstants_t::k_moveFragmentsPerPoint / 3);

    UseFixtureDifficulty_(fixture, "librarian");
    CHECK(Strength_(attacker, defender, fixture).attackStrength == 74); // 224 x 1/3
    UseFixtureDifficulty_(fixture, "specialist");
    CHECK(Strength_(attacker, defender, fixture).attackStrength == 111); // 74 x 1.5
}

TEST_CASE("A partial-move exemption keeps full attack strength with moves left",
          "[combat][strength][natives]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Faction& planet = AddNativeLifeFaction(fixture);
    UseFixtureDifficulty_(fixture, "librarian");
    Unit& defender = fixture.MakeUnit(enemy, 9, 9, {"test_chassis", "test_weak_armor"});
    const auto attackWithThirdMove = [&](Faction& rOwner, const char* pExemption)
    {
        Unit& attacker =
            fixture.MakeUnit(rOwner, 8, 8, {"test_chassis", "test_weak_weapon", pExemption});
        attacker.SetXp(2);
        attacker.SetMoveFragmentsRemaining(MovementConstants_t::k_moveFragmentsPerPoint / 3);
        return Strength_(attacker, defender, fixture).attackStrength;
    };

    SECTION("an unconditional exemption holds for any owner")
    {
        CHECK(attackWithThirdMove(player, "test_partial_move_exempt") == 256);
    }

    SECTION("a wild-only exemption holds for the native-life faction")
    {
        CHECK(attackWithThirdMove(planet, "test_wild_partial_move_exempt") == 256);
    }

    SECTION("a wild-only exemption does not hold for a real faction")
    {
        CHECK(attackWithThirdMove(player, "test_wild_partial_move_exempt") == 85);
    }
}

TEST_CASE("A duel defender rated on attack takes no difficulty handicap",
          "[combat][difficulty][bombard]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& ai = fixture.MakeFaction();
    Unit& playerGun = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon"});
    Unit& aiGun = fixture.MakeUnit(ai, 9, 9, {"test_chassis", "test_weapon"});
    playerGun.SetXp(2);
    aiGun.SetXp(2);

    UseFixtureDifficulty_(fixture, "citizen");
    const CombatStrength_t duel = ResolveCombatStrength(playerGun, aiGun, fixture.morale(),
                                                        CombatEngagement_t::ArtilleryDuel);
    CHECK(duel.attackStrength == 2 * 4 * k_CombatStrengthScale);
    CHECK(duel.defenseStrength == 4 * k_CombatStrengthScale);
}

TEST_CASE("Psi values follow the defender's domain", "[combat][psi]")
{
    FactionFixture fixture;
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    UseFixtureDifficulty_(fixture, "librarian");
    fixture.At(8, 8).SetElevation(100);
    fixture.At(7, 7).SetElevation(100);
    fixture.At(9, 9).SetElevation(-100);
    Unit& worm = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon", "test_psi"});
    Unit& landDefender = fixture.MakeUnit(enemy, 7, 7, {"test_chassis", "test_armor"});
    Unit& seaDefender = fixture.MakeUnit(enemy, 9, 9, {"test_sea_chassis", "test_armor"});
    worm.SetXp(2);
    landDefender.SetXp(2);
    seaDefender.SetXp(2);

    const CombatStrength_t vsLand = Strength_(worm, landDefender, fixture);
    CHECK(vsLand.attackStrength == 768);
    CHECK(vsLand.defenseStrength == 512);

    const CombatStrength_t vsSea = Strength_(worm, seaDefender, fixture);
    CHECK(vsSea.attackStrength == 256);
    CHECK(vsSea.defenseStrength == 256);
}

TEST_CASE("In a psi duel each side's attack reads the other's domain", "[combat][psi][bombard]")
{
    FactionFixture fixture;
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    UseFixtureDifficulty_(fixture, "librarian");
    fixture.At(8, 8).SetElevation(-100);
    fixture.At(9, 9).SetElevation(100);
    Unit& seaLauncher =
        fixture.MakeUnit(player, 8, 8, {"test_sea_chassis", "test_weapon", "test_psi"});
    Unit& landGun = fixture.MakeUnit(enemy, 9, 9, {"test_chassis", "test_weapon"});
    seaLauncher.SetXp(2);
    landGun.SetXp(2);

    const CombatStrength_t duel = ResolveCombatStrength(seaLauncher, landGun, fixture.morale(),
                                                        CombatEngagement_t::ArtilleryDuel);
    CHECK(duel.attackStrength == 768);
    CHECK(duel.defenseStrength == 256);
}

TEST_CASE("A lost round costs the winner's damage times the loser's damage taken",
          "[combat][psi]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& gun = fixture.MakeUnit(
        player, 8, 8,
        {"test_chassis", "test_weapon", "test_psi_tier_2", "test_extra_conventional_damage"});
    Unit& rifle = fixture.MakeUnit(enemy, 9, 9, {"test_chassis", "test_weapon"});
    Unit& worm =
        fixture.MakeUnit(enemy, 7, 7, {"test_chassis", "test_weapon", "test_psi_defense"});
    Unit& padded = fixture.MakeUnit(
        enemy, 9, 7, {"test_chassis", "test_weapon", "test_half_conventional_damage_taken"});
    Unit& harmless = fixture.MakeUnit(
        enemy, 7, 9, {"test_chassis", "test_weapon", "test_no_conventional_damage"});

    const CombatStrength_t conventional = Strength_(gun, rifle, fixture);
    CHECK(conventional.attackerRoundDamage == 1);
    CHECK(conventional.defenderRoundDamage == 3); // combat.json's 1, plus 2

    const CombatStrength_t psi = Strength_(gun, worm, fixture);
    CHECK(psi.attackerRoundDamage == 2); // the worm's 1, taken twice
    CHECK(psi.defenderRoundDamage == 1); // conventional damage is not psi damage

    CHECK(Strength_(gun, padded, fixture).defenderRoundDamage == 2); // 1.5, rounded
    CHECK(Strength_(gun, harmless, fixture).attackerRoundDamage == 1); // 0, raised to 1
}

TEST_CASE("Outside combat a psi attack is only the base value", "[combat][psi]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    const Unit& worm = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_psi"});

    CHECK(ResolveStat(worm, StatId_t::PsiAttack, EffectContext_t{}) == 1);
}

TEST_CASE("In psi combat Rocky and Bunker give nothing and Base still does",
          "[combat][psi][tile]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    UseFixtureDifficulty_(fixture, "librarian");
    Unit& worm = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon", "test_psi"});
    Unit& defender = fixture.MakeUnit(enemy, 9, 9, {"test_chassis", "test_armor"});
    worm.SetXp(2);
    defender.SetXp(2);

    SECTION("rocky ground with a bunker")
    {
        fixture.At(9, 9).SetRockiness(Rockiness_t::Rocky);
        fixture.ctx.AddOccupantWithEffects(fixture.At(9, 9), "Bunker");
        CHECK(Strength_(worm, defender, fixture).defenseStrength == 512);
    }

    SECTION("a base")
    {
        fixture.ctx.AddOccupantWithEffects(fixture.At(9, 9), "Base");
        CHECK(Strength_(worm, defender, fixture).defenseStrength == 640);
    }
}

TEST_CASE("The Alien Artifact defends at value 1 in both kinds of fight", "[combat][natives]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    UseFixtureDifficulty_(fixture, "librarian");
    Unit& gun = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon"});
    Unit& worm = fixture.MakeUnit(player, 8, 10, {"test_chassis", "test_weapon", "test_psi"});
    Unit& artifact = fixture.MakeUnit(enemy, 9, 9, {"test_artifact_chassis"});
    gun.SetXp(2);
    worm.SetXp(2);
    artifact.SetXp(2);

    SECTION("rocky ground does not raise it")
    {
        fixture.At(9, 9).SetRockiness(Rockiness_t::Rocky);
        CHECK(Strength_(gun, artifact, fixture).defenseStrength == 256);
        CHECK(Strength_(worm, artifact, fixture).defenseStrength == 256);
    }

    SECTION("a base does")
    {
        fixture.ctx.AddOccupantWithEffects(fixture.At(9, 9), "Base");
        CHECK(Strength_(gun, artifact, fixture).defenseStrength == 320);
    }
}
