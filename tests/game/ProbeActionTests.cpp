#include "GameFixtures.h"

#include "game/CurrentGameState.h"
#include "game/atrocities/AtrocityLedger.h"
#include "game/atrocities/AtrocityConfig.h"

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
#include "game/base/BaseMoodEffects.h"
#include "game/population/PopCompositionConfigParser.h"
#include "game/map/Tile.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingManager.h"
#include "game/population/PopulationManager.h"
#include "game/probes/MindControlLedger.h"
#include "game/research/TechCostCalculator.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechRegistry.h"
#include "game/PlayerInteraction.h"
#include "game/PlayerInteractionQueue.h"
#include "game/effects/ActiveEffect.h"
#include "game/production/IConstructable.h"
#include "game/production/ProductionManager.h"
#include "game/probes/ProbeActionMutations.h"
#include "game/probes/ProbeActionResult.h"
#include "game/probes/ProbeRules.h"
#include "game/probes/ProbeTarget.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/population/RiotLatch.h"
#include "lib/LuaRuntime.h"

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
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
            fixtures.dataContext, fixtures.state.GetWorldMap(), fixtures.settings, fixtures.state.GetUnitIdAllocator(), actest::k_TestFactionSeed);
        auto pFactionB = std::make_unique<Faction>(
            fixtures.state.AllocateFactionId(), false, fixtures.factionDefinition,
            fixtures.dataContext, fixtures.state.GetWorldMap(), fixtures.settings, fixtures.state.GetUnitIdAllocator(), actest::k_TestFactionSeed);

        pPlayer = &fixtures.state.AddFaction(std::move(pFactionA));
        pAi = &fixtures.state.AddFaction(std::move(pFactionB));
    }

    BaseManager& MakeBase(Faction& rFaction, int x, int y)
    {
        Tile* pTile = fixtures.state.GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        BaseManager* pBase = rFaction.CreateBase(
            fixtures.state.AllocateBaseId(), "TestBase", pTile,
            fixtures.state.GetTileEffects(), std::nullopt);
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
            fixtures.state.GetWorldMap().GetUnitPositions(), *pTile, ac::UnitOrigin::Gift(pHomeBase));
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

void ForceSureThing_(ProbeGame_& rGame, ProbeActionId_t actionId)
{
    for (ProbeActionConfig_t& rAction : rGame.fixtures.dataContext.probeActionsConfig->actions)
    {
        if (rAction.id == actionId)
        {
            rAction.risk = -1;
            return;
        }
    }
    FAIL("fixture has no such probe action");
}

bool UnitAlive_(const Faction& rFaction, UnitId_t unitId)
{
    for (const Unit& rUnit : rFaction.GetUnitManager().Units())
    {
        if (rUnit.GetUnitId() == unitId)
        {
            return true;
        }
    }
    return false;
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

// The default fixture occupation formula is 0 so capture tests stay drone-free. Incite
// seats occupation drones; this term is 1 while an occupier window is active.
void UseActiveWindowOccupationDrone_(SessionInputs& rInputs)
{
    rInputs.dataContext.popCompositionConfig->occupationDroneFormula =
        "((assimilation_peak > 0 and assimilation_duration > 0 and "
        "turns_since_conquered < assimilation_duration) and 1 or 0)";
}

const ProbeNoticeInteraction_t& RequireProbeNotice_(GameState& rState, const BaseManager& rBase)
{
    const QueuedInteraction_t* pFront = rState.GetPlayerInteractions().Front();
    REQUIRE(pFront);
    const auto* pNotice = std::get_if<ProbeNoticeInteraction_t>(&pFront->payload);
    REQUIRE(pNotice);
    CHECK(pNotice->subjectName == rBase.GetName());
    REQUIRE(pNotice->cameraTile.has_value());
    CHECK(*pNotice->cameraTile == TileCoord_t{rBase.GetTile().GetX(), rBase.GetTile().GetY()});
    CHECK(pNotice->targetFactionId == rBase.GetFactionId());
    CHECK(pNotice->actionId == ProbeActionId_t::InciteDroneRiots);
    return *pNotice;
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
    REQUIRE(ResolveFlag(probe, RuleFlagId_t::ProbeTeam));

    const int xpBefore = probe.GetXp();
    const ProbeActionResult_t result =
        game.fixtures.state.GetProbeActions().TryProbeAction(
            probe, ProbeActionId_t::Infiltrate, enemy.GetTile(), game.fixtures.state, BuildingId_t{});

    REQUIRE((result.outcome == ProbeActionOutcome_t::Succeeded
             || result.outcome == ProbeActionOutcome_t::Captured));
    CHECK(game.fixtures.state.GetDiplomacyLedger().HasInfiltration(
        game.pPlayer->GetFactionId(), game.pAi->GetFactionId()));
    const QueuedInteraction_t* pQueued = game.fixtures.state.GetPlayerInteractions().Front();
    REQUIRE(pQueued);
    const auto* pNotice = std::get_if<ProbeNoticeInteraction_t>(&pQueued->payload);
    REQUIRE(pNotice);
    CHECK(pNotice->actionId == ProbeActionId_t::Infiltrate);
    CHECK(std::holds_alternative<ProbeInfiltrated_t>(pNotice->result.detail));
    if (result.outcome == ProbeActionOutcome_t::Succeeded)
    {
        CHECK(probe.GetXp() >= xpBefore);
    }
}

TEST_CASE("A busted infiltrate still sets the infiltration bit", "[probe][action]")
{
    ProbeGame_ game;
    ForceMissionFailure_(game, ProbeActionId_t::Infiltrate);
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    const ProbeActionResult_t result = game.fixtures.state.GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::Infiltrate, enemy.GetTile(), game.fixtures.state, BuildingId_t{});

    REQUIRE(result.outcome == ProbeActionOutcome_t::MissionFailed);
    CHECK(game.fixtures.state.GetDiplomacyLedger().HasInfiltration(
        game.pPlayer->GetFactionId(), game.pAi->GetFactionId()));
    CHECK(std::holds_alternative<ProbeInfiltrated_t>(result.detail));
}

TEST_CASE("TryProbeAction incite riots when the extra drone tips the base", "[probe][riot]")
{
    ProbeGame_ game;
    UseActiveWindowOccupationDrone_(game.fixtures);
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    const ProbeActionResult_t result =
        game.fixtures.state.GetProbeActions().TryProbeAction(
            probe, ProbeActionId_t::InciteDroneRiots, enemy.GetTile(), game.fixtures.state,
            BuildingId_t{});

    REQUIRE((result.outcome == ProbeActionOutcome_t::Succeeded
             || result.outcome == ProbeActionOutcome_t::Captured));
    CHECK(std::holds_alternative<ProbeRiot_t>(result.detail));
    CHECK(enemy.GetPopulation().IsRioting());
    // Riot effects apply before the first Mood commit, as SMAC's flag blocks the next production.
    const RiotTier_t* pTier = ActiveRiotTierFor(enemy);
    REQUIRE(pTier);
    CHECK(pTier->minTurns == 1);
    CHECK(std::holds_alternative<ProbeRiot_t>(
        RequireProbeNotice_(game.fixtures.state, enemy).result.detail));
}

