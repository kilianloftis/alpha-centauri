#include "GameFixtures.h"
#include "TempConfigFile.h"

#include "game/PlayerInteractionQueue.h"
#include "game/base/BaseManager.h"
#include "game/combat/CaptureMutations.h"
#include "game/combat/CaptureRules.h"
#include "game/combat/CombatConfigParser.h"
#include "game/combat/CombatResolver.h"
#include "game/combat/MoraleRules.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticTransitionMutations.h"
#include "game/diplomacy/DiplomaticTransitionRules.h"
#include "game/effects/WorldRulesConfigParser.h"
#include "game/faction/Faction.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/movement/MoveCostRules.h"
#include "game/movement/MovementConstants.h"
#include "game/movement/StepEvaluator.h"
#include "game/units/UnitAttackExecutor.h"
#include "game/units/UnitManager.h"
#include "game/units/UnitOrder.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>
#include <random>
#include <string>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

constexpr int k_FirstWaterColumn = 18;

// The fixture's board_hp_limit (0.6667) of a 10 HP defender.
constexpr int k_BoardingHpOfTen = 6;

// Player A and AI B, under the fixture world rules. Columns left of k_FirstWaterColumn are land,
// the rest water.
struct CaptureGame_
{
    FactionFixture fixtures{k_TestMapWidth, k_TestMapHeight,
                            WorldRulesConfigParser::ParseConfig(FixturePath("world_rules.json"))};
    Faction* pA = nullptr;
    Faction* pB = nullptr;

    CaptureGame_()
    {
        for (auto& pTile : fixtures.map.GetTiles())
        {
            pTile->SetElevation(pTile->GetX() < k_FirstWaterColumn ? 100 : -100);
        }
        pA = &AddSessionFaction(fixtures, fixtures.factionDefinition, true);
        pB = &AddSessionFaction(fixtures, fixtures.factionDefinition, false);
    }

    Unit& Make(Faction& rFaction, int x, int y, const std::vector<std::string>& rParts)
    {
        return fixtures.MakeUnit(rFaction, x, y, rParts);
    }

    Tile& At(int x, int y) { return fixtures.At(x, y); }

    std::optional<CombatResult_t> Attack(Unit& rAttacker, int x, int y)
    {
        return AttackTile(fixtures.state.GetUnitAttacks(), rAttacker, At(x, y));
    }

    DiplomacyLedger& Ledger() { return fixtures.state.GetDiplomacyLedger(); }

    int Friction(const Faction& rHolder, const Faction& rAgainst)
    {
        return Ledger().GetFriction(rHolder.GetFactionId(), rAgainst.GetFactionId());
    }

    std::vector<CaptureNoticeInteraction_t> CaptureNotices()
    {
        std::vector<CaptureNoticeInteraction_t> found;
        fixtures.state.GetPlayerInteractions().AnyOf([&](const QueuedInteraction_t& rQueued)
        {
            if (const auto* pNotice = std::get_if<CaptureNoticeInteraction_t>(&rQueued.payload))
            {
                found.push_back(*pNotice);
            }
            return false;
        });
        return found;
    }
};

struct Resolver_
{
    MoveCostRules moveCosts;
    StepEvaluator steps;
    std::mt19937 rng;
    CombatResolver combat;

    Resolver_(FactionFixture& rFixture, uint32_t seed)
        : moveCosts(rFixture.improvements, MovementConstants_t{})
        , steps(rFixture.map, rFixture.ctx)
        , rng(seed)
        , combat(moveCosts, steps, rFixture.map, rFixture.ctx, rFixture.morale(), rng)
    {
    }
};

CombatResult_t Resolve_(CaptureGame_& rGame, Unit& rAttacker, Unit& rDefender, uint32_t seed)
{
    Resolver_ resolver(rGame.fixtures, seed);
    return resolver.combat.Resolve(rAttacker, rDefender);
}

const std::vector<std::string> k_Marine = {"test_sea_chassis", "test_weapon",
                                           "test_marine_detachment"};
const std::vector<std::string> k_Ship = {"test_sea_chassis"};
const std::vector<std::string> k_Artifact = {"test_artifact_chassis"};
const std::vector<std::string> k_Infantry = {"test_chassis", "test_weapon"};

bool Contains_(const std::vector<UnitId_t>& rIds, UnitId_t id)
{
    return std::ranges::find(rIds, id) != rIds.end();
}

} // namespace

