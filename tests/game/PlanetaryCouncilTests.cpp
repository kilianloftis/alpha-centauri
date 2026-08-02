#include "GameFixtures.h"

#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/council/CouncilAiStub.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/CouncilRulesConfigParser.h"
#include "game/council/PlanetaryCouncil.h"
#include "game/effects/InfiltrationRules.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <stdexcept>

using namespace ac;
using namespace actest;

namespace
{

struct CouncilGame_
{
    FactionFixture fixtures;
    GameSettings settings;
    CouncilProposalRegistry councilRegistry;
    CouncilRulesConfig_t councilRules;
    std::unique_ptr<GameState> pState;
    Faction* pA = nullptr;
    Faction* pB = nullptr;
    Faction* pC = nullptr;
    FactionConfig_t alienDefinition;

    CouncilGame_()
    {
        councilRegistry.Load(FixturePath("council/proposals.json"));
        councilRules = CouncilRulesConfigParser{}.ParseConfig(FixturePath("council/rules.json"));

        auto pMap = std::make_unique<WorldMap>(9, 9);
        for (auto& pTile : pMap->GetTiles())
        {
            pTile->SetElevation(100);
        }
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
            *fixtures.dataContext.moraleCalculator);

        auto pFactionA = std::make_unique<Faction>(
            pState->AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext);
        auto pFactionB = std::make_unique<Faction>(
            pState->AllocateFactionId(), false, fixtures.factionDefinition, fixtures.dataContext);
        auto pFactionC = std::make_unique<Faction>(
            pState->AllocateFactionId(), false, fixtures.factionDefinition, fixtures.dataContext);

        pA = &pState->AddFaction(std::move(pFactionA));
        pB = &pState->AddFaction(std::move(pFactionB));
        pC = &pState->AddFaction(std::move(pFactionC));

        // Default bases give each faction equal population for representative votes.
        fixtures.MakeFactionBase(*pA, 2, 2);
        fixtures.MakeFactionBase(*pB, 6, 2);
        fixtures.MakeFactionBase(*pC, 4, 6);

        pState->CreatePlanetaryCouncil(councilRegistry, councilRules);
        pState->SetMissionYear(2100);
    }

    void GiveAllCommlinksTo(Faction& rFaction)
    {
        DiplomacyLedger& rDiplomacy = pState->GetDiplomacyLedger();
        for (Faction* pOther : pState->GetPlanetaryCouncil()->Members())
        {
            if (pOther->GetFactionId() == rFaction.GetFactionId())
            {
                continue;
            }
            rDiplomacy.SetKnown(rFaction.GetFactionId(), pOther->GetFactionId());
        }
    }

    void Discover(Faction& rFaction, const std::string& rTechId)
    {
        if (!rFaction.GetResearch().HasDiscoveredTech(rTechId))
        {
            rFaction.GetResearch().AddDiscoveredTech(rTechId);
        }
    }

    void AdvancePastProposeCooldown(Faction& rFaction)
    {
        PlanetaryCouncil& rCouncil = *pState->GetPlanetaryCouncil();
        const int wait = rCouncil.YearsUntilCanPropose(*pState, rFaction);
        if (wait > 0)
        {
            pState->SetMissionYear(pState->GetMissionYear() + wait);
        }
    }

    void PassStandard(Faction& rProposer, const std::string& rProposalId)
    {
        PlanetaryCouncil& rCouncil = *pState->GetPlanetaryCouncil();
        GiveAllCommlinksTo(rProposer);
        AdvancePastProposeCooldown(rProposer);
        rCouncil.Propose(*pState, rProposer, rProposalId);
        for (Faction* pMember : rCouncil.Members())
        {
            rCouncil.CastVote(*pMember, CouncilBallot_t::Yea);
        }
        const ResolveProposalResult_t result = rCouncil.Resolve(*pState);
        REQUIRE((result == ResolveProposalResult_t::Passed
                 || result == ResolveProposalResult_t::VetoOverruled));
    }
};

} // namespace

