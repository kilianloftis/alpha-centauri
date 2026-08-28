#pragma once

#include "game/population/pop-types/PopCompositionConfigParser.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace ac
{

class LuaRuntime;

// One input struct per drone source rather than one wide struct shared by all of them: each
// term is evaluated on its own formula, so a struct that carried every field would hand each
// calculator two thirds of it as noise.

// Residue class past the bureaucracy limit — the empire-size penalty.
struct BureaucracyDroneInputs_t
{
    // Resolved PureMultiplier Bureaucracy (Citizen + Efficiency 0 → 32). May be fractional.
    double bureaucracy = 1.0;
    int mapWidth = 0;
    int mapHeight = 0;
    int baseId = 0;
    int factionBaseCount = 0;
};

// Every pop past SizeFreeDrones is a size drone.
struct SizeDroneInputs_t
{
    int baseSize = 0;
    // Resolved SizeFreeDrones: pops at or below this are free of size drones.
    int sizeFreeDrones = 0;
};

// Decaying unrest at a recently conquered base.
struct OccupationDroneInputs_t
{
    int baseSize = 0;
    int turnsSinceConquered = 0;
    // 0 duration / peak means the base is not assimilating (never captured, or the window ended).
    int assimilationDuration = 0;
    int assimilationPeak = 0;
    // Resolved ConqueredDroneCap (difficulty 0.25×level + base_conquest −0.5). Kept as a
    // double so the 0.25 steps survive until Lua takes floor(base_size/4 + this).
    double conqueredDroneCap = 0.0;
};

uint64_t StableBaseHash(int baseId);

// Computes the drone-pressure terms that need real math. Each result is a contribution to
// StatId_t::Drones, which BaseManager sums and seeds ResolveBaseStat with — facilities and SE
// then Add on top of it through the ordinary effect pipeline. There is no combined drone
// formula: "how much pressure is there" is a resolved stat, not a Lua expression.
class DroneCalculator
{
public:
    DroneCalculator(const PopCompositionConfig_t& rConfig, LuaRuntime& rLua);
    ~DroneCalculator() = default;

    int CalculateBureaucracyLimit(const BureaucracyDroneInputs_t& rInputs) const;
    int CalculateBureaucracyDrones(const BureaucracyDroneInputs_t& rInputs) const;
    int CalculateSizeDrones(const SizeDroneInputs_t& rInputs) const;
    int CalculateOccupationDrones(const OccupationDroneInputs_t& rInputs) const;

private:
    int EvalNonNegative_(const std::string& rFormula, const char* pTermName,
                         const std::unordered_map<std::string, double>& rVars) const;

    const PopCompositionConfig_t& m_rConfig;
    LuaRuntime& m_rLua;
};

} // namespace ac
