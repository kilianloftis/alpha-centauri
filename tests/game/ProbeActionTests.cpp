#include "GameFixtures.h"

#include "game/atrocities/AtrocityLedger.h"

#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/FactionRevealedUnits.h"
#include "game/faction/UnitVisibility.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/mind-control/MindControlLedger.h"
#include "game/research/TechCostCalculator.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechRegistry.h"
#include "game/units/ProbeActionConfigParser.h"
#include "game/units/ProbeActionEffects.h"
#include "game/units/ProbeActionResult.h"
#include "game/units/ProbeRules.h"
#include "game/units/ProbeTarget.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/population/calculators/RiotCalculator.h"
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
    GameSettings settings;
    std::unique_ptr<GameState> pState;
    Faction* pPlayer = nullptr;
    Faction* pAi = nullptr;
    std::unique_ptr<TechCostConfig_t> pTechCostConfig;

    ProbeGame_(bool bWithTechs = false)
    {
        fixtures.dataContext.probeActionsConfig = std::make_unique<ProbeActionsConfig_t>(
            ProbeActionConfigParser{}.ParseConfig(FixturePath("probe_actions.json")));

        if (bWithTechs)
        {
            fixtures.dataContext.techRegistry = std::make_unique<TechRegistry>();
            fixtures.dataContext.techRegistry->Load(FixturePath("techs.json"));
            // luaRuntime is already created by BaseFixture; reuse it.
            TechCostConfigParser techCostParser;
            pTechCostConfig = std::make_unique<TechCostConfig_t>(techCostParser.ParseConfig(
                std::string(AC_TEST_FIXTURES_DIR) + "/../../config/tech_cost.lua",
                *fixtures.dataContext.luaRuntime));
            fixtures.dataContext.techCostCalculator = std::make_unique<TechCostCalculator>(
                *pTechCostConfig, *fixtures.dataContext.luaRuntime);
        }

        auto pMap = std::make_unique<WorldMap>(9, 9, actest::TestMapRules());
        for (auto& pTile : pMap->GetTiles())
        {
            pTile->SetElevation(100);
        }
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
            *fixtures.dataContext.moraleCalculator, fixtures.dataContext.tileYieldRules,
            fixtures.dataContext.interactionGrids,
            actest::k_TestRngSeed);

        auto pFactionA = std::make_unique<Faction>(
            pState->AllocateFactionId(), true, fixtures.factionDefinition,
            fixtures.dataContext, pState->GetWorldMap(), settings, actest::k_TestFactionSeed);
        auto pFactionB = std::make_unique<Faction>(
            pState->AllocateFactionId(), false, fixtures.factionDefinition,
            fixtures.dataContext, pState->GetWorldMap(), settings, actest::k_TestFactionSeed);

        pPlayer = &pState->AddFaction(std::move(pFactionA));
        pAi = &pState->AddFaction(std::move(pFactionB));
    }

    BaseManager& MakeBase(Faction& rFaction, int x, int y)
    {
        Tile* pTile = pState->GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        BaseManager* pBase = rFaction.CreateBase(
            pState->AllocateBaseId(), "TestBase", pTile, fixtures.dataContext,
            pState->GetTileEffects(), pState->GetSecretProjectAvailability());
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

        Tile* pTile = pState->GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        return rFaction.GetUnitManager().CreateUnit(
            pState->AllocateUnitId(), fixtures.designs.back(),
            pState->GetWorldMap().GetUnitPositions(), *pTile,
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
    rGame.MakeBase(*rGame.pAi, 7, 7);
    BaseManager& rTarget = rGame.MakeBase(*rGame.pAi, 4, 4);
    REQUIRE_FALSE(IsHeadquarters(rTarget));
    return rTarget;
}

} // namespace

TEST_CASE("TryProbeAction infiltrate sets diplomacy infiltration", "[probe][action]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 1, 1);
    BaseManager& enemy = game.MakeBase(*game.pAi, 4, 4);
    Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"}, &home);
    REQUIRE(probe.GetFlag(RuleFlagId_t::ProbeTeam));

    const int xpBefore = probe.GetXp();
    const ProbeActionResult_t result =
        game.pState->GetProbeActions().TryProbeAction(
            probe, ProbeActionId_t::Infiltrate, enemy.GetTile(), *game.pState,
            game.fixtures.dataContext);

    REQUIRE((result.outcome == ProbeActionOutcome_t::Succeeded
             || result.outcome == ProbeActionOutcome_t::Captured));
    CHECK(game.pState->GetDiplomacyLedger().HasInfiltration(
        game.pPlayer->GetFactionId(), game.pAi->GetFactionId()));
    if (result.outcome == ProbeActionOutcome_t::Succeeded)
    {
        CHECK(probe.GetXp() >= xpBefore);
    }
}

