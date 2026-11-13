#include "game/combat/TileInterceptDeployLedger.h"

#include <algorithm>

namespace ac
{

bool TileInterceptDeployLedger::IsReady(TileCoord_t tile, const std::string& sourceId,
                                        int missionYear) const
{
    for (const Deploy_t& rDeploy : m_deploys)
    {
        if (rDeploy.tile == tile && rDeploy.sourceId == sourceId
            && missionYear < rDeploy.readyMissionYear)
        {
            return false;
        }
    }
    return true;
}

void TileInterceptDeployLedger::Deploy(TileCoord_t tile, const std::string& sourceId,
                                       int readyMissionYear)
{
    m_deploys.push_back(Deploy_t{tile, sourceId, readyMissionYear});
}

void TileInterceptDeployLedger::PruneExpired(int missionYear)
{
    std::erase_if(m_deploys, [missionYear](const Deploy_t& rDeploy) {
        return missionYear >= rDeploy.readyMissionYear;
    });
}

void TileInterceptDeployLedger::Drop(TileCoord_t tile, const std::string& sourceId)
{
    std::erase_if(m_deploys, [tile, &sourceId](const Deploy_t& rDeploy) {
        return rDeploy.tile == tile && rDeploy.sourceId == sourceId;
    });
}

} // namespace ac
