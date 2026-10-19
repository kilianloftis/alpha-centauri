#include "GameFixtures.h"

#include "game/CurrentGameState.h"
#include "game/atrocities/AtrocityLedger.h"

#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticTransitionMutations.h"
#include "game/economy/EconomyManager.h"
#include "game/visibility/FactionExploredMap.h"
#include "game/visibility/FactionRevealedUnits.h"
#include "game/visibility/VisibilityRules.h"
#include "game/research/ResearchManager.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitSlotConfig.h"
#include "game/units/UnitManager.h"
#include "game/base/BaseManager.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingManager.h"
#include "game/population/PopulationManager.h"
#include "game/probes/MindControlLedger.h"
#include "game/research/TechCostCalculator.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechRegistry.h"
#include "game/probes/ProbeActionMutations.h"
#include "game/probes/ProbeActionResult.h"
#include "game/probes/ProbeRules.h"
#include "game/probes/ProbeTarget.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/population/RiotLatch.h"
#include "lib/LuaRuntime.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <optional>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

struct ProbeGame_
{
    FactionFixture fixtures;
    Faction* pPlayer = nullptr;
    Faction* pAi = nullptr;
    std::unique_ptr<TechCostConfig_t> pTechCostConfig;

    ProbeGame_(bool bWithTechs = false)
    {
        if (bWithTechs)
        {
            fixtures.dataContext.techRegistry = std::make_unique<TechRegistry>();
            fixtures.dataContext.techRegistry->Load(FixturePath("techs.json"));
            // luaRuntime is already created by BaseFixture; reuse it.
            TechCostConfigParser techCostParser;
            pTechCostConfig = std::make_unique<TechCostConfig_t>(techCostParser.ParseConfig(
                actest::FixturePath("tech_cost.lua"),
                *fixtures.dataContext.luaRuntime));
            fixtures.dataContext.techCostCalculator = std::make_unique<TechCostCalculator>(
                *pTechCostConfig, *fixtures.dataContext.luaRuntime);
        }

        FillLand(fixtures.map);

        auto pFactionA = std::make_unique<Faction>(
            fixtures.state.AllocateFactionId(), true, fixtures.factionDefinition,
            fixtures.dataContext, fixtures.state.GetWorldMap(), fixtures.settings, actest::k_TestFactionSeed);
        auto pFactionB = std::make_unique<Faction>(
            fixtures.state.AllocateFactionId(), false, fixtures.factionDefinition,
            fixtures.dataContext, fixtures.state.GetWorldMap(), fixtures.settings, actest::k_TestFactionSeed);

        pPlayer = &fixtures.state.AddFaction(std::move(pFactionA));
        pAi = &fixtures.state.AddFaction(std::move(pFactionB));
    }

    BaseManager& MakeBase(Faction& rFaction, int x, int y)
    {
        Tile* pTile = fixtures.state.GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        BaseManager* pBase = rFaction.CreateBase(
            fixtures.state.AllocateBaseId(), "TestBase", pTile,
            fixtures.state.GetTileEffects());
        REQUIRE(pBase);
        return *pBase;
    }

    Unit& MakeUnit(Faction& rFaction, int x, int y,
                   const std::vector<std::string>& rComponentIds,
                   BaseManager* pHomeBase = nullptr)
    {
        std::vector<UnitSlotConfig_t> slots;
        std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
        int slotIndex = 0;
        for (const std::string& rId : rComponentIds)
        {
            const UnitComponentConfig_t* pComponent = fixtures.unitComponents.Find(rId);
            REQUIRE(pComponent);
            UnitSlotConfig_t slot;
            slot.id = "slot_" + std::to_string(slotIndex++);
            slot.displayName = slot.id;
            slot.componentType = pComponent->type;
            slot.required = true;
            assigned[slot.id] = pComponent;
            slots.push_back(slot);
        }
        fixtures.designs.emplace_back(slots, assigned);

        Tile* pTile = fixtures.state.GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        return rFaction.GetUnitManager().CreateUnit(
            fixtures.state.AllocateUnitId(), fixtures.designs.back(),
            fixtures.state.GetWorldMap().GetUnitPositions(), *pTile,
            pHomeBase);
    }
};

// Raising RISK far past any probe's strength zeroes the success rate, so the mission fails
// after its cost is paid.
void ForceMissionFailure_(ProbeGame_& rGame, ProbeActionId_t actionId)
{
    for (ProbeActionConfig_t& rAction : rGame.fixtures.dataContext.probeActionsConfig->actions)
    {
        if (rAction.id == actionId)
        {
            rAction.risk = 100;
            return;
        }
    }
    FAIL("fixture has no such probe action");
}

