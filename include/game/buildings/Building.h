#pragma once

#include <string>

namespace ac
{

struct BuildingConfig;

// A single building instance. Behaviour is entirely driven by its BuildingConfig.
class Building
{
public:
    explicit Building(const BuildingConfig& rConfig);
    ~Building();

    // Type id string matching the config (e.g. "Nutrient_Bank")
    const char* GetBuildingId() const;

    // Flat nutrients bonus this building provides to the base each turn.
    int GetNutrientsBonus() const;

    // Nutrients bonus this building provides when a specific improvement is present on a tile.
    // Returns 0 if the improvement is not listed in the building's config.
    int GetImprovementNutrientsBonus(const std::string& improvementName) const;

private:
    const BuildingConfig* m_pConfig;
};

} // namespace ac
