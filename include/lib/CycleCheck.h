#pragma once

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ac
{

// Depth-first walk over string ids, where edgesOf(id) returns the ids that id points to.
// Iterative, since a mod's graph can be deep and a stack overflow is a worse diagnostic than the
// error being reported.
template <typename EdgesFn>
class AcyclicCheck
{
public:
    AcyclicCheck(EdgesFn edgesOf, std::string label)
        : m_edgesOf(std::move(edgesOf))
        , m_label(std::move(label))
    {
    }

    // Throws "<label>: a -> b -> a" on the first cycle reachable from rStartId.
    void VisitFrom(const std::string& rStartId)
    {
        if (MarkOf_(rStartId) != Mark_t::Unvisited)
        {
            return;
        }
        std::vector<std::pair<std::string, size_t>> stack{{rStartId, 0}};
        while (!stack.empty())
        {
            auto& [rId, nextEdge] = stack.back();
            const std::vector<std::string>& rEdges = m_edgesOf(rId);
            if (nextEdge == 0)
            {
                // Only Unvisited nodes are ever pushed, and nothing marks them in between.
                m_marks[rId] = Mark_t::InProgress;
                m_path.push_back(rId);
            }
            if (nextEdge >= rEdges.size())
            {
                m_marks[rId] = Mark_t::Done;
                m_path.pop_back();
                stack.pop_back();
                continue;
            }
            const std::string nextId = rEdges[nextEdge];
            ++nextEdge;
            if (MarkOf_(nextId) == Mark_t::InProgress)
            {
                ThrowCycle_(nextId);
            }
            if (MarkOf_(nextId) == Mark_t::Unvisited)
            {
                stack.push_back({nextId, 0});
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

    [[noreturn]] void ThrowCycle_(const std::string& rClosingId) const
    {
        std::string cycle;
        const auto start = std::find(m_path.begin(), m_path.end(), rClosingId);
        for (auto it = start; it != m_path.end(); ++it)
        {
            cycle += *it + " -> ";
        }
        throw std::runtime_error(m_label + ": " + cycle + rClosingId);
    }

    EdgesFn m_edgesOf;
    std::string m_label;
    std::unordered_map<std::string, Mark_t> m_marks;
    std::vector<std::string> m_path;
};

// Throws on the first cycle reachable from any config's id, following edgesOf.
template <typename Config_t, typename EdgesFn>
void RequireAcyclic(const std::vector<Config_t>& rConfigs, EdgesFn edgesOf,
                    const std::string& rLabel)
{
    AcyclicCheck<EdgesFn> check(std::move(edgesOf), rLabel);
    for (const Config_t& rConfig : rConfigs)
    {
        check.VisitFrom(rConfig.id);
    }
}

} // namespace ac