// A paid base mission needs a target that is not the owner's HQ, so the owner founds its HQ
// first, away from the target.
BaseManager& MakeMindControlTarget_(ProbeGame_& rGame)
{
    rGame.MakeBase(*rGame.pAi, 8, 14);
    BaseManager& rTarget = rGame.MakeBase(*rGame.pAi, 8, 8);
    REQUIRE_FALSE(IsHeadquarters(rTarget));
    return rTarget;
}

DiplomaticStatus_t PlayerStatusWithAi_(ProbeGame_& rGame)
{
    return rGame.fixtures.state.GetDiplomacyLedger().GetStatus(rGame.pPlayer->GetFactionId(),
                                                        rGame.pAi->GetFactionId());
}

// Signed before any probe is placed, since a Treaty moves each side out of the other's
// territory.
void SignTreaty_(ProbeGame_& rGame)
{
    ApplyStatusChange(rGame.fixtures.state, rGame.pPlayer->GetFactionId(), rGame.pAi->GetFactionId(),
                      DiplomaticStatus_t::Treaty);
}

} // namespace

TEST_CASE("TryProbeAction infiltrate sets diplomacy infiltration", "[probe][action]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);
    REQUIRE(probe.GetFlag(RuleFlagId_t::ProbeTeam));

    const int xpBefore = probe.GetXp();
    const ProbeActionResult_t result =
        game.fixtures.state.GetProbeActions().TryProbeAction(
            probe, ProbeActionId_t::Infiltrate, enemy.GetTile(), game.fixtures.state);

    REQUIRE((result.outcome == ProbeActionOutcome_t::Succeeded
             || result.outcome == ProbeActionOutcome_t::Captured));
    CHECK(game.fixtures.state.GetDiplomacyLedger().HasInfiltration(
        game.pPlayer->GetFactionId(), game.pAi->GetFactionId()));
    if (result.outcome == ProbeActionOutcome_t::Succeeded)
    {
        CHECK(probe.GetXp() >= xpBefore);
    }
}

TEST_CASE("ForceRiot sets pending riot until CommitRiot", "[probe][riot]")
{
    BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 8);
    CHECK_FALSE(base.GetPopulation().IsRioting());
    base.GetPopulation().ForceRiot(/*turns=*/1);
    CHECK(base.GetPopulation().IsPendingRiot());
    CHECK_FALSE(base.GetPopulation().IsRioting());
    base.GetPopulation().CommitMood();
    CHECK(base.GetPopulation().IsRioting());
}

TEST_CASE("TryProbeAction drain_energy transfers credits", "[probe][action]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    game.pAi->GetEconomy().AddEnergy(1000);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    const int playerBefore = game.pPlayer->GetEconomy().GetEnergy();
    const int aiBefore = game.pAi->GetEconomy().GetEnergy();

    const ProbeActionResult_t result =
        game.fixtures.state.GetProbeActions().TryProbeAction(
            probe, ProbeActionId_t::DrainEnergy, enemy.GetTile(), game.fixtures.state);

    REQUIRE((result.outcome == ProbeActionOutcome_t::Succeeded
             || result.outcome == ProbeActionOutcome_t::Captured));
    CHECK(game.pPlayer->GetEconomy().GetEnergy() >= playerBefore);
    CHECK(game.pAi->GetEconomy().GetEnergy() <= aiBefore);
}

TEST_CASE("TryProbeAction steal_tech grants a prereq-met tech the target knows",
          "[probe][action][steal]")
{
    ProbeGame_ game(/*bWithTechs=*/true);
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    // Target knows a root tech and a gated tech; actor has neither.
    game.pAi->GetResearch().AddDiscoveredTech("build_tech");
    game.pAi->GetResearch().AddDiscoveredTech("advanced_build");

    const ProbeActionResult_t result =
        game.fixtures.state.GetProbeActions().TryProbeAction(
            probe, ProbeActionId_t::StealTech, enemy.GetTile(), game.fixtures.state);

    REQUIRE((result.outcome == ProbeActionOutcome_t::Succeeded
             || result.outcome == ProbeActionOutcome_t::Captured));
    // Only build_tech is in the actor's AvailableTechs ∩ target discoveries.
    const auto* pStolen = std::get_if<ProbeStolenTech_t>(&result.detail);
    REQUIRE(pStolen);
    CHECK(pStolen->techId == "build_tech");
    CHECK(game.pPlayer->GetResearch().HasDiscoveredTech("build_tech"));
    CHECK_FALSE(game.pPlayer->GetResearch().HasDiscoveredTech("advanced_build"));
}