TEST_CASE("Council proposals config loads with expected vote weights", "[council][parser]")
{
    CouncilProposalRegistry registry;
    registry.Load(FixturePath("council/proposals.json"));

    const CouncilProposalConfig_t& rTrade = registry.Get("global_trade_pact");
    CHECK(rTrade.voteWeight == CouncilVoteWeight_t::Representative);
    CHECK_FALSE(rTrade.repeatable);

    const CouncilProposalConfig_t& rGovernor = registry.Get("elect_planetary_governor");
    CHECK(rGovernor.kind == CouncilProposalKind_t::Election);
    CHECK(rGovernor.voteWeight == CouncilVoteWeight_t::Population);
    CHECK(rGovernor.electionOutcome == CouncilElectionOutcome_t::PlanetaryGovernor);

    const CouncilProposalConfig_t& rLeader = registry.Get("elect_supreme_leader");
    CHECK(rLeader.voteThreshold == Catch::Approx(0.75));
    CHECK(rLeader.requiredTech == "mind_machine_interface");

    const CouncilProposalConfig_t& rShade = registry.Get("increase_solar_shade");
    CHECK(rShade.repeatable);
}

TEST_CASE("Council rules config loads propose intervals and governor effects", "[council][parser]")
{
    const CouncilRulesConfig_t rules =
        CouncilRulesConfigParser{}.ParseConfig(FixturePath("council/rules.json"));

    CHECK(rules.governorProposeIntervalYears == 10);
    CHECK(rules.memberProposeIntervalYears == 20);
    REQUIRE(rules.governorEffects.size() == 2);
    const auto* pStat = std::get_if<StatModifierEffect_t>(&rules.governorEffects[0].effect);
    REQUIRE(pStat);
    CHECK(pStat->stat == StatId_t::CommerceEnergyBonus);
    CHECK(pStat->amount == Catch::Approx(1.0));
    CHECK(rules.governorEffects[1].persistence == EffectPersistence_t::Continuous);
    CHECK(std::get_if<InfiltrationEffect_t>(&rules.governorEffects[1].effect));
    REQUIRE(rules.governorEffects[1].factionFilter);
    CHECK(rules.governorEffects[1].factionFilter->kind == FactionFilterKind_t::CouncilMembers);
}

TEST_CASE("Proposing requires commlinks to every council member and shares them", "[council]")
{
    CouncilGame_ game;
    PlanetaryCouncil& rCouncil = *game.pState->GetPlanetaryCouncil();

    CHECK_THROWS_AS(
        rCouncil.Propose(*game.pState, *game.pA, "elect_planetary_governor"),
        std::invalid_argument);

    game.pState->GetDiplomacyLedger().SetKnown(game.pA->GetFactionId(), game.pB->GetFactionId());
    CHECK_THROWS_AS(
        rCouncil.Propose(*game.pState, *game.pA, "elect_planetary_governor"),
        std::invalid_argument);

    game.GiveAllCommlinksTo(*game.pA);
    // B and C do not know each other yet.
    CHECK_FALSE(game.pState->GetDiplomacyLedger().AreKnown(game.pB->GetFactionId(),
                                                           game.pC->GetFactionId()));

    rCouncil.Propose(*game.pState, *game.pA, "elect_planetary_governor");
    REQUIRE(rCouncil.GetPending());
    CHECK(rCouncil.GetPending()->proposalId == "elect_planetary_governor");
    CHECK(game.pState->GetDiplomacyLedger().AreKnown(game.pB->GetFactionId(),
                                                     game.pC->GetFactionId()));
}

