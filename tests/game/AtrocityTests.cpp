#include "GameFixtures.h"

#include "game/DifficultyConfig.h"
#include "game/DifficultyConfigParser.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/PlayerInteractionQueue.h"
#include "game/atrocities/AtrocityConfigParser.h"
#include "game/atrocities/AtrocityEffects.h"
#include "game/atrocities/AtrocityLedger.h"
#include "game/atrocities/AtrocityRules.h"
#include "game/council/PlanetaryCouncil.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/ResearchManager.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

constexpr int k_StartYear = 2100;
constexpr AtrocitySeverityId_t k_Simple = AtrocitySeverityId_t::Simple;
constexpr AtrocitySeverityId_t k_Major = AtrocitySeverityId_t::Major;

struct AtrocityGame_
{
    FactionFixture fixtures;
    GameSettings settings;
    std::unique_ptr<GameState> pState;
    Faction* pA = nullptr;
    Faction* pB = nullptr;
    Faction* pC = nullptr;

    AtrocityGame_()
    {
        InstallCouncil(fixtures.dataContext);

        auto pMap = std::make_unique<WorldMap>(9, 9, actest::TestMapRules());
        for (auto& pTile : pMap->GetTiles())
        {
            pTile->SetElevation(100);
        }
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.dataContext, settings, actest::k_TestRngSeed);

        pA = &AddFaction_(fixtures.factionDefinition, true);
        pB = &AddFaction_(fixtures.factionDefinition, false);
        pC = &AddFaction_(fixtures.factionDefinition, false);

        fixtures.MakeFactionBase(*pA, 2, 2);
        fixtures.MakeFactionBase(*pB, 6, 2);
        fixtures.MakeFactionBase(*pC, 4, 6);

        pState->CreatePlanetaryCouncil();
        pState->SetMissionYear(k_StartYear);
    }

    Faction& AddFaction_(const FactionConfig_t& rDefinition, bool bPlayer)
    {
        auto pFaction = std::make_unique<Faction>(
            pState->AllocateFactionId(), bPlayer, rDefinition, fixtures.dataContext,
            pState->GetWorldMap(), settings, actest::k_TestFactionSeed);
        return pState->AddFaction(std::move(pFaction));
    }

    AtrocityLedger& Ledger() { return pState->GetAtrocityLedger(); }
    bool Sanctioned(FactionId_t faction)
    {
        return Ledger().IsSanctioned(faction, pState->GetMissionYear());
    }
    DiplomacyLedger& Diplomacy() { return pState->GetDiplomacyLedger(); }
    const AtrocitiesConfig_t& Config() const { return *fixtures.dataContext.atrocitiesConfig; }

    // Every member votes Yea on rProposalId, which is how the Charter gets repealed here.
    void PassStandard(Faction& rProposer, const std::string& rProposalId)
    {
        PlanetaryCouncil& rCouncil = *pState->GetPlanetaryCouncil();
        for (Faction* pOther : rCouncil.Members())
        {
            if (pOther->GetFactionId() != rProposer.GetFactionId())
            {
                Diplomacy().SetKnown(rProposer.GetFactionId(), pOther->GetFactionId());
            }
        }
        const int wait = rCouncil.YearsUntilCanPropose(*pState, rProposer);
        if (wait > 0)
        {
            pState->SetMissionYear(pState->GetMissionYear() + wait);
        }
        rCouncil.Propose(*pState, rProposer, rProposalId);
        for (Faction* pMember : rCouncil.Members())
        {
            rCouncil.CastVote(*pMember, CouncilBallot_t::Yea);
        }
        const ResolveProposalResult_t result = rCouncil.Resolve(*pState);
        REQUIRE((result == ResolveProposalResult_t::Passed
                 || result == ResolveProposalResult_t::VetoOverruled));
    }

    // rProposer must still hold a seat, so a faction already expelled for a major atrocity
    // cannot be the one to move the repeal.
    void RepealCharter(Faction& rProposer)
    {
        if (!rProposer.GetResearch().HasDiscoveredTech("advanced_military_algorithms"))
        {
            rProposer.GetResearch().AddDiscoveredTech("advanced_military_algorithms");
        }
        PassStandard(rProposer, "repeal_un_charter");
        REQUIRE_FALSE(pState->GetPlanetaryCouncil()->HasActiveRuleFlag(
            RuleFlagId_t::AtrocitiesForbidden));
    }
};

} // namespace