TEST_CASE("ForceRiot sets pending riot until CommitRiot", "[probe][riot]")
{
    BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
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
    BaseManager& home = game.MakeBase(*game.pPlayer, 1, 1);
    BaseManager& enemy = game.MakeBase(*game.pAi, 4, 4);
    game.pAi->GetEconomy().AddEnergy(1000);
    Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"}, &home);

    const int playerBefore = game.pPlayer->GetEconomy().GetEnergy();
    const int aiBefore = game.pAi->GetEconomy().GetEnergy();

    const ProbeActionResult_t result =
        game.pState->GetProbeActions().TryProbeAction(
            probe, ProbeActionId_t::DrainEnergy, enemy.GetTile(), *game.pState,
            game.fixtures.dataContext);

    REQUIRE((result.outcome == ProbeActionOutcome_t::Succeeded
             || result.outcome == ProbeActionOutcome_t::Captured));
    CHECK(game.pPlayer->GetEconomy().GetEnergy() >= playerBefore);
    CHECK(game.pAi->GetEconomy().GetEnergy() <= aiBefore);
}

TEST_CASE("TryProbeAction steal_tech grants a prereq-met tech the target knows",
          "[probe][action][steal]")
{
    ProbeGame_ game(/*bWithTechs=*/true);
    BaseManager& home = game.MakeBase(*game.pPlayer, 1, 1);
    BaseManager& enemy = game.MakeBase(*game.pAi, 4, 4);
    Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"}, &home);

    // Target knows a root tech and a gated tech; actor has neither.
    game.pAi->GetResearch().AddDiscoveredTech("build_tech");
    game.pAi->GetResearch().AddDiscoveredTech("advanced_build");

    const ProbeActionResult_t result =
        game.pState->GetProbeActions().TryProbeAction(
            probe, ProbeActionId_t::StealTech, enemy.GetTile(), *game.pState,
            game.fixtures.dataContext);

    REQUIRE((result.outcome == ProbeActionOutcome_t::Succeeded
             || result.outcome == ProbeActionOutcome_t::Captured));
    // Only build_tech is in actor GetAvailableTechs ∩ target discoveries.
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
    BaseManager& home = game.MakeBase(*game.pPlayer, 1, 1);
    BaseManager& enemy = game.MakeBase(*game.pAi, 4, 4);
    Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"}, &home);

    // Target only has the gated tech; actor lacks build_tech so nothing is stealable.
    game.pAi->GetResearch().AddDiscoveredTech("advanced_build");

    const ProbeActionResult_t result =
        game.pState->GetProbeActions().TryProbeAction(
            probe, ProbeActionId_t::StealTech, enemy.GetTile(), *game.pState,
            game.fixtures.dataContext);

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
        BaseManager& home = game.MakeBase(*game.pPlayer, 1, 1);
        BaseManager& enemy = game.MakeBase(*game.pAi, 4, 4);
        Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"}, &home);

        game.pAi->GetResearch().AddDiscoveredTech("build_tech");
        game.pAi->GetResearch().AddDiscoveredTech("grow_tech");

        const ProbeActionConfig_t* pAction =
            game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::StealTech);
        REQUIRE(pAction);

        const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
            probe, enemy.GetTile(), ProbeTargetKind_t::Base, *game.pState);
        REQUIRE(target.has_value());

        ProbeActionResult_t result;
        std::mt19937 rng(seed);
        REQUIRE(ApplyProbeActionEffect(probe, *pAction, *target, *game.pState,
                                       game.fixtures.dataContext, {}, result, rng));
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
    BaseManager& home = game.MakeBase(*game.pPlayer, 1, 1);
    BaseManager& enemy = game.MakeBase(*game.pAi, 4, 4);

    // steal_tech is the fixture action that declares risk_repeat (1) distinct from risk (0).
    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::StealTech);
    REQUIRE(pAction);
    REQUIRE(pAction->riskRepeat.has_value());
    REQUIRE(*pAction->riskRepeat != pAction->risk);

    // Two probes, so whether the first survives does not decide the second attempt.
    Unit& first = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"}, &home);
    const ProbeActionResult_t firstResult = game.pState->GetProbeActions().TryProbeAction(
        first, ProbeActionId_t::StealTech, enemy.GetTile(), *game.pState,
        game.fixtures.dataContext);
    CHECK(firstResult.chances.risk == pAction->risk);

    Unit& second = game.MakeUnit(*game.pPlayer, 5, 4, {"test_chassis", "Probe_Team"}, &home);
    const ProbeActionResult_t secondResult = game.pState->GetProbeActions().TryProbeAction(
        second, ProbeActionId_t::StealTech, enemy.GetTile(), *game.pState,
        game.fixtures.dataContext);
    CHECK(secondResult.chances.risk == *pAction->riskRepeat);
}