TEST_CASE("Proposing while on cooldown is rejected", "[council]")
{
    CouncilGame_ game;
    PlanetaryCouncil& rCouncil = *game.pState->GetPlanetaryCouncil();
    game.GiveAllCommlinksTo(*game.pA);

    const int proposedYear = game.pState->GetMissionYear();
    rCouncil.Propose(*game.pState, *game.pA, "elect_planetary_governor");
    rCouncil.CastElectionVote(*game.pA, game.pA);
    rCouncil.CastElectionVote(*game.pB, game.pA);
    rCouncil.CastElectionVote(*game.pC, game.pA);
    REQUIRE(rCouncil.Resolve(*game.pState) == ResolveProposalResult_t::Passed);

    CHECK(rCouncil.YearsUntilCanPropose(*game.pState, *game.pA) == 10);
    REQUIRE(rCouncil.LastProposedYear(*game.pA).has_value());
    CHECK(*rCouncil.LastProposedYear(*game.pA) == proposedYear);
    CHECK_THROWS_AS(
        rCouncil.Propose(*game.pState, *game.pA, "elect_planetary_governor"),
        std::invalid_argument);

    game.pState->SetMissionYear(game.pState->GetMissionYear() + 10);
    CHECK(rCouncil.YearsUntilCanPropose(*game.pState, *game.pA) == 0);
    rCouncil.Propose(*game.pState, *game.pA, "elect_planetary_governor");
    CHECK(rCouncil.GetPending());
}

TEST_CASE("Non-participating factions are excluded from the council", "[council]")
{
    CouncilGame_ game;
    game.alienDefinition = game.fixtures.factionDefinition;
    game.alienDefinition.id = "alien";
    game.alienDefinition.identity.participatesInCouncil = false;

    auto pAlien = std::make_unique<Faction>(
        game.pState->AllocateFactionId(), false, game.alienDefinition, game.fixtures.dataContext);
    Faction& rAlien = game.pState->AddFaction(std::move(pAlien));

    PlanetaryCouncil& rCouncil = *game.pState->GetPlanetaryCouncil();
    CHECK(rCouncil.Members().size() == 3);
    CHECK(rCouncil.IsCouncilMember(*game.pA));
    CHECK_FALSE(rCouncil.IsCouncilMember(rAlien));

    // Only need commlinks among participating members to open a vote.
    game.GiveAllCommlinksTo(*game.pA);
    rCouncil.Propose(*game.pState, *game.pA, "elect_planetary_governor");
    CHECK_FALSE(game.pState->GetDiplomacyLedger().AreKnown(game.pA->GetFactionId(),
                                                           rAlien.GetFactionId()));

    // Alien cannot put an issue before the council (resolve first so pending is clear).
    rCouncil.CastElectionVote(*game.pA, game.pA);
    rCouncil.CastElectionVote(*game.pB, game.pA);
    rCouncil.CastElectionVote(*game.pC, game.pA);
    REQUIRE(rCouncil.Resolve(*game.pState) == ResolveProposalResult_t::Passed);
    CHECK_THROWS_AS(
        rCouncil.Propose(*game.pState, rAlien, "elect_planetary_governor"),
        std::invalid_argument);
}

TEST_CASE("Non-members cannot cast council votes", "[council]")
{
    CouncilGame_ game;
    game.Discover(*game.pA, "planetary_economics"); // for global_trade_pact below

    game.alienDefinition = game.fixtures.factionDefinition;
    game.alienDefinition.id = "alien";
    game.alienDefinition.identity.participatesInCouncil = false;
    auto pAlien = std::make_unique<Faction>(
        game.pState->AllocateFactionId(), false, game.alienDefinition, game.fixtures.dataContext);
    Faction& rAlien = game.pState->AddFaction(std::move(pAlien));

    game.GiveAllCommlinksTo(*game.pA);
    PlanetaryCouncil& rCouncil = *game.pState->GetPlanetaryCouncil();

    // Election ballot: a non-member cannot cast an election vote.
    rCouncil.Propose(*game.pState, *game.pA, "elect_planetary_governor");
    CHECK_THROWS_AS(rCouncil.CastElectionVote(rAlien, game.pA), std::invalid_argument);
    rCouncil.CastElectionVote(*game.pA, game.pA);
    rCouncil.CastElectionVote(*game.pB, game.pA);
    rCouncil.CastElectionVote(*game.pC, game.pA);
    REQUIRE(rCouncil.Resolve(*game.pState) == ResolveProposalResult_t::Passed);

    // Standard ballot: a non-member cannot cast a standard vote.
    game.AdvancePastProposeCooldown(*game.pA);
    rCouncil.Propose(*game.pState, *game.pA, "global_trade_pact");
    CHECK_THROWS_AS(rCouncil.CastVote(rAlien, CouncilBallot_t::Yea), std::invalid_argument);
}