TEST_CASE("TryProbeAction steal_tech skips techs whose prerequisites the actor lacks",
          "[probe][action][steal]")
{
    ProbeGame_ game(/*bWithTechs=*/true);
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    // Target only has the gated tech; actor lacks build_tech so nothing is stealable.
    game.pAi->GetResearch().AddDiscoveredTech("advanced_build");

    const ProbeActionResult_t result =
        game.fixtures.state.GetProbeActions().TryProbeAction(
            probe, ProbeActionId_t::StealTech, enemy.GetTile(), game.fixtures.state);

    REQUIRE((result.outcome == ProbeActionOutcome_t::Succeeded
             || result.outcome == ProbeActionOutcome_t::Captured));
    const auto* pStatus = std::get_if<ProbeActionStatus_t>(&result.detail);
    REQUIRE(pStatus);
    CHECK(*pStatus == ProbeActionStatus_t::NoTech);
    CHECK_FALSE(game.pPlayer->GetResearch().HasDiscoveredTech("advanced_build"));
}

TEST_CASE("steal_tech effect picks randomly among eligible techs", "[probe][action][steal]")
{
    std::unordered_set<TechId> seen;
    for (uint32_t seed = 0; seed < 64 && seen.size() < 2; ++seed)
    {
        ProbeGame_ game(/*bWithTechs=*/true);
        BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
        BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
        Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

        game.pAi->GetResearch().AddDiscoveredTech("build_tech");
        game.pAi->GetResearch().AddDiscoveredTech("grow_tech");

        const ProbeActionConfig_t* pAction =
            game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::StealTech);
        REQUIRE(pAction);

        const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
            probe, enemy.GetTile(), ProbeTargetKind_t::Base, game.fixtures.state);
        REQUIRE(target.has_value());

        ProbeActionResult_t result;
        std::mt19937 rng(seed);
        REQUIRE(ApplyProbeActionEffect(probe, *pAction, *target, game.fixtures.state,
                                       {}, result, rng));
        const auto* pStolen = std::get_if<ProbeStolenTech_t>(&result.detail);
        REQUIRE(pStolen);
        REQUIRE((pStolen->techId == "build_tech" || pStolen->techId == "grow_tech"));
        seen.insert(pStolen->techId);
    }
    CHECK(seen.size() == 2);
}

TEST_CASE("A second probe against the same base uses risk_repeat", "[probe][action][risk]")
{
    // risk_repeat was parsed and stored but unreachable: TryProbeAction took a bRepeatAtBase
    // flag defaulting to false that no caller ever set. The executor owns the history now.
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);

    // steal_tech is the fixture action that declares risk_repeat (1) distinct from risk (0).
    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::StealTech);
    REQUIRE(pAction);
    REQUIRE(pAction->riskRepeat.has_value());
    REQUIRE(*pAction->riskRepeat != pAction->risk);

    // Two probes, so whether the first survives does not decide the second attempt.
    Unit& first = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);
    const ProbeActionResult_t firstResult = game.fixtures.state.GetProbeActions().TryProbeAction(
        first, ProbeActionId_t::StealTech, enemy.GetTile(), game.fixtures.state);
    CHECK(firstResult.chances.risk == pAction->risk);

    Unit& second = game.MakeUnit(*game.pPlayer, 9, 9, {"test_chassis", "Probe_Team"}, &home);
    const ProbeActionResult_t secondResult = game.fixtures.state.GetProbeActions().TryProbeAction(
        second, ProbeActionId_t::StealTech, enemy.GetTile(), game.fixtures.state);
    CHECK(secondResult.chances.risk == *pAction->riskRepeat);
}

