#include "game/base/RebelMutations.h"

#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/base/RebelRules.h"
#include "game/faction/Faction.h"
#include "game/population/PopulationManager.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

std::optional<FactionId_t> PickRebelFactionAndTransfer(BaseManager& rBase, GameState& rGameState,
                                                       std::mt19937& rRng)
{
    const std::vector<RebelCandidate_t> candidates = RebelCandidateWeights(rBase);
    if (candidates.empty())
    {
        std::cerr << "[Rebel] no candidate factions for base " << rBase.GetBaseId() << '\n';
        return std::nullopt;
    }

    int totalWeight = 0;
    for (const RebelCandidate_t& rCand : candidates)
    {
        totalWeight += rCand.weight;
    }

    std::uniform_int_distribution<int> dist(1, totalWeight);
    int roll = dist(rRng);
    FactionId_t chosenId = candidates.front().factionId;
    for (const RebelCandidate_t& rCand : candidates)
    {
        roll -= rCand.weight;
        if (roll <= 0)
        {
            chosenId = rCand.factionId;
            break;
        }
    }
    Faction* pChosen = rGameState.FindFaction(chosenId);
    if (!pChosen)
    {
        throw std::logic_error("PickRebelFactionAndTransfer: candidate faction "
                               + std::to_string(chosenId) + " is not in the game");
    }

    Faction& rOwner = rBase.GetFaction();
    const BaseId_t baseId = rBase.GetBaseId();
    // Same order conquest and mind control use: the assimilation window is stamped while the
    // base still belongs to the loser, then TransferBaseTo moves it (and resets escalation).
    rBase.GetPopulation().NotifyCaptured(rOwner.GetFactionId(), chosenId);
    rOwner.TransferBaseTo(baseId, *pChosen);
    return chosenId;
}

} // namespace ac
