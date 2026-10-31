#include "game/combat/CaptureRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/combat/CombatConfig.h"
#include "game/diplomacy/DiplomacyConfig.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Faction.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/movement/TransportRules.h"
#include "game/natives/NativeRules.h"
#include "game/population/PopulationManager.h"
#include "game/units/Unit.h"

#include <cmath>

namespace ac
{

namespace
{

// The context CombatStrength resolves the attacker under.
EffectContext_t AttackerContext_(const Unit& rAttacker, const Unit& rDefender)
{
    EffectContext_t ctx{&rDefender.GetTile(), CombatRole_t::Attacker};
    ctx.pAttacker = &rAttacker;
    ctx.pDefender = &rDefender;
    return ctx;
}

bool HoldsHomeBasePopulation_(const BaseManager& rBase)
{
    const CombatConfig_t& rConfig = *CurrentGameState().GetGameData().combatConfig;
    return rBase.GetPopulation().GetSize() >= rConfig.boardingHomeBaseMinPop;
}

// The defeated unit can be taken, and this attacker is one that takes it.
bool MayTakeOnDefeat_(const Unit& rAttacker, const Unit& rDefeated)
{
    return ResolveFlag(rDefeated, RuleFlagId_t::CapturedOnDefeat) && !IsWildNative(rAttacker)
           && !ResolveFlag(rAttacker, RuleFlagId_t::SingleUse);
}

std::optional<BaseId_t> BoardingHomeBase_(FactionId_t captorId, const Tile& rTile)
{
    const BaseManager* pBase = CurrentGameState().FindNearestBase(rTile);
    if (pBase == nullptr || pBase->GetFactionId() != captorId || !HoldsHomeBasePopulation_(*pBase))
    {
        return std::nullopt;
    }
    return pBase->GetBaseId();
}

} // namespace

std::optional<int> BoardingThreshold(const Unit& rAttacker, const Unit& rDefender)
{
    if (BoardingChance(rAttacker, rDefender) <= 0)
    {
        return std::nullopt;
    }
    const EffectContext_t ctx = AttackerContext_(rAttacker, rDefender);
    const UnitEffects_t effects = CollectLiveUnitEffects(rAttacker);
    const StatId_t stat = StatId_t::BoardHpLimit;
    const double limit = ResolveUnitStat(effects, stat, SeedFor(stat), &ctx);
    const double hpToLose = static_cast<double>(rDefender.GetCurrentHp()) * limit;
    const int threshold = static_cast<int>(std::floor(hpToLose));
    if (threshold <= 0)
    {
        return std::nullopt;
    }
    return threshold;
}

int BoardingChance(const Unit& rAttacker, const Unit& rDefender)
{
    const EffectContext_t ctx = AttackerContext_(rAttacker, rDefender);
    return ResolveStat(rAttacker, StatId_t::BoardChance, ctx);
}

std::vector<CapturedUnit_t> BoardingCapture(const Unit& rAttacker, const Tile& rTile)
{
    const WorldMap& rWorldMap = CurrentGameState().GetWorldMap();
    const FactionId_t captorId = rAttacker.GetFaction().GetFactionId();
    const std::optional<BaseId_t> homeBaseId = BoardingHomeBase_(captorId, rTile);
    std::vector<CapturedUnit_t> captured;
    // Occupants before cargo, so each carrier changes hands before its passengers are re-homed.
    for (Unit* pUnit : rWorldMap.GetUnitsOnTile(rTile))
    {
        captured.push_back(CapturedUnit_t{.pUnit = pUnit, .homeBaseId = homeBaseId});
    }
    for (Unit* pUnit : rWorldMap.GetCargoOnTile(rTile))
    {
        captured.push_back(CapturedUnit_t{.pUnit = pUnit, .homeBaseId = homeBaseId});
    }
    return captured;
}

std::optional<CapturedUnit_t> DefeatedUnitCapture(const Unit& rAttacker, Unit& rDefeated)
{
    if (!MayTakeOnDefeat_(rAttacker, rDefeated))
    {
        return std::nullopt;
    }
    const Tile& rTile = rAttacker.GetTile();
    Unit* pCarrier = nullptr;
    if (rTile.IsWater())
    {
        const FactionId_t captorId = rAttacker.GetFaction().GetFactionId();
        pCarrier = FindCarrierWithRoom(rTile, rDefeated.GetDomain(), captorId);
        if (pCarrier == nullptr)
        {
            return std::nullopt;
        }
    }
    return CapturedUnit_t{.pUnit = &rDefeated,
                          .pDestination = &rTile,
                          .pCarrier = pCarrier,
                          .bRestoresHp = true};
}

CapturedUnit_t SubvertCapture(const Faction& rActor, Unit& rTarget)
{
    CapturedUnit_t captured{.pUnit = &rTarget, .moves = CapturedMoves_t::Refilled};
    const BaseManager* pHomeBase = rActor.FindNearestBase(rTarget.GetTile());
    if (pHomeBase != nullptr)
    {
        captured.homeBaseId = pHomeBase->GetBaseId();
    }
    return captured;
}

int ArtifactCaptureFriction(FactionId_t captor, FactionId_t victim)
{
    // TODO: SMAC also marks the victim as wanting to talk; there is no such state yet.
    const GameState& rGame = CurrentGameState();
    if (rGame.RequireFaction(victim).IsPlayerControlled()
        || rGame.GetDiplomacyLedger().HasVendetta(captor, victim))
    {
        return 0;
    }
    return rGame.GetGameData().diplomacyConfig->artifactCaptureFriction;
}

} // namespace ac