TEST_CASE("Sabotage retires the destroyed copy's deploy record", "[probe][action][sabotage]")
{
    // Every other destruction path (raze, orbital attack, intercept fail) notifies the owning
    // faction so a cooling ASAT/interceptor record is retired. Sabotage did not, so
    // CountReadyBuildings kept subtracting a charge for a building that no longer existed.
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 1, 1);
    BaseManager& enemy = game.MakeBase(*game.pAi, 4, 4);
    Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"}, &home);

    // Two copies, one of them cooling: the sabotaged base's, so the *other* base's copy is what
    // exposes a stale record. Without the notification the phantom charge keeps suppressing it.
    BaseManager& enemySecond = game.MakeBase(*game.pAi, 7, 7);
    enemy.GetBuildingManager().AddBuilding("test_facility_a");
    enemySecond.GetBuildingManager().AddBuilding("test_facility_a");
    game.pAi->DeployBuilding(enemy.GetBaseId(), "test_facility_a", /*readyMissionYear*/ 100);
    REQUIRE(game.pAi->CountBuildings("test_facility_a") == 2);
    REQUIRE(game.pAi->CountReadyBuildings("test_facility_a", /*missionYear*/ 1) == 1);

    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::SabotageRandom);
    REQUIRE(pAction);
    const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
        probe, enemy.GetTile(), ProbeTargetKind_t::Base, *game.pState);
    REQUIRE(target.has_value());

    ProbeActionResult_t result;
    std::mt19937 rng(7);
    REQUIRE(ApplyProbeActionEffect(probe, *pAction, *target, *game.pState,
                                   game.fixtures.dataContext, {}, result, rng));

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
    BaseManager& home = game.MakeBase(*game.pPlayer, 1, 1);
    BaseManager& enemy = game.MakeBase(*game.pAi, 4, 4);
    Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"}, &home);

    enemy.GetBuildingManager().AddBuilding("Command_Center");

    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::SabotageFacility);
    REQUIRE(pAction);
    const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
        probe, enemy.GetTile(), ProbeTargetKind_t::Base, *game.pState);
    REQUIRE(target.has_value());

    ProbeActionResult_t result;
    std::mt19937 rng(7);
    // Naming a facility this base does not have fails outright...
    CHECK_FALSE(ApplyProbeActionEffect(probe, *pAction, *target, *game.pState,
                                       game.fixtures.dataContext, {"flat_nutrient"}, result, rng));
    CHECK(std::get_if<ProbeActionStatus_t>(&result.detail) != nullptr);
    // ...and does not quietly destroy the building it *does* have instead.
    CHECK(game.pAi->CountBuildings("Command_Center") == 1);

    // The legal case still works.
    ProbeActionResult_t hit;
    REQUIRE(ApplyProbeActionEffect(probe, *pAction, *target, *game.pState,
                                   game.fixtures.dataContext, {"Command_Center"}, hit, rng));
    CHECK(game.pAi->CountBuildings("Command_Center") == 0);
}