TEST_CASE("A Simple atrocity under the Charter sanctions and the victim remembers it",
          "[atrocity]")
{
    AtrocityGame_ game;

    const AtrocityCommitted_t committed =
        CommitAtrocity(*game.pState, *game.pA, game.pB, k_Simple);

    CHECK(committed.severity == k_Simple);
    CHECK(committed.PenaltiesApplied());
    CHECK(game.Ledger().Records().back().bCounted);
    CHECK(committed.sanctionUntilYear == k_StartYear + game.Config().sanctionYearsPerAtrocity);

    CHECK(game.Sanctioned(game.pA->GetFactionId()));
    CHECK(game.Ledger().Records().size() == 1);
    CHECK(game.Ledger().HasVictimized(game.pA->GetFactionId(), game.pB->GetFactionId()));
    CHECK_FALSE(game.Ledger().HasCommittedMajorAgainst(game.pA->GetFactionId(),
                                                       game.pB->GetFactionId()));

    // The act is an attack on the victim. A Simple atrocity is not a Major one: no universal
    // vendetta from bystanders, and the seat is kept.
    CHECK(game.Diplomacy().HasVendetta(game.pB->GetFactionId(), game.pA->GetFactionId()));
    CHECK_FALSE(game.Diplomacy().HasVendetta(game.pC->GetFactionId(), game.pA->GetFactionId()));
    CHECK(game.pState->GetPlanetaryCouncil()->IsCouncilMember(*game.pA));
}

TEST_CASE("A sanction lapses on its expiry year, not the turn before", "[atrocity]")
{
    AtrocityGame_ game;
    const std::optional<int> until =
        CommitAtrocity(*game.pState, *game.pA, game.pB, k_Simple).sanctionUntilYear;
    REQUIRE(until.has_value());
    const FactionId_t id = game.pA->GetFactionId();

    game.pState->SetMissionYear(*until - 1);
    CHECK(game.Sanctioned(id));

    game.pState->SetMissionYear(*until);
    CHECK_FALSE(game.Sanctioned(id));

    // The sweep only drops the entry the query already reports as lifted.
    game.Ledger().ExpireSanctions(*until);
    CHECK_FALSE(game.Ledger().SanctionUntilYear(id).has_value());
}

TEST_CASE("Simple sanctions stack as the coefficient times the simple count", "[atrocity]")
{
    AtrocityGame_ game;
    const int per = game.Config().sanctionYearsPerAtrocity;
    REQUIRE(per == 7);

    // per, then +2×per, +3×per, +4×per, +5×per. Five in the same year come to 15×per.
    int expected = k_StartYear;
    for (int count = 1; count <= 5; ++count)
    {
        expected += per * count;
        const AtrocityCommitted_t committed =
            CommitAtrocity(*game.pState, *game.pA, game.pB, k_Simple);
        CHECK(committed.sanctionUntilYear == expected);
    }
    CHECK(game.Ledger().SanctionUntilYear(game.pA->GetFactionId()) == k_StartYear + 15 * per);
}

TEST_CASE("A Major atrocity carries no commerce sanction", "[atrocity]")
{
    AtrocityGame_ game;
    const FactionId_t id = game.pA->GetFactionId();

    const std::optional<int> untilSimple =
        CommitAtrocity(*game.pState, *game.pA, game.pB, k_Simple).sanctionUntilYear;
    REQUIRE(untilSimple == k_StartYear + game.Config().sanctionYearsPerAtrocity);

    const AtrocityCommitted_t major =
        CommitAtrocity(*game.pState, *game.pA, game.pB, k_Major);
    CHECK(major.PenaltiesApplied());
    CHECK(major.sanctionUntilYear.has_value() == false);
    CHECK(game.Ledger().SanctionUntilYear(id) == untilSimple);
    CHECK(game.Ledger().SimpleCount(id) == 1);
}

TEST_CASE("Lesser atrocities escalate to Major once their count reaches the threshold",
          "[atrocity]")
{
    AtrocityGame_ game;
    const DifficultyLevel_t& rLevel = game.fixtures.dataContext.difficultyConfig->RequireForSession(
        game.settings.GetGameRules().difficultyId);
    const int threshold = rLevel.rules.playerAtrocityThreshold;
    // The act that lands exactly on the threshold is still Simple; the next one is Major.
    REQUIRE(threshold == 6);

    for (int i = 0; i < threshold; ++i)
    {
        const AtrocityCommitted_t committed =
            CommitAtrocity(*game.pState, *game.pA, game.pB, k_Simple);
        REQUIRE(committed.severity == k_Simple);
    }
    CHECK(game.pState->GetPlanetaryCouncil()->IsCouncilMember(*game.pA));

    const AtrocityCommitted_t escalated =
        CommitAtrocity(*game.pState, *game.pA, game.pB, k_Simple);
    CHECK(escalated.severity == k_Major);
    // Universal Vendetta covers living AI bystanders; the victim is at Vendetta from the act.
    CHECK(game.Diplomacy().GetStatus(game.pC->GetFactionId(), game.pA->GetFactionId())
          == DiplomaticStatus_t::Vendetta);
    CHECK_FALSE(game.pState->GetPlanetaryCouncil()->IsCouncilMember(*game.pA));
}