TEST_CASE("TryProbeAction incite reports increased activity when talents still outnumber drones",
          "[probe][riot]")
{
    ProbeGame_ game;
    UseActiveWindowOccupationDrone_(game.fixtures);
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    enemy.GetPopulation().AddPop();
    enemy.GetBuildingManager().AddBuilding("talent_academy");
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    const ProbeActionResult_t result =
        game.fixtures.state.GetProbeActions().TryProbeAction(
            probe, ProbeActionId_t::InciteDroneRiots, enemy.GetTile(), game.fixtures.state,
            BuildingId_t{});

    REQUIRE((result.outcome == ProbeActionOutcome_t::Succeeded
             || result.outcome == ProbeActionOutcome_t::Captured));
    CHECK(std::holds_alternative<ProbeIncreasedActivity_t>(result.detail));
    CHECK_FALSE(enemy.GetPopulation().IsRioting());
    CHECK(enemy.GetPopulation().GetDroneCount() >= 1);
    CHECK(std::holds_alternative<ProbeIncreasedActivity_t>(
        RequireProbeNotice_(game.fixtures.state, enemy).result.detail));
}

TEST_CASE("TryProbeAction incite on an already-rioting base stays Riot", "[probe][riot]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    MoodState_t riot;
    riot.bRioting = true;
    riot.consecutiveRiotTurns = 1;
    enemy.GetPopulation().RestoreMoodState(riot);
    REQUIRE(enemy.GetPopulation().IsRioting());
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    const ProbeActionResult_t result =
        game.fixtures.state.GetProbeActions().TryProbeAction(
            probe, ProbeActionId_t::InciteDroneRiots, enemy.GetTile(), game.fixtures.state,
            BuildingId_t{});

    REQUIRE((result.outcome == ProbeActionOutcome_t::Succeeded
             || result.outcome == ProbeActionOutcome_t::Captured));
    CHECK(std::holds_alternative<ProbeRiot_t>(result.detail));
    CHECK(enemy.GetPopulation().IsRioting());
    CHECK(std::holds_alternative<ProbeRiot_t>(
        RequireProbeNotice_(game.fixtures.state, enemy).result.detail));
}

TEST_CASE("TryProbeAction incite notifies the player when their base is the target",
          "[probe][riot]")
{
    ProbeGame_ game;
    UseActiveWindowOccupationDrone_(game.fixtures);
    BaseManager& home = game.MakeBase(*game.pAi, 8, 14);
    BaseManager& playerBase = game.MakeBase(*game.pPlayer, 8, 8);
    Unit& probe = game.MakeUnit(*game.pAi, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    const ProbeActionResult_t result =
        game.fixtures.state.GetProbeActions().TryProbeAction(
            probe, ProbeActionId_t::InciteDroneRiots, playerBase.GetTile(), game.fixtures.state,
            BuildingId_t{});

    REQUIRE((result.outcome == ProbeActionOutcome_t::Succeeded
             || result.outcome == ProbeActionOutcome_t::Captured));
    CHECK(std::holds_alternative<ProbeRiot_t>(
        RequireProbeNotice_(game.fixtures.state, playerBase).result.detail));
}

TEST_CASE("TryProbeAction incite does not notify when neither side is the player",
          "[probe][riot]")
{
    ProbeGame_ game;
    auto pOther = std::make_unique<Faction>(
        game.fixtures.state.AllocateFactionId(), false, game.fixtures.factionDefinition,
        game.fixtures.dataContext, game.fixtures.state.GetWorldMap(), game.fixtures.settings,
        game.fixtures.state.GetUnitIdAllocator(), actest::k_TestFactionSeed);
    Faction& other = game.fixtures.state.AddFaction(std::move(pOther));
    BaseManager& home = game.MakeBase(*game.pAi, 8, 14);
    BaseManager& target = game.MakeBase(other, 8, 8);
    Unit& probe = game.MakeUnit(*game.pAi, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    const ProbeActionResult_t result =
        game.fixtures.state.GetProbeActions().TryProbeAction(
            probe, ProbeActionId_t::InciteDroneRiots, target.GetTile(), game.fixtures.state,
            BuildingId_t{});

    REQUIRE((result.outcome == ProbeActionOutcome_t::Succeeded
             || result.outcome == ProbeActionOutcome_t::Captured));
    CHECK(game.fixtures.state.GetPlayerInteractions().Empty());
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
            probe, ProbeActionId_t::DrainEnergy, enemy.GetTile(), game.fixtures.state, BuildingId_t{});

    REQUIRE((result.outcome == ProbeActionOutcome_t::Succeeded
             || result.outcome == ProbeActionOutcome_t::Captured));
    CHECK(game.pPlayer->GetEconomy().GetEnergy() >= playerBefore);
    CHECK(game.pAi->GetEconomy().GetEnergy() <= aiBefore);
}

TEST_CASE("TryProbeAction steal_tech grants any tech the target knows that the actor does not",
          "[probe][action][steal]")
{
    ProbeGame_ game(/*bWithTechs=*/true);
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    game.pAi->GetResearch().AddDiscoveredTech("advanced_build");

    const ProbeActionResult_t result =
        game.fixtures.state.GetProbeActions().TryProbeAction(
            probe, ProbeActionId_t::StealTech, enemy.GetTile(), game.fixtures.state, BuildingId_t{});

    REQUIRE((result.outcome == ProbeActionOutcome_t::Succeeded
             || result.outcome == ProbeActionOutcome_t::Captured));
    const auto* pStolen = std::get_if<ProbeStolenTech_t>(&result.detail);
    REQUIRE(pStolen);
    CHECK(pStolen->techId == "advanced_build");
    CHECK(game.pPlayer->GetResearch().HasDiscoveredTech("advanced_build"));
    CHECK(enemy.HasStolenResearch());
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

TEST_CASE("Steal-tech with an empty pool copies the target's explored tiles",
          "[probe][action][steal]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);
    Tile& rSecret = game.fixtures.At(8, 6);
    Tile& rUnknown = game.fixtures.At(14, 14);

    FactionExploredMap& rActorMap = game.pPlayer->GetExploredMap();
    FactionExploredMap& rTargetMap = game.pAi->GetExploredMap();
    rActorMap.Reset(actest::k_TestMapWidth, actest::k_TestMapHeight);
    rTargetMap.Reset(actest::k_TestMapWidth, actest::k_TestMapHeight);
    rActorMap.Mark(enemy.GetTile());
    rTargetMap.Mark(enemy.GetTile());
    rTargetMap.Mark(rSecret);
    REQUIRE_FALSE(rActorMap.IsExplored(rSecret));
    REQUIRE_FALSE(rTargetMap.IsExplored(rUnknown));

    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::StealTech);
    REQUIRE(pAction);
    const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
        probe, enemy.GetTile(), ProbeTargetKind_t::Base, game.fixtures.state);
    REQUIRE(target.has_value());

    ProbeActionResult_t result;
    std::mt19937 rng(actest::k_TestRngSeed);
    REQUIRE(ApplyProbeActionEffect(probe, *pAction, *target, game.fixtures.state, {}, result,
                                   rng));
    CHECK(std::holds_alternative<ProbeStolenMap_t>(result.detail));
    CHECK(rActorMap.IsExplored(rSecret));
    CHECK_FALSE(rActorMap.IsExplored(rUnknown));
    CHECK_FALSE(rActorMap.IsFullyExplored());
    CHECK(enemy.HasStolenResearch());
}

