#pragma once

namespace ac
{

class LuaRuntime;
struct NativeLifeConfig_t;

// Evaluates config/native_life.lua fauna formulas. Plain values in; no game reads.
class PlanetFaunaCalculator
{
public:
    PlanetFaunaCalculator(const NativeLifeConfig_t& rConfig, LuaRuntime& rLua);

    int BoatPeriod(int faunaForms) const;
    int EarlyLimitThreshold(int faunaForms, int faunaDiff) const;
    // cargoScale is resolved planet_fauna_cargo_scale (1 normally, 2 under Perihelion).
    int BoatCargo(int turnNumber, int faunaForms, int faunaDiff, double cargoScale) const;
    int AmbushAttempts(int faunaForms) const;
    int SwarmYearThreshold(int faunaForms) const;
    // Turn → base lifecycle index from planet_lifecycle_turn_thresholds (0 entry skips that index).
    int LifecycleFloor(int turnNumber) const;

private:
    const NativeLifeConfig_t& m_rConfig;
    LuaRuntime& m_rLua;
};

} // namespace ac
