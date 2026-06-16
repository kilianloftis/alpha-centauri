#pragma once

#include "game/research/Tech.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ac
{

class TechRegistry
{
public:
    TechRegistry();
    ~TechRegistry();

    // Load all techs from a config file. Throws on failure.
    void Load(const std::string& configPath);

    Tech* GetTech(TechId techId);
    const Tech* GetTech(TechId techId) const;

    bool HasTech(TechId techId) const;

    std::vector<TechId> GetAllTechIds() const;
    std::vector<TechId> GetAvailableTechs(const std::vector<TechId>& discoveredTechs) const;

    void Clear();

private:
    void ValidatePrerequisites_(const TechConfig& config, const std::vector<TechConfig>& configs);
    void ValidateUniqueIds_(const TechConfig& config, const std::vector<TechConfig>& configs);

    std::map<TechId, std::unique_ptr<Tech>> m_techs;
};

} // namespace ac
