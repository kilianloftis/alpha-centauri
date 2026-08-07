#include "game/population/calculators/PopTypeAvailabilityCalculator.h"

#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/population/pop-types/PopTypeConfigParser.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace ac
{

PopTypeAvailabilityCalculator::PopTypeAvailabilityCalculator(const PopTypeRegistry& rRegistry)
    : m_pRegistry(&rRegistry)
{
}

std::vector<const PopTypeConfig_t*> PopTypeAvailabilityCalculator::GetAvailable(
    const std::vector<std::string>& rDiscoveredTechs) const
{
    std::vector<const PopTypeConfig_t*> available;
    for (const PopTypeConfig_t& rConfig : m_pRegistry->GetAll())
    {
        if (!rConfig.bPlayerAssignable)
        {
            continue;
        }

        if (!rConfig.requiredTech.empty())
        {
            auto it = std::find(rDiscoveredTechs.begin(), rDiscoveredTechs.end(), rConfig.requiredTech);
            if (it == rDiscoveredTechs.end())
            {
                continue;
            }
        }

        available.push_back(&rConfig);
    }

    std::unordered_set<std::string> obsoletedIds;
    for (const PopTypeConfig_t* pConfig : available)
    {
        for (const std::string& rId : pConfig->obsoletes)
        {
            obsoletedIds.insert(rId);
        }
    }

    available.erase(
        std::remove_if(available.begin(), available.end(),
            [&obsoletedIds](const PopTypeConfig_t* pConfig)
            {
                return obsoletedIds.count(pConfig->id) > 0;
            }),
        available.end());

    return available;
}

const PopTypeConfig_t& PopTypeAvailabilityCalculator::ResolveCurrentType(
    const std::string& rTypeId,
    const std::vector<std::string>& rDiscoveredTechs) const
{
    // A config where two types obsolete each other (or one obsoletes itself) would otherwise
    // spin here forever. Since every conversion path now walks this chain — including the
    // per-turn composition pass for every base — a cycle would hang the game rather than
    // report a config error, and a hang is not something the caller's try/catch can absorb.
    std::unordered_set<std::string> visited;
    visited.insert(rTypeId);

    std::string resolvedId = rTypeId;
    bool bChanged = true;
    while (bChanged)
    {
        bChanged = false;
        for (const PopTypeConfig_t& rConfig : m_pRegistry->GetAll())
        {
            if (!rConfig.requiredTech.empty())
            {
                auto it = std::find(rDiscoveredTechs.begin(), rDiscoveredTechs.end(), rConfig.requiredTech);
                if (it == rDiscoveredTechs.end())
                {
                    continue;
                }
            }
            for (const std::string& rObsoletedId : rConfig.obsoletes)
            {
                if (rObsoletedId == resolvedId)
                {
                    if (!visited.insert(rConfig.id).second)
                    {
                        throw std::runtime_error(
                            "Pop type obsolescence cycle involving '" + rConfig.id
                            + "': check the 'obsoletes' lists in the pop type config");
                    }
                    resolvedId = rConfig.id;
                    bChanged = true;
                    break;
                }
            }
            if (bChanged)
            {
                break;
            }
        }
    }

    const PopTypeConfig_t* pResolved = m_pRegistry->Find(resolvedId);
    if (!pResolved)
    {
        throw std::runtime_error("Resolved pop type not found: " + resolvedId);
    }
    return *pResolved;
}

} // namespace ac
