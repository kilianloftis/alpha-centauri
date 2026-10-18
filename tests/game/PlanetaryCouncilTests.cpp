#include "GameFixtures.h"

#include "game/CurrentGameState.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/council/CouncilAiStub.h"
#include "game/council/CouncilProposalConfigParser.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/CouncilRulesConfigParser.h"
#include "game/council/CouncilRules.h"
#include "game/council/CouncilVoteExecutor.h"
#include "game/council/PlanetaryCouncil.h"
#include "game/effects/ActiveEffect.h"
#include "game/diplomacy/InfiltrationRules.h"
#include "game/effects/TileYieldRulesConfigParser.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/economy/EconomyManager.h"
#include "game/research/ResearchManager.h"
#include "game/population/PopulationManager.h"
#include "game/map/WorldMap.h"

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <variant>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

struct CouncilGame_
{
    FactionFixture fixtures;
    Faction* pA = nullptr;
    Faction* pB = nullptr;
    Faction* pC = nullptr;
    FactionConfig_t alienDefinition;

    CouncilGame_()
    {
        InstallCouncil(fixtures.dataContext);

        FillLand(fixtures.map);

        auto pFactionA = std::make_unique<Faction>(
            fixtures.state.AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
            fixtures.state.GetWorldMap(), fixtures.settings, actest::k_TestFactionSeed);
        auto pFactionB = std::make_unique<Faction>(
            fixtures.state.AllocateFactionId(), false, fixtures.factionDefinition, fixtures.dataContext,
            fixtures.state.GetWorldMap(), fixtures.settings, actest::k_TestFactionSeed);
        auto pFactionC = std::make_unique<Faction>(
            fixtures.state.AllocateFactionId(), false, fixtures.factionDefinition, fixtures.dataContext,
            fixtures.state.GetWorldMap(), fixtures.settings, actest::k_TestFactionSeed);

        pA = &fixtures.state.AddFaction(std::move(pFactionA));
        pB = &fixtures.state.AddFaction(std::move(pFactionB));
        pC = &fixtures.state.AddFaction(std::move(pFactionC));

        // Default bases give each faction equal population for representative votes.
        fixtures.MakeFactionBase(*pA, 8, 4);
        fixtures.MakeFactionBase(*pB, 12, 8);
        fixtures.MakeFactionBase(*pC, 6, 10);

        fixtures.state.CreatePlanetaryCouncil();
        fixtures.state.SetMissionYear(2100);
    }

    void GiveAllCommlinksTo(Faction& rFaction)
    {
        DiplomacyLedger& rDiplomacy = fixtures.state.GetDiplomacyLedger();
        for (Faction* pOther : fixtures.state.GetPlanetaryCouncil()->Members())
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
        PlanetaryCouncil& rCouncil = *fixtures.state.GetPlanetaryCouncil();
        const int wait = YearsUntilCanPropose(rCouncil, rFaction);
        if (wait > 0)
        {
            fixtures.state.SetMissionYear(fixtures.state.GetMissionYear() + wait);
        }
    }

    void PassStandard(Faction& rProposer, const std::string& rProposalId)
    {
        PlanetaryCouncil& rCouncil = *fixtures.state.GetPlanetaryCouncil();
        CouncilVoteExecutor& rVotes = *fixtures.state.GetCouncilVotes();
        GiveAllCommlinksTo(rProposer);
        AdvancePastProposeCooldown(rProposer);
        rVotes.Propose(fixtures.state, rProposer, rProposalId);
        for (Faction* pMember : rCouncil.Members())
        {
            rVotes.CastVote(*pMember, CouncilBallot_t::Yea);
        }
        const ResolveProposalResult_t result = rVotes.Resolve(fixtures.state);
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
    CHECK(std::get_if<InfiltrationEffect_t>(&rules.governorEffects[1].effect));
    REQUIRE(rules.governorEffects[1].factionFilter);
    CHECK(rules.governorEffects[1].factionFilter->kind == FactionFilterKind_t::CouncilMembers);
}

TEST_CASE("Council proposal honored shapes: stock OK; inert shapes throw", "[council][parser]")
{
    // Stock fixture already exercised WorldGlobal `effects`, on_passed GrantEnergy and
    // on_passed WorldParameter via the load test above.
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ac_council_bad_proposal.json";

    SECTION("A non-WorldGlobal standing law throws")
    {
        {
            std::ofstream out(path);
            out << R"([{
                "id": "bad_faction_global",
                "name": "Bad",
                "effects": [{
                    "type": "StatModifier",
                    "scope": "FactionGlobal",
                    "parameters": { "stat": "energy", "amount": 1 }
                }]
            }])";
        }
        CHECK_THROWS(CouncilProposalConfigParser{}.ParseConfig(path.string()));
    }

    SECTION("A one-shot outcome the applier does not handle throws")
    {
        {
            std::ofstream out(path);
            out << R"([{
                "id": "bad_passed_effect",
                "name": "Bad",
                "on_passed_effects": [{ "type": "Rebel" }]
            }])";
        }
        CHECK_THROWS(CouncilProposalConfigParser{}.ParseConfig(path.string()));
    }

    std::filesystem::remove(path);
}

TEST_CASE("Council governor honored shapes: inert shapes throw", "[council][parser]")
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ac_council_bad_rules.json";

    SECTION("A non-FactionGlobal standing governor effect throws")
    {
        {
            std::ofstream out(path);
            out << R"({
                "governor_propose_interval_years": 10,
                "member_propose_interval_years": 20,
                "governor_effects": [{
                    "type": "StatModifier",
                    "scope": "WorldGlobal",
                    "parameters": { "stat": "energy", "amount": 1 }
                }]
            })";
        }
        CHECK_THROWS(CouncilRulesConfigParser{}.ParseConfig(path.string()));
    }

    SECTION("A one-shot effect in governor_effects throws, naming on_elected_effects")
    {
        {
            std::ofstream out(path);
            out << R"({
                "governor_propose_interval_years": 10,
                "member_propose_interval_years": 20,
                "governor_effects": [{ "type": "SetInfiltration" }]
            })";
        }
        CHECK_THROWS(CouncilRulesConfigParser{}.ParseConfig(path.string()));
    }

    SECTION("A one-shot effect in on_elected_effects loads")
    {
        {
            std::ofstream out(path);
            out << R"({
                "governor_propose_interval_years": 10,
                "member_propose_interval_years": 20,
                "on_elected_effects": [{ "type": "SetInfiltration" }]
            })";
        }
        const CouncilRulesConfig_t rules = CouncilRulesConfigParser{}.ParseConfig(path.string());
        REQUIRE(rules.onElectedEffects.size() == 1);
        CHECK(std::get_if<SetInfiltrationEffect_t>(&rules.onElectedEffects[0].effect) != nullptr);
    }

    std::filesystem::remove(path);
}