TEST_CASE("Sabotage retires the destroyed copy's deploy record", "[probe][action][sabotage]")
{
    // Every other destruction path (raze, orbital attack, intercept fail) notifies the owning
    // faction so a cooling ASAT/interceptor record is retired. Sabotage did not, so
    // CountReadyBuildings kept subtracting a charge for a building that no longer existed.
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    // Two copies, one of them cooling: the sabotaged base's, so the *other* base's copy is what
    // exposes a stale record. Without the notification the phantom charge keeps suppressing it.
    BaseManager& enemySecond = game.MakeBase(*game.pAi, 8, 14);
    enemy.GetBuildingManager().AddBuilding("test_facility_a");
    enemySecond.GetBuildingManager().AddBuilding("test_facility_a");
    game.pAi->DeployBuilding(enemy.GetBaseId(), "test_facility_a", /*readyMissionYear*/ 100);
    REQUIRE(game.pAi->CountBuildings("test_facility_a") == 2);
    REQUIRE(game.pAi->CountReadyBuildings("test_facility_a", /*missionYear*/ 1) == 1);

    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::SabotageRandom);
    REQUIRE(pAction);
    const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
        probe, enemy.GetTile(), ProbeTargetKind_t::Base, game.fixtures.state);
    REQUIRE(target.has_value());

    ProbeActionResult_t result;
    std::mt19937 rng(7);
    REQUIRE(ApplyProbeActionEffect(probe, *pAction, *target, game.fixtures.state,
                                   {}, result, rng));

    // The sabotaged copy is gone and took its cooldown record with it, so the surviving copy in
    // the other base reads ready. Without the notification the stale charge would suppress it.
    CHECK(game.pAi->CountBuildings("test_facility_a") == 1);
    CHECK(game.pAi->CountReadyBuildings("test_facility_a", /*missionYear*/ 1) == 1);
}

TEST_CASE("Targeted sabotage of a facility the base lacks fails", "[probe][action][sabotage]")
{
    // An empty or unknown facility id used to fall through to the random branch, and a
    // non-empty missing id still reported ProbeDestroyedFacility_t after DestroyBuilding's
    // documented no-op — claiming a kill that never happened.
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    enemy.GetBuildingManager().AddBuilding("Command_Center");

    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::SabotageFacility);
    REQUIRE(pAction);
    const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
        probe, enemy.GetTile(), ProbeTargetKind_t::Base, game.fixtures.state);
    REQUIRE(target.has_value());

    ProbeActionResult_t result;
    std::mt19937 rng(7);
    // Naming a facility this base does not have fails outright...
    CHECK_FALSE(ApplyProbeActionEffect(probe, *pAction, *target, game.fixtures.state,
                                       {"flat_nutrient"}, result, rng));
    CHECK(std::get_if<ProbeActionStatus_t>(&result.detail) != nullptr);
    // ...and does not quietly destroy the building it *does* have instead.
    CHECK(game.pAi->CountBuildings("Command_Center") == 1);

    // The legal case still works.
    ProbeActionResult_t hit;
    REQUIRE(ApplyProbeActionEffect(probe, *pAction, *target, game.fixtures.state,
                                   {"Command_Center"}, hit, rng));
    CHECK(game.pAi->CountBuildings("Command_Center") == 0);
}

TEST_CASE("sabotage_random effect picks randomly among non-HQ buildings",
          "[probe][action][sabotage]")
{
    std::unordered_set<BuildingId_t> seen;
    for (uint32_t seed = 0; seed < 64 && seen.size() < 2; ++seed)
    {
        ProbeGame_ game;
        BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
        BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
        Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

        enemy.GetBuildingManager().AddBuilding("flat_nutrient");
        enemy.GetBuildingManager().AddBuilding("Command_Center");

        const ProbeActionConfig_t* pAction =
            game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::SabotageRandom);
        REQUIRE(pAction);

        const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
            probe, enemy.GetTile(), ProbeTargetKind_t::Base, game.fixtures.state);
        REQUIRE(target.has_value());

        ProbeActionResult_t result;
        std::mt19937 rng(seed);
        REQUIRE(ApplyProbeActionEffect(probe, *pAction, *target, game.fixtures.state,
                                       {}, result, rng));
        const auto* pFacility = std::get_if<ProbeDestroyedFacility_t>(&result.detail);
        REQUIRE(pFacility);
        REQUIRE((pFacility->buildingId == "flat_nutrient"
                 || pFacility->buildingId == "Command_Center"));
        seen.insert(pFacility->buildingId);
    }
    CHECK(seen.size() == 2);
}

TEST_CASE("genetic_plague halves base population via ModifyPopulation effect",
          "[probe][action][plague]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    while (enemy.GetPopulation().GetSize() < 5)
    {
        enemy.GetPopulation().AddPop();
    }
    REQUIRE(enemy.GetPopulation().GetSize() == 5);

    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);
    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::GeneticPlague);
    REQUIRE(pAction);

    const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
        probe, enemy.GetTile(), ProbeTargetKind_t::Base, game.fixtures.state);
    REQUIRE(target.has_value());

    ProbeActionResult_t result;
    std::mt19937 rng(actest::k_TestRngSeed);
    REQUIRE(ApplyProbeActionEffect(probe, *pAction, *target, game.fixtures.state,
                                   {}, result, rng));
    CHECK(enemy.GetPopulation().GetSize() == 3);
    const auto* pKilled = std::get_if<ProbePopulationKilled_t>(&result.detail);
    REQUIRE(pKilled);
    CHECK(pKilled->count == 2);

    // Genetic warfare is a Simple atrocity, recorded against the target base's owner.
    const AtrocityLedger& rLedger = game.fixtures.state.GetAtrocityLedger();
    REQUIRE(rLedger.Records().size() == 1);
    CHECK(rLedger.Records().front().perpetrator == game.pPlayer->GetFactionId());
    CHECK(rLedger.Records().front().victim == game.pAi->GetFactionId());
    CHECK(rLedger.Records().front().severity == AtrocitySeverityId_t::Simple);
    CHECK(rLedger.IsSanctioned(game.pPlayer->GetFactionId(), game.fixtures.state.GetMissionYear()));
}