TEST_CASE("Transcend escalates lesser atrocities at its stated threshold", "[atrocity]")
{
    AtrocityGame_ game;
    GameRulesConfig_t rules = game.settings.GetGameRules();
    rules.difficultyId = "transcend";
    game.settings.SetGameRules(rules);

    const int threshold = game.fixtures.dataContext.difficultyConfig
                              ->RequireForSession("transcend")
                              .rules.playerAtrocityThreshold;
    REQUIRE(threshold == 3);

    for (int i = 0; i < threshold; ++i)
    {
        const AtrocityCommitted_t committed =
            CommitAtrocity(*game.pState, *game.pA, game.pB, k_Simple);
        REQUIRE(committed.severity == k_Simple);
    }
    CHECK(CommitAtrocity(*game.pState, *game.pA, game.pB, k_Simple).severity == k_Major);
}

TEST_CASE("An AI faction escalates at the configured AI level, not the session one", "[atrocity]")
{
    AtrocityGame_ game;
    GameRulesConfig_t rules = game.settings.GetGameRules();
    rules.difficultyId = "transcend";
    game.settings.SetGameRules(rules);

    const DifficultyRules_t& rRules =
        game.fixtures.dataContext.difficultyConfig->RequireForSession("transcend").rules;
    const int sessionThreshold = rRules.playerAtrocityThreshold;
    const int aiThreshold = rRules.aiAtrocityThreshold;
    REQUIRE(sessionThreshold == 3);
    REQUIRE(aiThreshold == 7);
    REQUIRE_FALSE(game.pB->IsPlayerControlled());

    for (int i = 0; i < aiThreshold; ++i)
    {
        const AtrocityCommitted_t committed =
            CommitAtrocity(*game.pState, *game.pB, game.pC, k_Simple);
        REQUIRE(committed.severity == k_Simple);
    }
    CHECK(CommitAtrocity(*game.pState, *game.pB, game.pC, k_Simple).severity == k_Major);
}

TEST_CASE("An excused Simple act is recorded and does not escalate", "[atrocity]")
{
    AtrocityGame_ game;
    game.RepealCharter(*game.pA);

    const int threshold = game.fixtures.dataContext.difficultyConfig
                              ->RequireForSession(game.settings.GetGameRules().difficultyId)
                              .rules.playerAtrocityThreshold;
    REQUIRE(threshold > 0);

    for (int i = 0; i < threshold; ++i)
    {
        const AtrocityCommitted_t committed =
            CommitAtrocity(*game.pState, *game.pA, game.pB, k_Simple);
        CHECK(committed.severity == k_Simple);
        CHECK_FALSE(committed.PenaltiesApplied());
        CHECK_FALSE(game.Ledger().Records().back().bCounted);
    }

    CHECK(game.Ledger().SimpleCount(game.pA->GetFactionId()) == 0);
    CHECK(game.pState->GetPlanetaryCouncil()->IsCouncilMember(*game.pA));
    // Excused from penalties, but still an attack on the victim.
    CHECK(game.Diplomacy().HasVendetta(game.pB->GetFactionId(), game.pA->GetFactionId()));
    CHECK_FALSE(game.Diplomacy().HasVendetta(game.pC->GetFactionId(), game.pA->GetFactionId()));
}

TEST_CASE("A Simple act against another species does not move the simple counter", "[atrocity]")
{
    AtrocityGame_ game;
    FactionConfig_t alien = game.fixtures.factionDefinition;
    alien.id = "alien";
    alien.identity.species = FactionSpecies_t::Progenitor;
    Faction& rAlien = game.AddFaction_(alien, false);

    const int threshold = game.fixtures.dataContext.difficultyConfig
                              ->RequireForSession(game.settings.GetGameRules().difficultyId)
                              .rules.playerAtrocityThreshold;
    REQUIRE(threshold > 0);

    for (int i = 0; i < threshold; ++i)
    {
        const AtrocityCommitted_t committed =
            CommitAtrocity(*game.pState, *game.pA, &rAlien, k_Simple);
        CHECK(committed.severity == k_Simple);
        CHECK_FALSE(committed.PenaltiesApplied());
    }
    CHECK(game.Ledger().SimpleCount(game.pA->GetFactionId()) == 0);

    // The same-species act is the first one that counts, so it is still Simple.
    const AtrocityCommitted_t counted =
        CommitAtrocity(*game.pState, *game.pA, game.pB, k_Simple);
    CHECK(counted.severity == k_Simple);
    CHECK(counted.PenaltiesApplied());
    CHECK(game.pState->GetPlanetaryCouncil()->IsCouncilMember(*game.pA));
}

