#include "game/faction/ResearchSelector.h"

#include "game/faction/ResearchManager.h"

#include <random>
#include <stdexcept>

namespace ac
{

namespace
{

size_t CategoryIndex_(GameCategory_t category)
{
    switch (category)
    {
    case GameCategory_t::Build:    return 0;
    case GameCategory_t::Grow:     return 1;
    case GameCategory_t::Discover: return 2;
    case GameCategory_t::Conquer:  return 3;
    }

    throw std::runtime_error("Unknown game category");
}

} // namespace

ResearchSelector::ResearchSelector(ResearchManager* pResearchManager)
    : ResearchSelector(pResearchManager, std::random_device{}())
{
}

ResearchSelector::ResearchSelector(ResearchManager* pResearchManager, uint32_t seed)
    : m_pResearchManager(pResearchManager)
    , m_categoryEnabled{true, true, true, true}
    , m_rng(seed)
{
}

void ResearchSelector::SetCategoryEnabled(GameCategory_t category, bool enabled)
{
    m_categoryEnabled[CategoryIndex_(category)] = enabled;
}

bool ResearchSelector::IsCategoryEnabled(GameCategory_t category) const
{
    return m_categoryEnabled[CategoryIndex_(category)];
}

std::vector<const TechConfig_t*> ResearchSelector::GetCandidateTargets() const
{
    if (!m_pResearchManager)
    {
        throw std::runtime_error("ResearchManager is null");
    }

    const std::vector<const TechConfig_t*> available = m_pResearchManager->GetAvailableTechs();
    if (available.empty())
    {
        return {};
    }

    std::vector<const TechConfig_t*> preferred;
    for (const TechConfig_t* pConfig : available)
    {
        if (!pConfig)
        {
            continue;
        }

        if (IsTechInSelectedCategory_(*pConfig))
        {
            preferred.push_back(pConfig);
        }
    }

    if (!preferred.empty())
    {
        return preferred;
    }

    return available;
}

bool ResearchSelector::AssignResearchTarget()
{
    if (!m_pResearchManager)
    {
        return false;
    }

    const std::vector<const TechConfig_t*> candidates = GetCandidateTargets();
    if (candidates.empty())
    {
        return false;
    }

    const TechConfig_t* pChosen = PickRandom_(candidates);
    if (!pChosen)
    {
        return false;
    }

    m_pResearchManager->SetResearchTarget(pChosen->id);
    return true;
}

void ResearchSelector::EnsureResearchTarget()
{
    if (!m_pResearchManager || m_pResearchManager->HasResearchTarget())
    {
        return;
    }

    AssignResearchTarget();
}

bool ResearchSelector::IsTechInSelectedCategory_(const TechConfig_t& rConfig) const
{
    return IsCategoryEnabled(rConfig.category);
}

const TechConfig_t* ResearchSelector::PickRandom_(const std::vector<const TechConfig_t*>& rCandidates) const
{
    std::uniform_int_distribution<size_t> dist(0, rCandidates.size() - 1);
    return rCandidates[dist(m_rng)];
}

} // namespace ac
