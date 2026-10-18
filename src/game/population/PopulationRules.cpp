#include "game/population/PopulationRules.h"

#include "game/base/BaseManager.h"
#include "game/base/BaseTypes.h"
#include "game/map/Tile.h"
#include "game/population/Pop.h"
#include "game/population/PopulationManager.h"

#include <stdexcept>
#include <utility>

namespace ac
{

namespace
{

int PopValue_(const BaseManager& rBase, const Pop& rPop)
{
    if (!rPop.IsWorker())
    {
        const SpecialistOutput_t output = rPop.GetSpecialistOutput();
        return output.econ + output.labs + output.psych;
    }
    const Tile* pTile = rPop.GetTile();
    if (!pTile)
    {
        // An unassigned worker produces nothing, so it is the first to go.
        return 0;
    }
    const TileResources_t yield = rBase.GetWorkedTileYield(*pTile).effective;
    return yield.nutrients + yield.minerals + yield.energy;
}

} // namespace

const Pop& SelectPopToRemove(const BaseManager& rBase)
{
    const Pop* pDoomed = nullptr;
    std::pair<bool, int> worst;
    for (const Pop& rPop : rBase.GetPopulation().Pops())
    {
        const std::pair<bool, int> key{rPop.IsPlayerChoiceType(), PopValue_(rBase, rPop)};
        if (!pDoomed || key < worst)
        {
            pDoomed = &rPop;
            worst = key;
        }
    }
    if (!pDoomed)
    {
        throw std::runtime_error("SelectPopToRemove: base has no population");
    }
    return *pDoomed;
}

} // namespace ac