TEST_CASE("A Major atrocity does not fill the Simple counter", "[atrocity]")
{
    AtrocityGame_ game;
    const FactionId_t id = game.pA->GetFactionId();
    const int threshold = game.fixtures.dataContext.difficultyConfig
                              ->RequireForSession(game.settings.GetGameRules().difficultyId)
                              .rules.playerAtrocityThreshold;
    REQUIRE(threshold == 6);

    const AtrocityCommitted_t major =
        CommitAtrocity(*game.pState, *game.pA, game.pB, k_Major);
    CHECK(major.severity == k_Major);
    CHECK(major.sanctionUntilYear.has_value() == false);
    CHECK_FALSE(game.Sanctioned(id));
    CHECK(game.Ledger().Records().back().bCounted);
    CHECK(game.Ledger().SimpleCount(id) == 0);

    // One Planet Buster leaves the full Simple ladder, including the act that lands on the
    // threshold. The next Simple act is not Major.
    for (int i = 0; i < threshold; ++i)
    {
        const AtrocityCommitted_t committed =
            CommitAtrocity(*game.pState, *game.pA, game.pB, k_Simple);
        REQUIRE(committed.severity == k_Simple);
    }
    CHECK(game.Ledger().SimpleCount(id) == threshold);
    CHECK(CommitAtrocity(*game.pState, *game.pA, game.pB, k_Simple).severity == k_Major);
}

TEST_CASE("An uncounted atrocity does not lengthen the next sanction", "[atrocity]")
{
    AtrocityGame_ game;
    FactionConfig_t alien = game.fixtures.factionDefinition;
    alien.id = "alien";
    alien.identity.species = FactionSpecies_t::Progenitor;
    Faction& rAlien = game.AddFaction_(alien, false);

    const int per = game.Config().sanctionYearsPerAtrocity;
    REQUIRE(per == 7);

    const AtrocityCommitted_t first =
        CommitAtrocity(*game.pState, *game.pA, game.pB, k_Simple);
    CHECK(first.sanctionUntilYear == k_StartYear + per);

    const AtrocityCommitted_t excused =
        CommitAtrocity(*game.pState, *game.pA, &rAlien, k_Simple);
    CHECK_FALSE(excused.PenaltiesApplied());
    CHECK_FALSE(game.Ledger().Records().back().bCounted);
    CHECK(game.Ledger().SanctionUntilYear(game.pA->GetFactionId()) == first.sanctionUntilYear);

    // The excused act is not in the simple count, so this one adds per × 2 onto the per left.
    const AtrocityCommitted_t third =
        CommitAtrocity(*game.pState, *game.pA, game.pB, k_Simple);
    CHECK(third.sanctionUntilYear == k_StartYear + per + per * 2);
}

TEST_CASE("A repealed Charter records a Simple atrocity but charges nothing for it", "[atrocity]")
{
    AtrocityGame_ game;
    game.RepealCharter(*game.pA);

    const AtrocityCommitted_t committed =
        CommitAtrocity(*game.pState, *game.pA, game.pB, k_Simple);

    CHECK_FALSE(committed.PenaltiesApplied());
    CHECK(committed.sanctionUntilYear.has_value() == false);
    CHECK(game.Ledger().Records().size() == 1);
    CHECK_FALSE(game.Sanctioned(game.pA->GetFactionId()));
    CHECK(game.Ledger().HasVictimized(game.pA->GetFactionId(), game.pB->GetFactionId()));
}

TEST_CASE("A repealed Charter lifts Major penalties and the victim still remembers it", "[atrocity]")
{
    AtrocityGame_ game;
    game.RepealCharter(*game.pA);
    const FactionId_t id = game.pA->GetFactionId();

    const AtrocityCommitted_t committed =
        CommitAtrocity(*game.pState, *game.pA, game.pB, k_Major);

    CHECK_FALSE(committed.PenaltiesApplied());
    CHECK(committed.sanctionUntilYear.has_value() == false);
    CHECK_FALSE(game.Ledger().Records().back().bCounted);
    CHECK(game.pState->GetPlanetaryCouncil()->IsCouncilMember(*game.pA));
    CHECK(game.Diplomacy().HasVendetta(game.pB->GetFactionId(), id));
    CHECK(game.Diplomacy().GetStatus(game.pC->GetFactionId(), id) != DiplomaticStatus_t::Vendetta);
    CHECK(game.Ledger().EcoVirtualMinerals(id, game.Config()) == 0);
    CHECK(game.Ledger().HasVictimized(id, game.pB->GetFactionId()));
    CHECK(game.Ledger().HasCommittedMajorAgainst(id, game.pB->GetFactionId()));
}

