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

namespace
{

// Iterative DFS: a mod tree can be deep, and a stack overflow is a worse diagnostic than
// the error being reported.
class PrerequisiteCycleFinder_
{
public:
    explicit PrerequisiteCycleFinder_(const TechRegistry& rRegistry)
        : m_rRegistry(rRegistry)
    {
    }

    // Throws on the first prerequisite cycle reachable from rStartId.
    void VisitFrom(const std::string& rStartId)
    {
        if (MarkOf_(rStartId) != Mark_t::Unvisited)
        {
            return;
        }
        std::vector<std::pair<std::string, size_t>> stack{{rStartId, 0}};
        while (!stack.empty())
        {
            auto& [rId, nextPrereq] = stack.back();
            const TechConfig_t& rConfig = m_rRegistry.Get(rId);
            if (nextPrereq == 0)
            {
                // Only Unvisited nodes are ever pushed, and nothing marks them in between.
                m_marks[rId] = Mark_t::InProgress;
                m_path.push_back(rId);
            }
            if (nextPrereq >= rConfig.prerequisites.size())
            {
                m_marks[rId] = Mark_t::Done;
                m_path.pop_back();
                stack.pop_back();
                continue;
            }
            const std::string prereqId = rConfig.prerequisites[nextPrereq];
            ++nextPrereq;
            if (MarkOf_(prereqId) == Mark_t::InProgress)
            {
                ThrowCycle_(prereqId);
            }
            if (MarkOf_(prereqId) == Mark_t::Unvisited)
            {
                stack.push_back({prereqId, 0});
            }
        }
    }

private:
    enum class Mark_t
    {
        Unvisited,
        InProgress,
        Done
    };

    Mark_t MarkOf_(const std::string& rId) const
    {
        const auto it = m_marks.find(rId);
        return it == m_marks.end() ? Mark_t::Unvisited : it->second;
    }

    [[noreturn]] void ThrowCycle_(const std::string& rPrereqId) const
    {
        std::string cycle;
        const auto start = std::find(m_path.begin(), m_path.end(), rPrereqId);
        for (auto it = start; it != m_path.end(); ++it)
        {
            cycle += *it + " -> ";
        }
        throw std::runtime_error("Tech prerequisite cycle: " + cycle + rPrereqId);
    }

    const TechRegistry& m_rRegistry;
    std::unordered_map<std::string, Mark_t> m_marks;
    std::vector<std::string> m_path;
};

} // namespace

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

// A cycle can never be researched, so it is a broken tree, reported at load.
void TechRegistry::ValidateNoPrerequisiteCycles_() const
{
    PrerequisiteCycleFinder_ finder(*this);
    for (const TechConfig_t& rConfig : GetAll())
    {
        finder.VisitFrom(rConfig.id);
    }
}

} // namespace ac