TEST_CASE("Steal-tech with an empty pool reports nothing when the actor already has the "
          "target's map",
          "[probe][action][steal]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    FactionExploredMap& rActorMap = game.pPlayer->GetExploredMap();
    FactionExploredMap& rTargetMap = game.pAi->GetExploredMap();
    rActorMap.Reset(actest::k_TestMapWidth, actest::k_TestMapHeight);
    rTargetMap.Reset(actest::k_TestMapWidth, actest::k_TestMapHeight);
    rActorMap.Mark(enemy.GetTile());
    rTargetMap.Mark(enemy.GetTile());
    REQUIRE_FALSE(rActorMap.IsFullyExplored());

    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::StealTech);
    REQUIRE(pAction);
    const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
        probe, enemy.GetTile(), ProbeTargetKind_t::Base, game.fixtures.state);
    REQUIRE(target.has_value());

    ProbeActionResult_t result;
    std::mt19937 rng(actest::k_TestRngSeed);
    REQUIRE(ApplyProbeActionEffect(probe, *pAction, *target, game.fixtures.state, {}, result,
                                   rng));
    CHECK(std::holds_alternative<ProbeNoTech_t>(result.detail));
}

TEST_CASE("Assassinate subtracts a random labs amount, not the whole stock",
          "[probe][action][assassinate]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);
    game.pAi->GetResearch().SetAccumulatedPoints(20);

    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::Assassinate);
    REQUIRE(pAction);
    const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
        probe, enemy.GetTile(), ProbeTargetKind_t::Base, game.fixtures.state);
    REQUIRE(target.has_value());

    ProbeActionResult_t result;
    std::mt19937 rng(actest::k_TestRngSeed);
    REQUIRE(ApplyProbeActionEffect(probe, *pAction, *target, game.fixtures.state, {}, result,
                                   rng));
    const auto* pWiped = std::get_if<ProbeResearchWiped_t>(&result.detail);
    REQUIRE(pWiped);
    CHECK(pWiped->points >= 0);
    CHECK(pWiped->points < 20);
    CHECK(game.pAi->GetResearch().GetAccumulatedPoints() == 20 - pWiped->points);
}

TEST_CASE("A successful steal-tech sets the base stolen-research flag for risk_repeat",
          "[probe][action][risk]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);

    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::StealTech);
    REQUIRE(pAction);
    REQUIRE(pAction->riskRepeat.has_value());
    REQUIRE(*pAction->riskRepeat != pAction->risk);

    Unit& first = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);
    const ProbeActionResult_t firstResult = game.fixtures.state.GetProbeActions().TryProbeAction(
        first, ProbeActionId_t::StealTech, enemy.GetTile(), game.fixtures.state, BuildingId_t{});
    CHECK(firstResult.chances.risk == pAction->risk);
    REQUIRE((firstResult.outcome == ProbeActionOutcome_t::Succeeded
             || firstResult.outcome == ProbeActionOutcome_t::Captured));
    CHECK(enemy.HasStolenResearch());

    Unit& second = game.MakeUnit(*game.pPlayer, 9, 9, {"test_chassis", "Probe_Team"}, &home);
    const ProbeActionResult_t secondResult = game.fixtures.state.GetProbeActions().TryProbeAction(
        second, ProbeActionId_t::StealTech, enemy.GetTile(), game.fixtures.state, BuildingId_t{});
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
    // An unknown facility id used to fall through to the random branch, and a missing id
    // still reported ProbeDestroyedFacility_t after DestroyBuilding's documented no-op —
    // claiming a kill that never happened.
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
    CHECK(std::holds_alternative<ProbeNoTarget_t>(result.detail));
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
    CHECK(enemy.GetPopulation().GetSize() == 2);
    const auto* pKilled = std::get_if<ProbePopulationKilled_t>(&result.detail);
    REQUIRE(pKilled);
    CHECK(pKilled->count == 3);

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

TEST_CASE("genetic_plague wipes nutrients, damages the garrison, and halves a repeat",
          "[probe][action][plague]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    while (enemy.GetPopulation().GetSize() < 5)
    {
        enemy.GetPopulation().AddPop();
    }
    enemy.GetPopulation().SetNutrientStockpile(40);
    Unit& garrison = game.MakeUnit(*game.pAi, 8, 8, {"test_chassis"});
    const int hpBefore = garrison.GetCurrentHp();
    REQUIRE(hpBefore > 0);

    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);
    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::GeneticPlague);
    REQUIRE(pAction);
    const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
        probe, enemy.GetTile(), ProbeTargetKind_t::Base, game.fixtures.state);
    REQUIRE(target.has_value());

    ProbeActionResult_t first;
    std::mt19937 rng(actest::k_TestRngSeed);
    REQUIRE(ApplyProbeActionEffect(probe, *pAction, *target, game.fixtures.state,
                                   {}, first, rng));
    CHECK(enemy.GetPopulation().GetNutrientStockpile() == 0);
    CHECK(enemy.HasPlagueIntroduced());
    CHECK(garrison.GetCurrentHp() < hpBefore);
    const int sizeAfterFirst = enemy.GetPopulation().GetSize();
    REQUIRE(sizeAfterFirst == 2);

    enemy.GetPopulation().SetNutrientStockpile(20);
    ProbeActionResult_t second;
    REQUIRE(ApplyProbeActionEffect(probe, *pAction, *target, game.fixtures.state,
                                   {}, second, rng));
    const auto* pKilled = std::get_if<ProbePopulationKilled_t>(&second.detail);
    REQUIRE(pKilled);
    CHECK(pKilled->count == 1);
    CHECK(enemy.GetPopulation().GetSize() == 1);
    CHECK(enemy.GetPopulation().GetNutrientStockpile() == 0);
}

TEST_CASE("genetic_plague kill count follows defense techs and hospital scale",
          "[probe][action][plague]")
{
    EffectPool pool;
    ProbeGame_ game(/*bWithTechs=*/true);

    BuildingConfig_t hospital;
    hospital.id = "test_hospital";
    hospital.name = "Test Hospital";
    hospital.effects = {pool.StatMod(StatId_t::PlagueDamageScale, -50, ModifierOp_t::AddPercent,
                                     EffectScope_t::ThisBase)};
    std::vector<BuildingConfig_t> buildings = game.fixtures.buildings().GetAll();
    buildings.push_back(hospital);
    game.fixtures.buildings().Assign(std::move(buildings));

    game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    while (enemy.GetPopulation().GetSize() < 5)
    {
        enemy.GetPopulation().AddPop();
    }
    CHECK(GeneticPlagueKilled(enemy) == 3);

    enemy.GetBuildingManager().AddBuilding("test_hospital");
    CHECK(GeneticPlagueKilled(enemy) == 2);

    game.pAi->GetResearch().AddDiscoveredTech("discover_tech");
    game.pAi->GetResearch().AddDiscoveredTech("conquer_tech");
    CHECK(GeneticPlagueKilled(enemy) == 1);
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
        probe, ProbeActionId_t::MindControlBase, target.GetTile(), game.fixtures.state, BuildingId_t{});

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
        probe, ProbeActionId_t::TotalThoughtControl, target.GetTile(), game.fixtures.state, BuildingId_t{});

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
        probe, ProbeActionId_t::MindControlBase, target.GetTile(), game.fixtures.state, BuildingId_t{});

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
        probe, ProbeActionId_t::SubvertUnit, victim.GetTile(), game.fixtures.state, BuildingId_t{});

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
        probe, ProbeActionId_t::SubvertUnit, victim.GetTile(), game.fixtures.state, BuildingId_t{});

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

