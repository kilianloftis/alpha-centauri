#pragma once

#include "game/base/BaseTypes.h"
#include "game/buildings/BuildingConfig.h"

#include <vector>

namespace ac
{

// ASAT / Intercept cooldowns for owned building copies, keyed by (base, building).
class BuildingDeployLedger
{
public:
    // Copies still cooling at missionYear for buildingId.
    int CountCooling(const BuildingId_t& buildingId, int missionYear) const;
    // ownedCopies minus CountCooling, floored at 0.
    int CountReady(const BuildingId_t& buildingId, int missionYear, int ownedCopies) const;
    // Retires records ready at missionYear so the ledger cannot grow without bound.
    void PruneExpired(int missionYear);
    // Marks one copy at baseId deployed until readyMissionYear.
    void Deploy(BaseId_t baseId, const BuildingId_t& buildingId, int readyMissionYear);
    // Drops this base's most-cooling record for buildingId; a no-op if none are cooling.
    void NotifyDestroyed(BaseId_t baseId, const BuildingId_t& buildingId);
    // Moves baseId's records onto rReceiver so cooldowns follow the base.
    void MigrateTo(BaseId_t baseId, BuildingDeployLedger& rReceiver);
    // Drops every record for baseId (base destroyed).
    void DropBase(BaseId_t baseId);

private:
    struct Deploy_t
    {
        BaseId_t baseId = 0;
        BuildingId_t buildingId;
        int readyMissionYear = 0;
    };

    std::vector<Deploy_t> m_deploys;
};

} // namespace ac
