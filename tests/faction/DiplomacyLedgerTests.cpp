#include "game/faction/DiplomacyLedger.h"

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

using namespace ac;

TEST_CASE("Diplomatic status defaults to None", "[diplomacy]")
{
    DiplomacyLedger ledger;
    CHECK(ledger.GetStatus(1, 2) == DiplomaticStatus_t::Neutral);
    CHECK_FALSE(ledger.HasVendetta(1, 2));
}

TEST_CASE("Diplomatic status is symmetric", "[diplomacy]")
{
    DiplomacyLedger ledger;
    ledger.SetStatus(1, 2, DiplomaticStatus_t::Treaty);
    CHECK(ledger.GetStatus(1, 2) == DiplomaticStatus_t::Treaty);
    CHECK(ledger.GetStatus(2, 1) == DiplomaticStatus_t::Treaty);
}

TEST_CASE("Each diplomatic status round-trips", "[diplomacy]")
{
    DiplomacyLedger ledger;

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Truce);
    CHECK(ledger.GetStatus(1, 2) == DiplomaticStatus_t::Truce);

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Treaty);
    CHECK(ledger.GetStatus(1, 2) == DiplomaticStatus_t::Treaty);

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Pact);
    CHECK(ledger.GetStatus(1, 2) == DiplomaticStatus_t::Pact);

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Vendetta);
    CHECK(ledger.GetStatus(1, 2) == DiplomaticStatus_t::Vendetta);
    CHECK(ledger.HasVendetta(1, 2));
}

TEST_CASE("Setting None clears a stored status", "[diplomacy]")
{
    DiplomacyLedger ledger;
    ledger.SetStatus(3, 5, DiplomaticStatus_t::Pact);
    ledger.SetStatus(3, 5, DiplomaticStatus_t::Neutral);
    CHECK(ledger.GetStatus(3, 5) == DiplomaticStatus_t::Neutral);
}

TEST_CASE("Self-pair status is rejected", "[diplomacy]")
{
    DiplomacyLedger ledger;
    CHECK_THROWS_AS(ledger.GetStatus(1, 1), std::invalid_argument);
    CHECK_THROWS_AS(ledger.SetStatus(1, 1, DiplomaticStatus_t::Truce), std::invalid_argument);
}

TEST_CASE("Known contact is symmetric", "[diplomacy]")
{
    DiplomacyLedger ledger;
    CHECK_FALSE(ledger.AreKnown(1, 2));
    ledger.SetKnown(1, 2);
    CHECK(ledger.AreKnown(1, 2));
    CHECK(ledger.AreKnown(2, 1));
    ledger.SetKnown(2, 1, false);
    CHECK_FALSE(ledger.AreKnown(1, 2));
}

TEST_CASE("SetKnown among many factions links every pair", "[diplomacy]")
{
    DiplomacyLedger ledger;
    ledger.SetKnown({1, 2, 3});
    CHECK(ledger.AreKnown(1, 2));
    CHECK(ledger.AreKnown(1, 3));
    CHECK(ledger.AreKnown(2, 3));
    CHECK_FALSE(ledger.AreKnown(1, 4));
}

TEST_CASE("Grievance and infiltration are directed", "[diplomacy]")
{
    DiplomacyLedger ledger;
    ledger.AddGrievance(1, 2, 3);
    CHECK(ledger.GetGrievance(1, 2) == 3);
    CHECK(ledger.GetGrievance(2, 1) == 0);

    ledger.SetInfiltration(1, 2);
    CHECK(ledger.HasInfiltration(1, 2));
    CHECK_FALSE(ledger.HasInfiltration(2, 1));
}

TEST_CASE("Integrity is per-faction", "[diplomacy]")
{
    DiplomacyLedger ledger;
    CHECK(ledger.GetIntegrity(1) == 0);
    ledger.AddIntegrity(1, 5);
    CHECK(ledger.GetIntegrity(1) == 5);
    CHECK(ledger.GetIntegrity(2) == 0);
}

TEST_CASE("Turns held count up per turn and restart only on a new status", "[diplomacy][ledger]")
{
    DiplomacyLedger ledger;
    ledger.SetStatus(1, 2, DiplomaticStatus_t::Truce);
    ledger.AgeStatuses();
    ledger.AgeStatuses();
    CHECK(ledger.GetTurnsHeld(2, 1) == 2);

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Truce);
    CHECK(ledger.GetTurnsHeld(1, 2) == 2);

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Treaty);
    CHECK(ledger.GetTurnsHeld(1, 2) == 0);

    ledger.SetStatus(1, 2, DiplomaticStatus_t::Neutral);
    ledger.AgeStatuses();
    CHECK(ledger.GetTurnsHeld(1, 2) == 0);
    CHECK(ledger.GetStatusPairs().empty());
}