TEST_CASE("Boarding takes the defender once it has lost board_hp_limit of its starting HP",
          "[capture][board]")
{
    CaptureGame_ game;
    Unit& rMarine = game.Make(*game.pA, 20, 8, k_Marine);
    Unit& rShip = game.Make(*game.pB, 21, 9, k_Ship);
    rShip.SetOrder(HoldOrder_t{});
    const int startHp = rShip.GetCurrentHp();

    const CombatResult_t result = Resolve_(game, rMarine, rShip, 1);

    REQUIRE(result.bDefenderBoarded);
    CHECK_FALSE(result.bDefenderDestroyed);
    CHECK(result.victor == CombatSide_t::Attacker);
    CHECK(&rShip.GetFaction() == game.pA);
    CHECK(startHp - rShip.GetCurrentHp() == k_BoardingHpOfTen);
    CHECK(&rShip.GetTile() == &game.At(21, 9));
    CHECK(rShip.GetMoveFragmentsRemaining() == 0);
    CHECK_FALSE(rShip.GetOrder().has_value());
    CHECK(result.capturedUnitIds == std::vector<UnitId_t>{rShip.GetUnitId()});
}

TEST_CASE("The boarding threshold comes from the defender's HP when the fight starts",
          "[capture][board]")
{
    CaptureGame_ game;
    Unit& rMarine = game.Make(*game.pA, 20, 8, k_Marine);
    Unit& rShip = game.Make(*game.pB, 21, 9, k_Ship);
    rShip.SetCurrentHp(7);

    const CombatResult_t result = Resolve_(game, rMarine, rShip, 1);

    REQUIRE(result.bDefenderBoarded);
    // floor(7 × 0.6667)
    CHECK(7 - rShip.GetCurrentHp() == 4);
}

TEST_CASE("A failed boarding roll is retried on every later round the defender loses",
          "[capture][board]")
{
    bool bRetriedLater = false;
    for (uint32_t seed = 1; seed <= 32 && !bRetriedLater; ++seed)
    {
        CaptureGame_ game;
        Unit& rMarine = game.Make(*game.pA, 20, 8,
                                  {"test_sea_chassis", "test_weapon", "test_marine_coin"});
        Unit& rShip = game.Make(*game.pB, 21, 9, k_Ship);
        const int startHp = rShip.GetCurrentHp();

        const CombatResult_t result = Resolve_(game, rMarine, rShip, seed);

        if (result.bDefenderBoarded)
        {
            const int lost = startHp - rShip.GetCurrentHp();
            CHECK(lost >= k_BoardingHpOfTen);
            bRetriedLater = lost > k_BoardingHpOfTen;
        }
    }
    CHECK(bRetriedLater);
}

TEST_CASE("Boarding takes every unit on the defender's tile, cargo included", "[capture][board]")
{
    CaptureGame_ game;
    Unit& rMarine = game.Make(*game.pA, 20, 8, k_Marine);
    Unit& rTransport = game.Make(*game.pB, 21, 9, {"test_sea_chassis", "test_transport"});
    Unit& rEscort = game.Make(*game.pB, 21, 9, k_Ship);
    Unit& rPassenger = game.Make(*game.pB, 21, 9, {"test_chassis"});
    rPassenger.Transport().EmbarkInto(rTransport);
    rEscort.SetCurrentHp(4);

    const CombatResult_t result = Resolve_(game, rMarine, rTransport, 1);

    REQUIRE(result.bDefenderBoarded);
    CHECK(result.capturedUnitIds.size() == 3);
    for (Unit* pUnit : {&rTransport, &rEscort, &rPassenger})
    {
        CHECK(&pUnit->GetFaction() == game.pA);
        CHECK(pUnit->GetMoveFragmentsRemaining() == 0);
    }
    CHECK(rPassenger.Transport().GetCarrier() == &rTransport);
    CHECK(rEscort.GetCurrentHp() == 4);
}