TEST_CASE("genetic_plague never empties a base (min_size 1)", "[probe][action][plague]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    while (enemy.GetPopulation().GetSize() > 1)
    {
        enemy.GetPopulation().RemovePop();
    }
    REQUIRE(enemy.GetPopulation().GetSize() == 1);

    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);
    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::GeneticPlague);
    REQUIRE(pAction);

    const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
        probe, enemy.GetTile(), ProbeTargetKind_t::Base, game.fixtures.state);
    REQUIRE(target.has_value());

    ProbeActionResult_t result;
    std::mt19937 rng(actest::k_TestRngSeed);
    REQUIRE(ApplyProbeActionEffect(probe, *pAction, *target, game.fixtures.state,
                                   {}, result, rng));
    CHECK(enemy.GetPopulation().GetSize() == 1);
    const auto* pKilled = std::get_if<ProbePopulationKilled_t>(&result.detail);
    REQUIRE(pKilled);
    CHECK(pKilled->count == 0);
}

// Ordering contract: a probe may only act on something its faction can actually see, so a
// concealed occupant resolves to no target and the click stays a move. Bumping into it
// contact-reveals it (UnitOrderExecutor::RevealBlockingUnits_ on a blocked step), and the
// next attempt can act.
TEST_CASE("Probe cannot target a concealed unit until contact reveals it",
          "[probe][target][visibility]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);
    Unit& cloaked = game.MakeUnit(*game.pAi, 8, 8, {"test_chassis", "Cloaking_Device"});
    const Tile& rTargetTile = cloaked.GetTile();

    REQUIRE_FALSE(IsUnitVisibleTo(*game.pPlayer, cloaked));
    CHECK_FALSE(ResolveProbeTarget(probe, rTargetTile, ProbeTargetKind_t::Unit, game.fixtures.state)
                    .has_value());
    CHECK_FALSE(game.fixtures.state.GetProbeActions().CanOpenProbeActions(
        probe, rTargetTile, game.fixtures.state));

    game.pPlayer->GetRevealedUnits().Reveal(cloaked);

    const std::optional<ProbeTarget_t> target =
        ResolveProbeTarget(probe, rTargetTile, ProbeTargetKind_t::Unit, game.fixtures.state);
    REQUIRE(target.has_value());
    CHECK(KindOf(target->ref) == ProbeTargetKind_t::Unit);
}

TEST_CASE("Probe cannot target a base on a tile its faction has not explored",
          "[probe][target][visibility]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);
    const Tile& rTargetTile = enemy.GetTile();

    FactionExploredMap& rExplored = game.pPlayer->GetExploredMap();
    rExplored.Reset(actest::k_TestMapWidth, actest::k_TestMapHeight);
    REQUIRE_FALSE(rExplored.IsExplored(rTargetTile));
    CHECK_FALSE(ResolveProbeTarget(probe, rTargetTile, ProbeTargetKind_t::Base, game.fixtures.state)
                    .has_value());

    rExplored.Mark(rTargetTile);
    CHECK(ResolveProbeTarget(probe, rTargetTile, ProbeTargetKind_t::Base, game.fixtures.state)
              .has_value());
}

TEST_CASE("Mind Control adds 4 to the actor's mind-control total, not the former owner's",
          "[probe][action][mind-control]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& target = MakeMindControlTarget_(game);
    game.pPlayer->GetEconomy().AddEnergy(10000);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    const ProbeActionResult_t result = game.fixtures.state.GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::MindControlBase, target.GetTile(), game.fixtures.state);

    REQUIRE(result.outcome != ProbeActionOutcome_t::Rejected);
    REQUIRE(&target.GetFaction() == game.pPlayer);
    CHECK(game.fixtures.state.GetMindControlLedger().Total(game.pPlayer->GetFactionId()) == 4);
    CHECK(game.fixtures.state.GetMindControlLedger().Total(game.pAi->GetFactionId()) == 0);
}