TEST_CASE("sabotage_random effect picks randomly among non-HQ buildings",
          "[probe][action][sabotage]")
{
    std::unordered_set<BuildingId_t> seen;
    for (uint32_t seed = 0; seed < 64 && seen.size() < 2; ++seed)
    {
        ProbeGame_ game;
        BaseManager& home = game.MakeBase(*game.pPlayer, 1, 1);
        BaseManager& enemy = game.MakeBase(*game.pAi, 4, 4);
        Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"}, &home);

        enemy.GetBuildingManager().AddBuilding("flat_nutrient");
        enemy.GetBuildingManager().AddBuilding("Command_Center");

        const ProbeActionConfig_t* pAction =
            game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::SabotageRandom);
        REQUIRE(pAction);

        const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
            probe, enemy.GetTile(), ProbeTargetKind_t::Base, *game.pState);
        REQUIRE(target.has_value());

        ProbeActionResult_t result;
        std::mt19937 rng(seed);
        REQUIRE(ApplyProbeActionEffect(probe, *pAction, *target, *game.pState,
                                       game.fixtures.dataContext, {}, result, rng));
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
    BaseManager& home = game.MakeBase(*game.pPlayer, 1, 1);
    BaseManager& enemy = game.MakeBase(*game.pAi, 4, 4);
    while (enemy.GetPopulation().GetSize() < 5)
    {
        enemy.GetPopulation().AddPop();
    }
    REQUIRE(enemy.GetPopulation().GetSize() == 5);

    Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"}, &home);
    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::GeneticPlague);
    REQUIRE(pAction);

    const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
        probe, enemy.GetTile(), ProbeTargetKind_t::Base, *game.pState);
    REQUIRE(target.has_value());

    ProbeActionResult_t result;
    std::mt19937 rng(actest::k_TestRngSeed);
    REQUIRE(ApplyProbeActionEffect(probe, *pAction, *target, *game.pState,
                                   game.fixtures.dataContext, {}, result, rng));
    CHECK(enemy.GetPopulation().GetSize() == 3);
    const auto* pKilled = std::get_if<ProbePopulationKilled_t>(&result.detail);
    REQUIRE(pKilled);
    CHECK(pKilled->count == 2);

    // Genetic warfare is a Simple atrocity, recorded against the target base's owner.
    const AtrocityLedger& rLedger = game.pState->GetAtrocityLedger();
    REQUIRE(rLedger.Records().size() == 1);
    CHECK(rLedger.Records().front().perpetrator == game.pPlayer->GetFactionId());
    CHECK(rLedger.Records().front().victim == game.pAi->GetFactionId());
    CHECK(rLedger.Records().front().severity == AtrocitySeverityId_t::Simple);
    CHECK(rLedger.IsSanctioned(game.pPlayer->GetFactionId(), game.pState->GetMissionYear()));
}

TEST_CASE("genetic_plague never empties a base (min_size 1)", "[probe][action][plague]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 1, 1);
    BaseManager& enemy = game.MakeBase(*game.pAi, 4, 4);
    while (enemy.GetPopulation().GetSize() > 1)
    {
        enemy.GetPopulation().RemovePop();
    }
    REQUIRE(enemy.GetPopulation().GetSize() == 1);

    Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"}, &home);
    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::GeneticPlague);
    REQUIRE(pAction);

    const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
        probe, enemy.GetTile(), ProbeTargetKind_t::Base, *game.pState);
    REQUIRE(target.has_value());

    ProbeActionResult_t result;
    std::mt19937 rng(actest::k_TestRngSeed);
    REQUIRE(ApplyProbeActionEffect(probe, *pAction, *target, *game.pState,
                                   game.fixtures.dataContext, {}, result, rng));
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
    BaseManager& home = game.MakeBase(*game.pPlayer, 1, 1);
    Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"}, &home);
    Unit& cloaked = game.MakeUnit(*game.pAi, 4, 4, {"test_chassis", "Cloaking_Device"});
    const Tile& rTargetTile = cloaked.GetTile();

    REQUIRE_FALSE(IsUnitVisibleTo(*game.pPlayer, cloaked, game.pState->GetTileEffects()));
    CHECK_FALSE(ResolveProbeTarget(probe, rTargetTile, ProbeTargetKind_t::Unit, *game.pState)
                    .has_value());
    CHECK_FALSE(game.pState->GetProbeActions().CanOpenProbeActions(
        probe, rTargetTile, *game.pState, game.fixtures.dataContext));

    game.pPlayer->GetRevealedUnits().Reveal(cloaked);

    const std::optional<ProbeTarget_t> target =
        ResolveProbeTarget(probe, rTargetTile, ProbeTargetKind_t::Unit, *game.pState);
    REQUIRE(target.has_value());
    CHECK(KindOf(target->ref) == ProbeTargetKind_t::Unit);
}

