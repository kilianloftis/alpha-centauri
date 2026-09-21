#pragma once

#include "game/buildings/BuildingConfig.h"
#include "game/effects/TriggeredEffect.h"
#include "game/faction/base/BaseTypes.h"
#include "game/research/TechConfigParser.h"

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

// What a triggered effect actually did. The dispatcher returns these so callers can report —
// probe missions need the destroyed facility's id for their result detail, and riot tiers need
// the same to tell the player what rioting cost them. An entry that fired but had nothing to do
// (no eligible facility, no placeable tile) still reports, with an empty list or a zero count.

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

struct BaseRebelled_t
{
    BaseId_t baseId = 0;
    FactionId_t newOwner = 0;
};

struct InfiltrationSet_t
{
    std::vector<FactionId_t> targets;
};

using TriggeredEffectResult_t = std::variant<
    FacilitiesDestroyed_t,
    PopulationChanged_t,
    BuildingAdded_t,
    TechGranted_t,
    UnitsGranted_t,
    EnergyGranted_t,
    BaseRebelled_t,
    InfiltrationSet_t
>;

// Everything a triggered effect may need to resolve itself. Fields are optional because the
// trigger decides what it has: a production completion has a base; a council vote has a faction
// list and no base at all; a monolith visit has a unit and a tile and no base. An effect that
// needs a subject the context lacks is skipped rather than guessed at.
struct TriggeredEffectContext_t
{
    TriggeredEffectContext_t(GameState& rGameStateIn, std::vector<Faction*> factionsIn)
        : rGameState(rGameStateIn)
        , factions(std::move(factionsIn))
    {
    }
    // Convenience for the common single-faction trigger.
    TriggeredEffectContext_t(GameState& rGameStateIn, Faction& rFaction);
    // The base the trigger fired against; also supplies the faction.
    TriggeredEffectContext_t(GameState& rGameStateIn, BaseManager& rBase);

    GameState& rGameState;
    // Factions the effects apply to. Never empty.
    std::vector<Faction*> factions;
    // The base the trigger fired against, when it had one.
    BaseManager* pBase = nullptr;
    // The unit the trigger fired for, when it had one. Set even where pBase is — a trigger may
    // name both — so an effect that targets a unit never has to guess from tile occupancy.
    Unit* pUnit = nullptr;
    // Location anchor when there is no base (probe target tile, monolith tile).
    const Tile* pTile = nullptr;
    // Probe mission target, for a factionFilter of kind ActionTarget.
    std::optional<FactionId_t> actionTarget;
    // Generator for the effects that roll (DestroyFacility, Rebel). Null = the session's own,
    // which is what a turn-stage trigger wants; a caller driving its own sequence (a probe
    // mission, a seeded test) sets this so the roll stays reproducible from its seed.
    std::mt19937* pRng = nullptr;

    std::mt19937& Rng() const;
};

// Fire every entry in order against rContext, returning what each one did (entries that report
// nothing contribute no result). Entries carrying `oncePer` are skipped when their subject has
// already consumed that key, and record it when they fire — so a list applies partially and
// honestly rather than all-or-nothing.
std::vector<TriggeredEffectResult_t>
ApplyTriggeredEffects(std::span<const TriggeredEffectConfig_t> rEffects,
                      TriggeredEffectContext_t& rContext);

} // namespace ac
