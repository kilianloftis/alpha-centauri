#include "game/research/TechRegistry.h"
#include "game/research/TechConfigParser.h"
#include <algorithm>
#include <iostream>

namespace ac
{

TechRegistry::TechRegistry()
    : m_techs()
{
}

TechRegistry::~TechRegistry()
{
}

void TechRegistry::Load(const std::string& configPath)
{
    TechConfigParser parser;
    auto configs = parser.ParseConfig(configPath);

    Clear();

    
    for (const TechConfig& rConfig : configs)
    {
        ValidateUniqueIds_(rConfig, configs);
        ValidatePrerequisites_(rConfig, configs);
        auto pTech = std::make_unique<Tech>(rConfig);
        m_techs[rConfig.id] = std::move(pTech);
    }

    std::cout << "Registered " << m_techs.size() << " techs\n";
}

void TechRegistry::ValidatePrerequisites_(const TechConfig& config, const std::vector<TechConfig>& configs)
{
    // Check for self-reference
    if (std::find(config.prerequisites.begin(), config.prerequisites.end(), config.id) != config.prerequisites.end())
    {
        throw std::runtime_error("Tech '" + config.id + "' cannot have itself as a prerequisite");
    }

    // Check all prerequisites exist
    for (const std::string& prereqId : config.prerequisites)
    {
        auto it = std::find_if(configs.begin(), configs.end(),
            [&prereqId](const TechConfig& c) { return c.id == prereqId; });
        if (it == configs.end())
        {
            throw std::runtime_error("Prerequisite '" + prereqId + "' not found for tech '" + config.id + "'");
        }
    }
}

void TechRegistry::ValidateUniqueIds_(const TechConfig& config, const std::vector<TechConfig>& configs)
{
    int count = 0;
    for (const TechConfig& c : configs)
    {
        if (c.id == config.id)
        {
            ++count;
        }
        if (count > 1)
        {
            throw std::runtime_error("Tech '" + config.id + "' has duplicate ID");
        }
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
