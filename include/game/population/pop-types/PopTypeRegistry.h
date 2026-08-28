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
        ValidatePromotionGraph_();
        DeriveClasses_();
        ValidateDroneWeights_();
    }

private:
    // The promotion graph must be one chain running through the is_default type, because the
    // citizen classes are read off position relative to that type. A branch has no defined
    // answer, and would misclassify silently rather than error. Branching graphs are a deferred
    // feature; rejecting them is not deferred.
    void ValidatePromotionGraph_() const
    {
        for (const PopTypeConfig_t& rConfig : this->m_configs)
        {
            if (rConfig.promotesToId.empty())
            {
                continue;
            }
            if (!this->Find(rConfig.promotesToId))
            {
                throw std::runtime_error("Pop type '" + rConfig.id + "': promotes_to '"
                                         + rConfig.promotesToId + "' is not a known pop type");
            }
            if (rConfig.promotesToId == rConfig.id)
            {
                throw std::runtime_error("Pop type '" + rConfig.id + "': promotes_to itself");
            }
        }

        // In a chain every type is the target of at most one edge. Two types promoting to the
        // same target is the branch case.
        std::unordered_map<std::string, std::string> promotedFrom;
        for (const PopTypeConfig_t& rConfig : this->m_configs)
        {
            if (rConfig.promotesToId.empty())
            {
                continue;
            }
            const auto [it, bInserted] = promotedFrom.emplace(rConfig.promotesToId, rConfig.id);
            if (!bInserted)
            {
                throw std::runtime_error(
                    "Pop types '" + it->second + "' and '" + rConfig.id + "' both promote to '"
                    + rConfig.promotesToId + "'; the promotion graph must be a single chain");
            }
        }

        // Walking forward from any node must reach a terminal without repeating: a cycle would
        // hang the psych ladder rather than merely misclassifying.
        for (const PopTypeConfig_t& rConfig : this->m_configs)
        {
            std::vector<std::string> path{rConfig.id};
            const PopTypeConfig_t* pAt = &rConfig;
            while (!pAt->promotesToId.empty())
            {
                const std::string& rNextId = pAt->promotesToId;
                if (std::find(path.begin(), path.end(), rNextId) != path.end())
                {
                    std::string cycle;
                    for (const std::string& rStep : path)
                    {
                        cycle += rStep + " -> ";
                    }
                    throw std::runtime_error("Pop type promotion cycle: " + cycle + rNextId);
                }
                path.push_back(rNextId);
                pAt = &this->Get(rNextId);
            }
        }

        // Every type in the graph must sit on the default's chain. A second disconnected chain
        // is not a branch by the check above, but is still unclassifiable.
        const PopTypeConfig_t& rDefault = GetDefault();
        for (const PopTypeConfig_t& rConfig : this->m_configs)
        {
            const bool bInGraph = !rConfig.promotesToId.empty() || promotedFrom.count(rConfig.id) > 0;
            if (!bInGraph || rConfig.bIsDefault)
            {
                continue;
            }
            if (!PromotesInto_(rConfig, rDefault.id) && !PromotesInto_(rDefault, rConfig.id))
            {
                throw std::runtime_error(
                    "Pop type '" + rConfig.id + "' is in the promotion graph but not on the '"
                    + rDefault.id + "' chain; the graph must be a single chain through is_default");
            }
        }
    }

    // True if walking promotes_to from rFrom reaches targetId.
    bool PromotesInto_(const PopTypeConfig_t& rFrom, const std::string& rTargetId) const
    {
        const PopTypeConfig_t* pAt = &rFrom;
        while (!pAt->promotesToId.empty())
        {
            if (pAt->promotesToId == rTargetId)
            {
                return true;
            }
            pAt = &this->Get(pAt->promotesToId);
        }
        return false;
    }

    // Class is position relative to is_default: promotes into it (drone), is it (plain worker),
    // reachable from it (talent), or absent from the graph entirely (specialist).
    void DeriveClasses_()
    {
        const std::string defaultId = GetDefault().id;
        for (PopTypeConfig_t& rConfig : this->m_configs)
        {
            if (rConfig.bIsDefault)
            {
                rConfig.derivedClass = PopClass_t::PlainWorker;
            }
            else if (PromotesInto_(rConfig, defaultId))
            {
                rConfig.derivedClass = PopClass_t::Drone;
            }
            else if (PromotesInto_(this->Get(defaultId), rConfig.id))
            {
                rConfig.derivedClass = PopClass_t::Talent;
            }
            else
            {
                rConfig.derivedClass = PopClass_t::Outside;
            }
        }
    }

    // drone_weight seats pressure; class comes from the graph. The two must agree, and weight
    // order along the chain must match promote order (heavier = farther from the default), or
    // the psych ladder's "worst first" (by seated weight) would disagree with graph depth.
    // Fixtures with no promotion edges (fallback/obsoletes-only) skip the "need a drone" rule —
    // there is nothing to seat — but a stray drone_weight is still rejected.
    void ValidateDroneWeights_() const
    {
        bool bAnyPromoteEdge = false;
        for (const PopTypeConfig_t& rConfig : this->m_configs)
        {
            if (!rConfig.promotesToId.empty())
            {
                bAnyPromoteEdge = true;
                break;
            }
        }

        std::unordered_map<int, std::string> weightOwners;
        bool bAnyDrone = false;
        for (const PopTypeConfig_t& rConfig : this->m_configs)
        {
            if (rConfig.derivedClass == PopClass_t::Drone)
            {
                bAnyDrone = true;
                if (rConfig.droneWeight <= 0)
                {
                    throw std::runtime_error(
                        "Pop type '" + rConfig.id
                        + "': drone-class types must declare a positive drone_weight");
                }
                const auto [it, bInserted] =
                    weightOwners.emplace(rConfig.droneWeight, rConfig.id);
                if (!bInserted)
                {
                    throw std::runtime_error(
                        "Pop types '" + it->second + "' and '" + rConfig.id
                        + "' share drone_weight " + std::to_string(rConfig.droneWeight)
                        + "; each drone-class weight must be unique");
                }
            }
            else if (rConfig.droneWeight != 0)
            {
                throw std::runtime_error(
                    "Pop type '" + rConfig.id
                    + "': drone_weight is only valid on drone-class types (got "
                    + std::to_string(rConfig.droneWeight) + ")");
            }
        }
        if (bAnyPromoteEdge && !bAnyDrone)
        {
            throw std::runtime_error(
                "pop_types: the promotion graph has no drone-class type, so drone pressure "
                "could never be seated");
        }
        if (!bAnyDrone)
        {
            return;
        }

        for (const PopTypeConfig_t& rConfig : this->m_configs)
        {
            if (rConfig.derivedClass != PopClass_t::Drone || rConfig.promotesToId.empty())
            {
                continue;
            }
            const PopTypeConfig_t& rNext = this->Get(rConfig.promotesToId);
            if (rNext.derivedClass == PopClass_t::Drone
                && rConfig.droneWeight <= rNext.droneWeight)
            {
                throw std::runtime_error(
                    "Pop type '" + rConfig.id + "' (drone_weight "
                    + std::to_string(rConfig.droneWeight) + ") promotes to '" + rNext.id
                    + "' (drone_weight " + std::to_string(rNext.droneWeight)
                    + "); weight must strictly decrease toward is_default so ladder order "
                      "matches the graph");
            }
        }
    }

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