TEST_CASE("Tile yield rules reject illegal scopes", "[effects][parser][tile_yield]")
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ac_bad_tile_yield_rules.json";
    {
        std::ofstream out(path);
        out << R"({
            "effects": [{
                "type": "StatModifier",
                "scope": "ThisPop",
                "parameters": { "stat": "nutrients", "amount": 1 }
            }]
        })";
    }
    CHECK_THROWS(TileYieldRulesConfigParser{}.ParseConfig(path.string()));
    std::filesystem::remove(path);

    CHECK_NOTHROW(
        TileYieldRulesConfigParser{}.ParseConfig(FixturePath("tile_yield_rules.json")));
}

TEST_CASE("Proposing requires commlinks to every council member and shares them", "[council]")
{
    CouncilGame_ game;
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();

    CHECK_THROWS_AS(
        rVotes.Propose(game.fixtures.state, *game.pA, "elect_planetary_governor"),
        std::invalid_argument);

    game.fixtures.state.GetDiplomacyLedger().SetKnown(game.pA->GetFactionId(), game.pB->GetFactionId());
    CHECK_THROWS_AS(
        rVotes.Propose(game.fixtures.state, *game.pA, "elect_planetary_governor"),
        std::invalid_argument);

    game.GiveAllCommlinksTo(*game.pA);
    // B and C do not know each other yet.
    CHECK_FALSE(game.fixtures.state.GetDiplomacyLedger().AreKnown(game.pB->GetFactionId(),
                                                           game.pC->GetFactionId()));

    rVotes.Propose(game.fixtures.state, *game.pA, "elect_planetary_governor");
    REQUIRE(rVotes.GetPending());
    CHECK(rVotes.GetPending()->proposalId == "elect_planetary_governor");
    CHECK(game.fixtures.state.GetDiplomacyLedger().AreKnown(game.pB->GetFactionId(),
                                                     game.pC->GetFactionId()));
}

TEST_CASE("Proposing while on cooldown is rejected", "[council]")
{
    CouncilGame_ game;
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();
    game.GiveAllCommlinksTo(*game.pA);

    const int proposedYear = game.fixtures.state.GetMissionYear();
    rVotes.Propose(game.fixtures.state, *game.pA, "elect_planetary_governor");
    rVotes.CastElectionVote(*game.pA, game.pA);
    rVotes.CastElectionVote(*game.pB, game.pA);
    rVotes.CastElectionVote(*game.pC, game.pA);
    REQUIRE(rVotes.Resolve(game.fixtures.state) == ResolveProposalResult_t::Passed);

    CHECK(YearsUntilCanPropose(rCouncil, *game.pA) == 10);
    REQUIRE(rCouncil.LastProposedYear(*game.pA).has_value());
    CHECK(*rCouncil.LastProposedYear(*game.pA) == proposedYear);
    CHECK_THROWS_AS(
        rVotes.Propose(game.fixtures.state, *game.pA, "elect_planetary_governor"),
        std::invalid_argument);

    game.fixtures.state.SetMissionYear(game.fixtures.state.GetMissionYear() + 10);
    CHECK(YearsUntilCanPropose(rCouncil, *game.pA) == 0);
    rVotes.Propose(game.fixtures.state, *game.pA, "elect_planetary_governor");
    CHECK(rVotes.GetPending());
}

TEST_CASE("CouncilMembers filter matches nobody when no PlanetaryCouncil exists", "[council][infiltration]")
{
    // participatesInCouncil is eligibility for council construction, not membership.
    // Without CreatePlanetaryCouncil, CouncilMembers must fail closed for every candidate.
    FactionFixture fixtures;
    REQUIRE(fixtures.factionDefinition.identity.participatesInCouncil);

    EffectConfig_t config;
    config.effect = InfiltrationEffect_t{};
    config.scope = EffectScope_t::FactionGlobal;
    config.factionFilter = FactionFilter_t{FactionFilterKind_t::CouncilMembers};
    // Declared on the faction definition so it reaches the faction's active effect pool —
    // otherwise HasInfiltration walks an empty list and passes regardless of the filter.
    fixtures.factionDefinition.effects.push_back(config);

    FillLand(fixtures.map);
    Faction& rA = fixtures.state.AddFaction(std::make_unique<Faction>(
        fixtures.state.AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
        fixtures.map, fixtures.settings, actest::k_TestFactionSeed));
    Faction& rB = fixtures.state.AddFaction(std::make_unique<Faction>(
        fixtures.state.AllocateFactionId(), false, fixtures.factionDefinition, fixtures.dataContext,
        fixtures.map, fixtures.settings, actest::k_TestFactionSeed));
    REQUIRE(fixtures.state.GetPlanetaryCouncil() == nullptr);

    const std::vector<ActiveEffect_t>& rActive = rA.GetActiveEffects().effects;
    REQUIRE(std::any_of(rActive.begin(), rActive.end(), [](const ActiveEffect_t& rEffect)
    {
        return std::get_if<InfiltrationEffect_t>(&rEffect.config->effect) != nullptr;
    }));

    CHECK_FALSE(FactionFilterCoversTarget(config, rA.GetFactionId(), rB.GetFactionId()));
    CHECK_FALSE(HasInfiltration(rA.GetFactionId(), rB.GetFactionId()));
}

