#include "game/faction/DiplomacyManager.h"

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

using namespace ac;

TEST_CASE("Diplomatic status defaults to None", "[diplomacy]")
{
    DiplomacyManager diplomacy;
    CHECK(diplomacy.GetStatus(1, 2) == DiplomaticStatus::None);
    CHECK_FALSE(diplomacy.HasTruce(1, 2));
    CHECK_FALSE(diplomacy.HasFriendship(1, 2));
    CHECK_FALSE(diplomacy.HasPact(1, 2));
    CHECK_FALSE(diplomacy.HasVendetta(1, 2));
}

TEST_CASE("Diplomatic status is symmetric", "[diplomacy]")
{
    DiplomacyManager diplomacy;
    diplomacy.SetStatus(1, 2, DiplomaticStatus::Friendship);
    CHECK(diplomacy.GetStatus(1, 2) == DiplomaticStatus::Friendship);
    CHECK(diplomacy.GetStatus(2, 1) == DiplomaticStatus::Friendship);
    CHECK(diplomacy.HasFriendship(2, 1));
}

TEST_CASE("Each diplomatic status round-trips", "[diplomacy]")
{
    DiplomacyManager diplomacy;

    diplomacy.SetStatus(1, 2, DiplomaticStatus::Truce);
    CHECK(diplomacy.GetStatus(1, 2) == DiplomaticStatus::Truce);
    CHECK(diplomacy.HasTruce(1, 2));

    diplomacy.SetStatus(1, 2, DiplomaticStatus::Friendship);
    CHECK(diplomacy.GetStatus(1, 2) == DiplomaticStatus::Friendship);
    CHECK(diplomacy.HasFriendship(1, 2));
    CHECK_FALSE(diplomacy.HasTruce(1, 2));

    diplomacy.SetStatus(1, 2, DiplomaticStatus::Pact);
    CHECK(diplomacy.GetStatus(1, 2) == DiplomaticStatus::Pact);
    CHECK(diplomacy.HasPact(1, 2));

    diplomacy.SetStatus(1, 2, DiplomaticStatus::Vendetta);
    CHECK(diplomacy.GetStatus(1, 2) == DiplomaticStatus::Vendetta);
    CHECK(diplomacy.HasVendetta(1, 2));
}

TEST_CASE("Setting None clears a stored status", "[diplomacy]")
{
    DiplomacyManager diplomacy;
    diplomacy.SetStatus(3, 5, DiplomaticStatus::Pact);
    diplomacy.SetStatus(3, 5, DiplomaticStatus::None);
    CHECK(diplomacy.GetStatus(3, 5) == DiplomaticStatus::None);
    CHECK_FALSE(diplomacy.HasPact(3, 5));
}

TEST_CASE("Self-pair status is rejected", "[diplomacy]")
{
    DiplomacyManager diplomacy;
    CHECK_THROWS_AS(diplomacy.GetStatus(1, 1), std::invalid_argument);
    CHECK_THROWS_AS(diplomacy.SetStatus(1, 1, DiplomaticStatus::Truce), std::invalid_argument);
}