TEST_CASE("Mind-control quote doubles against a Pact partner", "[probe][cost][mind-control]")
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
    const std::optional<int> before = QuoteProbeActionCost(*pAction, *game.pPlayer, *target);
    REQUIRE(before.has_value());
    ApplyStatusChange(game.fixtures.state, game.pPlayer->GetFactionId(), game.pAi->GetFactionId(),
                      DiplomaticStatus_t::Pact);
    CHECK(QuoteProbeActionCost(*pAction, *game.pPlayer, *target) == *before * 2);
}

TEST_CASE("Mind-control quote halves when the actor is the former owner",
          "[probe][cost][mind-control]")
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
    const std::optional<int> fullCost = QuoteProbeActionCost(*pAction, *game.pPlayer, *target);
    REQUIRE(fullCost.has_value());
    targetBase.GetPopulation().NotifyCaptured(game.pPlayer->GetFactionId(),
                                              game.pAi->GetFactionId());
    CHECK(QuoteProbeActionCost(*pAction, *game.pPlayer, *target) == *fullCost / 2);
}

TEST_CASE("Mind-control quote doubles when the actor has victimized the target",
          "[probe][cost][mind-control]")
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
    const std::optional<int> before = QuoteProbeActionCost(*pAction, *game.pPlayer, *target);
    REQUIRE(before.has_value());

    AtrocityRecord_t record;
    record.perpetrator = game.pPlayer->GetFactionId();
    record.victim = game.pAi->GetFactionId();
    record.severity = AtrocitySeverityId_t::Simple;
    game.fixtures.state.GetAtrocityLedger().Record(record);
    CHECK(QuoteProbeActionCost(*pAction, *game.pPlayer, *target) == *before * 2);
}

TEST_CASE("Mind Control transfers the tile garrison to the actor",
          "[probe][action][mind-control]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& target = MakeMindControlTarget_(game);
    game.pPlayer->GetEconomy().AddEnergy(10000);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);
    Unit& garrison = game.MakeUnit(*game.pAi, 8, 8, {"test_chassis"});
    Unit& neighbor = game.MakeUnit(*game.pAi, 9, 9, {"test_chassis"});

    const ProbeActionResult_t result = game.fixtures.state.GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::MindControlBase, target.GetTile(), game.fixtures.state,
        BuildingId_t{});

    REQUIRE(result.outcome != ProbeActionOutcome_t::Rejected);
    CHECK(&garrison.GetFaction() == game.pPlayer);
    CHECK(&neighbor.GetFaction() == game.pPlayer);
}

namespace
{

const ProbeActionConfig_t& RequireAction_(ProbeGame_& rGame, ProbeActionId_t actionId)
{
    const ProbeActionConfig_t* pAction =
        rGame.fixtures.dataContext.probeActionsConfig->Find(actionId);
    REQUIRE(pAction);
    return *pAction;
}

int Quote_(ProbeGame_& rGame, const Unit& rProbe, const Tile& rTile, ProbeActionId_t actionId,
           ProbeTargetKind_t kind)
{
    const std::optional<ProbeTarget_t> target =
        ResolveProbeTarget(rProbe, rTile, kind, rGame.fixtures.state);
    REQUIRE(target.has_value());
    const std::optional<int> quote =
        QuoteProbeActionCost(RequireAction_(rGame, actionId), rProbe.GetFaction(), *target);
    REQUIRE(quote.has_value());
    return *quote;
}

int QuoteMindControl_(ProbeGame_& rGame, const Unit& rProbe, const BaseManager& rTarget)
{
    return Quote_(rGame, rProbe, rTarget.GetTile(), ProbeActionId_t::MindControlBase,
                  ProbeTargetKind_t::Base);
}

int QuoteSubvert_(ProbeGame_& rGame, const Unit& rProbe, const Unit& rVictim)
{
    return Quote_(rGame, rProbe, rVictim.GetTile(), ProbeActionId_t::SubvertUnit,
                  ProbeTargetKind_t::Unit);
}

// The binary's subversion cost before any multiplier: unit cost * ((energy + 800) / (dist + 2)),
// dividing first.
int SubvertFormula_(ProbeGame_& rGame, const Unit& rVictim, int distance)
{
    const ProbeCostConfig_t& rCost = *RequireAction_(rGame, ProbeActionId_t::SubvertUnit).cost;
    const int energy = rVictim.GetFaction().GetEconomy().GetEnergy();
    const int mineralCost = std::max(1, rVictim.GetDesign().GetBaseCost());
    return mineralCost * ((energy + rCost.energyBias) / (distance + rCost.distBias));
}

// The binary's mind-control cost with no garrison and no mind-control history.
int MindControlFormula_(ProbeGame_& rGame, const BaseManager& rTarget, int distance)
{
    const ProbeCostConfig_t& rCost =
        *RequireAction_(rGame, ProbeActionId_t::MindControlBase).cost;
    const int energy = rTarget.GetFaction().GetEconomy().GetEnergy();
    const int population = rTarget.GetPopulation().GetSize();
    return population * ((energy + rCost.energyBias) / (distance + rCost.distBias));
}

// Leaves the unit formula's division with a remainder, so multiplying first would differ.
void MakeSubvertDivisionInexact_(ProbeGame_& rGame, Faction& rTarget, int distance)
{
    const ProbeCostConfig_t& rCost = *RequireAction_(rGame, ProbeActionId_t::SubvertUnit).cost;
    const int divisor = distance + rCost.distBias;
    const int remainder = (rTarget.GetEconomy().GetEnergy() + rCost.energyBias) % divisor;
    rTarget.GetEconomy().AddEnergy((divisor / 2 - remainder + divisor) % divisor);
}

void SelectDifficulty_(ProbeGame_& rGame, const char* pDifficultyId)
{
    GameRulesConfig_t rules = rGame.fixtures.settings.GetGameRules();
    rules.difficultyId = pDifficultyId;
    rGame.fixtures.settings.SetGameRules(rules);
}

// Must be called before any base exists, so no building manager holds the replaced configs.
void AddDistanceBuildings_(ProbeGame_& rGame, EffectPool& rPool)
{
    const auto distanceScale = [&rPool](StatId_t stat, double factor)
    { return rPool.StatMod(stat, factor, ModifierOp_t::MultiplyGeometric, EffectScope_t::ThisBase); };
    BuildingConfig_t creche;
    creche.id = "test_creche";
    creche.name = "Test Creche";
    creche.effects = {distanceScale(StatId_t::MindControlDistanceScale, 0.5)};
    BuildingConfig_t genejack;
    genejack.id = "test_genejack";
    genejack.name = "Test Genejack";
    genejack.effects = {distanceScale(StatId_t::MindControlDistanceScale, 2)};
    BuildingConfig_t punishment;
    punishment.id = "test_punishment";
    punishment.name = "Test Punishment Sphere";
    punishment.effects = {distanceScale(StatId_t::MindControlDistanceScale, 0.5),
                          distanceScale(StatId_t::SubversionDistanceScale, 0.5)};
    std::vector<BuildingConfig_t> buildings = rGame.fixtures.buildings().GetAll();
    buildings.push_back(creche);
    buildings.push_back(genejack);
    buildings.push_back(punishment);
    rGame.fixtures.buildings().Assign(std::move(buildings));
}

} // namespace

