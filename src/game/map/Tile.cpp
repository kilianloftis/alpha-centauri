#include "game/map/Tile.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include <magic_enum.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

namespace ac
{

std::string ToString(Rockiness_t rockiness)
{
    return std::string(magic_enum::enum_name(rockiness));
}

std::string ToString(Moisture_t moisture)
{
    return std::string(magic_enum::enum_name(moisture));
}

const std::vector<std::string>& AllTerrainFeatureIds()
{
    // Built from the same ToString mappings HasFeature matches against, so the list can't
    // drift from the actual matching logic.
    static const std::vector<std::string> ids = {
        ToString(Rockiness_t::Flat), ToString(Rockiness_t::Rolling), ToString(Rockiness_t::Rocky),
        ToString(Moisture_t::Arid), ToString(Moisture_t::Moist), ToString(Moisture_t::Wet),
        "River", "Fungus",
    };
    return ids;
}

Tile::Tile()
    : m_x(0)
    , m_y(0)
    , m_moisture(Moisture_t::Arid)
    , m_baseMoisture(Moisture_t::Arid)
    , m_rockiness(Rockiness_t::Flat)
    , m_elevation(0)
    , m_bHasRiver(false)
    , m_bHasFungus(false)
{
}

Tile::Tile(int x, int y)
    : m_x(x)
    , m_y(y)
    , m_moisture(Moisture_t::Arid)
    , m_baseMoisture(Moisture_t::Arid)
    , m_rockiness(Rockiness_t::Flat)
    , m_elevation(0)
    , m_bHasRiver(false)
    , m_bHasFungus(false)
{
}

Tile::~Tile()
{
}

int Tile::GetX() const
{
    return m_x;
}

int Tile::GetY() const
{
    return m_y;
}

void Tile::SetMoisture(Moisture_t moisture)
{
    m_moisture = moisture;
    RefreshTerrainFeatures_();
}

Moisture_t Tile::GetMoisture() const
{
    return m_moisture;
}

void Tile::SetBaseMoisture(Moisture_t moisture)
{
    m_baseMoisture = moisture;
}

Moisture_t Tile::GetBaseMoisture() const
{
    return m_baseMoisture;
}

void Tile::SetRockiness(Rockiness_t rockiness)
{
    m_rockiness = rockiness;
    RefreshTerrainFeatures_();
}

Rockiness_t Tile::GetRockiness() const
{
    return m_rockiness;
}

void Tile::SetElevation(int elevation)
{
    m_elevation = elevation;
}

int Tile::GetElevation() const
{
    return m_elevation;
}

bool Tile::IsWater() const
{
    return m_elevation < 0;
}

bool Tile::IsLand() const
{
    return !IsWater();
}

int Tile::GetElevationEnergySeed() const
{
    if (IsWater())
    {
        return 0;
    }
    return static_cast<int>(std::floor(m_elevation / 1000.0));
}

void Tile::SetHasRiver(bool bHasRiver)
{
    m_bHasRiver = bHasRiver;
    RefreshTerrainFeatures_();
}

bool Tile::GetHasRiver() const
{
    return m_bHasRiver;
}

void Tile::SetHasFungus(bool bHasFungus)
{
    m_bHasFungus = bHasFungus;
    RefreshTerrainFeatures_();
}

bool Tile::GetHasFungus() const
{
    return m_bHasFungus;
}

void Tile::BindImprovements(const ImprovementRegistry& rImprovements)
{
    m_pImprovements = &rImprovements;
    RefreshTerrainFeatures_();
}

void Tile::AddImprovement(const ImprovementConfig_t& rConfig)
{
    if (!HasImprovement(rConfig.id))
    {
        m_improvements.push_back(&rConfig);
    }
}

void Tile::RemoveImprovement(std::string_view improvementId)
{
    auto it = std::remove_if(m_improvements.begin(), m_improvements.end(),
                             [&](const ImprovementConfig_t* pConfig)
                             {
                                 return pConfig->id == improvementId;
                             });
    m_improvements.erase(it, m_improvements.end());
}

bool Tile::HasImprovement(std::string_view improvementId) const
{
    return std::any_of(m_improvements.begin(), m_improvements.end(),
                       [&](const ImprovementConfig_t* pConfig)
                       {
                           return pConfig->id == improvementId;
                       });
}

const std::vector<const ImprovementConfig_t*>& Tile::GetImprovements() const
{
    return m_improvements;
}

const std::vector<const ImprovementConfig_t*>& Tile::GetTerrainFeatures() const
{
    return m_terrainFeatures;
}

bool Tile::HasFeature(std::string_view featureId) const
{
    if (ToString(m_rockiness) == featureId) return true;
    if (ToString(m_moisture)  == featureId) return true;
    if (m_bHasRiver  && featureId == "River")  return true;
    if (m_bHasFungus && featureId == "Fungus") return true;
    return HasImprovement(featureId);
}

void Tile::RefreshTerrainFeatures_()
{
    m_terrainFeatures.clear();
    if (!m_pImprovements)
    {
        return;
    }

    if (const ImprovementConfig_t* pRockiness = m_pImprovements->Find(ToString(m_rockiness)))
    {
        m_terrainFeatures.push_back(pRockiness);
    }
    if (const ImprovementConfig_t* pMoisture = m_pImprovements->Find(ToString(m_moisture)))
    {
        m_terrainFeatures.push_back(pMoisture);
    }
    if (m_bHasRiver)
    {
        if (const ImprovementConfig_t* pRiver = m_pImprovements->Find("River"))
        {
            m_terrainFeatures.push_back(pRiver);
        }
    }
    if (m_bHasFungus)
    {
        if (const ImprovementConfig_t* pFungus = m_pImprovements->Find("Fungus"))
        {
            m_terrainFeatures.push_back(pFungus);
        }
    }
}

} // namespace ac
