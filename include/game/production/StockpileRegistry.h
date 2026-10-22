#pragma once

#include "game/production/StockpileConfigParser.h"
#include "lib/Registry.h"

#include <string>
#include <vector>

namespace ac
{

class StockpileRegistry : public Registry<StockpileConfig_t, StockpileConfigParser>
{
public:
    // Highest fallbackPriority among researched stockpiles; nullptr leaves the queue empty.
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
