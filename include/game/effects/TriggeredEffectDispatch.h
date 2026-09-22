#pragma once

#include "game/buildings/BuildingConfig.h"
#include "game/effects/ActiveEffect.h"
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

struct XpGranted_t
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

using TriggeredEffectResult_t = std::variant<
    FacilitiesDestroyed_t,
    PopulationChanged_t,
    BuildingAdded_t,
    TechGranted_t,
    UnitsGranted_t,
    EnergyGranted_t,
    XpGranted_t,
    HitPointsRestored_t,
    BaseRebelled_t,
    InfiltrationSet_t
>;

// Trigger-only fields plus non-const subjects for mutate arms. Conditions and amount sources
// read through Subjects(), which materialises a const EffectContext_t bag — the same shape
// continuous resolve uses — without sharing a const_cast seam.
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
    std::optional<FactionId_t> actionTarget;
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

// Fire every entry in order against rContext, returning what each one did (entries that report
// nothing contribute no result). Entries carrying `oncePer` are skipped when their subject has
// already consumed that key, and record it when they fire — so a list applies partially and
// honestly rather than all-or-nothing. Entries with a `condition` that fails against
// Subjects() (after pFaction is stamped for the current apply) are skipped without spending
// oncePer.
std::vector<TriggeredEffectResult_t>
ApplyTriggeredEffects(std::span<const TriggeredEffectConfig_t> rEffects,
                      TriggeredEffectContext_t& rContext);

// Production-base train / prototype XP and other on_unit_produced_effects: production.json
// first, then each building present at rProducedAt. Stamps pUnit. No-op when the faction has
// no bound GameState (pre-session construction).
void ApplyUnitProducedTriggers(Unit& rUnit, BaseManager& rProducedAt);

// Improvement on_visit_effects for each visit-capable improvement on the mover's tile.
// Stamps pUnit / pTile / hostImprovementId per list. Investigate (or AI auto) only.
void ApplyVisitEffects(Unit& rMover, std::mt19937& rRng);

// True when any improvement on the tile carries a non-empty on_visit_effects list.
bool TileHasVisitEffects(const Tile& rTile);

// Tech on_discover_effects for rTechId. No-op when the faction has no bound GameState.
void ApplyTechDiscoverEffects(Faction& rFaction, const TechId& rTechId);

} // namespace ac
