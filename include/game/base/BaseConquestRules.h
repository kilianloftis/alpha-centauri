#pragma once

#include <optional>
#include <vector>

namespace ac
{

class Unit;
class BaseManager;
class Tile;
enum class FactionSpecies_t;
struct BuildingConfig_t;

// Conquest decisions. BaseConquestMutations draws within them and carries them out.

// Owner units that still hold the base against capture.
bool HasBaseGarrison(const BaseManager& rBase);

// Capturer may take an undefended base once on its tile. Domain / channel-crossing
// entry is gated by TryStep (CanEnterTile) and the garrison by HasBaseGarrison, so this
// asks only about the unit: CannotCaptureBases is the sole veto (Needlejet / Missile
// chassis, noncombat weapon modules).
bool CanCaptureBase(const Unit& rCapturer);

// Human ↔ Progenitor conquest uses the harsher population rule.
bool IsCrossSpeciesConquest(FactionSpecies_t attacker, FactionSpecies_t defender);

enum class ConquestKind_t
{
    Capture,
    NativeRaid,
    LastDefenderCasualty,
};

// Stepping onto an undefended foreign base: native life raids it, a unit that may capture
// takes it.
std::optional<ConquestKind_t> ConquestOnEntry(const Unit& rMover, const BaseManager& rBase);

// Killing an undefended base's last defender always costs LastDefenderCasualty; NativeRaid
// means native life then raids it.
std::optional<ConquestKind_t> ConquestOnLastDefenderKill(const Unit& rAttacker,
                                                         const BaseManager& rBase);

// Pops lost with the last defender (the Additive last_defender_pop_loss).
int LastDefenderPopLoss(const BaseManager& rBase);

struct CaptureOutcome_t
{
    std::vector<const BuildingConfig_t*> facilityCandidates;
    // How many candidates are destroyed: a uniform draw in [min, max].
    int minFacilitiesDestroyed = 0;
    int maxFacilitiesDestroyed = 0;
    int popLoss = 0;
    // A cross-species capture keeps one pop; half the displaced colonists flee as escape pods.
    int escapePods = 0;
    bool bRepairsCapturer = false;
};

// Read before anything is destroyed, so a ThisBase clamp still counts even if the building
// granting it is then demolished.
CaptureOutcome_t PlanCapture(const Unit& rCapturer, const BaseManager& rBase);

struct NativeRaidPlan_t
{
    std::vector<const BuildingConfig_t*> facilityCandidates;
    // Chance to destroy one candidate; otherwise the raid takes popLoss.
    int facilityChancePercent = 0;
    int popLoss = 0;
};

NativeRaidPlan_t PlanNativeRaid(const BaseManager& rBase);

} // namespace ac