TEST_CASE("Probe cannot target a base on a tile its faction has not explored",
          "[probe][target][visibility]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 1, 1);
    BaseManager& enemy = game.MakeBase(*game.pAi, 4, 4);
    Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"}, &home);
    const Tile& rTargetTile = enemy.GetTile();

    FactionExploredMap& rExplored = game.pPlayer->GetExploredMap();
    rExplored.Reset(9, 9);
    REQUIRE_FALSE(rExplored.IsExplored(rTargetTile));
    CHECK_FALSE(ResolveProbeTarget(probe, rTargetTile, ProbeTargetKind_t::Base, *game.pState)
                    .has_value());

    rExplored.Mark(rTargetTile);
    CHECK(ResolveProbeTarget(probe, rTargetTile, ProbeTargetKind_t::Base, *game.pState)
              .has_value());
}

TEST_CASE("Mind Control adds 4 to the actor's mind-control total, not the former owner's",
          "[probe][action][mind-control]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 1, 1);
    BaseManager& target = MakeMindControlTarget_(game);
    game.pPlayer->GetEconomy().AddEnergy(10000);
    Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"}, &home);

    const ProbeActionResult_t result = game.pState->GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::MindControlBase, target.GetTile(), *game.pState,
        game.fixtures.dataContext);

    REQUIRE(result.outcome != ProbeActionOutcome_t::Rejected);
    REQUIRE(&target.GetFaction() == game.pPlayer);
    CHECK(game.pState->GetMindControlLedger().Total(game.pPlayer->GetFactionId()) == 4);
    CHECK(game.pState->GetMindControlLedger().Total(game.pAi->GetFactionId()) == 0);
}

TEST_CASE("Total Thought Control adds 4 to the actor's mind-control total",
          "[probe][action][mind-control]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 1, 1);
    BaseManager& target = MakeMindControlTarget_(game);
    game.pPlayer->GetEconomy().AddEnergy(10000);
    Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"}, &home);

    const ProbeActionResult_t result = game.pState->GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::TotalThoughtControl, target.GetTile(), *game.pState,
        game.fixtures.dataContext);

    REQUIRE(result.outcome != ProbeActionOutcome_t::Rejected);
    CHECK(game.pState->GetMindControlLedger().Total(game.pPlayer->GetFactionId()) == 4);
}

// SMAC adds to the total when the cost is paid, before the success roll.
TEST_CASE("A failed Mind Control still adds 4 to the actor's mind-control total",
          "[probe][action][mind-control]")
{
    ProbeGame_ game;
    ForceMissionFailure_(game, ProbeActionId_t::MindControlBase);
    BaseManager& home = game.MakeBase(*game.pPlayer, 1, 1);
    BaseManager& target = MakeMindControlTarget_(game);
    game.pPlayer->GetEconomy().AddEnergy(10000);
    Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"}, &home);

    const ProbeActionResult_t result = game.pState->GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::MindControlBase, target.GetTile(), *game.pState,
        game.fixtures.dataContext);

    REQUIRE(result.outcome == ProbeActionOutcome_t::MissionFailed);
    CHECK(&target.GetFaction() == game.pAi);
    CHECK(game.pState->GetMindControlLedger().Total(game.pPlayer->GetFactionId()) == 4);
}

TEST_CASE("Subvert Unit adds 1 to the actor's mind-control total", "[probe][action][mind-control]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 1, 1);
    game.pPlayer->GetEconomy().AddEnergy(10000);
    Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"}, &home);
    Unit& victim = game.MakeUnit(*game.pAi, 4, 4, {"test_chassis"});

    const ProbeActionResult_t result = game.pState->GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::SubvertUnit, victim.GetTile(), *game.pState,
        game.fixtures.dataContext);

    REQUIRE(result.outcome != ProbeActionOutcome_t::Rejected);
    REQUIRE(&victim.GetFaction() == game.pPlayer);
    CHECK(game.pState->GetMindControlLedger().Total(game.pPlayer->GetFactionId()) == 1);
    CHECK(game.pState->GetMindControlLedger().Total(game.pAi->GetFactionId()) == 0);
}

