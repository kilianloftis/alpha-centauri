#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ac
{

struct ImprovementConfig_t;
class ImprovementRegistry;

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
    Aquifer,
    Fungus
};

// String ids matching ImprovementConfig_t::id entries in config/improvements.json,
// used to look up effects/exclusivity for these terrain classifications.
std::string ToString(Rockiness_t rockiness);
std::string ToString(Moisture_t moisture);

// SMAC ocean-shelf / "one above sea" mapped onto meter elevations: deep ocean below this
// cannot host kelp / sea terraform; shallower water is OceanShelf.
inline constexpr int k_OceanShelfMinElevation = -2000;

// Elevation is stored in meters and clamped to Planet's range by SetElevation. World-gen preset
// minElevation/maxElevation must stay inside this.
inline constexpr int k_MinElevation = -4000;
inline constexpr int k_MaxElevation = 4000;

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

    void SetElevation(int elevation);  // in meters; throws outside [k_MinElevation, k_MaxElevation]
    int GetElevation() const;

    // Water threshold shared by landform rendering and territory: elevation < 0 is sea.
    bool IsWater() const;
    bool IsLand() const;

    // Rivers (derived from aquifer downhill flow; rebuilt by RecomputeRivers)
    void SetHasRiver(bool bHasRiver);
    bool GetHasRiver() const;

    // Aquifers (persistent river sources; placed by world-gen / Former Aquifer)
    void SetHasAquifer(bool bHasAquifer);
    bool GetHasAquifer() const;

    // Fungus (alien vegetation; presence-only for now, spreading is a future enhancement)
    void SetHasFungus(bool bHasFungus);
    bool GetHasFungus() const;

    // Binds this tile to the improvement registry so terrain enums/bools can be mirrored as
    // non-owning ImprovementConfig_t pointers (see GetTerrainFeatures). Call once after the
    // registry is loaded — TileEffectsContext does this for every map tile. Terrain setters
    // refresh the cached configs whenever the registry is bound.
    void BindImprovements(const ImprovementRegistry& rImprovements);

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
    void RefreshTerrainFeatures_();

    int m_x;
    int m_y;

    Moisture_t m_moisture;
    Moisture_t m_baseMoisture;
    Rockiness_t m_rockiness;
    int m_elevation;

    bool m_bHasRiver;
    bool m_bHasAquifer;
    bool m_bHasFungus;

    const ImprovementRegistry* m_pImprovements = nullptr;
    std::vector<const ImprovementConfig_t*> m_terrainFeatures;
    std::vector<const ImprovementConfig_t*> m_improvements;
};

} // namespace ac