TEST_CASE("The mind-control garrison counts offense and combat plans, and Poly multiplies the "
          "whole stack",
          "[probe][cost][mind-control][garrison]")
{
    ProbeGame_ game;
    BaseManager& targetBase = MakeMindControlTarget_(game);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"});
    const int population = targetBase.GetPopulation().GetSize();
    const int empty = QuoteMindControl_(game, probe, targetBase);
    REQUIRE(empty % population == 0);
    const int perPoint = empty / population;

    // Defense and non-combat plans weigh nothing.
    game.MakeUnit(*game.pAi, 8, 8, {"test_slow_chassis", "test_weak_weapon", "test_armor"});
    game.MakeUnit(*game.pAi, 8, 8, {"test_chassis", "test_colony_pod"});
    CHECK(QuoteMindControl_(game, probe, targetBase) == population * perPoint);

    game.MakeUnit(*game.pAi, 8, 8, {"test_chassis", "test_weapon"});
    game.MakeUnit(*game.pAi, 8, 8, {"test_slow_chassis", "test_weapon", "test_armor"});
    CHECK(QuoteMindControl_(game, probe, targetBase) == (population + 2) * perPoint);

    // Three weighted units, one of them Poly: 3 x (1 + 1).
    game.MakeUnit(*game.pAi, 8, 8, {"test_chassis", "test_weapon", "test_poly_encryption"});
    CHECK(QuoteMindControl_(game, probe, targetBase) == (population + 6) * perPoint);
}

TEST_CASE("A Poly defender raises the garrison multiplier without counting itself",
          "[probe][cost][mind-control][garrison]")
{
    ProbeGame_ game;
    BaseManager& targetBase = MakeMindControlTarget_(game);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"});
    const int population = targetBase.GetPopulation().GetSize();
    const int perPoint = QuoteMindControl_(game, probe, targetBase) / population;

    game.MakeUnit(*game.pAi, 8, 8, {"test_chassis", "test_weapon"});
    game.MakeUnit(*game.pAi, 8, 8, {"test_chassis", "test_weapon"});
    game.MakeUnit(*game.pAi, 8, 8,
                  {"test_slow_chassis", "test_weak_weapon", "test_armor", "test_poly_encryption"});
    CHECK(QuoteMindControl_(game, probe, targetBase) == (population + 4) * perPoint);
}

TEST_CASE("An allied unit in the base counts toward the mind-control garrison",
          "[probe][cost][mind-control][garrison]")
{
    ProbeGame_ game;
    BaseManager& targetBase = MakeMindControlTarget_(game);
    Faction& rAlly = game.fixtures.state.AddFaction(std::make_unique<Faction>(
        game.fixtures.state.AllocateFactionId(), false, game.fixtures.factionDefinition,
        game.fixtures.dataContext, game.fixtures.state.GetWorldMap(), game.fixtures.settings,
        game.fixtures.state.GetUnitIdAllocator(), actest::k_TestFactionSeed));
    ApplyStatusChange(game.fixtures.state, game.pAi->GetFactionId(), rAlly.GetFactionId(),
                      DiplomaticStatus_t::Pact);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"});
    const int population = targetBase.GetPopulation().GetSize();
    const int perPoint = QuoteMindControl_(game, probe, targetBase) / population;

    game.MakeUnit(rAlly, 8, 8, {"test_chassis", "test_weapon"});
    CHECK(QuoteMindControl_(game, probe, targetBase) == (population + 1) * perPoint);
}

TEST_CASE("Subverting a unit divides first, halves all but colony pods and formers, and Poly "
          "doubles",
          "[probe][cost][subvert]")
{
    ProbeGame_ game;
    // No AI base, so no HQ: the binary's distance 12.
    MakeSubvertDivisionInexact_(game, *game.pAi, 12);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"});
    Unit& pod = game.MakeUnit(*game.pAi, 8, 8,
                              {"test_slow_chassis", "test_colony_pod", "test_costly_armor"});
    Unit& combat = game.MakeUnit(*game.pAi, 6, 10,
                                 {"test_slow_chassis", "test_costly_weapon", "test_armor"});
    Unit& polyCombat = game.MakeUnit(
        *game.pAi, 8, 10,
        {"test_slow_chassis", "test_costly_weapon", "test_armor", "test_poly_encryption"});
    REQUIRE(pod.GetDesign().GetBaseCost() > 1);

    CHECK(QuoteSubvert_(game, probe, pod) == SubvertFormula_(game, pod, 12));
    const double halved = SubvertFormula_(game, combat, 12) * 0.5;
    CHECK(QuoteSubvert_(game, probe, combat) == static_cast<int>(std::lround(halved)));
    CHECK(QuoteSubvert_(game, probe, polyCombat) == SubvertFormula_(game, polyCombat, 12));
}

TEST_CASE("Probe quotes measure HQ distance as the binary's vector_dist", "[probe][cost]")
{
    ProbeGame_ game;
    // Twelve columns along one row: 9 by vector_dist, where king moves would count 6.
    game.MakeBase(*game.pAi, 8, 8);
    REQUIRE(game.pAi->GetHeadquarters());
    MakeSubvertDivisionInexact_(game, *game.pAi, 9);
    Unit& probe = game.MakeUnit(*game.pPlayer, 19, 9, {"test_chassis", "Probe_Team"});
    Unit& pod = game.MakeUnit(*game.pAi, 20, 8,
                              {"test_slow_chassis", "test_colony_pod", "test_costly_armor"});

    CHECK(QuoteSubvert_(game, probe, pod) == SubvertFormula_(game, pod, 9));
}

TEST_CASE("The base quote scales its distance by each facility in turn, truncating",
          "[probe][cost][mind-control]")
{
    EffectPool pool;
    ProbeGame_ game;
    AddDistanceBuildings_(game, pool);
    // HQ 6 rows south: vector_dist 4.
    game.MakeBase(*game.pAi, 8, 14);
    BaseManager& target = game.MakeBase(*game.pAi, 8, 8);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"});

    CHECK(QuoteMindControl_(game, probe, target) == MindControlFormula_(game, target, 4));
    target.GetBuildingManager().AddBuilding("test_genejack");
    target.GetBuildingManager().AddBuilding("test_creche");
    CHECK(QuoteMindControl_(game, probe, target) == MindControlFormula_(game, target, 4));
    target.GetBuildingManager().AddBuilding("test_punishment");
    // 4 x 2 x 0.5 x 0.5.
    CHECK(QuoteMindControl_(game, probe, target) == MindControlFormula_(game, target, 2));
}

TEST_CASE("A base one tile from its HQ is quoted after a facility halves the distance to 0",
          "[probe][cost][mind-control]")
{
    EffectPool pool;
    ProbeGame_ game;
    AddDistanceBuildings_(game, pool);
    game.MakeBase(*game.pAi, 9, 9);
    BaseManager& target = game.MakeBase(*game.pAi, 8, 8);
    target.GetBuildingManager().AddBuilding("test_creche");
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 7, {"test_chassis", "Probe_Team"});

    CHECK(QuoteMindControl_(game, probe, target) == MindControlFormula_(game, target, 0));
}

