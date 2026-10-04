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

// Feature ids Tile::HasFeature resolves from intrinsic tile state (elevation and the terrain
// bools) rather than from an occupant list. Enumerator names ARE the corresponding
// ImprovementConfig_t::id strings - magic_enum maps between the two, so there is no second
// place to keep them in sync - and every one must exist in config/terrain.json, which
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

    // Binds this tile to the occupant registry so terrain enums/bools can be mirrored as
    // non-owning config pointers (see GetTerrainFeatures). Call once after the registry is
    // loaded. Terrain setters refresh the cached configs whenever the registry is bound.
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

    // Improvements placed on this tile, held as non-owning pointers into ImprovementRegistry
    // (the same way BuildingManager holds BuildingConfig_t*): player-built improvements
    // (Farm, Mine, Bunker) and the "Base" marker added when a base is founded here (see
    // BaseManager). Configs are resolved by the caller (the registry funnel is
    // TileEffectsContext); Tile never looks them up itself.
    void AddImprovement(const ImprovementConfig_t& rConfig);
    void RemoveImprovement(std::string_view improvementId);
    bool HasImprovement(std::string_view improvementId) const;
    const std::vector<const ImprovementConfig_t*>& GetImprovements() const;

    // Optional terrain occupants (fungus, landmarks, resource bonuses, Monolith). Axes,
    // rivers, and aquifers stay on their own fields and are mirrored alongside these.
    void AddTerrainFeature(const ImprovementConfig_t& rConfig);
    void RemoveTerrainFeature(std::string_view featureId);
    bool HasTerrainFeature(std::string_view featureId) const;

    // Routes an occupant to the list its ImprovementConfig_t::placement names. Callers that
    // hold a config use these rather than each re-deriving which of the two lists it lives in.
    void AddOccupant(const ImprovementConfig_t& rConfig);
    void RemoveOccupant(const ImprovementConfig_t& rConfig);

    // Coexistence waivers earned at construction: (improvement id, occupant id) pairs a
    // CoexistenceOverride allowed past a stock exclude. They live on the tile, not on the
    // former, so a later occupancy sweep does not evict what was legitimately built. Dropped
    // with the improvement they belong to (RemoveImprovement) and with the occupant they name
    // (any terrain change), so a licence never outlives the pair it was granted for.
    void AddCoexistenceWaiver(const std::string& rImprovementId, const std::string& rFeatureId);
    const std::vector<std::pair<std::string, std::string>>& GetCoexistenceWaivers() const;

    // Terrain configs: rockiness, moisture, every active TerrainFeature_t, then optional
    // terrain. Intrinsic properties are mirrored as registry pointers after BindOccupants,
    // ordered general-to-specific (Water before its depth band).
    // Improvements are NOT included — effect collectors iterate GetImprovements().
    const std::vector<const ImprovementConfig_t*>& GetTerrainFeatures() const;

    // Every occupant on this tile: terrain configs first (general-to-specific), then
    // improvements. fn takes a const ImprovementConfig_t& and returns true to stop the walk;
    // ForEachOccupant returns whether it stopped, so a search reads as a predicate and a full
    // sweep simply never returns true. Null entries are skipped.
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

    // Returns true if featureId matches any active feature on this tile: a rockiness/moisture
    // name, an active TerrainFeature_t, or an improvement id. Used by conditions/selectors and
    // CanBuildImprovement, which reference features by string id.
    bool HasFeature(std::string_view featureId) const;

private:
    friend class TileChangeDeferral;

    void RefreshTerrainFeatures_();
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
