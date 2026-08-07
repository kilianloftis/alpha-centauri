#pragma once

#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "lib/Registry.h"
#include "lib/config/EnumNames.h"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ac
{

class PopTypeRegistry : public Registry<PopTypeConfig_t, PopTypeConfigParser, Pop>
{
public:
    const PopTypeConfig_t& GetDefault() const
    {
        for (const PopTypeConfig_t& rConfig : this->m_configs)
        {
            if (rConfig.bIsDefault)
            {
                return rConfig;
            }
        }
        throw std::runtime_error("No pop type marked as is_default in pop_types config");
    }

protected:
    void Validate_() override
    {
        Registry::Validate_();

        int defaultCount = 0;
        for (const PopTypeConfig_t& rConfig : this->m_configs)
        {
            if (rConfig.bIsDefault)
            {
                ++defaultCount;
            }
        }
        if (defaultCount != 1)
        {
            throw std::runtime_error(
                "pop_types config must have exactly one is_default entry, found " +
                std::to_string(defaultCount));
        }

        // A typo'd fallback surfaces only when a pop actually converts, and a typo'd obsoletes
        // entry never surfaces at all - it is silently inert in ResolveCurrentType.
        for (const PopTypeConfig_t& rConfig : this->m_configs)
        {
            const bool bRoleWorksTile = rConfig.role != PopRole_t::Specialist;
            if (bRoleWorksTile != rConfig.bCanWorkTile)
            {
                throw std::runtime_error(
                    "Pop type '" + rConfig.id + "': role '" + EnumToLowerName(rConfig.role)
                    + "' and can_work_tile " + (rConfig.bCanWorkTile ? "true" : "false")
                    + " contradict each other");
            }

            if (!rConfig.fallbackPopTypeId.empty() && !this->Find(rConfig.fallbackPopTypeId))
            {
                throw std::runtime_error("Pop type '" + rConfig.id + "': fallback_pop_type '"
                                         + rConfig.fallbackPopTypeId + "' is not a known pop type");
            }
            for (const std::string& rObsoleteId : rConfig.obsoletes)
            {
                if (!this->Find(rObsoleteId))
                {
                    throw std::runtime_error("Pop type '" + rConfig.id + "': obsoletes entry '"
                                             + rObsoleteId + "' is not a known pop type");
                }
            }
        }

        ValidateNoObsolescenceCycles_();
    }

private:
    // PopTypeAvailabilityCalculator walks this graph for every pop of every base, every turn.
    // A cycle there is a config error, and it has to be reported here rather than from the
    // middle of a turn.
    void ValidateNoObsolescenceCycles_() const
    {
        enum class Mark_t
        {
            Unvisited,
            InProgress,
            Done
        };
        std::unordered_map<std::string, Mark_t> marks;
        for (const PopTypeConfig_t& rConfig : this->m_configs)
        {
            marks[rConfig.id] = Mark_t::Unvisited;
        }

        std::vector<std::string> path;
        const auto visit = [&](const std::string& rStartId) {
            std::vector<std::pair<std::string, size_t>> stack{{rStartId, 0}};
            while (!stack.empty())
            {
                auto& [rId, nextEdge] = stack.back();
                const PopTypeConfig_t& rConfig = this->Get(rId);

                if (nextEdge == 0)
                {
                    marks[rId] = Mark_t::InProgress;
                    path.push_back(rId);
                }
                if (nextEdge >= rConfig.obsoletes.size())
                {
                    marks[rId] = Mark_t::Done;
                    path.pop_back();
                    stack.pop_back();
                    continue;
                }

                const std::string obsoletedId = rConfig.obsoletes[nextEdge];
                ++nextEdge;

                if (marks[obsoletedId] == Mark_t::InProgress)
                {
                    std::string cycle;
                    const auto start = std::find(path.begin(), path.end(), obsoletedId);
                    for (auto it = start; it != path.end(); ++it)
                    {
                        cycle += *it + " -> ";
                    }
                    throw std::runtime_error("Pop type obsolescence cycle: " + cycle
                                             + obsoletedId);
                }
                if (marks[obsoletedId] == Mark_t::Unvisited)
                {
                    stack.push_back({obsoletedId, 0});
                }
            }
        };

        for (const PopTypeConfig_t& rConfig : this->m_configs)
        {
            if (marks[rConfig.id] == Mark_t::Unvisited)
            {
                visit(rConfig.id);
            }
        }
    }
};

} // namespace ac