TEST_CASE("Total Thought Control adds 4 to the actor's mind-control total",
          "[probe][action][mind-control]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& target = MakeMindControlTarget_(game);
    game.pPlayer->GetEconomy().AddEnergy(10000);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    const ProbeActionResult_t result = game.fixtures.state.GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::TotalThoughtControl, target.GetTile(), game.fixtures.state);

    REQUIRE(result.outcome != ProbeActionOutcome_t::Rejected);
    CHECK(game.fixtures.state.GetMindControlLedger().Total(game.pPlayer->GetFactionId()) == 4);
}

// SMAC adds to the total when the cost is paid, before the success roll.
TEST_CASE("A failed Mind Control still adds 4 to the actor's mind-control total",
          "[probe][action][mind-control]")
{
    ProbeGame_ game;
    ForceMissionFailure_(game, ProbeActionId_t::MindControlBase);
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& target = MakeMindControlTarget_(game);
    game.pPlayer->GetEconomy().AddEnergy(10000);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    const ProbeActionResult_t result = game.fixtures.state.GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::MindControlBase, target.GetTile(), game.fixtures.state);

    REQUIRE(result.outcome == ProbeActionOutcome_t::MissionFailed);
    CHECK(&target.GetFaction() == game.pAi);
    CHECK(game.fixtures.state.GetMindControlLedger().Total(game.pPlayer->GetFactionId()) == 4);
}

TEST_CASE("Subvert Unit adds 1 to the actor's mind-control total", "[probe][action][mind-control]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    game.pPlayer->GetEconomy().AddEnergy(10000);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);
    Unit& victim = game.MakeUnit(*game.pAi, 8, 8, {"test_chassis"});

    const ProbeActionResult_t result = game.fixtures.state.GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::SubvertUnit, victim.GetTile(), game.fixtures.state);

    REQUIRE(result.outcome != ProbeActionOutcome_t::Rejected);
    REQUIRE(&victim.GetFaction() == game.pPlayer);
    CHECK(game.fixtures.state.GetMindControlLedger().Total(game.pPlayer->GetFactionId()) == 1);
    CHECK(game.fixtures.state.GetMindControlLedger().Total(game.pAi->GetFactionId()) == 0);
}

TEST_CASE("A failed Subvert Unit adds nothing to the actor's mind-control total",
          "[probe][action][mind-control]")
{
    ProbeGame_ game;
    ForceMissionFailure_(game, ProbeActionId_t::SubvertUnit);
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    game.pPlayer->GetEconomy().AddEnergy(10000);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);
    Unit& victim = game.MakeUnit(*game.pAi, 8, 8, {"test_chassis"});

    const ProbeActionResult_t result = game.fixtures.state.GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::SubvertUnit, victim.GetTile(), game.fixtures.state);

    REQUIRE(result.outcome == ProbeActionOutcome_t::MissionFailed);
    CHECK(&victim.GetFaction() == game.pAi);
    CHECK(game.fixtures.state.GetMindControlLedger().Total(game.pPlayer->GetFactionId()) == 0);
}

TEST_CASE("The mind-control quote adds the actor's total over the divisor to its first factor",
          "[probe][cost][mind-control]")
{
    ProbeGame_ game;
    BaseManager& targetBase = MakeMindControlTarget_(game);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"});
    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::MindControlBase);
    REQUIRE(pAction);
    REQUIRE(pAction->cost->mindControlDivisor == 4);
    const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
        probe, targetBase.GetTile(), ProbeTargetKind_t::Base, game.fixtures.state);
    REQUIRE(target.has_value());
    const auto quote = [&]
    { return QuoteProbeActionCost(*pAction, *game.pPlayer, *target); };

    // No garrison, so the first factor is the population alone.
    const int population = targetBase.GetPopulation().GetSize();
    const std::optional<int> historyFree = quote();
    REQUIRE(historyFree.has_value());
    REQUIRE(*historyFree % population == 0);
    const int costPerFactorPoint = *historyFree / population;

    MindControlLedger& rLedger = game.fixtures.state.GetMindControlLedger();
    rLedger.Record(game.pPlayer->GetFactionId(), 7);
    CHECK(quote() == (population + 1) * costPerFactorPoint);

    rLedger.Record(game.pPlayer->GetFactionId(), 1);
    CHECK(quote() == (population + 2) * costPerFactorPoint);
}