TEST_CASE("A Punishment Sphere at a unit's home base halves its subversion distance",
          "[probe][cost][subvert]")
{
    EffectPool pool;
    ProbeGame_ game;
    AddDistanceBuildings_(game, pool);
    BaseManager& home = game.MakeBase(*game.pAi, 8, 8);
    REQUIRE(IsHeadquarters(home));
    MakeSubvertDivisionInexact_(game, *game.pAi, 4);
    Unit& probe = game.MakeUnit(*game.pPlayer, 19, 9, {"test_chassis", "Probe_Team"});
    Unit& pod = game.MakeUnit(*game.pAi, 20, 8,
                              {"test_slow_chassis", "test_colony_pod", "test_costly_armor"},
                              &home);

    CHECK(QuoteSubvert_(game, probe, pod) == SubvertFormula_(game, pod, 9));
    home.GetBuildingManager().AddBuilding("test_punishment");
    CHECK(QuoteSubvert_(game, probe, pod) == SubvertFormula_(game, pod, 4));
}

TEST_CASE("An AI pays the hardest difficulties' discount against a player, and a player does not",
          "[probe][cost][difficulty]")
{
    ProbeGame_ game;
    BaseManager& playerHome = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& aiHome = game.MakeBase(*game.pAi, 8, 14);
    Unit& aiProbe = game.MakeUnit(*game.pAi, 7, 9, {"test_chassis", "Probe_Team"}, &aiHome);
    Unit& playerPod = game.MakeUnit(*game.pPlayer, 8, 8,
                                    {"test_slow_chassis", "test_colony_pod", "test_costly_armor"},
                                    &playerHome);
    Unit& playerProbe =
        game.MakeUnit(*game.pPlayer, 21, 9, {"test_chassis", "Probe_Team"}, &playerHome);
    Unit& aiPod = game.MakeUnit(*game.pAi, 22, 8,
                                {"test_slow_chassis", "test_colony_pod", "test_costly_armor"},
                                &aiHome);

    const int aiPaysAtTalent = QuoteSubvert_(game, aiProbe, playerPod);
    const int playerPaysAtTalent = QuoteSubvert_(game, playerProbe, aiPod);

    SelectDifficulty_(game, "thinker");
    CHECK(QuoteSubvert_(game, aiProbe, playerPod)
          == static_cast<int>(std::lround(aiPaysAtTalent * 0.75)));
    CHECK(QuoteSubvert_(game, playerProbe, aiPod) == playerPaysAtTalent);

    SelectDifficulty_(game, "transcend");
    CHECK(QuoteSubvert_(game, aiProbe, playerPod)
          == static_cast<int>(std::lround(aiPaysAtTalent * 0.6)));
}

TEST_CASE("Infiltrate against a Treaty partner does not declare Vendetta",
          "[probe][action][diplomacy]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    SignTreaty_(game);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    const ProbeActionResult_t result = game.fixtures.state.GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::Infiltrate, enemy.GetTile(), game.fixtures.state, BuildingId_t{});

    REQUIRE(result.outcome != ProbeActionOutcome_t::Rejected);
    CHECK(PlayerStatusWithAi_(game) == DiplomaticStatus_t::Treaty);
}

TEST_CASE("A completed steal-tech against a Treaty partner declares Vendetta",
          "[probe][action][diplomacy]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    SignTreaty_(game);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    const ProbeActionResult_t result = game.fixtures.state.GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::StealTech, enemy.GetTile(), game.fixtures.state, BuildingId_t{});

    REQUIRE(result.outcome != ProbeActionOutcome_t::Rejected);
    CHECK(PlayerStatusWithAi_(game) == DiplomaticStatus_t::Vendetta);
}

TEST_CASE("A covert probe's steal-tech still declares Vendetta",
          "[probe][action][diplomacy][covert]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    SignTreaty_(game);
    Unit& probe =
        game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team", "covert"}, &home);

    const ProbeActionResult_t result = game.fixtures.state.GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::StealTech, enemy.GetTile(), game.fixtures.state, BuildingId_t{});

    REQUIRE(result.outcome != ProbeActionOutcome_t::Rejected);
    CHECK(PlayerStatusWithAi_(game) == DiplomaticStatus_t::Vendetta);
}

TEST_CASE("Total Thought Control against a Treaty partner does not declare Vendetta",
          "[probe][action][diplomacy]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& target = MakeMindControlTarget_(game);
    SignTreaty_(game);
    game.pPlayer->GetEconomy().AddEnergy(10000);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    const ProbeActionResult_t result = game.fixtures.state.GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::TotalThoughtControl, target.GetTile(), game.fixtures.state,
        BuildingId_t{});

    REQUIRE(result.outcome != ProbeActionOutcome_t::Rejected);
    CHECK(PlayerStatusWithAi_(game) == DiplomaticStatus_t::Treaty);
}

TEST_CASE("Subverting a covert unit against a Treaty partner declares Vendetta",
          "[probe][action][diplomacy][covert]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    SignTreaty_(game);
    game.pPlayer->GetEconomy().AddEnergy(10000);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);
    Unit& victim = game.MakeUnit(*game.pAi, 8, 8, {"test_chassis", "covert"});

    const ProbeActionResult_t result = game.fixtures.state.GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::SubvertUnit, victim.GetTile(), game.fixtures.state, BuildingId_t{});

    REQUIRE(result.outcome != ProbeActionOutcome_t::Rejected);
    CHECK(PlayerStatusWithAi_(game) == DiplomaticStatus_t::Vendetta);
}

TEST_CASE("A drain never takes more than the treasury, and a better probe takes more",
          "[probe][action]")
{
    ProbeGame_ game;
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    game.pAi->GetEconomy().SpendEnergy(game.pAi->GetEconomy().GetEnergy());
    CHECK(QuoteStealEnergy(enemy, *game.pAi, 3).treasury == 0);
    CHECK(StealEnergyAmount(QuoteStealEnergy(enemy, *game.pAi, 3), 0) == 0);

    game.pAi->GetEconomy().AddEnergy(1000);
    const int treasury = game.pAi->GetEconomy().GetEnergy();
    const StealEnergyQuote_t green = QuoteStealEnergy(enemy, *game.pAi, 1);
    const StealEnergyQuote_t elite = QuoteStealEnergy(enemy, *game.pAi, 6);
    CHECK(green.floor > 0);
    CHECK(elite.floor > green.floor);
    CHECK(StealEnergyAmount(elite, 0) <= treasury);
    CHECK(StealEnergyAmount(green, green.remainderBound) <= treasury);
}

TEST_CASE("A drain of 1 energy stores 0", "[probe][action]")
{
    const StealEnergyQuote_t quote{.floor = 1, .remainderBound = 0, .treasury = 1};
    CHECK(StealEnergyAmount(quote, 0) == 0);
}

TEST_CASE("Mission risk uses stolen-research on the base, and at least 2 at an HQ facility",
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
    const ProbeActionConfig_t& rSteal = *rActions.Find(ProbeActionId_t::StealTech);
    const ProbeActionConfig_t& rSabotage = *rActions.Find(ProbeActionId_t::SabotageFacility);
    CHECK(MissionRisk(rSteal, colonyTarget, "") == 0);
    colony.SetStolenResearch();
    CHECK(MissionRisk(rSteal, colonyTarget, "") == 1);
    CHECK(MissionRisk(rSabotage, colonyTarget, "Command_Center") == 1);
    CHECK(MissionRisk(rSabotage, hqTarget, "Command_Center") == 2);
    colony.GetBuildingManager().AddBuilding("Perimeter_Defense");
    CHECK(MissionRisk(rSabotage, colonyTarget, "Perimeter_Defense") == 2);
    colony.GetBuildingManager().AddBuilding("Tachyon_Field");
    CHECK(MissionRisk(rSabotage, colonyTarget, "Tachyon_Field") == 2);
}