TEST_CASE("A boarded unit is homed at the nearest base when the captor owns it with enough "
          "population",
          "[capture][board][home]")
{
    CaptureGame_ game;
    Unit& rMarine = game.Make(*game.pA, 20, 8, k_Marine);
    Unit& rShip = game.Make(*game.pB, 21, 9, k_Ship);

    SECTION("the captor's base, at boarding_home_base_min_pop")
    {
        BaseManager& rHome = game.fixtures.MakeFactionBase(*game.pA, 16, 8, 3);
        REQUIRE(Resolve_(game, rMarine, rShip, 1).bDefenderBoarded);
        CHECK(rShip.GetHomeBase() == &rHome);
    }

    SECTION("the captor's base below boarding_home_base_min_pop")
    {
        game.fixtures.MakeFactionBase(*game.pA, 16, 8, 2);
        REQUIRE(Resolve_(game, rMarine, rShip, 1).bDefenderBoarded);
        CHECK(rShip.GetHomeBase() == nullptr);
    }

    SECTION("another faction's base is nearer")
    {
        game.fixtures.MakeFactionBase(*game.pA, 16, 8, 3);
        game.fixtures.MakeFactionBase(*game.pB, 17, 9, 3);
        REQUIRE(Resolve_(game, rMarine, rShip, 1).bDefenderBoarded);
        CHECK(rShip.GetHomeBase() == nullptr);
    }
}

TEST_CASE("Boarding needs a sea attacker and a sea defender that is not native life, on water "
          "with no base",
          "[capture][board]")
{
    CaptureGame_ game;

    SECTION("land attacker")
    {
        Unit& rAttacker = game.Make(*game.pA, 17, 9,
                                    {"test_chassis", "test_weapon", "test_marine_detachment"});
        Unit& rShip = game.Make(*game.pB, 18, 8, k_Ship);
        const CombatResult_t result = Resolve_(game, rAttacker, rShip, 1);
        CHECK_FALSE(result.bDefenderBoarded);
        CHECK(result.bDefenderDestroyed);
    }

    SECTION("air defender")
    {
        Unit& rMarine = game.Make(*game.pA, 20, 8, k_Marine);
        Unit& rPlane = game.Make(*game.pB, 21, 9, {"test_flight_chassis"});
        const CombatResult_t result = Resolve_(game, rMarine, rPlane, 1);
        CHECK_FALSE(result.bDefenderBoarded);
        CHECK(result.bDefenderDestroyed);
    }

    SECTION("native life defender")
    {
        Unit& rMarine = game.Make(*game.pA, 20, 8, k_Marine);
        Unit& rWorm = game.Make(*game.pB, 21, 9, {"test_sea_chassis", "test_native_life"});
        const CombatResult_t result = Resolve_(game, rMarine, rWorm, 1);
        CHECK_FALSE(result.bDefenderBoarded);
        CHECK(result.bDefenderDestroyed);
    }

    SECTION("base tile")
    {
        game.fixtures.MakeFactionBase(*game.pB, 21, 9, 3);
        Unit& rMarine = game.Make(*game.pA, 20, 8, k_Marine);
        Unit& rShip = game.Make(*game.pB, 21, 9, k_Ship);
        const CombatResult_t result = Resolve_(game, rMarine, rShip, 1);
        CHECK_FALSE(result.bDefenderBoarded);
        CHECK(result.bDefenderDestroyed);
    }
}

TEST_CASE("Boarding is checked before disengage in the round both become possible",
          "[capture][board][disengage]")
{
    // A 3 HP defender may board or disengage once it has lost 2.
    CaptureGame_ game;
    const std::vector<std::string> runner = {"test_sea_chassis", "test_weak_weapon",
                                             "test_extra_movement"};

    SECTION("without a Marine Detachment the defender disengages")
    {
        Unit& rAttacker = game.Make(*game.pA, 20, 8, {"test_sea_chassis", "test_weapon"});
        Unit& rRunner = game.Make(*game.pB, 21, 9, runner);
        rRunner.SetCurrentHp(3);
        CHECK(Resolve_(game, rAttacker, rRunner, 1).bDefenderDisengaged);
    }

    SECTION("with one it is boarded")
    {
        Unit& rMarine = game.Make(*game.pA, 20, 8, k_Marine);
        Unit& rRunner = game.Make(*game.pB, 21, 9, runner);
        rRunner.SetCurrentHp(3);
        const CombatResult_t result = Resolve_(game, rMarine, rRunner, 1);
        CHECK(result.bDefenderBoarded);
        CHECK_FALSE(result.bDefenderDisengaged);
    }
}

TEST_CASE("A defeated artifact joins a land attacker's tile at full HP", "[capture][artifact]")
{
    CaptureGame_ game;
    Unit& rAttacker = game.Make(*game.pA, 8, 8, k_Infantry);
    Unit& rArtifact = game.Make(*game.pB, 9, 9, k_Artifact);
    const UnitId_t artifactId = rArtifact.GetUnitId();

    const std::optional<CombatResult_t> result = game.Attack(rAttacker, 9, 9);

    REQUIRE(result);
    CHECK(result->bDefenderDestroyed);
    CHECK(result->capturedUnitIds == std::vector<UnitId_t>{artifactId});
    CHECK(&rArtifact.GetFaction() == game.pA);
    CHECK(&rArtifact.GetTile() == &game.At(8, 8));
    CHECK(rArtifact.GetCurrentHp() == 1);
    CHECK(rArtifact.GetMoveFragmentsRemaining() == 0);
    CHECK(rArtifact.GetHomeBase() == nullptr);
}