TEST_CASE("The mind-control quote ignores the target faction's total", "[probe][cost][mind-control]")
{
    ProbeGame_ game;
    BaseManager& targetBase = MakeMindControlTarget_(game);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"});
    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::MindControlBase);
    REQUIRE(pAction);
    const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
        probe, targetBase.GetTile(), ProbeTargetKind_t::Base, game.fixtures.state);
    REQUIRE(target.has_value());

    const std::optional<int> before =
        QuoteProbeActionCost(*pAction, *game.pPlayer, *target);
    game.fixtures.state.GetMindControlLedger().Record(game.pAi->GetFactionId(), 8);
    CHECK(QuoteProbeActionCost(*pAction, *game.pPlayer, *target) == before);
}

TEST_CASE("The subvert quote ignores the actor's mind-control total", "[probe][cost][mind-control]")
{
    ProbeGame_ game;
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"});
    Unit& victim = game.MakeUnit(*game.pAi, 8, 8, {"test_chassis"});
    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::SubvertUnit);
    REQUIRE(pAction);
    const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
        probe, victim.GetTile(), ProbeTargetKind_t::Unit, game.fixtures.state);
    REQUIRE(target.has_value());

    const std::optional<int> before =
        QuoteProbeActionCost(*pAction, *game.pPlayer, *target);
    REQUIRE(before.has_value());
    game.fixtures.state.GetMindControlLedger().Record(game.pPlayer->GetFactionId(), 8);
    CHECK(QuoteProbeActionCost(*pAction, *game.pPlayer, *target) == before);
}

TEST_CASE("An ordinary probe's action against a Treaty partner declares Vendetta",
          "[probe][action][diplomacy][covert]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    SignTreaty_(game);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    const ProbeActionResult_t result = game.fixtures.state.GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::Infiltrate, enemy.GetTile(), game.fixtures.state);

    REQUIRE(result.outcome != ProbeActionOutcome_t::Rejected);
    CHECK(PlayerStatusWithAi_(game) == DiplomaticStatus_t::Vendetta);
}

TEST_CASE("A covert probe's action keeps the Treaty", "[probe][action][diplomacy][covert]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    SignTreaty_(game);
    Unit& probe =
        game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team", "covert"}, &home);

    const ProbeActionResult_t result = game.fixtures.state.GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::Infiltrate, enemy.GetTile(), game.fixtures.state);

    REQUIRE(result.outcome != ProbeActionOutcome_t::Rejected);
    CHECK(PlayerStatusWithAi_(game) == DiplomaticStatus_t::Treaty);
}

TEST_CASE("Subverting a covert unit keeps the Treaty", "[probe][action][diplomacy][covert]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    SignTreaty_(game);
    game.pPlayer->GetEconomy().AddEnergy(10000);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);
    Unit& victim = game.MakeUnit(*game.pAi, 8, 8, {"test_chassis", "covert"});

    const ProbeActionResult_t result = game.fixtures.state.GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::SubvertUnit, victim.GetTile(), game.fixtures.state);

    REQUIRE(result.outcome != ProbeActionOutcome_t::Rejected);
    CHECK(PlayerStatusWithAi_(game) == DiplomaticStatus_t::Treaty);
}

TEST_CASE("A drain never takes more than the treasury, and a better probe takes more",
          "[probe][action]")
{
    ProbeGame_ game;
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    game.pAi->GetEconomy().SpendEnergy(game.pAi->GetEconomy().GetEnergy());
    CHECK(StealEnergyAmount(enemy, *game.pAi, 3) == 0);

    game.pAi->GetEconomy().AddEnergy(1000);
    const int treasury = game.pAi->GetEconomy().GetEnergy();
    const int green = StealEnergyAmount(enemy, *game.pAi, 1);
    const int elite = StealEnergyAmount(enemy, *game.pAi, 6);
    CHECK(green > 0);
    CHECK(elite > green);
    CHECK(elite <= treasury);
}

