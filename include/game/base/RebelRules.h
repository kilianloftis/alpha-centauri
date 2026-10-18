#pragma once

#include "game/base/BaseTypes.h"

#include <vector>

namespace ac
{

class BaseManager;

struct RebelCandidate_t
{
    FactionId_t factionId = -1;
    int weight = 0;
};

// Every other faction that still holds a base, weighted by its RebelJoinWeight plus the distance
// bonus for the configured mode. A faction with no positive weight is left out.
std::vector<RebelCandidate_t> RebelCandidateWeights(const BaseManager& rRebelling);

} // namespace ac