TEST_CASE("An attacker on water takes an artifact only onto a carrier with room",
          "[capture][artifact]")
{
    CaptureGame_ game;
    Unit& rArtifact = game.Make(*game.pB, 17, 9, k_Artifact);

    SECTION("a free slot")
    {
        Unit& rTransport = game.Make(*game.pA, 18, 8, {"test_sea_chassis", "test_transport_2"});
        Unit& rPods = game.Make(*game.pA, 18, 8, {"test_chassis", "test_weapon", "test_amphibious"});
        rPods.Transport().EmbarkInto(rTransport);

        const std::optional<CombatResult_t> result = game.Attack(rPods, 17, 9);

        REQUIRE(result);
        REQUIRE(result->capturedUnitIds.size() == 1);
        CHECK(&rArtifact.GetFaction() == game.pA);
        CHECK(rArtifact.Transport().GetCarrier() == &rTransport);
    }

    SECTION("no free slot")
    {
        Unit& rTransport = game.Make(*game.pA, 18, 8, {"test_sea_chassis", "test_transport"});
        Unit& rPods = game.Make(*game.pA, 18, 8, {"test_chassis", "test_weapon", "test_amphibious"});
        rPods.Transport().EmbarkInto(rTransport);

        const std::optional<CombatResult_t> result = game.Attack(rPods, 17, 9);

        REQUIRE(result);
        CHECK(result->bDefenderDestroyed);
        CHECK(result->capturedUnitIds.empty());
        CHECK(game.pB->GetUnitManager().Units().empty());
    }
}

TEST_CASE("Wild native life and single-use attackers destroy artifacts", "[capture][artifact]")
{
    CaptureGame_ game;
    Unit& rArtifact = game.Make(*game.pB, 9, 9, k_Artifact);

    SECTION("wild native life")
    {
        Faction& rPlanet = AddNativeLifeFaction(game.fixtures);
        Unit& rWorm = game.Make(rPlanet, 8, 8,
                                {"native_life_chassis", "test_weapon", "test_extra_hp_10"});
        const CombatResult_t result = Resolve_(game, rWorm, rArtifact, 1);
        CHECK(result.bDefenderDestroyed);
        CHECK(result.capturedUnitIds.empty());
    }

    SECTION("single use")
    {
        Unit& rMissile = game.Make(*game.pA, 8, 8, {"test_chassis", "test_single_use_weapon"});
        const CombatResult_t result = Resolve_(game, rMissile, rArtifact, 1);
        CHECK(result.bDefenderDestroyed);
        CHECK(result.capturedUnitIds.empty());
    }

    CHECK(game.pB->GetUnitManager().Units().empty());
}

TEST_CASE("An artifact is taken when its stack loses its last combatant, and not before",
          "[capture][artifact][collateral]")
{
    CaptureGame_ game;
    Unit& rAttacker = game.Make(*game.pA, 8, 8,
                                {"test_chassis", "test_weapon", "test_collateral_1"});
    Unit& rGuard = game.Make(*game.pB, 9, 9, {"test_chassis"});
    Unit& rArtifact = game.Make(*game.pB, 9, 9, k_Artifact);

    SECTION("last combatant")
    {
        Resolver_ resolver(game.fixtures, 1);
        const CombatResult_t result =
            ResolveWithCollateral(resolver.combat, game.fixtures.map, rAttacker, rGuard);
        REQUIRE(result.bDefenderDestroyed);
        CHECK(Contains_(result.capturedUnitIds, rArtifact.GetUnitId()));
        CHECK(&rArtifact.GetFaction() == game.pA);
        CHECK(&rArtifact.GetTile() == &game.At(8, 8));
    }

    SECTION("a combatant survives the splash")
    {
        game.Make(*game.pB, 9, 9, {"test_chassis"});
        Resolver_ resolver(game.fixtures, 1);
        const CombatResult_t result =
            ResolveWithCollateral(resolver.combat, game.fixtures.map, rAttacker, rGuard);
        REQUIRE(result.bDefenderDestroyed);
        CHECK(result.capturedUnitIds.empty());
        CHECK(&rArtifact.GetFaction() == game.pB);
        CHECK(rArtifact.GetCurrentHp() == 1);
    }
}