TEST_CASE("Council emits open and resolve notifications", "[council]")
{
    CouncilGame_ game;
    game.Discover(*game.pA, "planetary_economics"); // for global_trade_pact below
    game.GiveAllCommlinksTo(*game.pA);
    PlanetaryCouncil& rCouncil = *game.pState->GetPlanetaryCouncil();

    int openedCount = 0;
    FactionId_t openedProposer = 0;
    std::string openedId;
    auto openedConn = rCouncil.OnProposalOpened.ConnectScoped(
        [&](Faction& rProposer, const std::string& rId)
        {
            ++openedCount;
            openedProposer = rProposer.GetFactionId();
            openedId = rId;
        });

    int resolvedCount = 0;
    std::string resolvedId;
    ResolveProposalResult_t resolvedResult = ResolveProposalResult_t::Failed;
    auto resolvedConn = rCouncil.OnResolved.ConnectScoped(
        [&](const std::string& rId, ResolveProposalResult_t result)
        {
            ++resolvedCount;
            resolvedId = rId;
            resolvedResult = result;
        });

    rCouncil.Propose(*game.pState, *game.pA, "elect_planetary_governor");
    CHECK(openedCount == 1);
    CHECK(openedProposer == game.pA->GetFactionId());
    CHECK(openedId == "elect_planetary_governor");

    rCouncil.CastElectionVote(*game.pA, game.pA);
    rCouncil.CastElectionVote(*game.pB, game.pA);
    rCouncil.CastElectionVote(*game.pC, game.pA);
    REQUIRE(rCouncil.Resolve(*game.pState) == ResolveProposalResult_t::Passed);
    CHECK(resolvedCount == 1);
    CHECK(resolvedId == "elect_planetary_governor");
    CHECK(resolvedResult == ResolveProposalResult_t::Passed);

    // A failed resolve notifies too (single-exit path fires for every outcome).
    game.AdvancePastProposeCooldown(*game.pA);
    rCouncil.Propose(*game.pState, *game.pA, "global_trade_pact");
    CHECK(openedCount == 2);
    rCouncil.CastVote(*game.pA, CouncilBallot_t::Nay);
    rCouncil.CastVote(*game.pB, CouncilBallot_t::Nay);
    rCouncil.CastVote(*game.pC, CouncilBallot_t::Nay);
    REQUIRE(rCouncil.Resolve(*game.pState) == ResolveProposalResult_t::Failed);
    CHECK(resolvedCount == 2);
    CHECK(resolvedId == "global_trade_pact");
    CHECK(resolvedResult == ResolveProposalResult_t::Failed);
}

TEST_CASE("Stub AI votes fill non-player ballots only", "[council]")
{
    CouncilGame_ game;
    game.Discover(*game.pA, "planetary_economics");
    game.GiveAllCommlinksTo(*game.pA);
    PlanetaryCouncil& rCouncil = *game.pState->GetPlanetaryCouncil();

    REQUIRE(game.pA->IsPlayerControlled());
    REQUIRE_FALSE(game.pB->IsPlayerControlled());
    REQUIRE_FALSE(game.pC->IsPlayerControlled());

    rCouncil.Propose(*game.pState, *game.pA, "global_trade_pact");
    CastStubCouncilVotes(rCouncil);

    const auto* pPending = rCouncil.GetPending();
    REQUIRE(pPending);
    CHECK(pPending->ballots.count(game.pA->GetFactionId()) == 0);
    REQUIRE(pPending->ballots.count(game.pB->GetFactionId()) == 1);
    REQUIRE(pPending->ballots.count(game.pC->GetFactionId()) == 1);
    CHECK(pPending->ballots.at(game.pB->GetFactionId()) == CouncilBallot_t::Yea);
    CHECK(pPending->ballots.at(game.pC->GetFactionId()) == CouncilBallot_t::Yea);

    // Idempotent: already-cast AI ballots are left alone.
    rCouncil.CastVote(*game.pB, CouncilBallot_t::Nay);
    CastStubCouncilVotes(rCouncil);
    CHECK(pPending->ballots.at(game.pB->GetFactionId()) == CouncilBallot_t::Nay);
}

