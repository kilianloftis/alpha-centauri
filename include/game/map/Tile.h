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

class Tile
{
public:
    Tile();
    Tile(int x, int y);
    ~Tile();

    // Position
    void SetPosition(int x, int y);
    int GetX() const;
    int GetY() const;

    // Terrain characteristics
    void SetMoisture(Moisture moisture);
    Moisture GetMoisture() const;

    void SetRockiness(Rockiness rockiness);
    Rockiness GetRockiness() const;

    void SetElevation(int elevation);  // in meters, range -4000 to 4000
    int GetElevation() const;

    // Resource production calculated from terrain
    int GetNutrientProduction() const;
    int GetMineralProduction() const;
    int GetEnergyProduction() const;

    // Rivers
    void SetHasRiver(bool bHasRiver);
    bool GetHasRiver() const;

    // Landmarks
    void SetLandmark(const std::string& landmarkId);
    void RemoveLandmark();
    bool HasLandmark() const;
    const std::string& GetLandmark() const;

    // Improvements
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

private:
    int m_x;
    int m_y;

    Moisture m_moisture;
    Rockiness m_rockiness;
    int m_elevation;

    bool m_bHasRiver;

    std::string m_landmark;
    std::vector<std::string> m_improvements;
    std::string m_bonus;

    int m_workedByBaseId;  // -1 if unworked

    int CalculateBonusNutrients_() const;
    int CalculateBonusMinerals_() const;
    int CalculateBonusEnergy_() const;

    int CalculateNutrients_() const;
    int CalculateMinerals_() const;
    int CalculateEnergy_() const;
};

using TilePtr_t = std::shared_ptr<Tile>;

} // namespace ac
