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
    // The manager is a reference: the owning Faction always has one, and the pointer form had
    // three different null policies for the same member (throw / return false / silent no-op).
    // The seed is required — the convenience overload that drew from std::random_device made
    // the starting research target unreproducible across runs of the same session.
    ResearchSelector(ResearchManager& rResearchManager, uint32_t seed);

    void SetCategoryEnabled(GameCategory_t category, bool enabled);
    bool IsCategoryEnabled(GameCategory_t category) const;

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

    ResearchManager& m_rResearchManager;
    std::array<bool, k_GameCategoryCount> m_categoryEnabled;
    mutable std::mt19937 m_rng;
};

} // namespace ac
