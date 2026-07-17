#pragma once

#include <memory>
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

// String ids matching ImprovementConfig_t::id entries in config/improvements.json,
// used to look up effects/exclusivity for these terrain classifications.
std::string ToString(Rockiness_t rockiness);
std::string ToString(Moisture_t moisture);

// Every terrain feature id Tile::HasFeature can match (all rockiness/moisture tiers plus
// River and Fungus). Together with improvement ids, these are the valid feature references
// for effect conditions/selectors — used by ValidateEffectReferences.
const std::vector<std::string>& AllTerrainFeatureIds();

class Tile
{
public:
    Tile();
    Tile(int x, int y);
    ~Tile();

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

    void SetElevation(int elevation);  // in meters, range -4000 to 4000
    int GetElevation() const;

    // Water threshold shared by landform rendering and territory: elevation < 0 is sea.
    bool IsWater() const;
    bool IsLand() const;

    // Raw energy seed derived purely from elevation. River/Fungus/improvement bonuses are
    // resolved separately via the effects system (see CollectTileEffects/ResolveTileYield)
    // and layered on top of this seed.
    int GetElevationEnergySeed() const;

    // Rivers
    void SetHasRiver(bool bHasRiver);
    bool GetHasRiver() const;

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

    // Terrain-only feature configs: rockiness, moisture, river, fungus. Intrinsic terrain
    // properties (enums/bools) are mirrored here as registry pointers after BindImprovements.
    // Improvements are NOT included — effect collectors iterate GetImprovements() for those.
    const std::vector<const ImprovementConfig_t*>& GetTerrainFeatures() const;

    // Returns true if featureId matches any active feature on this tile: a terrain feature
    // (rockiness/moisture/river/fungus) or an improvement id. Used by conditions/selectors
    // and CanBuildImprovement, which reference features by string id.
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
    bool m_bHasFungus;

    const ImprovementRegistry* m_pImprovements = nullptr;
    std::vector<const ImprovementConfig_t*> m_terrainFeatures;
    std::vector<const ImprovementConfig_t*> m_improvements;
};

} // namespace ac
