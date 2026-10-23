#pragma once

#include "game/map/ElevationRulesConfig.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ac
{

struct ImprovementConfig_t;
class ImprovementRegistry;
class Revision;
class TileChangeListener;

enum class Rockiness_t
{
    Flat,
    Rolling,
    Rocky
};

enum class Moisture_t
{
    Arid,
    Moist,
    Wet
};

// Features answered from intrinsic tile state. Names are the terrain.json ids (magic_enum).
// Water is not exclusive with the depth bands: a submerged tile has one of Ocean/OceanShelf.
enum class TerrainFeature_t
{
    Water,
    Ocean,
    OceanShelf,
    River,
    Aquifer
};

// String ids matching ImprovementConfig_t::id entries in config/terrain.json, used to look
// up effects/exclusivity for these terrain classifications.
std::string ToString(Rockiness_t rockiness);
std::string ToString(Moisture_t moisture);

class Tile
{
public:
    Tile();
    Tile(int x, int y);
    ~Tile() = default;

    int GetX() const;
    int GetY() const;

    // The effective moisture, which RecomputeMoisture re-derives from the base moisture world gen
    // set plus Condensers in range.
    void SetMoisture(Moisture_t moisture);
    Moisture_t GetMoisture() const;

    void SetBaseMoisture(Moisture_t moisture);
    Moisture_t GetBaseMoisture() const;

    void SetRockiness(Rockiness_t rockiness);
    Rockiness_t GetRockiness() const;

    // Throws if map rules are unbound, or elevation is outside their min/max.
    void SetElevation(int elevation);
    int GetElevation() const;

    // Bound by WorldMap from config/map_rules.json. Throws if unbound.
    void BindMapRules(const ElevationRulesConfig_t& rRules);
    const ElevationRulesConfig_t& MapRules() const;

    // Water when elevation is below the bound ocean level.
    bool IsWater() const;
    bool IsLand() const;

    // Rivers (derived from aquifer downhill flow; rebuilt by RecomputeRivers)
    void SetHasRiver(bool bHasRiver);
    bool GetHasRiver() const;

    // Aquifers (persistent river sources; placed by world-gen / Former Aquifer)
    void SetHasAquifer(bool bHasAquifer);
    bool GetHasAquifer() const;

    // Binds the tile to the occupant registry so terrain is mirrored as configs. Call once.
    void BindOccupants(const ImprovementRegistry& rOccupants);

    // Bound-registry lookup (not limited to occupants currently on this tile). Null when
    // unbound or the id is absent — used by stacked moisture drawing to fetch Arid/Moist/Wet.
    const ImprovementConfig_t* FindOccupantConfig(std::string_view id) const;

    // WorldMap appearance cache (minimap fill colours). Optional — unbound tiles used in unit
    // tests do not notify.
    void BindAppearanceRevision(Revision& rRevision);

    // Optional. Characteristic changes and AddImprovement notify it. Unbound tiles do not.
    void BindTileChangeListener(TileChangeListener* pListener);
    void UnbindTileChangeListener(TileChangeListener& rListener);

    // Built improvements and the Base marker, as pointers into ImprovementRegistry. The caller
    // resolves configs.
    void AddImprovement(const ImprovementConfig_t& rConfig);
    void RemoveImprovement(std::string_view improvementId);
    bool HasImprovement(std::string_view improvementId) const;
    const std::vector<const ImprovementConfig_t*>& GetImprovements() const;

    // Optional terrain (fungus, landmarks, Monolith). HasTerrainFeature includes dormant terrain;
    // HasFeature and GetTerrainFeatures do not.
    void AddTerrainFeature(const ImprovementConfig_t& rConfig);
    void RemoveTerrainFeature(std::string_view featureId);
    bool HasTerrainFeature(std::string_view featureId) const;

    // Routes an occupant to the list its placement names.
    void AddOccupant(const ImprovementConfig_t& rConfig);
    void RemoveOccupant(const ImprovementConfig_t& rConfig);

    // (improvement, occupant) pairs an override allowed at construction, so a later sweep does not
    // evict them. Dropped with either side.
    void AddCoexistenceWaiver(const std::string& rImprovementId, const std::string& rFeatureId);
    const std::vector<std::pair<std::string, std::string>>& GetCoexistenceWaivers() const;

    // Rockiness, moisture, active TerrainFeature_t and active optional terrain, general to
    // specific. Not improvements.
    const std::vector<const ImprovementConfig_t*>& GetTerrainFeatures() const;

    // Terrain then improvements; fn returns true to stop, and so does ForEachOccupant.
    template <typename Fn>
    bool ForEachOccupant(Fn&& fn) const
    {
        for (const ImprovementConfig_t* pConfig : m_terrainFeatures)
        {
            if (pConfig && fn(*pConfig))
            {
                return true;
            }
        }
        for (const ImprovementConfig_t* pConfig : m_improvements)
        {
            if (pConfig && fn(*pConfig))
            {
                return true;
            }
        }
        return false;
    }

    // True if featureId is an active feature: an axis value, a TerrainFeature_t or an improvement.
    bool HasFeature(std::string_view featureId) const;

private:
    friend class TileChangeDeferral;

    void RefreshTerrainFeatures_();
    // Rockiness, moisture, water depth band, river and aquifer: what the tile always is.
    std::vector<std::string_view> IntrinsicFeatureIds_() const;
    void DropWaiversForAbsentOccupants_();
    void NotifyAppearanceChanged_();
    void NotifyTileChanged_(std::string_view keepId);

    int m_x;
    int m_y;

    Moisture_t m_moisture;
    Moisture_t m_baseMoisture;
    Rockiness_t m_rockiness;
    int m_elevation;

    bool m_bHasRiver;
    bool m_bHasAquifer;

    const ElevationRulesConfig_t* m_pMapRules = nullptr;
    const ImprovementRegistry* m_pOccupants = nullptr;
    Revision* m_pAppearanceRevision = nullptr;
    TileChangeListener* m_pTileChangeListener = nullptr;
    std::vector<const ImprovementConfig_t*> m_terrainFeatures;
    std::vector<const ImprovementConfig_t*> m_optionalTerrain;
    std::vector<const ImprovementConfig_t*> m_improvements;
    std::vector<std::pair<std::string, std::string>> m_coexistenceWaivers;
};

// Holds tile-change notifications until the outermost deferral ends, so half-updated rivers or
// slopes are not reassessed.
class TileChangeDeferral
{
public:
    TileChangeDeferral();
    ~TileChangeDeferral();

    TileChangeDeferral(const TileChangeDeferral&) = delete;
    TileChangeDeferral& operator=(const TileChangeDeferral&) = delete;
};

} // namespace ac