TEST_CASE("A probe may steal any tech the target knows that the actor does not",
          "[probe][tech]")
{
    ProbeGame_ game(/*bWithTechs=*/true);
    game.pAi->GetResearch().AddDiscoveredTech("build_tech");
    game.pAi->GetResearch().AddDiscoveredTech("advanced_build");

    CHECK(StealableTechs(*game.pPlayer, *game.pAi)
          == std::vector<std::string>{"build_tech", "advanced_build"});
    game.pPlayer->GetResearch().AddDiscoveredTech("build_tech");
    CHECK(StealableTechs(*game.pPlayer, *game.pAi)
          == std::vector<std::string>{"advanced_build"});
}

namespace
{

// A probe design with its own ability part; the part outlives the design that points at it.
Unit& MakeProbeWithAbility_(ProbeGame_& rGame, const UnitComponentConfig_t& rAbility, int x, int y,
                            BaseManager& rHome)
{
    std::vector<UnitSlotConfig_t> slots;
    std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
    for (const UnitComponentConfig_t* pComponent :
         {rGame.fixtures.unitComponents.Find("test_chassis"),
          rGame.fixtures.unitComponents.Find("Probe_Team"), &rAbility})
    {
        const std::string slotId = "slot_" + pComponent->id;
        slots.push_back({.id = slotId, .displayName = slotId,
                         .componentType = pComponent->type, .required = true});
        assigned[slotId] = pComponent;
    }
    const UnitDesign& rDesign = rGame.fixtures.designs.emplace_back(slots, assigned);
    return rGame.pPlayer->GetUnitManager().CreateUnit(
        rGame.fixtures.state.AllocateUnitId(), rDesign,
        rGame.fixtures.state.GetWorldMap().GetUnitPositions(),
        *rGame.fixtures.state.GetWorldMap().GetTile(x, y), ac::UnitOrigin::Issued(rHome));
}

UnitComponentConfig_t IntrusionAbility_(EffectPool& rPool, const std::string& rId, int intrusion)
{
    UnitComponentConfig_t ability;
    ability.id = rId;
    ability.type = "ability";
    ability.effects = {
        rPool.StatMod(StatId_t::ProbeFailureScale, -50, ModifierOp_t::AddPercent,
                      EffectScope_t::ThisUnit),
        rPool.StatMod(StatId_t::ProbeFailureSave, 50, ModifierOp_t::Add, EffectScope_t::ThisUnit),
        rPool.StatMod(StatId_t::ProbeIntrusion, intrusion, ModifierOp_t::Add,
                      EffectScope_t::ThisUnit)};
    return ability;
}

} // namespace

TEST_CASE("A hunter that matches a probe's intrusion voids its failure scale and save, and the "
          "target's success scale and bust always apply",
          "[probe][rules][hsa]")
{
    // Declared before the game, whose designs hold references into them.
    EffectPool pool;
    UnitComponentConfig_t unaided;
    UnitComponentConfig_t matched;
    UnitComponentConfig_t stronger;
    ProbeGame_ game;

    BuildingConfig_t hunterSeeker;
    hunterSeeker.id = "test_hunter_seeker";
    hunterSeeker.name = "Test Hunter-Seeker";
    hunterSeeker.effects = {pool.StatMod(StatId_t::ProbeHunter, 1),
                            pool.StatMod(StatId_t::ProbeSuccessBust, 50),
                            pool.StatMod(StatId_t::ProbeSuccessScale, -50,
                                         ModifierOp_t::AddPercent)};
    // Assigned before any base exists, so no building manager holds the replaced configs.
    std::vector<BuildingConfig_t> buildings = game.fixtures.buildings().GetAll();
    buildings.push_back(hunterSeeker);
    game.fixtures.buildings().Assign(std::move(buildings));

    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    Unit& plain = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);
    // Intrusion 0 still carries a failure scale and save granted elsewhere.
    unaided = IntrusionAbility_(pool, "test_unaided", 0);
    matched = IntrusionAbility_(pool, "test_matched", 1);
    stronger = IntrusionAbility_(pool, "test_stronger", 2);
    Unit& unaidedProbe = MakeProbeWithAbility_(game, unaided, 9, 9, home);
    Unit& matchedProbe = MakeProbeWithAbility_(game, matched, 7, 7, home);
    Unit& strongerProbe = MakeProbeWithAbility_(game, stronger, 9, 7, home);
    // RISK 1 at starting_intrinsic is 0% success, so the scale comparison has no headroom.
    for (Unit* pProbe : {&plain, &unaidedProbe, &matchedProbe, &strongerProbe})
    {
        pProbe->SetXp(2);
    }

    const auto chancesAgainstEnemy = [&](const Unit& rProbe)
    {
        const ProbeTarget_t target =
            *ResolveProbeTarget(rProbe, enemy.GetTile(), ProbeTargetKind_t::Base, game.fixtures.state);
        return ProbeChancesFor(rProbe, target, /*risk=*/1);
    };

    // No hunter: every probe keeps its scale and save, and nothing busts.
    const ProbeChance_t plainOpen = chancesAgainstEnemy(plain);
    CHECK(plainOpen.successRate > 0);
    CHECK(plainOpen.successBustChance == 0);
    CHECK(chancesAgainstEnemy(unaidedProbe).successRate > plainOpen.successRate);
    CHECK(chancesAgainstEnemy(unaidedProbe).failureSaveChance == 50);

    enemy.GetBuildingManager().AddBuilding("test_hunter_seeker");
    const ProbeChance_t plainHunted = chancesAgainstEnemy(plain);
    CHECK(plainHunted.successRate < plainOpen.successRate);
    CHECK(plainHunted.successBustChance == 50);
    for (const Unit* pNeutralised : {&unaidedProbe, &matchedProbe})
    {
        const ProbeChance_t chances = chancesAgainstEnemy(*pNeutralised);
        CHECK(chances.successRate == plainHunted.successRate);
        CHECK(chances.failureSaveChance == 0);
        CHECK(chances.successBustChance == 50);
    }
    const ProbeChance_t strongerHunted = chancesAgainstEnemy(strongerProbe);
    CHECK(strongerHunted.successRate > plainHunted.successRate);
    CHECK(strongerHunted.failureSaveChance == 50);
    CHECK(strongerHunted.successBustChance == 50);

    const auto contactDestroys = [&](const Unit& rProbe)
    {
        const ProbeTarget_t target =
            *ResolveProbeTarget(rProbe, enemy.GetTile(), ProbeTargetKind_t::Base, game.fixtures.state);
        return ProbeContactDestroys(rProbe, target);
    };
    CHECK(contactDestroys(plain));
    CHECK(contactDestroys(unaidedProbe));
    CHECK_FALSE(contactDestroys(matchedProbe));
    CHECK_FALSE(contactDestroys(strongerProbe));
}

