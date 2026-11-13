#pragma once

#include "game/base/BaseTypes.h"

#include <string>
#include <vector>

namespace ac
{

// Per-tile ThisTile Intercept deploy cooldowns, keyed by (tile, sourceId).
class TileInterceptDeployLedger
{
public:
    // True when no cooling record blocks the charge at missionYear.
    bool IsReady(TileCoord_t tile, const std::string& sourceId, int missionYear) const;
    // Marks the charge deployed until readyMissionYear.
    void Deploy(TileCoord_t tile, const std::string& sourceId, int readyMissionYear);
    // Retires records ready at missionYear so the ledger cannot grow without bound.
    void PruneExpired(int missionYear);
    // Drops every record for that occupant on the tile (occupant removed).
    void Drop(TileCoord_t tile, const std::string& sourceId);

private:
    struct Deploy_t
    {
        TileCoord_t tile{0, 0};
        std::string sourceId;
        int readyMissionYear = 0;
    };

    std::vector<Deploy_t> m_deploys;
};

} // namespace ac
