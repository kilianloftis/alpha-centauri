#include "game/faction/DiplomacyLedger.h"

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

using namespace ac;

TEST_CASE("Diplomatic status defaults to None", "[diplomacy]")
{
    DiplomacyLedger ledger;
    CHECK(ledger.GetStatus(1, 2) == DiplomaticStatus::None);
    CHECK_FALSE(ledger.HasTruce(1, 2));
    CHECK_FALSE(ledger.HasFriendship(1, 2));
    CHECK_FALSE(ledger.HasPact(1, 2));
    CHECK_FALSE(ledger.HasVendetta(1, 2));
}

TEST_CASE("Diplomatic status is symmetric", "[diplomacy]")
{
    DiplomacyLedger ledger;
    ledger.SetStatus(1, 2, DiplomaticStatus::Friendship);
    CHECK(ledger.GetStatus(1, 2) == DiplomaticStatus::Friendship);
    CHECK(ledger.GetStatus(2, 1) == DiplomaticStatus::Friendship);
    CHECK(ledger.HasFriendship(2, 1));
}

TEST_CASE("Each diplomatic status round-trips", "[diplomacy]")
{
    DiplomacyLedger ledger;

    ledger.SetStatus(1, 2, DiplomaticStatus::Truce);
    CHECK(ledger.GetStatus(1, 2) == DiplomaticStatus::Truce);
    CHECK(ledger.HasTruce(1, 2));

    ledger.SetStatus(1, 2, DiplomaticStatus::Friendship);
    CHECK(ledger.GetStatus(1, 2) == DiplomaticStatus::Friendship);
    CHECK(ledger.HasFriendship(1, 2));
    CHECK_FALSE(ledger.HasTruce(1, 2));

    ledger.SetStatus(1, 2, DiplomaticStatus::Pact);
    CHECK(ledger.GetStatus(1, 2) == DiplomaticStatus::Pact);
    CHECK(ledger.HasPact(1, 2));

    ledger.SetStatus(1, 2, DiplomaticStatus::Vendetta);
    CHECK(ledger.GetStatus(1, 2) == DiplomaticStatus::Vendetta);
    CHECK(ledger.HasVendetta(1, 2));
}

TEST_CASE("Setting None clears a stored status", "[diplomacy]")
{
    DiplomacyLedger ledger;
    ledger.SetStatus(3, 5, DiplomaticStatus::Pact);
    ledger.SetStatus(3, 5, DiplomaticStatus::None);
    CHECK(ledger.GetStatus(3, 5) == DiplomaticStatus::None);
    CHECK_FALSE(ledger.HasPact(3, 5));
}

TEST_CASE("Self-pair status is rejected", "[diplomacy]")
{
    DiplomacyLedger ledger;
    CHECK_THROWS_AS(ledger.GetStatus(1, 1), std::invalid_argument);
    CHECK_THROWS_AS(ledger.SetStatus(1, 1, DiplomaticStatus::Truce), std::invalid_argument);
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