TEST_CASE("Either party being a Progenitor excuses every tier, and the victim remembers it",
          "[atrocity]")
{
    AtrocityGame_ game;
    FactionConfig_t alien = game.fixtures.factionDefinition;
    alien.id = "caretaker";
    alien.identity.species = FactionSpecies_t::Progenitor;
    Faction& rAlien = game.AddFaction_(alien, false);

    FactionConfig_t usurper = game.fixtures.factionDefinition;
    usurper.id = "usurper";
    usurper.identity.species = FactionSpecies_t::Progenitor;
    Faction& rUsurper = game.AddFaction_(usurper, false);

    FactionConfig_t worms = game.fixtures.factionDefinition;
    worms.id = "worms";
    worms.identity.species = FactionSpecies_t::NativeLife;
    Faction& rWorms = game.AddFaction_(worms, false);

    const FactionId_t humanId = game.pA->GetFactionId();

    const AtrocityCommitted_t simple =
        CommitAtrocity(*game.pState, *game.pA, &rAlien, k_Simple);
    CHECK_FALSE(simple.PenaltiesApplied());
    CHECK_FALSE(game.Ledger().Records().back().bCounted);
    CHECK_FALSE(game.Sanctioned(humanId));
    CHECK(game.Ledger().HasVictimized(humanId, rAlien.GetFactionId()));
    CHECK(game.Diplomacy().HasVendetta(rAlien.GetFactionId(), humanId));
    CHECK(game.pState->GetPlanetaryCouncil()->IsCouncilMember(*game.pA));

    const AtrocityCommitted_t major =
        CommitAtrocity(*game.pState, *game.pA, &rAlien, k_Major);
    CHECK_FALSE(major.PenaltiesApplied());
    CHECK_FALSE(game.Ledger().Records().back().bCounted);
    CHECK(game.Ledger().HasCommittedMajorAgainst(humanId, rAlien.GetFactionId()));
    CHECK(game.Diplomacy().GetStatus(game.pB->GetFactionId(), humanId)
          != DiplomaticStatus_t::Vendetta);
    CHECK(game.pState->GetPlanetaryCouncil()->IsCouncilMember(*game.pA));

    const AtrocityCommitted_t progenitorPerp =
        CommitAtrocity(*game.pState, rAlien, game.pA, k_Major);
    CHECK_FALSE(progenitorPerp.PenaltiesApplied());
    CHECK(game.Ledger().HasCommittedMajorAgainst(rAlien.GetFactionId(), humanId));
    CHECK(game.Diplomacy().HasVendetta(humanId, rAlien.GetFactionId()));

    const AtrocityCommitted_t bothProgenitors =
        CommitAtrocity(*game.pState, rUsurper, &rAlien, k_Major);
    CHECK_FALSE(bothProgenitors.PenaltiesApplied());
    CHECK(game.Ledger().HasCommittedMajorAgainst(rUsurper.GetFactionId(), rAlien.GetFactionId()));

    const AtrocityCommitted_t againstLife =
        CommitAtrocity(*game.pState, *game.pA, &rWorms, k_Simple);
    CHECK(againstLife.PenaltiesApplied());
    CHECK(game.Sanctioned(humanId));
}

TEST_CASE("A victimless Major atrocity is still answered for", "[atrocity]")
{
    AtrocityGame_ game;

    const AtrocityCommitted_t committed =
        CommitAtrocity(*game.pState, *game.pA, nullptr, k_Major);

    CHECK(committed.PenaltiesApplied());
    CHECK(committed.sanctionUntilYear.has_value() == false);
    CHECK_FALSE(game.Sanctioned(game.pA->GetFactionId()));
    CHECK_FALSE(game.pState->GetPlanetaryCouncil()->IsCouncilMember(*game.pA));
    CHECK(game.Ledger().Records().back().victim.has_value() == false);
    // No victim to exclude: every living AI declares Vendetta. The human player is not forced.
    CHECK(game.Diplomacy().GetStatus(game.pB->GetFactionId(), game.pA->GetFactionId())
          == DiplomaticStatus_t::Vendetta);
    CHECK(game.Diplomacy().GetStatus(game.pC->GetFactionId(), game.pA->GetFactionId())
          == DiplomaticStatus_t::Vendetta);
}

TEST_CASE("Universal Vendetta skips humans and existing Vendettas", "[atrocity]")
{
    AtrocityGame_ game;
    Faction& rHuman = *game.pA;
    Faction& rVictim = *game.pB;
    Faction& rBystander = *game.pC;
    Faction& rAlready =
        game.AddFaction_(game.fixtures.factionDefinition, /*bPlayer=*/false);
    game.Diplomacy().SetStatus(rAlready.GetFactionId(), rHuman.GetFactionId(),
                               DiplomaticStatus_t::Vendetta);

    CommitAtrocity(*game.pState, rHuman, &rVictim, k_Major);

    CHECK(game.Diplomacy().GetStatus(rBystander.GetFactionId(), rHuman.GetFactionId())
          == DiplomaticStatus_t::Vendetta);
    CHECK(game.Diplomacy().AreKnown(rBystander.GetFactionId(), rHuman.GetFactionId()));
    // Already at Vendetta: left alone (and not re-processed for eviction).
    CHECK(game.Diplomacy().HasVendetta(rAlready.GetFactionId(), rHuman.GetFactionId()));

    // An AI Major does not force the human player into Vendetta.
    Faction& rAiPerp =
        game.AddFaction_(game.fixtures.factionDefinition, /*bPlayer=*/false);
    CommitAtrocity(*game.pState, rAiPerp, &rVictim, k_Major);
    CHECK(game.Diplomacy().GetStatus(rHuman.GetFactionId(), rAiPerp.GetFactionId())
          != DiplomaticStatus_t::Vendetta);
}