TEST_CASE("Non-participating factions are excluded from the council", "[council]")
{
    CouncilGame_ game;
    game.alienDefinition = game.fixtures.factionDefinition;
    game.alienDefinition.id = "alien";
    game.alienDefinition.identity.participatesInCouncil = false;

    auto pAlien = std::make_unique<Faction>(
        game.fixtures.state.AllocateFactionId(), false, game.alienDefinition, game.fixtures.dataContext,
        game.fixtures.state.GetWorldMap(), game.fixtures.settings, actest::k_TestFactionSeed);
    Faction& rAlien = game.fixtures.state.AddFaction(std::move(pAlien));

    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();
    CHECK(rCouncil.Members().size() == 3);
    CHECK(rCouncil.IsCouncilMember(*game.pA));
    CHECK_FALSE(rCouncil.IsCouncilMember(rAlien));

    // Only need commlinks among participating members to open a vote.
    game.GiveAllCommlinksTo(*game.pA);
    rVotes.Propose(game.fixtures.state, *game.pA, "elect_planetary_governor");
    CHECK_FALSE(game.fixtures.state.GetDiplomacyLedger().AreKnown(game.pA->GetFactionId(),
                                                           rAlien.GetFactionId()));

    // Alien cannot put an issue before the council (resolve first so pending is clear).
    rVotes.CastElectionVote(*game.pA, game.pA);
    rVotes.CastElectionVote(*game.pB, game.pA);
    rVotes.CastElectionVote(*game.pC, game.pA);
    REQUIRE(rVotes.Resolve(game.fixtures.state) == ResolveProposalResult_t::Passed);
    CHECK_THROWS_AS(
        rVotes.Propose(game.fixtures.state, rAlien, "elect_planetary_governor"),
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
        game.fixtures.state.AllocateFactionId(), false, game.alienDefinition, game.fixtures.dataContext,
        game.fixtures.state.GetWorldMap(), game.fixtures.settings, actest::k_TestFactionSeed);
    Faction& rAlien = game.fixtures.state.AddFaction(std::move(pAlien));

    game.GiveAllCommlinksTo(*game.pA);
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();

    // Election ballot: a non-member cannot cast an election vote.
    rVotes.Propose(game.fixtures.state, *game.pA, "elect_planetary_governor");
    CHECK_THROWS_AS(rVotes.CastElectionVote(rAlien, game.pA), std::invalid_argument);
    rVotes.CastElectionVote(*game.pA, game.pA);
    rVotes.CastElectionVote(*game.pB, game.pA);
    rVotes.CastElectionVote(*game.pC, game.pA);
    REQUIRE(rVotes.Resolve(game.fixtures.state) == ResolveProposalResult_t::Passed);

    // Standard ballot: a non-member cannot cast a standard vote.
    game.AdvancePastProposeCooldown(*game.pA);
    rVotes.Propose(game.fixtures.state, *game.pA, "global_trade_pact");
    CHECK_THROWS_AS(rVotes.CastVote(rAlien, CouncilBallot_t::Yea), std::invalid_argument);
}

TEST_CASE("Council emits open and resolve notifications", "[council]")
{
    CouncilGame_ game;
    game.Discover(*game.pA, "planetary_economics"); // for global_trade_pact below
    game.GiveAllCommlinksTo(*game.pA);
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();

    int openedCount = 0;
    FactionId_t openedProposer = 0;
    std::string openedId;
    auto openedConn = rVotes.OnProposalOpened.ConnectScoped(
        [&](Faction& rProposer, const std::string& rId)
        {
            ++openedCount;
            openedProposer = rProposer.GetFactionId();
            openedId = rId;
        });

    int resolvedCount = 0;
    std::string resolvedId;
    ResolveProposalResult_t resolvedResult = ResolveProposalResult_t::Failed;
    auto resolvedConn = rVotes.OnResolved.ConnectScoped(
        [&](const std::string& rId, ResolveProposalResult_t result)
        {
            ++resolvedCount;
            resolvedId = rId;
            resolvedResult = result;
        });

    rVotes.Propose(game.fixtures.state, *game.pA, "elect_planetary_governor");
    CHECK(openedCount == 1);
    CHECK(openedProposer == game.pA->GetFactionId());
    CHECK(openedId == "elect_planetary_governor");

    rVotes.CastElectionVote(*game.pA, game.pA);
    rVotes.CastElectionVote(*game.pB, game.pA);
    rVotes.CastElectionVote(*game.pC, game.pA);
    REQUIRE(rVotes.Resolve(game.fixtures.state) == ResolveProposalResult_t::Passed);
    CHECK(resolvedCount == 1);
    CHECK(resolvedId == "elect_planetary_governor");
    CHECK(resolvedResult == ResolveProposalResult_t::Passed);

    // A failed resolve notifies too (single-exit path fires for every outcome).
    game.AdvancePastProposeCooldown(*game.pA);
    rVotes.Propose(game.fixtures.state, *game.pA, "global_trade_pact");
    CHECK(openedCount == 2);
    rVotes.CastVote(*game.pA, CouncilBallot_t::Nay);
    rVotes.CastVote(*game.pB, CouncilBallot_t::Nay);
    rVotes.CastVote(*game.pC, CouncilBallot_t::Nay);
    REQUIRE(rVotes.Resolve(game.fixtures.state) == ResolveProposalResult_t::Failed);
    CHECK(resolvedCount == 2);
    CHECK(resolvedId == "global_trade_pact");
    CHECK(resolvedResult == ResolveProposalResult_t::Failed);
}

TEST_CASE("Stub AI votes fill non-player ballots only", "[council]")
{
    CouncilGame_ game;
    game.Discover(*game.pA, "planetary_economics");
    game.GiveAllCommlinksTo(*game.pA);
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();

    REQUIRE(game.pA->IsPlayerControlled());
    REQUIRE_FALSE(game.pB->IsPlayerControlled());
    REQUIRE_FALSE(game.pC->IsPlayerControlled());

    rVotes.Propose(game.fixtures.state, *game.pA, "global_trade_pact");
    CastStubCouncilVotes(rVotes, rCouncil);

    const auto* pPending = rVotes.GetPending();
    REQUIRE(pPending);
    CHECK(pPending->ballots.count(game.pA->GetFactionId()) == 0);
    REQUIRE(pPending->ballots.count(game.pB->GetFactionId()) == 1);
    REQUIRE(pPending->ballots.count(game.pC->GetFactionId()) == 1);
    CHECK(pPending->ballots.at(game.pB->GetFactionId()) == CouncilBallot_t::Yea);
    CHECK(pPending->ballots.at(game.pC->GetFactionId()) == CouncilBallot_t::Yea);

    // Idempotent: already-cast AI ballots are left alone.
    rVotes.CastVote(*game.pB, CouncilBallot_t::Nay);
    CastStubCouncilVotes(rVotes, rCouncil);
    CHECK(pPending->ballots.at(game.pB->GetFactionId()) == CouncilBallot_t::Nay);
}

