#include "DiplomacyFixture.h"

#include "game/base/RebelRules.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace ac;

TEST_CASE("RebelCandidateWeights skips the owner and factions without bases", "[population][rebel]")
{
    actest::DiplomacyFixture game;

    const std::vector<RebelCandidate_t> candidates = RebelCandidateWeights(*game.pBaseA);
    REQUIRE(candidates.size() == 1);
    CHECK(candidates[0].factionId == game.pB->GetFactionId());
    // The fixture's distance mode is none, so the weight is the base join weight alone.
    CHECK(candidates[0].weight == 1);
}
