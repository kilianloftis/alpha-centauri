#include "game/map/Tile.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/TileChangeListener.h"
#include "lib/Revision.h"
#include <magic_enum.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ac
{

namespace
{

struct DeferredTileChange_t
{
    Tile* pTile = nullptr;
    std::string keepId;
};

int g_tileChangeDeferDepth = 0;
std::vector<DeferredTileChange_t> g_deferredTileChanges;

} // namespace

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
    if (m_moisture == moisture)
    {
        return;
    }
    m_moisture = moisture;
    RefreshTerrainFeatures_();
    NotifyTileChanged_({});
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
    if (m_rockiness == rockiness)
    {
        return;
    }
    m_rockiness = rockiness;
    RefreshTerrainFeatures_();
    NotifyTileChanged_({});
}

Rockiness_t Tile::GetRockiness() const
{
    return m_rockiness;
}

void Tile::BindMapRules(const ElevationRulesConfig_t& rRules)
{
    m_pMapRules = &rRules;
    if (m_pImprovements)
    {
        RefreshTerrainFeatures_();
    }
}

const ElevationRulesConfig_t& Tile::MapRules() const
{
    if (!m_pMapRules)
    {
        throw std::logic_error("Tile map rules are not bound");
    }
    return *m_pMapRules;
}

void Tile::SetElevation(int elevation)
{
    const ElevationRulesConfig_t& rRules = MapRules();
    if (elevation < rRules.minElevationMeters || elevation > rRules.maxElevationMeters)
    {
        throw std::out_of_range("Tile elevation " + std::to_string(elevation) + " is outside ["
                                + std::to_string(rRules.minElevationMeters) + ", "
                                + std::to_string(rRules.maxElevationMeters) + "]");
    }
    if (m_elevation == elevation)
    {
        return;
    }
    m_elevation = elevation;
    RefreshTerrainFeatures_();
    NotifyAppearanceChanged_();
    NotifyTileChanged_({});
}

int Tile::GetElevation() const
{
    return m_elevation;
}

bool Tile::IsWater() const
{
    return m_elevation < MapRules().oceanLevelMeters;
}

bool Tile::IsLand() const
{
    return !IsWater();
}

void Tile::SetHasRiver(bool bHasRiver)
{
    if (m_bHasRiver == bHasRiver)
    {
        return;
    }
    m_bHasRiver = bHasRiver;
    RefreshTerrainFeatures_();
    NotifyTileChanged_({});
}

bool Tile::GetHasRiver() const
{
    return m_bHasRiver;
}

void Tile::SetHasAquifer(bool bHasAquifer)
{
    if (m_bHasAquifer == bHasAquifer)
    {
        return;
    }
    m_bHasAquifer = bHasAquifer;
    RefreshTerrainFeatures_();
    NotifyTileChanged_({});
}

bool Tile::GetHasAquifer() const
{
    return m_bHasAquifer;
}

void Tile::BindImprovements(const ImprovementRegistry& rImprovements)
{
    m_pImprovements = &rImprovements;
    RefreshTerrainFeatures_();
}

void Tile::BindAppearanceRevision(Revision& rRevision)
{
    m_pAppearanceRevision = &rRevision;
}

void Tile::BindTileChangeListener(TileChangeListener* pListener)
{
    m_pTileChangeListener = pListener;
}

void Tile::UnbindTileChangeListener(TileChangeListener& rListener)
{
    if (m_pTileChangeListener == &rListener)
    {
        m_pTileChangeListener = nullptr;
    }
}

void Tile::NotifyTileChanged_(std::string_view keepId)
{
    if (!m_pTileChangeListener)
    {
        return;
    }
    if (g_tileChangeDeferDepth > 0)
    {
        for (DeferredTileChange_t& rPending : g_deferredTileChanges)
        {
            if (rPending.pTile != this)
            {
                continue;
            }
            if (!keepId.empty())
            {
                rPending.keepId = std::string(keepId);
            }
            return;
        }
        g_deferredTileChanges.push_back(DeferredTileChange_t{this, std::string(keepId)});
        return;
    }
    m_pTileChangeListener->OnTileChanged(*this, keepId);
}

void Tile::NotifyAppearanceChanged_()
{
    if (m_pAppearanceRevision)
    {
        m_pAppearanceRevision->Bump();
    }
}

void Tile::AddImprovement(const ImprovementConfig_t& rConfig)
{
    if (HasImprovement(rConfig.id))
    {
        return;
    }
    m_improvements.push_back(&rConfig);
    NotifyAppearanceChanged_();
    NotifyTileChanged_(rConfig.id);
}

void Tile::RemoveImprovement(std::string_view improvementId)
{
    const auto it = std::remove_if(m_improvements.begin(), m_improvements.end(),
                                   [&](const ImprovementConfig_t* pConfig) {
                                       return pConfig->id == improvementId;
                                   });
    if (it == m_improvements.end())
    {
        return;
    }
    m_improvements.erase(it, m_improvements.end());
    NotifyAppearanceChanged_();
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
    // cannot carry "River" or a depth band as a built improvement. The switch is
    // exhaustive so adding a TerrainFeature_t enumerator fails to compile until handled here.
    if (const auto feature = magic_enum::enum_cast<TerrainFeature_t>(featureId))
    {
        switch (*feature)
        {
            case TerrainFeature_t::Water:
                return IsWater();
            case TerrainFeature_t::Ocean:
                return IsWater() && m_elevation < MapRules().oceanShelfMeters;
            case TerrainFeature_t::OceanShelf:
                return IsWater() && m_elevation >= MapRules().oceanShelfMeters;
            case TerrainFeature_t::River:
                return m_bHasRiver;
            case TerrainFeature_t::Aquifer:
                return m_bHasAquifer;
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
        pushFeature(magic_enum::enum_name(m_elevation >= MapRules().oceanShelfMeters
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
}

TileChangeDeferral::TileChangeDeferral()
{
    ++g_tileChangeDeferDepth;
}

TileChangeDeferral::~TileChangeDeferral()
{
    if (--g_tileChangeDeferDepth > 0)
    {
        return;
    }

    std::vector<DeferredTileChange_t> pending;
    pending.swap(g_deferredTileChanges);
    for (const DeferredTileChange_t& rChange : pending)
    {
        if (!rChange.pTile || !rChange.pTile->m_pTileChangeListener)
        {
            continue;
        }
        rChange.pTile->m_pTileChangeListener->OnTileChanged(*rChange.pTile, rChange.keepId);
    }
}

} // namespace ac
