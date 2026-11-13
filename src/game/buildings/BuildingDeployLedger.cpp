#include "game/buildings/BuildingDeployLedger.h"

#include <algorithm>

namespace ac
{

int BuildingDeployLedger::CountCooling(const BuildingId_t& buildingId, int missionYear) const
{
    int cooling = 0;
    for (const Deploy_t& rDeploy : m_deploys)
    {
        if (rDeploy.buildingId == buildingId && missionYear < rDeploy.readyMissionYear)
        {
            ++cooling;
        }
    }
    return cooling;
}

int BuildingDeployLedger::CountReady(const BuildingId_t& buildingId, int missionYear,
                                     int ownedCopies) const
{
    const int cooling = CountCooling(buildingId, missionYear);
    return ownedCopies > cooling ? ownedCopies - cooling : 0;
}

void BuildingDeployLedger::PruneExpired(int missionYear)
{
    std::erase_if(m_deploys, [missionYear](const Deploy_t& rDeploy) {
        return missionYear >= rDeploy.readyMissionYear;
    });
}

void BuildingDeployLedger::Deploy(BaseId_t baseId, const BuildingId_t& buildingId,
                                  int readyMissionYear)
{
    m_deploys.push_back(Deploy_t{baseId, buildingId, readyMissionYear});
}

void BuildingDeployLedger::NotifyDestroyed(BaseId_t baseId, const BuildingId_t& buildingId)
{
    // This base's records only, dropping its most-cooling copy: a destroyed copy takes a
    // suppression with it, and retiring an expired one instead would under-count ready copies.
    auto latest = m_deploys.end();
    for (auto it = m_deploys.begin(); it != m_deploys.end(); ++it)
    {
        if (it->baseId != baseId || it->buildingId != buildingId)
        {
            continue;
        }
        if (latest == m_deploys.end() || it->readyMissionYear > latest->readyMissionYear)
        {
            latest = it;
        }
    }
    if (latest != m_deploys.end())
    {
        m_deploys.erase(latest);
    }
}

void BuildingDeployLedger::MigrateTo(BaseId_t baseId, BuildingDeployLedger& rReceiver)
{
    for (auto it = m_deploys.begin(); it != m_deploys.end();)
    {
        if (it->baseId == baseId)
        {
            rReceiver.m_deploys.push_back(*it);
            it = m_deploys.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void BuildingDeployLedger::DropBase(BaseId_t baseId)
{
    std::erase_if(m_deploys, [baseId](const Deploy_t& rDeploy) {
        return rDeploy.baseId == baseId;
    });
}

} // namespace ac