TEST_CASE("A defender that wins takes nothing from the attacker", "[capture][artifact]")
{
    CaptureGame_ game;
    Unit& rArtifact = game.Make(*game.pA, 8, 8, k_Artifact);
    Unit& rGuard = game.Make(*game.pB, 9, 9, {"test_chassis", "test_armor"});

    const CombatResult_t result = Resolve_(game, rArtifact, rGuard, 1);

    CHECK(result.bAttackerDestroyed);
    CHECK(result.capturedUnitIds.empty());
    CHECK(game.pA->GetUnitManager().Units().empty());
}

TEST_CASE("Attacking a lone artifact off a base is not a hostile act", "[capture][artifact][hostile]")
{
    CaptureGame_ game;
    ApplyStatusChange(game.fixtures.state, game.pA->GetFactionId(), game.pB->GetFactionId(),
                      DiplomaticStatus_t::Treaty);
    Unit& rAttacker = game.Make(*game.pA, 8, 8, k_Infantry);

    SECTION("off a base")
    {
        Unit& rArtifact = game.Make(*game.pB, 9, 9, k_Artifact);
        CHECK_FALSE(IsHostileAttack(rAttacker, rArtifact));
        REQUIRE(game.Attack(rAttacker, 9, 9));
        CHECK(game.Ledger().GetStatus(game.pA->GetFactionId(), game.pB->GetFactionId())
              == DiplomaticStatus_t::Treaty);
    }

    SECTION("on a base")
    {
        game.fixtures.MakeFactionBase(*game.pB, 9, 9, 3);
        Unit& rArtifact = game.Make(*game.pB, 9, 9, k_Artifact);
        CHECK(IsHostileAttack(rAttacker, rArtifact));
        REQUIRE(game.Attack(rAttacker, 9, 9));
        CHECK(game.Ledger().GetStatus(game.pA->GetFactionId(), game.pB->GetFactionId())
              == DiplomaticStatus_t::Vendetta);
    }

    SECTION("bombarding it")
    {
        Unit& rArtillery = game.Make(*game.pA, 10, 8, {"test_chassis", "bombard"});
        game.Make(*game.pB, 9, 9, k_Artifact);
        CHECK(BombardHostileActs(rArtillery, game.At(9, 9)).empty());
    }
}

TEST_CASE("Taking an AI faction's artifact outside Vendetta adds artifact_capture_friction",
          "[capture][artifact][friction]")
{
    CaptureGame_ game;
    ApplyStatusChange(game.fixtures.state, game.pA->GetFactionId(), game.pB->GetFactionId(),
                      DiplomaticStatus_t::Treaty);

    SECTION("AI victim")
    {
        Unit& rAttacker = game.Make(*game.pA, 8, 8, k_Infantry);
        game.Make(*game.pB, 9, 9, k_Artifact);
        const int before = game.Friction(*game.pB, *game.pA);
        REQUIRE(game.Attack(rAttacker, 9, 9)->capturedUnitIds.size() == 1);
        // The fixture's artifact_capture_friction.
        CHECK(game.Friction(*game.pB, *game.pA) == before + 5);
    }

    SECTION("player victim")
    {
        Unit& rAttacker = game.Make(*game.pB, 8, 8, k_Infantry);
        game.Make(*game.pA, 9, 9, k_Artifact);
        const int before = game.Friction(*game.pA, *game.pB);
        REQUIRE(game.Attack(rAttacker, 9, 9)->capturedUnitIds.size() == 1);
        CHECK(game.Friction(*game.pA, *game.pB) == before);
    }

    SECTION("Vendetta")
    {
        DeclareVendetta(game.fixtures.state, game.pA->GetFactionId(), game.pB->GetFactionId());
        Unit& rAttacker = game.Make(*game.pA, 8, 8, k_Infantry);
        game.Make(*game.pB, 9, 9, k_Artifact);
        const int before = game.Friction(*game.pB, *game.pA);
        REQUIRE(game.Attack(rAttacker, 9, 9)->capturedUnitIds.size() == 1);
        CHECK(game.Friction(*game.pB, *game.pA) == before);
    }
}

