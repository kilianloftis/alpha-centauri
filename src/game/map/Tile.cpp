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

Tile::Tile()
    : m_x(0)
    , m_y(0)
    , m_moisture(Moisture_t::Arid)
    , m_baseMoisture(Moisture_t::Arid)
    , m_rockiness(Rockiness_t::Flat)
    , m_elevation(0)
    , m_bHasRiver(false)
    , m_bHasAquifer(false)
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
    , m_bHasAquifer(false)
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
    RefreshTerrainFeatures_();
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

void Tile::SetHasAquifer(bool bHasAquifer)
{
    m_bHasAquifer = bHasAquifer;
    RefreshTerrainFeatures_();
}

bool Tile::GetHasAquifer() const
{
    return m_bHasAquifer;
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
    // Intrinsic features answer from tile state, never from the improvement list - a tile
    // cannot carry "River"/"Fungus"/a depth band as a built improvement. The switch is
    // exhaustive so adding a TerrainFeature_t enumerator fails to compile until handled here.
    if (const auto feature = magic_enum::enum_cast<TerrainFeature_t>(featureId))
    {
        switch (*feature)
        {
            case TerrainFeature_t::Water:
                return IsWater();
            case TerrainFeature_t::Ocean:
                return IsWater() && m_elevation < k_OceanShelfMinElevation;
            case TerrainFeature_t::OceanShelf:
                return IsWater() && m_elevation >= k_OceanShelfMinElevation;
            case TerrainFeature_t::River:
                return m_bHasRiver;
            case TerrainFeature_t::Aquifer:
                return m_bHasAquifer;
            case TerrainFeature_t::Fungus:
                return m_bHasFungus;
        }
    }
    if (magic_enum::enum_name(m_rockiness) == featureId) return true;
    if (magic_enum::enum_name(m_moisture)  == featureId) return true;
    return HasImprovement(featureId);
}

void Tile::RefreshTerrainFeatures_()
{
    m_terrainFeatures.clear();
    if (!m_pImprovements)
    {
        return;
    }

    // Get() rather than Find(): ValidateTerrainFeatures has already proven every id below
    // exists in the registry, so a miss here is a broken invariant, not a skippable feature.
    auto pushFeature = [&](std::string_view id)
    {
        m_terrainFeatures.push_back(&m_pImprovements->Get(std::string(id)));
    };

    pushFeature(magic_enum::enum_name(m_rockiness));
    pushFeature(magic_enum::enum_name(m_moisture));
    if (IsWater())
    {
        // General-to-specific: Water carries the rules shared by all sea tiles, then exactly
        // one depth band layers its own on top.
        pushFeature(magic_enum::enum_name(TerrainFeature_t::Water));
        pushFeature(magic_enum::enum_name(m_elevation >= k_OceanShelfMinElevation
                                              ? TerrainFeature_t::OceanShelf
                                              : TerrainFeature_t::Ocean));
    }
    if (m_bHasRiver)
    {
        pushFeature(magic_enum::enum_name(TerrainFeature_t::River));
    }
    if (m_bHasAquifer)
    {
        pushFeature(magic_enum::enum_name(TerrainFeature_t::Aquifer));
    }
    if (m_bHasFungus)
    {
        pushFeature(magic_enum::enum_name(TerrainFeature_t::Fungus));
    }
}

} // namespace ac
