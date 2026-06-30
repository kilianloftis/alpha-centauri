#pragma once

#include <memory>
#include <string>
#include <vector>

namespace ac
{

enum class Rockiness
{
    Flat,
    Rolling,
    Rocky
};

enum class Moisture
{
    Arid,
    Moist,
    Wet
};

// String ids matching ImprovementConfig_t::id entries in config/improvements.json,
// used to look up effects/exclusivity for these terrain classifications.
std::string ToString(Rockiness rockiness);
std::string ToString(Moisture moisture);

class Tile
{
public:
    Tile();
    Tile(int x, int y);
    ~Tile();

    int GetX() const;
    int GetY() const;

    // Terrain characteristics. GetMoisture()/SetMoisture() are the CURRENT/effective value -
    // what rendering and GetFeatureIds() see, and what a Condenser's MoistureTier effect
    // mutates via RecomputeMoisture(). GetBaseMoisture()/SetBaseMoisture() are the natural,
    // un-condensed terrain truth set once by world generation; RecomputeMoisture always
    // re-derives the current value from the base plus whatever Condensers currently reach
    // this tile, so the bonus disappears cleanly the moment a Condenser is removed - never
    // mutated incrementally, to avoid drift from overlapping Condensers or add/remove order.
    void SetMoisture(Moisture moisture);
    Moisture GetMoisture() const;

    void SetBaseMoisture(Moisture moisture);
    Moisture GetBaseMoisture() const;

    void SetRockiness(Rockiness rockiness);
    Rockiness GetRockiness() const;

    void SetElevation(int elevation);  // in meters, range -4000 to 4000
    int GetElevation() const;

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

    // Landmarks
    void SetLandmark(const std::string& landmarkId);
    void RemoveLandmark();
    bool HasLandmark() const;
    const std::string& GetLandmark() const;

    // Improvements (player-built, e.g. "Farm", "Mine", "Bunker" - also gains "Base" when a
    // base is founded here, see BaseManager)
    void AddImprovement(const std::string& improvementId);
    void RemoveImprovement(const std::string& improvementId);
    bool HasImprovement(const std::string& improvementId) const;
    const std::vector<std::string>& GetImprovements() const;

    // Bonus
    void SetBonus(const std::string& bonusId);
    void RemoveBonus();
    bool HasBonus() const;
    const std::string& GetBonus() const;

    // Worker assignment (one worker per tile, tracked by base ID)
    void AssignWorker(int baseId);
    void UnassignWorker();
    bool IsWorkerAssigned() const;
    int GetWorkedByBaseId() const;

    // Worked flag (set when a Pop is actively assigned to this tile).
    // Declared const because it is updated through a const Tile* held by Pop.
    void SetWorked(bool bWorked) const;
    bool IsWorked() const;

    // Every feature id active on this tile: rockiness, moisture, river, fungus, landmark,
    // and improvements. Used by CollectTileEffects/CanBuildImprovement to look entries up
    // in the ImprovementRegistry - rockiness/moisture/river/fungus are properties of the
    // terrain itself, but for effects/exclusivity purposes they're looked up the exact same
    // way as a player-built improvement.
    std::vector<std::string> GetFeatureIds() const;

private:
    int m_x;
    int m_y;

    Moisture m_moisture;
    Moisture m_baseMoisture;
    Rockiness m_rockiness;
    int m_elevation;

    bool m_bHasRiver;
    bool m_bHasFungus;

    std::string m_landmark;
    std::vector<std::string> m_improvements;
    std::string m_bonus;

    mutable bool m_bWorked;  // true when a Pop is assigned to this tile
    int m_workedByBaseId;  // -1 if unworked
};

} // namespace ac
