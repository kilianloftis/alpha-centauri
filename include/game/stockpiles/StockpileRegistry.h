#pragma once

#include "game/stockpiles/StockpileConfigParser.h"
#include "lib/Registry.h"

#include <string>
#include <vector>

namespace ac
{

class StockpileRegistry : public Registry<StockpileConfig_t, StockpileConfigParser>
{
public:
    // The empty-queue fallback: highest fallbackPriority among the stockpiles whose
    // required_tech is discovered, ties broken by load order. nullptr when none qualifies,
    // which leaves the base with an empty queue and wastes its surplus minerals.
    const StockpileConfig_t* FindFallback(const std::vector<std::string>& rDiscoveredTechs) const
    {
        const StockpileConfig_t* pBest = nullptr;
        for (const StockpileConfig_t& rConfig : GetAll())
        {
            if (!rConfig.IsAvailable(rDiscoveredTechs))
            {
                continue;
            }
            if (!pBest || rConfig.fallbackPriority > pBest->fallbackPriority)
            {
                pBest = &rConfig;
            }
        }
        return pBest;
    }
};

} // namespace ac