TEST_CASE("A probe the target's hunter out-ranks dies on contact without paying, naming the "
          "hunter's facility",
          "[probe][action][hsa]")
{
    EffectPool pool;
    ProbeGame_ game;

    BuildingConfig_t hunterSeeker;
    hunterSeeker.id = "test_hunter_seeker";
    hunterSeeker.name = "Test Hunter-Seeker";
    hunterSeeker.effects = {pool.StatMod(StatId_t::ProbeHunter, 1)};
    std::vector<BuildingConfig_t> buildings = game.fixtures.buildings().GetAll();
    buildings.push_back(hunterSeeker);
    game.fixtures.buildings().Assign(std::move(buildings));

    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    enemy.GetBuildingManager().AddBuilding("test_hunter_seeker");
    game.pPlayer->GetEconomy().AddEnergy(10000);
    const int energyBefore = game.pPlayer->GetEconomy().GetEnergy();
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);
    const Tile& rEnemyTile = enemy.GetTile();

    CHECK(game.fixtures.state.GetProbeActions().WouldContactDestroyProbe(
        probe, rEnemyTile, game.fixtures.state));
    CHECK(game.fixtures.state.GetProbeActions().CanOpenProbeActions(
        probe, rEnemyTile, game.fixtures.state));

    const ProbeActionResult_t result = game.fixtures.state.GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::MindControlBase, rEnemyTile, game.fixtures.state, BuildingId_t{});

    CHECK(result.outcome == ProbeActionOutcome_t::MissionFailed);
    const auto* pContact = std::get_if<ProbeContactDestroyed_t>(&result.detail);
    REQUIRE(pContact);
    CHECK(pContact->hunterSourceId == "test_hunter_seeker");
    const QueuedInteraction_t* pFront = game.fixtures.state.GetPlayerInteractions().Front();
    REQUIRE(pFront);
    const auto* pNotice = std::get_if<ProbeNoticeInteraction_t>(&pFront->payload);
    REQUIRE(pNotice);
    CHECK(pNotice->facilityName == "Test Hunter-Seeker");
    CHECK(game.pPlayer->GetEconomy().GetEnergy() == energyBefore);
    CHECK(game.fixtures.state.GetMindControlLedger().Total(game.pPlayer->GetFactionId()) == 0);
    CHECK(&enemy.GetFaction() == game.pAi);
}

TEST_CASE("SE Probe +3 is immune to paid actions even with Algorithmic Enhancement",
          "[probe][action][se]")
{
    EffectPool pool;
    UnitComponentConfig_t enhancement;
    ProbeGame_ game;

    BuildingConfig_t immunity;
    immunity.id = "test_probe_immune";
    immunity.name = "Test Probe Immune";
    immunity.effects = {pool.RuleFlag(RuleFlagId_t::ProbeSubversionImmune)};
    std::vector<BuildingConfig_t> buildings = game.fixtures.buildings().GetAll();
    buildings.push_back(immunity);
    game.fixtures.buildings().Assign(std::move(buildings));

    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    enemy.GetBuildingManager().AddBuilding("test_probe_immune");
    game.pPlayer->GetEconomy().AddEnergy(10000);

    enhancement.id = "test_algo";
    enhancement.type = "ability";
    enhancement.effects = {pool.StatMod(StatId_t::ProbeIntrusion, 1, ModifierOp_t::Add,
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
    Unit& enhanced = game.pPlayer->GetUnitManager().CreateUnit(
        game.fixtures.state.AllocateUnitId(), rEnhancedDesign,
        game.fixtures.state.GetWorldMap().GetUnitPositions(),
        *game.fixtures.state.GetWorldMap().GetTile(7, 9), ac::UnitOrigin::Issued(home));

    const ProbeActionConfig_t* pMindControl =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::MindControlBase);
    REQUIRE(pMindControl);
    CHECK_FALSE(game.fixtures.state.GetProbeActions().CanTryProbeAction(
        enhanced, *pMindControl, enemy.GetTile(), game.fixtures.state));

    const ProbeActionConfig_t* pInfiltrate =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::Infiltrate);
    REQUIRE(pInfiltrate);
    CHECK(game.fixtures.state.GetProbeActions().CanTryProbeAction(
        enhanced, *pInfiltrate, enemy.GetTile(), game.fixtures.state));
}

TEST_CASE("A surviving probe returns to the nearest occupiable own base",
          "[probe][action][return]")
{
    ProbeGame_ game;
    ForceSureThing_(game, ProbeActionId_t::Infiltrate);
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& nearer = game.MakeBase(*game.pPlayer, 8, 10);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    const ProbeActionResult_t result = game.fixtures.state.GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::Infiltrate, enemy.GetTile(), game.fixtures.state, BuildingId_t{});

    REQUIRE(result.outcome == ProbeActionOutcome_t::Succeeded);
    CHECK(&probe.GetTile() == &nearer.GetTile());
}

TEST_CASE("A surviving probe with no legal return base is captured",
          "[probe][action][return]")
{
    ProbeGame_ game;
    ForceSureThing_(game, ProbeActionId_t::Infiltrate);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"});
    const UnitId_t probeId = probe.GetUnitId();

    const ProbeActionResult_t result = game.fixtures.state.GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::Infiltrate, enemy.GetTile(), game.fixtures.state, BuildingId_t{});

    CHECK(result.outcome == ProbeActionOutcome_t::Captured);
    CHECK(game.fixtures.state.GetDiplomacyLedger().HasInfiltration(
        game.pPlayer->GetFactionId(), game.pAi->GetFactionId()));
    CHECK_FALSE(UnitAlive_(*game.pPlayer, probeId));
}

TEST_CASE("A sea probe cannot return to a land base and is captured",
          "[probe][action][return]")
{
    ProbeGame_ game;
    ForceSureThing_(game, ProbeActionId_t::Infiltrate);
    game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_sea_chassis", "Probe_Team"});
    const UnitId_t probeId = probe.GetUnitId();

    const ProbeActionResult_t result = game.fixtures.state.GetProbeActions().TryProbeAction(
        probe, ProbeActionId_t::Infiltrate, enemy.GetTile(), game.fixtures.state, BuildingId_t{});

    CHECK(result.outcome == ProbeActionOutcome_t::Captured);
    CHECK_FALSE(UnitAlive_(*game.pPlayer, probeId));
}

TEST_CASE("Sabotage facility is offered and an empty id wipes production",
          "[probe][action][sabotage]")
{
    ProbeGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 8, 2);
    BaseManager& enemy = game.MakeBase(*game.pAi, 8, 8);
    Unit& probe = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "Probe_Team"}, &home);

    const ProbeActionConfig_t* pAction =
        game.fixtures.dataContext.probeActionsConfig->Find(ProbeActionId_t::SabotageFacility);
    REQUIRE(pAction);
    CHECK(game.fixtures.state.GetProbeActions().CanTryProbeAction(
        probe, *pAction, enemy.GetTile(), game.fixtures.state));

    const BuildingConfig_t& rItem = game.fixtures.buildings().Get("test_facility_a");
    enemy.GetProduction().SetProduction(&rItem, enemy.GetBaseEffects());
    REQUIRE(enemy.GetProduction().GetCurrentProduction() != nullptr);

    const std::optional<ProbeTarget_t> target = ResolveProbeTarget(
        probe, enemy.GetTile(), ProbeTargetKind_t::Base, game.fixtures.state);
    REQUIRE(target.has_value());
    ProbeActionResult_t result;
    std::mt19937 rng(7);
    REQUIRE(ApplyProbeActionEffect(probe, *pAction, *target, game.fixtures.state,
                                   {}, result, rng));
    CHECK(std::holds_alternative<ProbeProductionWiped_t>(result.detail));
    CHECK(enemy.GetProduction().GetCurrentProduction() != &rItem);
}
