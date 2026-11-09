#pragma once

#include "game/atrocities/AtrocityMutations.h"
#include "game/buildings/BuildingConfig.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/TriggeredEffect.h"
#include "game/base/BaseTypes.h"
#include "game/research/TechConfigParser.h"
#include "game/units/Unit.h"

#include <optional>
#include <random>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace ac
{

class BaseManager;
class Faction;
class GameState;
class Tile;
class Unit;

// What a triggered effect did, for callers that report it. An entry with nothing to do still
// reports, empty.

struct FacilitiesDestroyed_t
{
    std::vector<BuildingId_t> buildingIds;
};

struct PopulationChanged_t
{
    int delta = 0;
};

struct BuildingAdded_t
{
    BuildingId_t buildingId;
};

struct TechGranted_t
{
    TechId techId;
};

struct UnitsGranted_t
{
    int count = 0;
    std::string designId;
};

struct EnergyGranted_t
{
    int amount = 0;
};

struct ResourceGranted_t
{
    StatId_t resource = StatId_t::Minerals;
    int amount = 0;
};

struct XpGranted_t
{
    int amount = 0;
};

struct MindControlRecorded_t
{
    int weight = 0;
};

struct CleanMineralsGranted_t
{
    int amount = 0;
};

struct VirtualMineralsAdded_t
{
    int amount = 0;
};

struct HitPointsRestored_t
{
    int amount = 0;
};

struct BaseRebelled_t
{
    BaseId_t baseId = 0;
    FactionId_t newOwner = 0;
};

struct InfiltrationSet_t
{
    std::vector<FactionId_t> targets;
};

struct UnitDestroyed_t
{
    UnitId_t unitId = 0;
};

struct EarthquakeApplied_t
{
    int levels = 0;
};

struct FungalBloomApplied_t
{
    int tiles = 0;
    int lifeforms = 0;
};

struct ExplosionApplied_t
{
    int radius = 0;
    int tiles = 0;
};

struct TerrainFeatureChanged_t
{
    std::string featureId;
    bool bPresent = true;
};

struct RockinessChanged_t
{
    int steps = 0;
};

struct ElevationChanged_t
{
    int deltaMeters = 0;
};

using TriggeredEffectResult_t = std::variant<
    FacilitiesDestroyed_t,
    PopulationChanged_t,
    BuildingAdded_t,
    TechGranted_t,
    UnitsGranted_t,
    EnergyGranted_t,
    ResourceGranted_t,
    XpGranted_t,
    HitPointsRestored_t,
    BaseRebelled_t,
    InfiltrationSet_t,
    UnitDestroyed_t,
    EarthquakeApplied_t,
    FungalBloomApplied_t,
    ExplosionApplied_t,
    TerrainFeatureChanged_t,
    RockinessChanged_t,
    ElevationChanged_t,
    AtrocityCommitted_t,
    MindControlRecorded_t,
    CleanMineralsGranted_t,
    VirtualMineralsAdded_t
>;

// Trigger fields and mutable subjects; Subjects() gives conditions the same const context
// continuous resolve uses.
struct TriggeredEffectContext_t
{
    TriggeredEffectContext_t(GameState& rGameStateIn, std::vector<Faction*> factionsIn)
        : rGameState(rGameStateIn)
        , factions(std::move(factionsIn))
    {
    }
    TriggeredEffectContext_t(GameState& rGameStateIn, Faction& rFaction);
    TriggeredEffectContext_t(GameState& rGameStateIn, BaseManager& rBase);

    GameState& rGameState;
    std::vector<Faction*> factions;
    BaseManager* pBase = nullptr;
    Unit* pUnit = nullptr;
    Faction* pFaction = nullptr;
    Tile* pTile = nullptr;
    // Visit-list host: GrantXp remove_host_chance removes this improvement from pTile.
    std::optional<std::string> hostImprovementId;
    // Set by the trigger site and read-only to the arms: the probed base's owner, and so on.
    std::optional<FactionId_t> actionTarget;
    // Set by an arm that works out who it wronged (Explosion), for CommitAtrocity. Separate so it
    // cannot redirect the ActionTarget filter.
    std::optional<FactionId_t> derivedVictim;
    // Set by a production completion: no ModifyPopulation shrinks the base below it.
    int populationFloor = 0;
    std::mt19937* pRng = nullptr;

    std::mt19937& Rng() const;

    EffectContext_t Subjects() const
    {
        EffectContext_t ctx;
        ctx.targetTile = pTile;
        ctx.pBase = pBase;
        ctx.pFaction = pFaction;
        ctx.pUnit = pUnit;
        return ctx;
    }
};

// Fires each entry in order and returns what each did. oncePer and condition skip entries
// without spending the key, so a list can apply in part.
std::vector<TriggeredEffectResult_t>
ApplyTriggeredEffects(std::span<const TriggeredEffectConfig_t> rEffects,
                      TriggeredEffectContext_t& rContext);

// Production-base train / prototype XP and other on_unit_produced_effects: production.json
// first, then each building present at rProducedAt. Stamps pUnit.
void ApplyUnitProducedTriggers(GameState& rGameState, Unit& rUnit, BaseManager& rProducedAt);

// Improvement on_visit_effects for each visit-capable improvement on the mover's tile.
// Stamps pUnit / pTile / hostImprovementId per list. Investigate (or AI auto) only.
void ApplyVisitEffects(GameState& rGameState, Unit& rMover, std::mt19937& rRng);

// Tech on_discover_effects for rTechId.
void ApplyTechDiscoverEffects(GameState& rGameState, Faction& rFaction, const TechId& rTechId);

// Run the design's on_hold_effects. A DestroyUnit entry in that list removes the subject.
void ApplyHoldLink(GameState& rGameState, Unit& rUnit);

// Runs on_detonate_effects on the unit's own tile, for a missile that cannot attack. False
// without a detonation.
bool ApplyDetonation(GameState& rGameState, Unit& rUnit);

} // namespace ac
