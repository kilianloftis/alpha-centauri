#include "game/population/calculators/PopTypeAvailabilityCalculator.h"

#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/population/pop-types/PopTypeConfigParser.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace ac
{

namespace
{

bool HasTech_(const PopTypeConfig_t& rConfig, const std::unordered_set<std::string>& rDiscovered)
{
    return rConfig.requiredTech.empty() || rDiscovered.count(rConfig.requiredTech) > 0;
}

} // namespace

PopTypeAvailabilityCalculator::PopTypeAvailabilityCalculator(const PopTypeRegistry& rRegistry)
    : m_rRegistry(rRegistry)
{
}

std::string PopTypeAvailabilityCalculator::ResolveSuccessorId_(
    const std::string& rTypeId,
    const std::unordered_set<std::string>& rDiscovered) const
{
    // Breadth-first over "who obsoletes this", so ids are reached in distance order and the
    // deepest available one wins. Within a level the earliest in registry order wins.
    std::string resolvedId = rTypeId;
    std::unordered_set<std::string> visited{rTypeId};
    std::vector<std::string> frontier{rTypeId};

    while (!frontier.empty())
    {
        std::vector<std::string> next;
        bool bLevelHasAvailable = false;
        std::string levelBestId;
        size_t levelBestIndex = 0;

        for (const std::string& rCurrentId : frontier)
        {
            size_t configIndex = 0;
            for (const PopTypeConfig_t& rConfig : m_rRegistry.GetAll())
            {
                const size_t index = configIndex++;
                if (std::find(rConfig.obsoletes.begin(), rConfig.obsoletes.end(), rCurrentId)
                    == rConfig.obsoletes.end())
                {
                    continue;
                }
                // Visiting each id at most once bounds the walk whatever the graph shape;
                // PopTypeRegistry rejects actual cycles at load.
                if (!visited.insert(rConfig.id).second)
                {
                    continue;
                }
                next.push_back(rConfig.id);

                // Intermediate steps are walked whether or not their own tech is discovered;
                // only the answer has to be something the player actually has.
                if (!HasTech_(rConfig, rDiscovered))
                {
                    continue;
                }
                if (!bLevelHasAvailable || index < levelBestIndex)
                {
                    bLevelHasAvailable = true;
                    levelBestId = rConfig.id;
                    levelBestIndex = index;
                }
            }
        }

        if (bLevelHasAvailable)
        {
            resolvedId = levelBestId;
        }
        frontier = std::move(next);
    }

    return resolvedId;
}

std::vector<const PopTypeConfig_t*> PopTypeAvailabilityCalculator::GetAvailable(
    const std::vector<std::string>& rDiscoveredTechs) const
{
    const std::unordered_set<std::string> discovered(rDiscoveredTechs.begin(),
                                                     rDiscoveredTechs.end());

    std::vector<const PopTypeConfig_t*> available;
    for (const PopTypeConfig_t& rConfig : m_rRegistry.GetAll())
    {
        if (!rConfig.bPlayerAssignable || !HasTech_(rConfig, discovered))
        {
            continue;
        }
        // Superseded by something the player has: the same question ResolveCurrentType answers.
        if (ResolveSuccessorId_(rConfig.id, discovered) != rConfig.id)
        {
            continue;
        }
        available.push_back(&rConfig);
    }

    return available;
}

const PopTypeConfig_t& PopTypeAvailabilityCalculator::ResolveCurrentType(
    const std::string& rTypeId,
    const std::vector<std::string>& rDiscoveredTechs) const
{
    const std::unordered_set<std::string> discovered(rDiscoveredTechs.begin(),
                                                     rDiscoveredTechs.end());
    const std::string resolvedId = ResolveSuccessorId_(rTypeId, discovered);

    const PopTypeConfig_t* pResolved = m_rRegistry.Find(resolvedId);
    if (!pResolved)
    {
        throw std::runtime_error("Resolved pop type not found: " + resolvedId);
    }
    return *pResolved;
}

} // namespace ac
