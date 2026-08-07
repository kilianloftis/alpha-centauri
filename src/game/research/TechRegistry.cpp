#include "game/research/TechRegistry.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ac
{

void TechRegistry::Validate_()
{
    Registry::Validate_();

    const auto& rConfigs = GetAll();
    for (const TechConfig_t& rConfig : rConfigs)
    {
        ValidatePrerequisites_(rConfig, rConfigs);
    }
    ValidateNoPrerequisiteCycles_();

    std::cout << "Loaded and validated " << rConfigs.size() << " tech configurations\n";
}

void TechRegistry::ValidatePrerequisites_(const TechConfig_t& rConfig,
                                          const std::vector<TechConfig_t>& rConfigs)
{
    if (std::find(rConfig.prerequisites.begin(), rConfig.prerequisites.end(), rConfig.id)
        != rConfig.prerequisites.end())
    {
        throw std::runtime_error("Tech '" + rConfig.id + "' cannot have itself as a prerequisite");
    }

    for (const std::string& rPrereqId : rConfig.prerequisites)
    {
        const auto it = std::find_if(rConfigs.begin(), rConfigs.end(),
            [&rPrereqId](const TechConfig_t& rOther) { return rOther.id == rPrereqId; });
        if (it == rConfigs.end())
        {
            throw std::runtime_error("Prerequisite '" + rPrereqId + "' not found for tech '"
                                     + rConfig.id + "'");
        }
    }
}

// A cyclic component is unreachable forever: ResearchManager only offers a tech once every
// prerequisite is discovered, so A->B->A can never be entered. That is a broken tech tree, and
// it has to be said at load rather than by its absence from the research menu.
void TechRegistry::ValidateNoPrerequisiteCycles_() const
{
    enum class Mark_t
    {
        Unvisited,
        InProgress,
        Done
    };
    std::unordered_map<std::string, Mark_t> marks;
    for (const TechConfig_t& rConfig : m_configs)
    {
        marks[rConfig.id] = Mark_t::Unvisited;
    }

    // Iterative DFS: a mod tree can be deep, and a stack overflow is a worse diagnostic than
    // the error being reported.
    std::vector<std::string> path;
    const auto visit = [&](const std::string& rStartId) {
        std::vector<std::pair<std::string, size_t>> stack{{rStartId, 0}};
        while (!stack.empty())
        {
            auto& [rId, nextPrereq] = stack.back();
            const TechConfig_t& rConfig = Get(rId);

            if (nextPrereq == 0)
            {
                // Only Unvisited nodes are ever pushed, and nothing marks them in between.
                marks[rId] = Mark_t::InProgress;
                path.push_back(rId);
            }

            if (nextPrereq >= rConfig.prerequisites.size())
            {
                marks[rId] = Mark_t::Done;
                path.pop_back();
                stack.pop_back();
                continue;
            }

            const std::string prereqId = rConfig.prerequisites[nextPrereq];
            ++nextPrereq;

            if (marks[prereqId] == Mark_t::InProgress)
            {
                std::string cycle;
                const auto start = std::find(path.begin(), path.end(), prereqId);
                for (auto it = start; it != path.end(); ++it)
                {
                    cycle += *it + " -> ";
                }
                throw std::runtime_error("Tech prerequisite cycle: " + cycle + prereqId);
            }
            if (marks[prereqId] == Mark_t::Unvisited)
            {
                stack.push_back({prereqId, 0});
            }
        }
    };

    for (const TechConfig_t& rConfig : m_configs)
    {
        if (marks[rConfig.id] == Mark_t::Unvisited)
        {
            visit(rConfig.id);
        }
    }
}


} // namespace ac
