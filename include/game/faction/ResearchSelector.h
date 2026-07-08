#pragma once

#include "game/GameCategory.h"
#include "game/research/TechConfigParser.h"

#include <array>
#include <cstdint>
#include <random>
#include <vector>

namespace ac
{

class ResearchManager;

class ResearchSelector
{
public:
    explicit ResearchSelector(ResearchManager* pResearchManager);
    ResearchSelector(ResearchManager* pResearchManager, uint32_t seed);

    void SetCategoryEnabled(GameCategory category, bool enabled);
    bool IsCategoryEnabled(GameCategory category) const;

    // Available techs in selected categories, or all available techs when none match.
    std::vector<const TechConfig_t*> GetCandidateTargets() const;

    // Picks a random candidate and sets it as the research target.
    // Returns false when no techs are available to research.
    bool AssignResearchTarget();

    // Assigns a research target when none is currently set.
    void EnsureResearchTarget();

private:
    bool IsTechInSelectedCategory_(const TechConfig_t& rConfig) const;
    const TechConfig_t* PickRandom_(const std::vector<const TechConfig_t*>& rCandidates) const;

    ResearchManager* m_pResearchManager;
    std::array<bool, k_GameCategoryCount> m_categoryEnabled;
    mutable std::mt19937 m_rng;
};

} // namespace ac