TEST_CASE("A failed Subvert Unit adds nothing to the actor's mind-control total",
          "[probe][action][mind-control]")
{
    ProbeGame_ game;
    ForceMissionFailure_(game, ProbeActionId_t::SubvertUnit);
    BaseManager& home = game.MakeBase(*game.pPlayer, 1, 1);
    game.pPlayer->GetEconomy().AddEnergy(10000);
    Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"}, &home);
    Unit& victim = game.MakeUnit(*game.pAi, 4, 4, {"test_chassis"});

    const ProbeActionResult_t result = game.pState->GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::SubvertUnit, victim.GetTile(), *game.pState,
        game.fixtures.dataContext);

    REQUIRE(result.outcome == ProbeActionOutcome_t::MissionFailed);
    CHECK(&victim.GetFaction() == game.pAi);
    CHECK(game.pState->GetMindControlLedger().Total(game.pPlayer->GetFactionId()) == 0);
}

TEST_CASE("The mind-control quote adds the actor's total over the divisor to its first factor",
          "[probe][cost][mind-control]")
{
    ProbeGame_ game;
    BaseManager& targetBase = MakeMindControlTarget_(game);
    Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"});
    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::MindControlBase);
    REQUIRE(pAction);
    REQUIRE(pAction->cost->mindControlDivisor == 4);
    const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
        probe, targetBase.GetTile(), ProbeTargetKind_t::Base, *game.pState);
    REQUIRE(target.has_value());
    const auto quote = [&]
    {
        return QuoteProbeActionCost(*pAction, *game.pPlayer, *target,
                                    game.pState->GetWorldMap(),
                                    game.pState->GetMindControlLedger());
    };

    // No garrison, so the first factor is the population alone.
    const int population = targetBase.GetPopulation().GetSize();
    const std::optional<int> historyFree = quote();
    REQUIRE(historyFree.has_value());
    REQUIRE(*historyFree % population == 0);
    const int costPerFactorPoint = *historyFree / population;

    MindControlLedger& rLedger = game.pState->GetMindControlLedger();
    rLedger.Record(game.pPlayer->GetFactionId(), 7);
    CHECK(quote() == (population + 1) * costPerFactorPoint);

    rLedger.Record(game.pPlayer->GetFactionId(), 1);
    CHECK(quote() == (population + 2) * costPerFactorPoint);
}

TEST_CASE("The mind-control quote ignores the target faction's total", "[probe][cost][mind-control]")
{
    ProbeGame_ game;
    BaseManager& targetBase = MakeMindControlTarget_(game);
    Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"});
    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::MindControlBase);
    REQUIRE(pAction);
    const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
        probe, targetBase.GetTile(), ProbeTargetKind_t::Base, *game.pState);
    REQUIRE(target.has_value());

    const std::optional<int> before =
        QuoteProbeActionCost(*pAction, *game.pPlayer, *target, game.pState->GetWorldMap(),
                             game.pState->GetMindControlLedger());
    game.pState->GetMindControlLedger().Record(game.pAi->GetFactionId(), 8);
    CHECK(QuoteProbeActionCost(*pAction, *game.pPlayer, *target, game.pState->GetWorldMap(),
                               game.pState->GetMindControlLedger())
          == before);
}

TEST_CASE("The subvert quote ignores the actor's mind-control total", "[probe][cost][mind-control]")
{
    ProbeGame_ game;
    Unit& probe = game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "Probe_Team"});
    Unit& victim = game.MakeUnit(*game.pAi, 4, 4, {"test_chassis"});
    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::SubvertUnit);
    REQUIRE(pAction);
    const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
        probe, victim.GetTile(), ProbeTargetKind_t::Unit, *game.pState);
    REQUIRE(target.has_value());

    const std::optional<int> before =
        QuoteProbeActionCost(*pAction, *game.pPlayer, *target, game.pState->GetWorldMap(),
                             game.pState->GetMindControlLedger());
    REQUIRE(before.has_value());
    game.pState->GetMindControlLedger().Record(game.pPlayer->GetFactionId(), 8);
    CHECK(QuoteProbeActionCost(*pAction, *game.pPlayer, *target, game.pState->GetWorldMap(),
                               game.pState->GetMindControlLedger())
          == before);
}