TEST_CASE("Stub AI election votes pick the proposer", "[council]")
{
    CouncilGame_ game;
    game.GiveAllCommlinksTo(*game.pA);
    PlanetaryCouncil& rCouncil = *game.pState->GetPlanetaryCouncil();

    rCouncil.Propose(*game.pState, *game.pA, "elect_planetary_governor");
    CastStubCouncilVotes(rCouncil);

    const auto* pPending = rCouncil.GetPending();
    REQUIRE(pPending);
    CHECK(pPending->electionVotes.count(game.pA->GetFactionId()) == 0);
    REQUIRE(pPending->electionVotes.count(game.pB->GetFactionId()) == 1);
    REQUIRE(pPending->electionVotes.count(game.pC->GetFactionId()) == 1);
    CHECK(pPending->electionVotes.at(game.pB->GetFactionId()) == game.pA->GetFactionId());
    CHECK(pPending->electionVotes.at(game.pC->GetFactionId()) == game.pA->GetFactionId());
}

TEST_CASE("Council membership is fixed at construction", "[council]")
{
    CouncilGame_ game;
    PlanetaryCouncil& rCouncil = *game.pState->GetPlanetaryCouncil();
    REQUIRE(rCouncil.Members().size() == 3);

    FactionConfig_t lateDef = game.fixtures.factionDefinition;
    lateDef.id = "late_joiner";
    lateDef.identity.participatesInCouncil = true;
    auto pLate = std::make_unique<Faction>(
        game.pState->AllocateFactionId(), false, lateDef, game.fixtures.dataContext);
    Faction& rLate = game.pState->AddFaction(std::move(pLate));

    CHECK(rCouncil.Members().size() == 3);
    CHECK_FALSE(rCouncil.IsCouncilMember(rLate));
}

TEST_CASE("Salvage Unity Fusion Core grants energy to every council member", "[council]")
{
    CouncilGame_ game;
    game.Discover(*game.pA, "orbital_spaceflight");

    const int beforeA = game.pA->GetEconomy().GetEnergy();
    const int beforeB = game.pB->GetEconomy().GetEnergy();
    const int beforeC = game.pC->GetEconomy().GetEnergy();

    game.PassStandard(*game.pA, "salvage_unity_fusion_core");

    CHECK(game.pA->GetEconomy().GetEnergy() == beforeA + 500);
    CHECK(game.pB->GetEconomy().GetEnergy() == beforeB + 500);
    CHECK(game.pC->GetEconomy().GetEnergy() == beforeC + 500);

    // One-shot: cannot pass again.
    CHECK_FALSE(game.pState->GetPlanetaryCouncil()->CanPropose(
        *game.pA, "salvage_unity_fusion_core"));
}

TEST_CASE("Global Trade Pact activates and can be repealed", "[council]")
{
    CouncilGame_ game;
    game.Discover(*game.pA, "planetary_economics");

    PlanetaryCouncil& rCouncil = *game.pState->GetPlanetaryCouncil();
    game.PassStandard(*game.pA, "global_trade_pact");
    CHECK(rCouncil.IsActive("global_trade_pact"));

    bool foundCommerce = false;
    for (const ActiveEffect_t& rEffect : rCouncil.CollectWorldEffects())
    {
        if (const auto* pStat = std::get_if<StatModifierEffect_t>(&rEffect.config->effect))
        {
            if (pStat->stat == StatId_t::CommerceRate)
            {
                foundCommerce = true;
            }
        }
    }
    CHECK(foundCommerce);

    game.PassStandard(*game.pA, "repeal_trade_pact");
    CHECK_FALSE(rCouncil.IsActive("global_trade_pact"));
}

