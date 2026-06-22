#pragma once

#include <string>

namespace ac
{

struct BuildingConfig_t;

// A single building instance. Behaviour is entirely driven by its BuildingConfig_t.
class Building
{
public:
    explicit Building(const BuildingConfig_t& rConfig);
    ~Building();

    // Type id string matching the config (e.g. "Nutrient_Bank")
    const char* GetBuildingId() const;

    // Display name of this building type.
    const std::string& GetName() const;

    // Mineral cost to construct this building.
    int GetMineralCost() const;

    // Whether multiple copies of this building can exist in the same base.
    bool GetAllowMultiple() const;

    // Flat nutrients bonus this building provides to the base each turn.
    int GetNutrientsBonus() const;

    // Nutrients bonus this building provides when a specific improvement is present on a tile.
    // Returns 0 if the improvement is not listed in the building's config.
    int GetImprovementNutrientsBonus(const std::string& improvementName) const;

private:
    const BuildingConfig_t* m_pConfig;
};

} // namespace ac
