#include "game/population/PopTypeAvailabilityCalculator.h"

#include "game/population/PopTypeRegistry.h"
#include "game/population/PopTypeConfigParser.h"
#include "game/research/TechRequirement.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace ac
{

PopTypeAvailabilityCalculator::PopTypeAvailabilityCalculator(const PopTypeRegistry& rRegistry)
    : m_rRegistry(rRegistry)
{
}

std::string PopTypeAvailabilityCalculator::ResolveSuccessorId_(
    const std::string& rTypeId,
    const std::vector<std::string>& rDiscovered) const
{
    // Breadth-first over "who obsoletes this", so ids are reached in distance order and the
    // deepest available one wins.
    std::string resolvedId = rTypeId;
    std::unordered_set<std::string> visited{rTypeId};
    std::vector<std::string> frontier{rTypeId};
    while (!frontier.empty())
    {
        SuccessorLevel_t level = NextLevel_(frontier, rDiscovered, visited);
        if (level.bestAvailableId)
        {
            resolvedId = *level.bestAvailableId;
        }
        frontier = std::move(level.ids);
    }
    return resolvedId;
}

// Within a level the earliest in registry order wins.
PopTypeAvailabilityCalculator::SuccessorLevel_t PopTypeAvailabilityCalculator::NextLevel_(
    const std::vector<std::string>& rFrontier,
    const std::vector<std::string>& rDiscovered,
    std::unordered_set<std::string>& rVisited) const
{
    SuccessorLevel_t level;
    size_t bestIndex = 0;
    for (const std::string& rCurrentId : rFrontier)
    {
        size_t configIndex = 0;
        for (const PopTypeConfig_t& rConfig : m_rRegistry.GetAll())
        {
            const size_t index = configIndex++;
            // Visiting each id at most once bounds the walk whatever the graph shape;
            // PopTypeRegistry rejects actual cycles at load.
            if (!std::ranges::contains(rConfig.obsoletes, rCurrentId)
                || !rVisited.insert(rConfig.id).second)
            {
                continue;
            }
            level.ids.push_back(rConfig.id);
            // Intermediate steps are walked whether or not their own tech is discovered;
            // only the answer has to be something the player actually has.
            if (MeetsTechRequirement(rConfig.requiredTech, rDiscovered) && (!level.bestAvailableId || index < bestIndex))
            {
                level.bestAvailableId = rConfig.id;
                bestIndex = index;
            }
        }
    }
    return level;
}

std::vector<const PopTypeConfig_t*> PopTypeAvailabilityCalculator::GetAvailable(
    const std::vector<std::string>& rDiscoveredTechs) const
{

    std::vector<const PopTypeConfig_t*> available;
    for (const PopTypeConfig_t& rConfig : m_rRegistry.GetAll())
    {
        if (!rConfig.bPlayerAssignable || !MeetsTechRequirement(rConfig.requiredTech, rDiscoveredTechs))
        {
            continue;
        }
        // Superseded by something the player has: the same question ResolveCurrentType answers.
        if (ResolveSuccessorId_(rConfig.id, rDiscoveredTechs) != rConfig.id)
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
    const std::string resolvedId = ResolveSuccessorId_(rTypeId, rDiscoveredTechs);

    const PopTypeConfig_t* pResolved = m_rRegistry.Find(resolvedId);
    if (!pResolved)
    {
        throw std::runtime_error("Resolved pop type not found: " + resolvedId);
    }
    return *pResolved;
}

} // namespace ac