TEST_CASE("Mission risk is the repeat risk at a probed base, and at least 2 at an HQ facility",
          "[probe][risk]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& hq = game.MakeBase(*game.pAi, 8, 8);
    BaseManager& colony = game.MakeBase(*game.pAi, 14, 8);
    REQUIRE(IsHeadquarters(hq));
    REQUIRE_FALSE(IsHeadquarters(colony));
    const Unit& atHq = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);
    const Unit& atColony =
        game.MakeUnit(*game.pPlayer, 13, 9, {"test_chassis", "Probe_Team"}, &home);
    const ProbeTarget_t hqTarget =
        *ResolveProbeTarget(atHq, hq.GetTile(), ProbeTargetKind_t::Base, game.fixtures.state);
    const ProbeTarget_t colonyTarget =
        *ResolveProbeTarget(atColony, colony.GetTile(), ProbeTargetKind_t::Base, game.fixtures.state);

    const ProbeActionsConfig_t& rActions = *game.fixtures.dataContext.probeActionsConfig;
    // Fixture risks: steal_tech 0, repeated 1; sabotage_facility 1.
    const ProbeActionConfig_t& rSteal = *rActions.Find(ProbeActionId_t::StealTech);
    const ProbeActionConfig_t& rSabotage = *rActions.Find(ProbeActionId_t::SabotageFacility);
    CHECK(MissionRisk(rSteal, colonyTarget, "", false) == 0);
    CHECK(MissionRisk(rSteal, colonyTarget, "", true) == 1);
    CHECK(MissionRisk(rSabotage, colonyTarget, "Command_Center", false) == 1);
    CHECK(MissionRisk(rSabotage, hqTarget, "Command_Center", false) == 2);
}

TEST_CASE("A probe may steal what the target knows and the actor could research now",
          "[probe][tech]")
{
    ProbeGame_ game(/*bWithTechs=*/true);
    game.pAi->GetResearch().AddDiscoveredTech("build_tech");
    game.pAi->GetResearch().AddDiscoveredTech("advanced_build");

    CHECK(StealableTechs(*game.pPlayer, *game.pAi) == std::vector<std::string>{"build_tech"});
    game.pPlayer->GetResearch().AddDiscoveredTech("build_tech");
    CHECK(StealableTechs(*game.pPlayer, *game.pAi)
          == std::vector<std::string>{"advanced_build"});
}

TEST_CASE("A probe's failure scale is void against a target that blocks probe teams, and the "
          "target's success scale always applies",
          "[probe][rules]")
{
    // Declared before the game, whose design holds references into them.
    EffectPool pool;
    UnitComponentConfig_t enhancement;
    ProbeGame_ game;

    BuildingConfig_t hunterSeeker;
    hunterSeeker.id = "test_hunter_seeker";
    hunterSeeker.name = "Test Hunter-Seeker";
    hunterSeeker.effects = {pool.RuleFlag(RuleFlagId_t::BlocksProbeTeams),
                            pool.StatMod(StatId_t::ProbeSuccessScale, -50,
                                         ModifierOp_t::AddPercent)};
    // Assigned before any base exists, so no building manager holds the replaced configs.
    std::vector<BuildingConfig_t> buildings = game.fixtures.buildings().GetAll();
    buildings.push_back(hunterSeeker);
    game.fixtures.buildings().Assign(std::move(buildings));

    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    const Unit& plain = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    enhancement.id = "test_failure_halving";
    enhancement.type = "ability";
    enhancement.effects = {pool.StatMod(StatId_t::ProbeFailureScale, -50, ModifierOp_t::AddPercent,
                                        EffectScope_t::ThisUnit)};
    std::vector<UnitSlotConfig_t> slots;
    std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
    for (const UnitComponentConfig_t* pComponent :
         {game.fixtures.unitComponents.Find("test_chassis"),
          game.fixtures.unitComponents.Find("Probe_Team"),
          static_cast<const UnitComponentConfig_t*>(&enhancement)})
    {
        const std::string slotId = "slot_" + pComponent->id;
        slots.push_back({.id = slotId, .displayName = slotId,
                         .componentType = pComponent->type, .required = true});
        assigned[slotId] = pComponent;
    }
    const UnitDesign& rEnhancedDesign = game.fixtures.designs.emplace_back(slots, assigned);
    const Unit& enhanced = game.pPlayer->GetUnitManager().CreateUnit(
        game.fixtures.state.AllocateUnitId(), rEnhancedDesign,
        game.fixtures.state.GetWorldMap().GetUnitPositions(), *game.fixtures.state.GetWorldMap().GetTile(9, 9),
        &home);

    const auto successAgainstEnemy = [&](const Unit& rProbe)
    {
        const ProbeTarget_t target =
            *ResolveProbeTarget(rProbe, enemy.GetTile(), ProbeTargetKind_t::Base, game.fixtures.state);
        return ProbeChancesFor(rProbe, target, /*risk=*/1).successRate;
    };
    const int plainOpen = successAgainstEnemy(plain);
    CHECK(plainOpen > 0);
    CHECK(successAgainstEnemy(enhanced) > plainOpen);

    enemy.GetBuildingManager().AddBuilding("test_hunter_seeker");
    const int plainBlocked = successAgainstEnemy(plain);
    CHECK(plainBlocked < plainOpen);
    CHECK(successAgainstEnemy(enhanced) == plainBlocked);
}
