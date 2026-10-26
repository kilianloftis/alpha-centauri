#pragma once

#include "game/research/TechRegistry.h"
#include "game/research/TechCostCalculator.h"
#include "lib/Revision.h"
#include "lib/Signal.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace ac
{

class IEffectsProvider;

class ResearchManager
{
public:
    // pEffectsProvider stays a pointer: the owning Faction passes `this` mid-construction.
    ResearchManager(const TechRegistry& rTechRegistry,
                    const TechCostCalculator& rTechCostCalculator,
                    const IEffectsProvider* pEffectsProvider);
    ~ResearchManager();

    void SetResearchTarget(TechId techId);
    // The config *id* — wire form. Use GetResearchTargetName for anything a player reads.
    TechId GetResearchTarget() const;
    // Player-facing name of the current target; empty when there is none.
    const std::string& GetResearchTargetName() const;
    void ClearResearchTarget();
    bool HasResearchTarget() const;

    int GetAccumulatedPoints() const;
    void AddResearchPoints(int points);
    void SetAccumulatedPoints(int points);

    // 0 without a target. The formula part is live until the first breakthrough and then fixed
    // until the next one, as terranx caches Faction.tech_cost; TechCost scales stay live.
    int GetPointsNeededForCurrentTech() const;

    // Turns remaining until the current target is discovered at researchPerTurn.
    // Empty when there is no target or researchPerTurn <= 0.
    std::optional<int> GetTurnsUntilBreakthrough(int researchPerTurn) const;

    bool CanDiscoverTech() const;
    bool DiscoverTech();

    const std::vector<TechId>& GetDiscoveredTechs() const;
    bool HasDiscoveredTech(TechId techId) const;
    void AddDiscoveredTech(TechId techId);

    // Emitted after a tech joins the discovered set. Bridged to the mod-facing EvTechDiscovered.
    Signal<const TechId&> OnTechDiscovered;

    // Bumped when a tech is discovered; FactionEffectsPool includes this in its stamp so
    // removed_by_tech effects drop out of the pool on the next Get.
    uint64_t GetRevision() const { return m_revision.Get(); }

    const TechRegistry& GetTechRegistry() const { return m_rTechRegistry; }

private:
    const TechRegistry& m_rTechRegistry;
    const TechCostCalculator& m_rTechCostCalculator;
    const IEffectsProvider* m_pEffectsProvider;

    std::vector<TechId> m_discoveredTechs;
    const TechConfig_t* m_pCurrentResearchTarget;
    int m_accumulatedPoints;
    // ResearchFormulaCost at the last breakthrough; empty before the first.
    std::optional<int> m_breakthroughFormulaCost;
    Revision m_revision;

    int FormulaCost_() const;
};

} // namespace ac
