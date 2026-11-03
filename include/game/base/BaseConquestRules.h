#pragma once

#include "game/base/BaseTypes.h"

#include <optional>
#include <vector>

namespace ac
{

class Unit;
class BaseManager;
class Faction;
class Tile;
enum class FactionSpecies_t;
struct BuildingConfig_t;

// Conquest decisions. BaseConquestMutations draws within them and carries them out.

// Owner units that still hold the base against capture.
bool HasBaseGarrison(const BaseManager& rBase);

// Whether this unit may take an undefended base it stands on; CannotCaptureBases is the only
// veto. Entry and garrison are checked elsewhere.
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

// Where rLost's Headquarters may escape to before rLost is taken (capture_base's ESCAPE,
// terranx-map.md "Base transfer and Headquarters"): the owner's other base with the lowest
// HeadquartersEscapeScore, the first on ties. nullopt unless rLost is the Headquarters and the
// owner can afford headquarters_escape's cost. Read while the owner still holds rLost.
std::optional<BaseId_t> HeadquartersEscapeDestination(const BaseManager& rLost);

// Whether rOwner may still pay to move its Headquarters to rDestination: it holds none, owns
// rDestination, and can afford headquarters_escape's cost.
bool MayEvacuateHeadquarters(const Faction& rOwner, const BaseManager& rDestination);

} // namespace ac