TEST_CASE("Stub AI election votes pick the proposer", "[council]")
{
    CouncilGame_ game;
    game.GiveAllCommlinksTo(*game.pA);
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();

    rVotes.Propose(game.fixtures.state, *game.pA, "elect_planetary_governor");
    CastStubCouncilVotes(rVotes, rCouncil);

    const auto* pPending = rVotes.GetPending();
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
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    REQUIRE(rCouncil.Members().size() == 3);

    FactionConfig_t lateDef = game.fixtures.factionDefinition;
    lateDef.id = "late_joiner";
    lateDef.identity.participatesInCouncil = true;
    auto pLate = std::make_unique<Faction>(
        game.fixtures.state.AllocateFactionId(), false, lateDef, game.fixtures.dataContext,
        game.fixtures.state.GetWorldMap(), game.fixtures.settings, actest::k_TestFactionSeed);
    Faction& rLate = game.fixtures.state.AddFaction(std::move(pLate));

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
    CHECK_FALSE(CanPropose(*game.fixtures.state.GetPlanetaryCouncil(), *game.pA,
                           "salvage_unity_fusion_core"));
}

TEST_CASE("Global Trade Pact activates and can be repealed", "[council]")
{
    CouncilGame_ game;
    game.Discover(*game.pA, "planetary_economics");

    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
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

TEST_CASE("Council world effect config pointers survive RebuildWorld", "[council][effects]")
{
    CouncilGame_ game;
    game.Discover(*game.pA, "planetary_economics");
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();

    // U.N. Charter starts active — retain a copy of its wrappers.
    REQUIRE_FALSE(rCouncil.CollectWorldEffects().empty());
    const std::vector<ActiveEffect_t> retained = rCouncil.CollectWorldEffects();
    const EffectScope_t firstScope = retained.front().config->scope;
    const EffectConfig_t* pFirstConfig = retained.front().config;

    game.PassStandard(*game.pA, "global_trade_pact");
    CHECK(rCouncil.IsActive("global_trade_pact"));

    // Prior wrappers' config addresses remain readable after an additive rebuild.
    CHECK(retained.front().config == pFirstConfig);
    CHECK(retained.front().config->scope == firstScope);
    CHECK(retained.front().config->scope == EffectScope_t::WorldGlobal);

    // Stronger: wrappers borrow the proposal registry, so a proposal that stayed active keeps
    // the *same* config address — a rebuild does not re-home unchanged effects.
    REQUIRE_FALSE(rCouncil.CollectWorldEffects().empty());
    CHECK(rCouncil.CollectWorldEffects().front().config == pFirstConfig);
}

TEST_CASE("Solar shade and polar caps proposals pass; the shade is repeatable", "[council]")
{
    // The world-map effect of these proposals (gradual sea-level change) is owned by the
    // WorldEvents system, not the council; here we only assert the proposal flow — passage,
    // repeatability, and tech gating.
    CouncilGame_ game;
    game.Discover(*game.pA, "advanced_spaceflight");
    game.Discover(*game.pA, "advanced_ecological_engineering");

    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    game.PassStandard(*game.pA, "launch_solar_shade");
    // These carry only one-shot outcomes, so they are history, not standing law: HasPassed,
    // not IsActive. The in-force set means "contributes continuous world effects right now" —
    // conflating the two is what let a pure repeal be consumed for the rest of the game.
    CHECK(rCouncil.HasPassed("launch_solar_shade"));
    CHECK_FALSE(rCouncil.IsActive("launch_solar_shade"));

    // increase_solar_shade is repeatable: it can be proposed and pass more than once. Its
    // required_proposals gate on launch_solar_shade is satisfied by that proposal having
    // passed, which is why the two meanings had to be separated rather than just narrowed.
    game.PassStandard(*game.pA, "increase_solar_shade");
    CHECK(CanPropose(rCouncil, *game.pA, "increase_solar_shade"));
    game.PassStandard(*game.pA, "increase_solar_shade");

    game.PassStandard(*game.pA, "melt_polar_caps");
    CHECK(rCouncil.HasPassed("melt_polar_caps"));
}

TEST_CASE("U.N. Charter starts in force and can be repealed then reinstated", "[council]")
{
    CouncilGame_ game;
    game.Discover(*game.pA, "advanced_military_algorithms");

    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
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
    game.fixtures.MakeFactionBase(*game.pA, 8, 2);
    game.fixtures.MakeFactionBase(*game.pA, 6, 4);
    game.fixtures.MakeFactionBase(*game.pB, 14, 8);

    game.GiveAllCommlinksTo(*game.pA);
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();

    const std::vector<Faction*> candidates = GovernorCandidates(rCouncil);
    REQUIRE(candidates.size() == 2);
    CHECK(candidates[0] == game.pA);
    CHECK(candidates[1] == game.pB);

    rVotes.Propose(game.fixtures.state, *game.pA, "elect_planetary_governor");
    rVotes.CastElectionVote(*game.pA, game.pA);
    rVotes.CastElectionVote(*game.pB, game.pA);
    rVotes.CastElectionVote(*game.pC, nullptr);
    REQUIRE(rVotes.Resolve(game.fixtures.state) == ResolveProposalResult_t::Passed);

    REQUIRE(rCouncil.GetPlanetaryGovernor());
    CHECK(rCouncil.GetPlanetaryGovernor() == game.pA);
    // Continuous governor Infiltration + CouncilMembers — query-time, not ledger sticky.
    CHECK(HasInfiltration(game.pA->GetFactionId(), game.pB->GetFactionId()));
    CHECK(HasInfiltration(game.pA->GetFactionId(), game.pC->GetFactionId()));
    CHECK_FALSE(game.fixtures.state.GetDiplomacyLedger().HasInfiltration(game.pA->GetFactionId(),
                                                                  game.pB->GetFactionId()));

    // Governor propose interval is 10 years; members are 20.
    CHECK(ProposeCooldownYears(rCouncil, *game.pA) == 10);
    CHECK(ProposeCooldownYears(rCouncil, *game.pB) == 20);
}

TEST_CASE("Governor veto stands unless every other member voted yea", "[council]")
{
    CouncilGame_ game;
    game.Discover(*game.pA, "planetary_economics");
    game.Discover(*game.pB, "planetary_economics");
    game.GiveAllCommlinksTo(*game.pA);

    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();

    // Elect A as governor first.
    rVotes.Propose(game.fixtures.state, *game.pA, "elect_planetary_governor");
    rVotes.CastElectionVote(*game.pA, game.pA);
    rVotes.CastElectionVote(*game.pB, game.pA);
    rVotes.CastElectionVote(*game.pC, game.pA);
    REQUIRE(rVotes.Resolve(game.fixtures.state) == ResolveProposalResult_t::Passed);

    // Non-unanimous non-governor support: veto stands.
    rVotes.Propose(game.fixtures.state, *game.pB, "global_trade_pact");
    rVotes.CastVote(*game.pA, CouncilBallot_t::Yea);
    rVotes.CastVote(*game.pB, CouncilBallot_t::Yea);
    rVotes.CastVote(*game.pC, CouncilBallot_t::Nay);
    REQUIRE(rVotes.VetoPending(*game.pA));
    CHECK(rVotes.Resolve(game.fixtures.state) == ResolveProposalResult_t::Vetoed);
    CHECK_FALSE(rCouncil.IsActive("global_trade_pact"));

    // Every non-governor voted Yea: veto is overruled from the ballots.
    game.AdvancePastProposeCooldown(*game.pB);
    rVotes.Propose(game.fixtures.state, *game.pB, "global_trade_pact");
    rVotes.CastVote(*game.pA, CouncilBallot_t::Nay);
    rVotes.CastVote(*game.pB, CouncilBallot_t::Yea);
    rVotes.CastVote(*game.pC, CouncilBallot_t::Yea);
    REQUIRE(rVotes.VetoPending(*game.pA));
    CHECK(rVotes.Resolve(game.fixtures.state) == ResolveProposalResult_t::VetoOverruled);
    CHECK(rCouncil.IsActive("global_trade_pact"));
}

TEST_CASE("Supreme Leader election stubs diplomatic victory at 75 percent", "[council]")
{
    CouncilGame_ game;
    game.Discover(*game.pA, "mind_machine_interface");
    game.GiveAllCommlinksTo(*game.pA);

    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();
    rVotes.Propose(game.fixtures.state, *game.pA, "elect_supreme_leader");

    // Equal population → each has 1/3. Need 75%, so A alone cannot win.
    rVotes.CastElectionVote(*game.pA, game.pA);
    rVotes.CastElectionVote(*game.pB, nullptr);
    rVotes.CastElectionVote(*game.pC, nullptr);
    CHECK(rVotes.Resolve(game.fixtures.state) == ResolveProposalResult_t::Failed);

    game.AdvancePastProposeCooldown(*game.pA);
    rVotes.Propose(game.fixtures.state, *game.pA, "elect_supreme_leader");
    rVotes.CastElectionVote(*game.pA, game.pA);
    rVotes.CastElectionVote(*game.pB, game.pA);
    rVotes.CastElectionVote(*game.pC, game.pA);
    CHECK(rVotes.Resolve(game.fixtures.state) == ResolveProposalResult_t::SupremeLeaderVictoryStub);
    REQUIRE(rCouncil.GetSupremeLeaderVictoryStub());
    CHECK(rCouncil.GetSupremeLeaderVictoryStub() == game.pA);
}

TEST_CASE("Council world and governor extras compose into Faction::GetActiveEffects",
          "[council][effects][composition]")
{
    CouncilGame_ game;
    game.Discover(*game.pA, "planetary_economics");
    game.GiveAllCommlinksTo(*game.pA);

    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();
    BaseManager& baseA = *game.pA->Bases().begin();
    BaseManager& baseB = *game.pB->Bases().begin();

    // Force composition once so post-law version bumps are measurable per faction.
    const uint64_t versionABefore = game.pA->GetEffectsVersion();
    const uint64_t versionBBefore = game.pB->GetEffectsVersion();
    const int nutrientsBefore = baseA.GetNutrientsRequired();

    // Passing a continuous WorldGlobal law must enter every faction's composed pool and
    // bump the effects version so BaseManager memos rebuild — without stage-appended lists.
    game.PassStandard(*game.pA, "global_trade_pact");
    CHECK(rCouncil.IsActive("global_trade_pact"));
    CHECK(game.pA->GetEffectsVersion() != versionABefore);
    CHECK(game.pB->GetEffectsVersion() != versionBBefore);

    // Diplomatic status rates are FactionPair effects in every local pool; only the council's
    // faction-wide CommerceRate is under test.
    auto hasCommerceRate = [](const FactionEffects_t& rPool)
    {
        for (const ActiveEffect_t& rEffect : rPool.effects)
        {
            if (LaneFor(rEffect.config->scope) == EffectLane_t::FactionPair)
            {
                continue;
            }
            if (const auto* pStat = std::get_if<StatModifierEffect_t>(&rEffect.config->effect))
            {
                if (pStat->stat == StatId_t::CommerceRate)
                {
                    return true;
                }
            }
        }
        return false;
    };
    CHECK(hasCommerceRate(game.pA->GetActiveEffects()));
    CHECK(hasCommerceRate(game.pB->GetActiveEffects()));
    // Local pool must not absorb council extras (harvest peers from local only).
    CHECK_FALSE(hasCommerceRate(game.pA->GetLocalActiveEffects()));

    // Memoized nutrient threshold still resolves after the world stamp moved (same list as
    // ApplyGrowth would use — no external vector).
    CHECK(baseA.GetNutrientsRequired() == nutrientsBefore);
    CHECK(baseB.GetNutrientsRequired() == baseA.GetNutrientsRequired());

    // Governor FactionGlobal extras reach only the governor's composed view.
    game.AdvancePastProposeCooldown(*game.pA);
    rVotes.Propose(game.fixtures.state, *game.pA, "elect_planetary_governor");
    rVotes.CastElectionVote(*game.pA, game.pA);
    rVotes.CastElectionVote(*game.pB, game.pA);
    rVotes.CastElectionVote(*game.pC, game.pA);
    REQUIRE(rVotes.Resolve(game.fixtures.state) == ResolveProposalResult_t::Passed);

    auto hasCommerceEnergyBonus = [](const FactionEffects_t& rPool)
    {
        for (const ActiveEffect_t& rEffect : rPool.effects)
        {
            if (const auto* pStat = std::get_if<StatModifierEffect_t>(&rEffect.config->effect))
            {
                if (pStat->stat == StatId_t::CommerceEnergyBonus)
                {
                    return true;
                }
            }
        }
        return false;
    };
    CHECK(hasCommerceEnergyBonus(game.pA->GetActiveEffects()));
    CHECK_FALSE(hasCommerceEnergyBonus(game.pB->GetActiveEffects()));
    CHECK_FALSE(hasCommerceEnergyBonus(game.pA->GetLocalActiveEffects()));
}

// --- Package 5: lifecycle exits, in-force vs enacted, tally rules -------------------------

TEST_CASE("A pending vote resolves when a member never votes", "[council][lifecycle]")
{
    // The brick: Resolve used to throw unless every member had a ballot, and nothing else
    // cleared m_pending while Propose threw whenever it was set. One silent member ended the
    // council permanently. An absent member abstains — which is what the tally already did.
    CouncilGame_ game;
    game.Discover(*game.pA, "planetary_economics");
    game.Discover(*game.pB, "planetary_economics");
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();

    game.GiveAllCommlinksTo(*game.pA);
    game.AdvancePastProposeCooldown(*game.pA);
    rVotes.Propose(game.fixtures.state, *game.pA, "global_trade_pact");

    // Only the proposer and one other vote; pC never does.
    rVotes.CastVote(*game.pA, CouncilBallot_t::Yea);
    rVotes.CastVote(*game.pB, CouncilBallot_t::Yea);
    REQUIRE_FALSE(rVotes.AllMembersVoted());

    CHECK(rVotes.Resolve(game.fixtures.state) == ResolveProposalResult_t::Passed);

    // The council is usable afterwards: the pending slot is clear and a new vote can open.
    CHECK(rVotes.GetPending() == nullptr);
    game.AdvancePastProposeCooldown(*game.pB);
    game.GiveAllCommlinksTo(*game.pB);
    CHECK_NOTHROW(rVotes.Propose(game.fixtures.state, *game.pB, "repeal_trade_pact"));
}

TEST_CASE("A repeal can be used again after its target is re-enacted", "[council][lifecycle]")
{
    // repeal_un_charter has no effects of its own, so marking it "active" on passage made
    // CanPropose's `IsActive && !repeatable` rule consume it for the rest of the game — the
    // charter could be repealed once and never again.
    CouncilGame_ game;
    game.Discover(*game.pA, "advanced_military_algorithms");
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();

    REQUIRE(rCouncil.IsActive("un_charter"));
    game.PassStandard(*game.pA, "repeal_un_charter");
    CHECK_FALSE(rCouncil.HasActiveRuleFlag(RuleFlagId_t::AtrocitiesForbidden));
    // A pure repeal is never "in force"; it is history.
    CHECK_FALSE(rCouncil.IsActive("repeal_un_charter"));
    CHECK(rCouncil.HasPassed("repeal_un_charter"));

    game.PassStandard(*game.pA, "reinstate_un_charter");
    REQUIRE(rCouncil.HasActiveRuleFlag(RuleFlagId_t::AtrocitiesForbidden));

    // The flip-flop the code always intended: repealable again now the charter is back.
    CHECK(CanPropose(rCouncil, *game.pA, "repeal_un_charter"));
    game.PassStandard(*game.pA, "repeal_un_charter");
    CHECK_FALSE(rCouncil.HasActiveRuleFlag(RuleFlagId_t::AtrocitiesForbidden));
}

TEST_CASE("A repeal is unavailable while its target is not in force", "[council][lifecycle]")
{
    // The other half of the split: proposing to repeal a law that is not standing is
    // meaningless, and this is what keeps the repeal re-proposable without making it free.
    CouncilGame_ game;
    game.Discover(*game.pA, "planetary_economics");
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();

    CHECK_FALSE(rCouncil.IsActive("global_trade_pact"));
    CHECK_FALSE(CanPropose(rCouncil, *game.pA, "repeal_trade_pact"));

    game.PassStandard(*game.pA, "global_trade_pact");
    REQUIRE(rCouncil.IsActive("global_trade_pact"));
    CHECK(CanPropose(rCouncil, *game.pA, "repeal_trade_pact"));

    // The case that actually pins the repeal gate. After the repeal passes, its
    // required_proposals entry is still satisfied (global_trade_pact *has passed*), and the
    // consumption rule now exempts repeals — so only "is the target still in force" can block
    // it. Without that rule this check fails.
    game.PassStandard(*game.pA, "repeal_trade_pact");
    REQUIRE(rCouncil.HasPassed("global_trade_pact"));
    REQUIRE_FALSE(rCouncil.IsActive("global_trade_pact"));
    CHECK_FALSE(CanPropose(rCouncil, *game.pA, "repeal_trade_pact"));
}

TEST_CASE("A standard proposal honors vote_threshold", "[council][tally]")
{
    // voteThreshold was read by the election tally only, so a modder setting it on a standard
    // proposal silently got a plain majority. Measured as a share of total vote weight, which
    // means an abstention counts against reaching the bar (same rule the election tally uses).
    CouncilGame_ game;
    game.Discover(*game.pA, "planetary_economics");
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();

    // Representative weight: one vote per member, three members. 2 of 3 is 0.667, so a 0.75
    // bar needs all three — and a bare majority must not be enough.
    CouncilProposalConfig_t& rConfig =
        const_cast<CouncilProposalConfig_t&>(rCouncil.GetRegistry().Get("global_trade_pact"));
    rConfig.voteThreshold = 0.75;

    game.GiveAllCommlinksTo(*game.pA);
    game.AdvancePastProposeCooldown(*game.pA);
    rVotes.Propose(game.fixtures.state, *game.pA, "global_trade_pact");
    rVotes.CastVote(*game.pA, CouncilBallot_t::Yea);
    rVotes.CastVote(*game.pB, CouncilBallot_t::Yea);
    rVotes.CastVote(*game.pC, CouncilBallot_t::Nay);
    // A bare majority (2 of 3) would pass under `yea > nay`; it must not clear a 0.66 bar.
    CHECK(rVotes.Resolve(game.fixtures.state) == ResolveProposalResult_t::Failed);

    game.GiveAllCommlinksTo(*game.pA);
    game.AdvancePastProposeCooldown(*game.pA);
    rVotes.Propose(game.fixtures.state, *game.pA, "global_trade_pact");
    for (Faction* pMember : rCouncil.Members())
    {
        rVotes.CastVote(*pMember, CouncilBallot_t::Yea);
    }
    CHECK(rVotes.Resolve(game.fixtures.state) == ResolveProposalResult_t::Passed);

    rConfig.voteThreshold = 0.0;
}

TEST_CASE("The AI stub never votes for an ineligible candidate", "[council][election][ai]")
{
    // The stub prefers to back the proposer, but the proposer need not be a governor
    // candidate. Since the stub runs inside Propose's OnProposalOpened emission, an
    // ineligible pick throws out of Propose — and would strand m_pending, which is a worse
    // brick than the one this package removed.
    CouncilGame_ game;
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();

    // Make the proposer the *least* populous member, so it is outside the top two.
    game.fixtures.MakeFactionBase(*game.pB, 11, 11);
    game.fixtures.MakeFactionBase(*game.pC, 2, 8);

    const std::vector<Faction*> eligible =
        EligibleCandidates(rCouncil, rCouncil.GetRegistry().Get("elect_planetary_governor"));
    REQUIRE(std::find(eligible.begin(), eligible.end(), game.pA) == eligible.end());

    game.GiveAllCommlinksTo(*game.pA);
    game.AdvancePastProposeCooldown(*game.pA);
    CHECK_NOTHROW(rVotes.Propose(game.fixtures.state, *game.pA, "elect_planetary_governor"));
    CastStubCouncilVotes(rVotes, rCouncil);

    // Every AI ballot names an eligible candidate (or abstains).
    for (const auto& [voterId, candidateId] : rVotes.GetPending()->electionVotes)
    {
        if (!candidateId)
        {
            continue;
        }
        const auto& rEligibleIds = rVotes.GetPending()->eligibleCandidateIds;
        CHECK(std::find(rEligibleIds.begin(), rEligibleIds.end(), *candidateId)
              != rEligibleIds.end());
    }
    CHECK_NOTHROW(rVotes.Resolve(game.fixtures.state));
}

TEST_CASE("CastElectionVote rejects an ineligible governor candidate", "[council][election]")
{
    // The "two most populous factions" rule lived only in GovernorCandidates(), which nothing
    // enforcing anything called — the UI passed full membership as the candidate list.
    CouncilGame_ game;
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();

    game.GiveAllCommlinksTo(*game.pA);
    game.AdvancePastProposeCooldown(*game.pA);
    rVotes.Propose(game.fixtures.state, *game.pA, "elect_planetary_governor");

    const std::vector<Faction*> eligible =
        EligibleCandidates(rCouncil, rCouncil.GetRegistry().Get("elect_planetary_governor"));
    REQUIRE(eligible.size() == 2);

    // Whichever member is not in the top two cannot be voted for.
    Faction* pIneligible = nullptr;
    for (Faction* pMember : rCouncil.Members())
    {
        if (std::find(eligible.begin(), eligible.end(), pMember) == eligible.end())
        {
            pIneligible = pMember;
        }
    }
    REQUIRE(pIneligible != nullptr);

    CHECK_THROWS(rVotes.CastElectionVote(*game.pA, pIneligible));
    CHECK_NOTHROW(rVotes.CastElectionVote(*game.pA, eligible.front()));
    // Abstaining is always allowed.
    CHECK_NOTHROW(rVotes.CastElectionVote(*game.pB, nullptr));
}

TEST_CASE("The governor cannot veto an election", "[council][veto]")
{
    // Governor rules do not apply to elections — the office cannot be defended by its incumbent.
    // See docs/game-rules-decisions.md #5. This is also why VetoOverruled may read
    // standard ballots: an election never reaches it.
    CouncilGame_ game;
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();

    // Elect a governor, then have that governor propose another election.
    game.GiveAllCommlinksTo(*game.pA);
    game.AdvancePastProposeCooldown(*game.pA);
    rVotes.Propose(game.fixtures.state, *game.pA, "elect_planetary_governor");
    for (Faction* pMember : rCouncil.Members())
    {
        rVotes.CastElectionVote(*pMember, game.pA);
    }
    REQUIRE(rVotes.Resolve(game.fixtures.state) == ResolveProposalResult_t::Passed);
    REQUIRE(rCouncil.GetPlanetaryGovernor() == game.pA);

    Faction& rGovernor = *rCouncil.GetPlanetaryGovernor();
    game.AdvancePastProposeCooldown(rGovernor);
    rVotes.Propose(game.fixtures.state, rGovernor, "elect_planetary_governor");
    REQUIRE(rVotes.GetPending() != nullptr);

    // A standard proposal would be vetoable by its governor; an election is not.
    CHECK_FALSE(rVotes.VetoPending(rGovernor));
    CHECK_FALSE(rVotes.GetPending()->vetoed);
}

// --- Expulsion (major atrocity) ------------------------------------------------------------

TEST_CASE("Expel removes the seat and discards the ballot it held", "[council][expel]")
{
    CouncilGame_ game;
    game.Discover(*game.pA, "planetary_economics");
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();

    game.GiveAllCommlinksTo(*game.pA);
    game.AdvancePastProposeCooldown(*game.pA);
    rVotes.Propose(game.fixtures.state, *game.pA, "global_trade_pact");
    rVotes.CastVote(*game.pA, CouncilBallot_t::Yea);
    rVotes.CastVote(*game.pB, CouncilBallot_t::Yea);
    REQUIRE_FALSE(rVotes.AllMembersVoted());

    // pC is the only member yet to vote, so removing it completes the ballot rather than
    // leaving the vote waiting on a seat that no longer exists.
    rVotes.Expel(*game.pC);
    CHECK_FALSE(rCouncil.IsCouncilMember(*game.pC));
    CHECK(rCouncil.Members().size() == 2);
    CHECK(rVotes.AllMembersVoted());
    CHECK(rVotes.Resolve(game.fixtures.state) == ResolveProposalResult_t::Passed);
}

TEST_CASE("Expelling the governor vacates the office and its standing effects", "[council][expel]")
{
    CouncilGame_ game;
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();
    game.GiveAllCommlinksTo(*game.pA);

    rVotes.Propose(game.fixtures.state, *game.pA, "elect_planetary_governor");
    rVotes.CastElectionVote(*game.pA, game.pA);
    rVotes.CastElectionVote(*game.pB, game.pA);
    rVotes.CastElectionVote(*game.pC, game.pA);
    REQUIRE(rVotes.Resolve(game.fixtures.state) == ResolveProposalResult_t::Passed);
    REQUIRE(rCouncil.GetPlanetaryGovernor() == game.pA);
    REQUIRE_FALSE(rCouncil.CollectFactionEffects(*game.pA).empty());

    rVotes.Expel(*game.pA);

    CHECK(rCouncil.GetPlanetaryGovernor() == nullptr);
    CHECK(rCouncil.CollectFactionEffects(*game.pA).empty());
    CHECK_FALSE(rCouncil.IsCouncilMember(*game.pA));
}

TEST_CASE("An expelled faction cannot propose", "[council][expel]")
{
    CouncilGame_ game;
    game.Discover(*game.pA, "planetary_economics");
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();
    game.GiveAllCommlinksTo(*game.pA);
    game.AdvancePastProposeCooldown(*game.pA);

    rVotes.Expel(*game.pA);

    CHECK_THROWS_AS(rVotes.Propose(game.fixtures.state, *game.pA, "global_trade_pact"),
                    std::invalid_argument);
}

TEST_CASE("Expelling a faction that holds no seat changes nothing", "[council][expel]")
{
    CouncilGame_ game;
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = *game.fixtures.state.GetCouncilVotes();
    const size_t before = rCouncil.Members().size();

    rVotes.Expel(*game.pA);
    CHECK(rCouncil.Members().size() == before - 1);

    // Idempotent: a second major atrocity by the same faction must not disturb the council.
    CHECK_NOTHROW(rVotes.Expel(*game.pA));
    CHECK(rCouncil.Members().size() == before - 1);
}

TEST_CASE("A tied election goes to the candidate with the lower faction id", "[council][election]")
{
    CouncilGame_ game;
    const PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    const FactionId_t a = game.pA->GetFactionId();
    const FactionId_t b = game.pB->GetFactionId();

    PendingProposal_t pending;
    pending.proposalId = "elect_planetary_governor";
    pending.eligibleCandidateIds = {a, b};
    pending.electionVotes[a] = b;
    pending.electionVotes[b] = a;

    const Faction* pLower = a < b ? game.pA : game.pB;
    CHECK(ElectionWinner(rCouncil, pending) == pLower);
}

TEST_CASE("A faction has commlinks to the council once it knows every other member",
          "[council][commlinks]")
{
    CouncilGame_ game;
    const PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    DiplomacyLedger& rDiplomacy = game.fixtures.state.GetDiplomacyLedger();
    const FactionId_t a = game.pA->GetFactionId();
    REQUIRE_FALSE(HasCommlinksToAllMembers(rCouncil, *game.pA));

    rDiplomacy.SetKnown(a, game.pB->GetFactionId());
    CHECK_FALSE(HasCommlinksToAllMembers(rCouncil, *game.pA));
    rDiplomacy.SetKnown(a, game.pC->GetFactionId());
    CHECK(HasCommlinksToAllMembers(rCouncil, *game.pA));
    CHECK_FALSE(HasCommlinksToAllMembers(rCouncil, *game.pB));
}

TEST_CASE("Without a threshold, a proposal passes when Yea outweighs Nay", "[council][tally]")
{
    CouncilGame_ game;
    const PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    PendingProposal_t pending;
    pending.proposalId = "global_trade_pact";
    pending.ballots[game.pA->GetFactionId()] = CouncilBallot_t::Yea;
    pending.ballots[game.pB->GetFactionId()] = CouncilBallot_t::Nay;

    // Abstaining and not voting count for neither side.
    CHECK_FALSE(ProposalPasses(rCouncil, pending));
    pending.ballots[game.pC->GetFactionId()] = CouncilBallot_t::Abstain;
    CHECK_FALSE(ProposalPasses(rCouncil, pending));
    pending.ballots[game.pC->GetFactionId()] = CouncilBallot_t::Yea;
    CHECK(ProposalPasses(rCouncil, pending));
}

TEST_CASE("Only the governor may veto, and never an election", "[council][veto]")
{
    CouncilGame_ game;
    PlanetaryCouncil& rCouncil = *game.fixtures.state.GetPlanetaryCouncil();
    rCouncil.InstallGovernor(*game.pA);
    PendingProposal_t standard;
    standard.proposalId = "global_trade_pact";
    PendingProposal_t election;
    election.proposalId = "elect_planetary_governor";

    CHECK(CanVeto(rCouncil, *game.pA, standard));
    CHECK_FALSE(CanVeto(rCouncil, *game.pB, standard));
    CHECK_FALSE(CanVeto(rCouncil, *game.pA, election));
}
