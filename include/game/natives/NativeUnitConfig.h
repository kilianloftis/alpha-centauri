#pragma once

#include "game/effects/EffectConfig.h"
#include "game/effects/TriggeredEffect.h"
#include "game/units/UnitDomain.h"
#include "game/units/UnitPlan.h"

#include <optional>
#include <string>
#include <vector>

namespace ac
{

enum class NativeFaunaRole_t
{
    BoatHull,
    BoatCargo,
    BoatEscort,
    Ambush,
    Swarm,
    SwarmLead,
};

enum class NativeLifecycleKind_t
{
    None,
    TurnFloor,
    AdjacentFungus,
};

struct NativeUnitPlanetConfig_t
{
    std::vector<NativeFaunaRole_t> faunaRoles;
    // Relative weight among candidates for the same role (and domain). Unused when roles empty.
    int faunaWeight = 0;
    // Empty: always eligible when role matches. Locusts list centauri_psi / quantum_power.
    std::vector<std::string> requiresAnyTech;
    NativeLifecycleKind_t lifecycle = NativeLifecycleKind_t::None;
    int lifecycleBonus = 0;
};

struct NativeUnitConfig_t
{
    std::string id;
    std::string name;
    UnitDomain_t domain = UnitDomain_t::Land;
    // As alpha.txt states it for the predefined unit.
    UnitPlan_t plan = UnitPlan_t::Offense;
    int mineralCost = 0;
    std::vector<EffectConfig_t> effects;
    // Considered when a unit of this design is ordered to Hold, and again when a building
    // is completed on its tile. A condition names the place (BaseHasBuilding, TargetTileHas).
    std::vector<TriggeredEffectConfig_t> onHoldEffects;
    // Absent: not Planet fauna / no special wild lifecycle (Alien Artifact).
    std::optional<NativeUnitPlanetConfig_t> planet;
};

NativeFaunaRole_t ParseNativeFaunaRole(const std::string& rRole);
NativeLifecycleKind_t ParseNativeLifecycleKind(const std::string& rKind);

} // namespace ac
