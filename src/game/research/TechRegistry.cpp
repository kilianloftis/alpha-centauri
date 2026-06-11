#include "game/research/TechRegistry.h"

namespace ac
{

TechRegistry::TechRegistry()
    : m_techs()
{
}

TechRegistry::~TechRegistry()
{
}

void TechRegistry::RegisterTech(std::unique_ptr<Tech> pTech)
{
    if (pTech)
    {
        TechId id = pTech->GetId();
        m_techs[id] = std::move(pTech);
    }
}

Tech* TechRegistry::GetTech(TechId techId)
{
    auto it = m_techs.find(techId);
    if (it != m_techs.end())
    {
        return it->second.get();
    }
    return nullptr;
}

const Tech* TechRegistry::GetTech(TechId techId) const
{
    auto it = m_techs.find(techId);
    if (it != m_techs.end())
    {
        return it->second.get();
    }
    return nullptr;
}

bool TechRegistry::HasTech(TechId techId) const
{
    return m_techs.find(techId) != m_techs.end();
}

std::vector<TechId> TechRegistry::GetAllTechIds() const
{
    std::vector<TechId> ids;
    for (const auto& pair : m_techs)
    {
        ids.push_back(pair.first);
    }
    return ids;
}

std::vector<TechId> TechRegistry::GetAvailableTechs(const std::vector<TechId>& discoveredTechs) const
{
    std::vector<TechId> available;

    for (const auto& pair : m_techs)
    {
        TechId techId = pair.first;
        const Tech* pTech = pair.second.get();

        bool bAlreadyDiscovered = false;
        for (TechId discovered : discoveredTechs)
        {
            if (discovered == techId)
            {
                bAlreadyDiscovered = true;
                break;
            }
        }

        if (bAlreadyDiscovered)
        {
            continue;
        }

        bool bPrerequisitesMet = true;
        for (TechId prereq : pTech->GetPrerequisites())
        {
            bool bHasPrereq = false;
            for (TechId discovered : discoveredTechs)
            {
                if (discovered == prereq)
                {
                    bHasPrereq = true;
                    break;
                }
            }
            if (!bHasPrereq)
            {
                bPrerequisitesMet = false;
                break;
            }
        }

        if (bPrerequisitesMet)
        {
            available.push_back(techId);
        }
    }

    return available;
}

void TechRegistry::Clear()
{
    m_techs.clear();
}

} // namespace ac