TEST_CASE("Solar shade and polar caps proposals pass; the shade is repeatable", "[council]")
{
    // The world-map effect of these proposals (gradual sea-level change) is owned by the
    // WorldEvents system, not the council; here we only assert the proposal flow — passage,
    // repeatability, and tech gating.
    CouncilGame_ game;
    game.Discover(*game.pA, "advanced_spaceflight");
    game.Discover(*game.pA, "advanced_ecological_engineering");

    PlanetaryCouncil& rCouncil = *game.pState->GetPlanetaryCouncil();
    game.PassStandard(*game.pA, "launch_solar_shade");
    CHECK(rCouncil.IsActive("launch_solar_shade"));

    // increase_solar_shade is repeatable: it can be proposed and pass more than once.
    game.PassStandard(*game.pA, "increase_solar_shade");
    CHECK(rCouncil.CanPropose(*game.pA, "increase_solar_shade"));
    game.PassStandard(*game.pA, "increase_solar_shade");

    game.PassStandard(*game.pA, "melt_polar_caps");
    CHECK(rCouncil.IsActive("melt_polar_caps"));
}

TEST_CASE("U.N. Charter starts in force and can be repealed then reinstated", "[council]")
{
    CouncilGame_ game;
    game.Discover(*game.pA, "advanced_military_algorithms");

    PlanetaryCouncil& rCouncil = *game.pState->GetPlanetaryCouncil();
    CHECK(rCouncil.IsActive("un_charter"));
    CHECK(rCouncil.HasActiveRuleFlag(RuleFlagId_t::AtrocitiesForbidden));

    game.PassStandard(*game.pA, "repeal_un_charter");
    CHECK_FALSE(rCouncil.IsActive("un_charter"));
    CHECK_FALSE(rCouncil.HasActiveRuleFlag(RuleFlagId_t::AtrocitiesForbidden));

    game.PassStandard(*game.pA, "reinstate_un_charter");
    CHECK(rCouncil.IsActive("reinstate_un_charter"));
    CHECK(rCouncil.HasActiveRuleFlag(RuleFlagId_t::AtrocitiesForbidden));
}

TEST_CASE("Elect Planetary Governor sets governor benefits and infiltrators", "[council]")
{
    CouncilGame_ game;
    // Extra bases: A = 9 pop, B = 6, C = 3 (each base starts at size 3).
    game.fixtures.MakeFactionBase(*game.pA, 1, 1);
    game.fixtures.MakeFactionBase(*game.pA, 1, 3);
    game.fixtures.MakeFactionBase(*game.pB, 7, 1);

    game.GiveAllCommlinksTo(*game.pA);
    PlanetaryCouncil& rCouncil = *game.pState->GetPlanetaryCouncil();

    const std::vector<Faction*> candidates = rCouncil.GovernorCandidates();
    REQUIRE(candidates.size() == 2);
    CHECK(candidates[0] == game.pA);
    CHECK(candidates[1] == game.pB);

    rCouncil.Propose(*game.pState, *game.pA, "elect_planetary_governor");
    rCouncil.CastElectionVote(*game.pA, game.pA);
    rCouncil.CastElectionVote(*game.pB, game.pA);
    rCouncil.CastElectionVote(*game.pC, nullptr);
    REQUIRE(rCouncil.Resolve(*game.pState) == ResolveProposalResult_t::Passed);

    REQUIRE(rCouncil.GetPlanetaryGovernor());
    CHECK(rCouncil.GetPlanetaryGovernor() == game.pA);
    // Continuous governor Infiltration + CouncilMembers — query-time, not ledger sticky.
    CHECK(HasInfiltration(*game.pState, game.pA->GetFactionId(), game.pB->GetFactionId()));
    CHECK(HasInfiltration(*game.pState, game.pA->GetFactionId(), game.pC->GetFactionId()));
    CHECK_FALSE(game.pState->GetDiplomacyLedger().HasInfiltration(game.pA->GetFactionId(),
                                                                  game.pB->GetFactionId()));

    // Governor propose interval is 10 years; members are 20.
    CHECK(rCouncil.ProposeCooldownYears(*game.pA) == 10);
    CHECK(rCouncil.ProposeCooldownYears(*game.pB) == 20);
}