TEST_CASE("A capture the player takes part in queues a capture notice", "[capture][notice]")
{
    CaptureGame_ game;

    SECTION("the player takes an artifact")
    {
        Unit& rAttacker = game.Make(*game.pA, 8, 8, k_Infantry);
        Unit& rArtifact = game.Make(*game.pB, 9, 9, k_Artifact);
        REQUIRE(game.Attack(rAttacker, 9, 9));
        const std::vector<CaptureNoticeInteraction_t> notices = game.CaptureNotices();
        REQUIRE(notices.size() == 1);
        CHECK(notices.front().kind == CaptureNoticeKind_t::TakenOnDefeat);
        CHECK(notices.front().captorId == game.pA->GetFactionId());
        CHECK(notices.front().victimId == game.pB->GetFactionId());
        CHECK(notices.front().unitName == rArtifact.GetDesign().GetName());
        CHECK(notices.front().count == 1);
    }

    SECTION("the player's ship is boarded")
    {
        Unit& rMarine = game.Make(*game.pB, 20, 8, k_Marine);
        game.Make(*game.pA, 21, 9, k_Ship);
        REQUIRE(game.Attack(rMarine, 21, 9)->bDefenderBoarded);
        const std::vector<CaptureNoticeInteraction_t> notices = game.CaptureNotices();
        REQUIRE(notices.size() == 1);
        CHECK(notices.front().kind == CaptureNoticeKind_t::Boarded);
        CHECK(notices.front().captorId == game.pB->GetFactionId());
        CHECK(notices.front().victimId == game.pA->GetFactionId());
    }

    SECTION("two AI factions")
    {
        Faction& rC = AddSessionFaction(game.fixtures, game.fixtures.factionDefinition, false);
        Unit& rAttacker = game.Make(rC, 8, 8, k_Infantry);
        game.Make(*game.pB, 9, 9, k_Artifact);
        REQUIRE(game.Attack(rAttacker, 9, 9)->capturedUnitIds.size() == 1);
        CHECK(game.CaptureNotices().empty());
    }
}

TEST_CASE("Killing a captured_on_defeat unit earns no promotion; boarding still does",
          "[capture][promotion]")
{
    CombatResult_t artifactKill;
    artifactKill.bDefenderDestroyed = true;
    artifactKill.bKillEarnsPromotion = false;
    CHECK_FALSE(PromotionCandidate(artifactKill).has_value());

    CombatResult_t boarding;
    boarding.bDefenderBoarded = true;
    CHECK(PromotionCandidate(boarding) == CombatSide_t::Attacker);

    CaptureGame_ game;
    CHECK_FALSE(KillEarnsPromotion(game.Make(*game.pB, 9, 9, k_Artifact)));
    CHECK(KillEarnsPromotion(game.Make(*game.pB, 10, 10, k_Infantry)));
}

TEST_CASE("A subverted unit is homed at the actor's nearest base with full moves",
          "[capture][probe]")
{
    CaptureGame_ game;
    BaseManager& rNear = game.fixtures.MakeFactionBase(*game.pA, 6, 8, 1);
    game.fixtures.MakeFactionBase(*game.pA, 2, 2, 1);
    Unit& rTarget = game.Make(*game.pB, 8, 8, k_Infantry);
    rTarget.SetCurrentHp(5);
    rTarget.SetMoveFragmentsRemaining(0);
    rTarget.SetOrder(HoldOrder_t{});

    CaptureUnits(game.fixtures.map.GetUnitPositions(), *game.pA,
                 {SubvertCapture(*game.pA, rTarget)});

    CHECK(&rTarget.GetFaction() == game.pA);
    CHECK(rTarget.GetHomeBase() == &rNear);
    CHECK(rTarget.GetCurrentHp() == 5);
    CHECK(rTarget.GetMoveFragmentsRemaining()
          == rTarget.GetMovementPoints() * MovementConstants_t::k_moveFragmentsPerPoint);
    CHECK_FALSE(rTarget.GetOrder().has_value());
}

TEST_CASE("combat.json requires a non-negative boarding_home_base_min_pop", "[capture][parser]")
{
    const TempConfigFile valid("ac_combat_valid.json", R"({ "boarding_home_base_min_pop": 2 })");
    CHECK(CombatConfigParser::ParseConfig(valid.Path()).boardingHomeBaseMinPop == 2);

    const TempConfigFile missing("ac_combat_missing.json", "{}");
    CHECK_THROWS_AS(CombatConfigParser::ParseConfig(missing.Path()), std::runtime_error);

    const TempConfigFile negative("ac_combat_negative.json",
                                  R"({ "boarding_home_base_min_pop": -1 })");
    CHECK_THROWS_AS(CombatConfigParser::ParseConfig(negative.Path()), std::runtime_error);
}
