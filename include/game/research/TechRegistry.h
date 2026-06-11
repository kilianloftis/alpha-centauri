#pragma once

#include "game/research/Tech.h"
#include <map>
#include <memory>
#include <vector>

namespace ac
{

class TechRegistry
{
public:
    TechRegistry();
    ~TechRegistry();

    void RegisterTech(std::unique_ptr<Tech> pTech);
    Tech* GetTech(TechId techId);
    const Tech* GetTech(TechId techId) const;

    bool HasTech(TechId techId) const;

    std::vector<TechId> GetAllTechIds() const;
    std::vector<TechId> GetAvailableTechs(const std::vector<TechId>& discoveredTechs) const;

    void Clear();

private:
    std::map<TechId, std::unique_ptr<Tech>> m_techs;
};

} // namespace ac
