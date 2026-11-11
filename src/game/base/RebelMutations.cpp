#include "game/base/RebelMutations.h"

#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/base/RebelRules.h"
#include "game/faction/Faction.h"
#include "game/population/PopulationManager.h"
#include "lib/WeightedPick.h"

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
        return std::nullopt;
    }

    // RebelCandidateWeights only returns positive weights, so the pick must succeed.
    const auto picked = PickWeightedIndex(
        candidates.size(), [&](std::size_t i) { return candidates[i].weight; }, rRng);
    if (!picked)
    {
        throw std::logic_error("PickRebelFactionAndTransfer: weighted pick failed with candidates");
    }

    const FactionId_t chosenId = candidates[*picked].factionId;
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