TEST_CASE("A Major atrocity's universal Vendetta obliges nobody to defend the perpetrator",
          "[atrocity]")
{
    AtrocityGame_ game;
    Faction& rHuman = *game.pA;
    Faction& rVictim = *game.pB;
    Faction& rPartner = *game.pC;
    Faction& rPerpetrator =
        game.AddFaction_(game.fixtures.factionDefinition, /*bPlayer=*/false);
    game.Diplomacy().SetStatus(rHuman.GetFactionId(), rPerpetrator.GetFactionId(),
                               DiplomaticStatus_t::Pact);
    game.Diplomacy().SetStatus(rPartner.GetFactionId(), rPerpetrator.GetFactionId(),
                               DiplomaticStatus_t::Pact);

    CommitAtrocity(*game.pState, rPerpetrator, &rVictim, k_Major);

    // The AI partner joins the world against the perpetrator, ending its Pact; the human
    // partner is never forced and is not asked to defend the perpetrator either.
    CHECK(game.Diplomacy().HasVendetta(rPartner.GetFactionId(), rPerpetrator.GetFactionId()));
    CHECK(game.Diplomacy().HasPact(rHuman.GetFactionId(), rPerpetrator.GetFactionId()));
    CHECK_FALSE(game.pState->GetPlayerInteractions().AnyOf(
        [](const QueuedInteraction_t& rQueued)
        { return std::holds_alternative<PactObligationInteraction_t>(rQueued.payload); }));
}

TEST_CASE("Nuking your own ground is an atrocity with nobody to resent you for it", "[atrocity]")
{
    AtrocityGame_ game;

    const AtrocityCommitted_t committed =
        CommitAtrocity(*game.pState, *game.pA, game.pA, k_Major);

    CHECK(committed.PenaltiesApplied());
    CHECK(committed.sanctionUntilYear.has_value() == false);
    CHECK_FALSE(game.Sanctioned(game.pA->GetFactionId()));
    // Recorded victimless rather than as a self-victim every query then has to discount.
    CHECK_FALSE(game.Ledger().Records().back().victim.has_value());
    CHECK_FALSE(game.Ledger().HasVictimized(game.pA->GetFactionId(), game.pA->GetFactionId()));
}

TEST_CASE("The eco term counts Major atrocities committed under the Charter only", "[atrocity]")
{
    AtrocityGame_ game;
    const FactionId_t id = game.pA->GetFactionId();
    const int majorWeight = game.Config().For(k_Major).ecoVirtualMinerals;
    REQUIRE(majorWeight > 0);

    CommitAtrocity(*game.pState, *game.pA, game.pB, k_Simple);
    CHECK(game.Ledger().EcoVirtualMinerals(id, game.Config()) == 0);

    CommitAtrocity(*game.pState, *game.pA, game.pB, k_Major);
    CHECK(game.Ledger().EcoVirtualMinerals(id, game.Config()) == majorWeight);

    // Two warheads under the Charter, two contributions.
    CommitAtrocity(*game.pState, *game.pA, game.pB, k_Major);
    CHECK(game.Ledger().EcoVirtualMinerals(id, game.Config()) == 2 * majorWeight);

    // "Any atrocities after lifting the UN-charter do not count." pA is off the council by now,
    // so a faction that still holds a seat has to move the repeal.
    game.RepealCharter(*game.pC);
    CommitAtrocity(*game.pState, *game.pA, game.pB, k_Major);
    CHECK(game.Ledger().EcoVirtualMinerals(id, game.Config()) == 2 * majorWeight);

    // Another faction's record is not ours.
    CommitAtrocity(*game.pState, *game.pB, game.pC, k_Major);
    CHECK(game.Ledger().EcoVirtualMinerals(id, game.Config()) == 2 * majorWeight);
}

// --- Config ---------------------------------------------------------------------------------

TEST_CASE("atrocities.json round-trips and rejects bad input", "[atrocity][parser]")
{
    const AtrocitiesConfig_t config =
        AtrocityConfigParser{}.ParseConfig(FixturePath("atrocities.json"));

    const AtrocitySeverityConfig_t& rSimple = config.For(k_Simple);
    CHECK(rSimple.ecoVirtualMinerals == 0);
    CHECK_FALSE(rSimple.bUniversalVendetta);
    CHECK_FALSE(rSimple.bExpelFromCouncil);

    const AtrocitySeverityConfig_t& rMajor = config.For(k_Major);
    CHECK(rMajor.ecoVirtualMinerals == 3);
    CHECK(rMajor.bUniversalVendetta);
    CHECK(rMajor.bExpelFromCouncil);

    CHECK(config.sanctionYearsPerAtrocity == 7);
}