TEST_CASE("Governor veto stands unless every other member voted yea", "[council]")
{
    CouncilGame_ game;
    game.Discover(*game.pA, "planetary_economics");
    game.Discover(*game.pB, "planetary_economics");
    game.GiveAllCommlinksTo(*game.pA);

    PlanetaryCouncil& rCouncil = *game.pState->GetPlanetaryCouncil();

    // Elect A as governor first.
    rCouncil.Propose(*game.pState, *game.pA, "elect_planetary_governor");
    rCouncil.CastElectionVote(*game.pA, game.pA);
    rCouncil.CastElectionVote(*game.pB, game.pA);
    rCouncil.CastElectionVote(*game.pC, game.pA);
    REQUIRE(rCouncil.Resolve(*game.pState) == ResolveProposalResult_t::Passed);

    // Non-unanimous non-governor support: veto stands.
    rCouncil.Propose(*game.pState, *game.pB, "global_trade_pact");
    rCouncil.CastVote(*game.pA, CouncilBallot_t::Yea);
    rCouncil.CastVote(*game.pB, CouncilBallot_t::Yea);
    rCouncil.CastVote(*game.pC, CouncilBallot_t::Nay);
    REQUIRE(rCouncil.VetoPending(*game.pA));
    CHECK(rCouncil.Resolve(*game.pState) == ResolveProposalResult_t::Vetoed);
    CHECK_FALSE(rCouncil.IsActive("global_trade_pact"));

    // Every non-governor voted Yea: veto is overruled from the ballots.
    game.AdvancePastProposeCooldown(*game.pB);
    rCouncil.Propose(*game.pState, *game.pB, "global_trade_pact");
    rCouncil.CastVote(*game.pA, CouncilBallot_t::Nay);
    rCouncil.CastVote(*game.pB, CouncilBallot_t::Yea);
    rCouncil.CastVote(*game.pC, CouncilBallot_t::Yea);
    REQUIRE(rCouncil.VetoPending(*game.pA));
    CHECK(rCouncil.Resolve(*game.pState) == ResolveProposalResult_t::VetoOverruled);
    CHECK(rCouncil.IsActive("global_trade_pact"));
}

TEST_CASE("Supreme Leader election stubs diplomatic victory at 75 percent", "[council]")
{
    CouncilGame_ game;
    game.Discover(*game.pA, "mind_machine_interface");
    game.GiveAllCommlinksTo(*game.pA);

    PlanetaryCouncil& rCouncil = *game.pState->GetPlanetaryCouncil();
    rCouncil.Propose(*game.pState, *game.pA, "elect_supreme_leader");

    // Equal population → each has 1/3. Need 75%, so A alone cannot win.
    rCouncil.CastElectionVote(*game.pA, game.pA);
    rCouncil.CastElectionVote(*game.pB, nullptr);
    rCouncil.CastElectionVote(*game.pC, nullptr);
    CHECK(rCouncil.Resolve(*game.pState) == ResolveProposalResult_t::Failed);

    game.AdvancePastProposeCooldown(*game.pA);
    rCouncil.Propose(*game.pState, *game.pA, "elect_supreme_leader");
    rCouncil.CastElectionVote(*game.pA, game.pA);
    rCouncil.CastElectionVote(*game.pB, game.pA);
    rCouncil.CastElectionVote(*game.pC, game.pA);
    CHECK(rCouncil.Resolve(*game.pState) == ResolveProposalResult_t::SupremeLeaderVictoryStub);
    REQUIRE(rCouncil.GetSupremeLeaderVictoryStub());
    CHECK(rCouncil.GetSupremeLeaderVictoryStub() == game.pA);
}
