#pragma once

#include "game/map/ElevationRulesConfig.h"

#include <string>
#include <string_view>
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

// Feature ids Tile::HasFeature resolves from intrinsic tile state (elevation and the terrain
// bools) rather than from the improvement list. Enumerator names ARE the corresponding
// ImprovementConfig_t::id strings - magic_enum maps between the two, so there is no second
// place to keep them in sync - and every one must exist in improvements.json, which
// ValidateTerrainFeatures enforces at load. Water is not exclusive with the depth bands: a
// submerged tile carries Water (shared sea rules) plus exactly one of Ocean/OceanShelf.
enum class TerrainFeature_t
{
    Water,
    Ocean,
    OceanShelf,
    River,
    Aquifer
};

// String ids matching ImprovementConfig_t::id entries in config/improvements.json,
// used to look up effects/exclusivity for these terrain classifications.
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

    // Terrain characteristics. GetMoisture()/SetMoisture() are the CURRENT/effective value -
    // what rendering and GetTerrainFeatures() see, and what a Condenser's MoistureTier effect
    // mutates via RecomputeMoisture(). GetBaseMoisture()/SetBaseMoisture() are the natural,
    // un-condensed terrain truth set once by world generation; RecomputeMoisture always
    // re-derives the current value from the base plus whatever Condensers currently reach
    // this tile, so the bonus disappears cleanly the moment a Condenser is removed - never
    // mutated incrementally, to avoid drift from overlapping Condensers or add/remove order.
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

    // Binds this tile to the improvement registry so terrain enums/bools can be mirrored as
    // non-owning ImprovementConfig_t pointers (see GetTerrainFeatures). Call once after the
    // registry is loaded — TileEffectsContext does this for every map tile. Terrain setters
    // refresh the cached configs whenever the registry is bound.
    void BindImprovements(const ImprovementRegistry& rImprovements);

    // WorldMap appearance cache (minimap fill colours). Optional — unbound tiles used in unit
    // tests do not notify.
    void BindAppearanceRevision(Revision& rRevision);

    // Optional. Characteristic changes and AddImprovement notify it. Unbound tiles do not.
    void BindTileChangeListener(TileChangeListener* pListener);
    void UnbindTileChangeListener(TileChangeListener& rListener);

    // Improvements: every non-terrain feature on this tile, held as non-owning pointers into
    // ImprovementRegistry (the same way BuildingManager holds BuildingConfig_t*). This one
    // collection covers player-built improvements (Farm, Mine, Bunker), the "Base" marker
    // added when a base is founded here (see BaseManager), and what were formerly separate
    // "bonus"/"landmark" slots - for the map they are all just improvements, with coexistence
    // governed by ImprovementConfig_t::excludes. Configs are resolved by the caller (the
    // registry funnel is TileEffectsContext); Tile never looks them up itself.
    void AddImprovement(const ImprovementConfig_t& rConfig);
    void RemoveImprovement(std::string_view improvementId);
    bool HasImprovement(std::string_view improvementId) const;
    const std::vector<const ImprovementConfig_t*>& GetImprovements() const;

    // Terrain-only feature configs: rockiness, moisture, and every active TerrainFeature_t.
    // Intrinsic terrain properties (enums/bools/elevation) are mirrored here as registry
    // pointers after BindImprovements, ordered general-to-specific (Water before its depth
    // band). Improvements are NOT included — effect collectors iterate GetImprovements().
    const std::vector<const ImprovementConfig_t*>& GetTerrainFeatures() const;

    // Returns true if featureId matches any active feature on this tile: a rockiness/moisture
    // name, an active TerrainFeature_t, or an improvement id. Used by conditions/selectors and
    // CanBuildImprovement, which reference features by string id.
    bool HasFeature(std::string_view featureId) const;

private:
    friend class TileChangeDeferral;

    void RefreshTerrainFeatures_();
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
    const ImprovementRegistry* m_pImprovements = nullptr;
    Revision* m_pAppearanceRevision = nullptr;
    TileChangeListener* m_pTileChangeListener = nullptr;
    std::vector<const ImprovementConfig_t*> m_terrainFeatures;
    std::vector<const ImprovementConfig_t*> m_improvements;
};

// Holds tile-change notifications until the batch finishes, then delivers one per tile.
// RecomputeRivers and ApplyElevationDelta use it so a half-updated river or slope is not
// reassessed. Nested deferrals deliver when the outermost one ends.
class TileChangeDeferral
{
public:
    TileChangeDeferral();
    ~TileChangeDeferral();

    TileChangeDeferral(const TileChangeDeferral&) = delete;
    TileChangeDeferral& operator=(const TileChangeDeferral&) = delete;
};

} // namespace ac