TEST_CASE("The atrocities parser rejects a missing severity and a bad name", "[atrocity][parser]")
{
    const auto writeTemp = [](const std::string& rName, const std::string& rBody)
    {
        const std::filesystem::path path = std::filesystem::temp_directory_path() / rName;
        std::ofstream out(path);
        out << rBody;
        out.close();
        return path.string();
    };

    // Every severity must be present: a tier silently falling back to a C++ default is how a
    // Major atrocity would quietly stop costing anything.
    const std::string onlySimple = R"({
      "severities": {
        "Simple": {
          "eco_virtual_minerals": 0, "universal_vendetta": false, "expel_from_council": false
        }
      },
      "sanction_years_per_atrocity": 10
    })";
    CHECK_THROWS_AS(
        AtrocityConfigParser{}.ParseConfig(writeTemp("ac_atrocities_partial.json", onlySimple)),
        std::runtime_error);

    const std::string badName = R"({
      "severities": {
        "Moderate": {
          "eco_virtual_minerals": 0, "universal_vendetta": false, "expel_from_council": false
        }
      },
      "sanction_years_per_atrocity": 10
    })";
    CHECK_THROWS_AS(
        AtrocityConfigParser{}.ParseConfig(writeTemp("ac_atrocities_badname.json", badName)),
        std::runtime_error);

    // A missing required key inside a severity is an error, not a zero.
    const std::string missingKey = R"({
      "severities": {
        "Simple": {
          "eco_virtual_minerals": 0, "universal_vendetta": false, "expel_from_council": false
        },
        "Major": { "universal_vendetta": true }
      },
      "sanction_years_per_atrocity": 10
    })";
    CHECK_THROWS_AS(
        AtrocityConfigParser{}.ParseConfig(writeTemp("ac_atrocities_missingkey.json", missingKey)),
        std::runtime_error);

    // A retired or misspelled key is a typo the author wants to hear about, not a silent no-op.
    const std::string unknownKey = R"({
      "severities": {
        "Simple": {
          "eco_virtual_minerals": 0, "universal_vendetta": false, "expel_from_council": false
        },
        "Major": {
          "eco_virtual_minerals": 5, "universal_vendetta": true, "expel_from_council": true,
          "integrity_loss": 50
        }
      },
      "sanction_years_per_atrocity": 10
    })";
    CHECK_THROWS_AS(
        AtrocityConfigParser{}.ParseConfig(writeTemp("ac_atrocities_unknown.json", unknownKey)),
        std::runtime_error);
}

// --- Rules (pure) ---------------------------------------------------------------------------

TEST_CASE("EffectiveSeverityId escalates on accumulated Simple acts", "[atrocity][rules]")
{
    const int threshold = 20;

    // An act authored as Major is Major on its first use.
    CHECK(EffectiveSeverityId(k_Major, 0, threshold) == k_Major);

    // The act that lands exactly on the threshold is still Simple. The next one is Major.
    CHECK(EffectiveSeverityId(k_Simple, 0, threshold) == k_Simple);
    CHECK(EffectiveSeverityId(k_Simple, 19, threshold) == k_Simple);
    CHECK(EffectiveSeverityId(k_Simple, 20, threshold) == k_Major);

    // A zero threshold disables escalation however deep the record.
    CHECK(EffectiveSeverityId(k_Simple, 999, 0) == k_Simple);
}

TEST_CASE("PenaltiesApply gates on the Charter and on a Progenitor party", "[atrocity][rules]")
{
    CHECK(PenaltiesApply(/*bCharterInForce=*/true, /*bSpeciesExempt=*/false));
    CHECK_FALSE(PenaltiesApply(false, false));
    CHECK_FALSE(PenaltiesApply(true, true));
    CHECK_FALSE(PenaltiesApply(false, true));

    const auto human = FactionSpecies_t::Human;
    const auto progenitor = FactionSpecies_t::Progenitor;
    const auto life = FactionSpecies_t::NativeLife;

    CHECK_FALSE(SpeciesExemptionApplies(human, human));
    CHECK_FALSE(SpeciesExemptionApplies(human, life));
    CHECK_FALSE(SpeciesExemptionApplies(human, std::nullopt));
    CHECK(SpeciesExemptionApplies(human, progenitor));
    CHECK(SpeciesExemptionApplies(progenitor, human));
    CHECK(SpeciesExemptionApplies(progenitor, progenitor));
    CHECK(SpeciesExemptionApplies(progenitor, std::nullopt));
}

TEST_CASE("SanctionYearsAdded is the coefficient times the simple count after the act",
          "[atrocity][rules]")
{
    AtrocitiesConfig_t config;
    config.sanctionYearsPerAtrocity = 10;

    CHECK(SanctionYearsAdded(config, 1) == 10);
    CHECK(SanctionYearsAdded(config, 2) == 20);
    CHECK(SanctionYearsAdded(config, 5) == 50);

    config.sanctionYearsPerAtrocity = 0;
    CHECK(SanctionYearsAdded(config, 5) == 0);
}

