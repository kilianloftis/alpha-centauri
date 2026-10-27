#pragma once

#include <stdexcept>
#include <string>

namespace ac
{

// SMAC's UNIT.plan: a design's AI role. Probe costs read it (docs/thinker/terranx-map.md,
// "Unit plan").
enum class UnitPlan_t
{
    Offense,
    Combat,
    Defense,
    Recon,
    AirSuperiority,
    PlanetBuster,
    NavalSuperiority,
    NavalTransport,
    Colony,
    Terraform,
    Supply,
    Probe,
    Artifact,
    TectonicMissile,
    FungalMissile,
};

inline UnitPlan_t ParseUnitPlan(const std::string& rPlan)
{
    if (rPlan == "offense")           return UnitPlan_t::Offense;
    if (rPlan == "combat")            return UnitPlan_t::Combat;
    if (rPlan == "defense")           return UnitPlan_t::Defense;
    if (rPlan == "recon")             return UnitPlan_t::Recon;
    if (rPlan == "air_superiority")   return UnitPlan_t::AirSuperiority;
    if (rPlan == "planet_buster")     return UnitPlan_t::PlanetBuster;
    if (rPlan == "naval_superiority") return UnitPlan_t::NavalSuperiority;
    if (rPlan == "naval_transport")   return UnitPlan_t::NavalTransport;
    if (rPlan == "colony")            return UnitPlan_t::Colony;
    if (rPlan == "terraform")         return UnitPlan_t::Terraform;
    if (rPlan == "supply")            return UnitPlan_t::Supply;
    if (rPlan == "probe")             return UnitPlan_t::Probe;
    if (rPlan == "artifact")          return UnitPlan_t::Artifact;
    if (rPlan == "tectonic_missile")  return UnitPlan_t::TectonicMissile;
    if (rPlan == "fungal_missile")    return UnitPlan_t::FungalMissile;
    throw std::runtime_error("Unknown unit plan '" + rPlan + "'");
}

} // namespace ac