TEST_CASE("BlastVictim prefers a razed base and never names the detonator", "[atrocity][rules]")
{
    constexpr FactionId_t k_Detonator = 1;
    constexpr FactionId_t k_Enemy = 2;
    constexpr FactionId_t k_Other = 3;

    // A razed base outranks a killed unit even when the unit was reached first.
    CHECK(BlastVictim({k_Enemy}, {k_Other}, k_Detonator) == k_Enemy);
    // Own losses are skipped on both lists.
    CHECK(BlastVictim({k_Detonator}, {k_Detonator, k_Enemy}, k_Detonator) == k_Enemy);
    // Units answer only when no foreign base was razed.
    CHECK(BlastVictim({}, {k_Enemy}, k_Detonator) == k_Enemy);
    // Razing only your own ground is victimless.
    CHECK_FALSE(BlastVictim({k_Detonator}, {k_Detonator}, k_Detonator).has_value());
    CHECK_FALSE(BlastVictim({}, {}, k_Detonator).has_value());
}

TEST_CASE("The atrocities parser rejects a severity outside the closed set", "[atrocity][parser]")
{
    const std::string extraTier = R"({
      "severities": {
        "Simple": {
          "eco_virtual_minerals": 0, "universal_vendetta": false, "expel_from_council": false
        },
        "Moderate": {
          "eco_virtual_minerals": 0, "universal_vendetta": false, "expel_from_council": false
        },
        "Major": {
          "eco_virtual_minerals": 5, "universal_vendetta": true, "expel_from_council": true
        }
      },
      "sanction_years_per_atrocity": 0
    })";
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ac_atrocities_extra_tier.json";
    std::ofstream out(path);
    out << extraTier;
    out.close();

    CHECK_THROWS_AS(AtrocityConfigParser{}.ParseConfig(path.string()), std::runtime_error);
}

// --- Notice ---------------------------------------------------------------------------------

TEST_CASE("Committing an atrocity tells the player what it cost", "[atrocity][notice]")
{
    AtrocityGame_ game;
    PlayerInteractionQueue& rQueue = game.pState->GetPlayerInteractions();
    REQUIRE(rQueue.Empty());

    const AtrocityCommitted_t committed =
        CommitAtrocity(*game.pState, *game.pA, game.pB, k_Simple);
    REQUIRE(committed.PenaltiesApplied());

    REQUIRE_FALSE(rQueue.Empty());
    const auto* pNotice = std::get_if<NoticeInteraction_t>(&rQueue.Front()->payload);
    REQUIRE(pNotice);
    CHECK(pNotice->event == PauseOnEventId_t::AtrocityCommitted);
    // The sanction year is the actionable part, so it has to be in the body.
    CHECK(pNotice->body.find(std::to_string(*committed.sanctionUntilYear)) != std::string::npos);
}

TEST_CASE("The notice names the tier in prose, not as the enumerator", "[atrocity][notice]")
{
    AtrocityGame_ game;
    PlayerInteractionQueue& rQueue = game.pState->GetPlayerInteractions();

    CommitAtrocity(*game.pState, *game.pA, game.pB, k_Major);
    const auto* pNotice = std::get_if<NoticeInteraction_t>(&rQueue.Front()->payload);
    REQUIRE(pNotice);
    CHECK(pNotice->body.find("major atrocity") != std::string::npos);
    // The C++ spelling is not player-facing copy.
    CHECK(pNotice->body.find("Major") == std::string::npos);
}

TEST_CASE("An unpunished atrocity says so rather than naming a sanction", "[atrocity][notice]")
{
    AtrocityGame_ game;
    game.RepealCharter(*game.pA);

    PlayerInteractionQueue& rQueue = game.pState->GetPlayerInteractions();
    while (!rQueue.Empty())
    {
        rQueue.CompleteFront();
    }

    const AtrocityCommitted_t committed =
        CommitAtrocity(*game.pState, *game.pA, game.pB, k_Simple);
    REQUIRE_FALSE(committed.PenaltiesApplied());

    REQUIRE_FALSE(rQueue.Empty());
    const auto* pNotice = std::get_if<NoticeInteraction_t>(&rQueue.Front()->payload);
    REQUIRE(pNotice);
    CHECK(pNotice->body.find("no longer forbids") != std::string::npos);
    CHECK(pNotice->body.find("no sanctions") != std::string::npos);
}

TEST_CASE("An atrocity with a Progenitor party names that exemption", "[atrocity][notice]")
{
    AtrocityGame_ game;
    FactionConfig_t alien = game.fixtures.factionDefinition;
    alien.id = "alien";
    alien.identity.species = FactionSpecies_t::Progenitor;
    Faction& rAlien = game.AddFaction_(alien, false);

    PlayerInteractionQueue& rQueue = game.pState->GetPlayerInteractions();
    REQUIRE(rQueue.Empty());

    const AtrocityCommitted_t committed =
        CommitAtrocity(*game.pState, *game.pA, &rAlien, k_Simple);
    REQUIRE_FALSE(committed.PenaltiesApplied());

    REQUIRE_FALSE(rQueue.Empty());
    const auto* pNotice = std::get_if<NoticeInteraction_t>(&rQueue.Front()->payload);
    REQUIRE(pNotice);
    CHECK(pNotice->body.find("take no interest") != std::string::npos);
    CHECK(pNotice->body.find("no longer forbids") == std::string::npos);
}
