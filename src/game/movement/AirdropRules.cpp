#include "game/movement/AirdropRules.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/faction/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/movement/MovementConstants.h"
#include "game/movement/MovementRules.h"
#include "game/units/Unit.h"

#include <algorithm>

namespace ac
{

namespace
{

bool UnitHasFullMoves_(const Unit& rUnit)
{
    const int full =
        rUnit.GetMovementPoints() * MovementConstants_t::k_moveFragmentsPerPoint;
    return rUnit.GetMoveFragmentsRemaining() >= full;
}

} // namespace

bool TileIsAirdropLaunchPad(const Tile& rTile, int factionId)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    return TileProvidesFlag(rTile, RuleFlagId_t::AirdropLaunch, rWorldMap, factionId);
}

bool OccupantBlocksAirdrop(const Unit& rDropper, const Unit& rOccupant)
{
    if (rOccupant.GetFaction().GetFactionId() == rDropper.GetFaction().GetFactionId())
    {
        return false;
    }
    if (rOccupant.IsEmbarked() && !rOccupant.GetTile().HasImprovement(ImprovementIds::k_Base))
    {
        return false;
    }
    return true;
}

AirdropLanding_t AirdropLanding(const Unit& rUnit, const Tile& rDest)
{
    AirdropLanding_t landing;
    landing.hpAfter = rUnit.GetCurrentHp();
    landing.bEndsTurn = !rUnit.IsCombatUnit();
    if (TileIsAirdropLaunchPad(rDest, rUnit.GetFaction().GetFactionId()))
    {
        return landing;
    }
    const int percent = ResolveStat(rUnit, StatId_t::AirdropLandingDamage);
    if (percent <= 0)
    {
        return landing;
    }
    const int maxHp = ResolveStat(rUnit, StatId_t::HitPoints);
    const int damage = maxHp * percent / 100;
    if (damage > 0)
    {
        landing.hpAfter = std::max(1, landing.hpAfter - damage);
    }
    return landing;
}

bool IsAirdropInterdicted(const Unit& rDropper, const Tile& rDest)
{
    const GameState& rGame = CurrentGameState();
    const TileEffectsContext& rTileEffects = rGame.GetTileEffects();
    const FactionId_t dropperId = rDropper.GetFaction().GetFactionId();
    for (const ActiveEffect_t& rEffect : rTileEffects.CollectAreaEffects(rDest))
    {
        // Fail closed without attribution — same pattern as Detect. Friendly auras do not deny.
        if (!rEffect.ownerFaction.has_value() || *rEffect.ownerFaction == dropperId)
        {
            continue;
        }
        const RuleFlagEffect_t* pFlag =
            std::get_if<RuleFlagEffect_t>(&rEffect.config->effect);
        if (pFlag && pFlag->flag == RuleFlagId_t::AirdropInterdiction)
        {
            return true;
        }
    }
    return false;
}

AirdropEligibility_t CanAttemptAirdrop(const Unit& rUnit)
{
    AirdropEligibility_t result;
    if (!ResolveFlag(rUnit, RuleFlagId_t::Airdrop))
    {
        result.failReason = AirdropFailReason_t::NotCapable;
        return result;
    }
    if (rUnit.HasAirdroppedThisTurn())
    {
        result.failReason = AirdropFailReason_t::AlreadyAirdropped;
        return result;
    }
    const FactionId_t factionId = rUnit.GetFaction().GetFactionId();
    if (!TileIsAirdropLaunchPad(rUnit.GetTile(), factionId))
    {
        result.failReason = AirdropFailReason_t::NotOnLaunchPad;
        return result;
    }
    if (!UnitHasFullMoves_(rUnit))
    {
        result.failReason = AirdropFailReason_t::NoMovesRemaining;
        return result;
    }
    return result;
}

AirdropEligibility_t CanAirdropTo(const Unit& rUnit, const Tile& rDest)
{
    AirdropEligibility_t result = CanAttemptAirdrop(rUnit);
    if (!result.Ok())
    {
        return result;
    }

    if (&rUnit.GetTile() == &rDest)
    {
        result.failReason = AirdropFailReason_t::CannotEnter;
        return result;
    }

    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    const Faction& rFaction = rUnit.GetFaction();
    const bool bOrbital = ResolveFlag(rFaction, RuleFlagId_t::OrbitalInsertion);
    if (!bOrbital)
    {
        const int range = ResolveStat(rUnit, StatId_t::AirdropRange);
        if (range <= 0
            || ChebyshevDistance(rUnit.GetTile(), rDest, rWorldMap.GetWidth()) > range)
        {
            result.failReason = AirdropFailReason_t::OutOfRange;
            return result;
        }
    }

    if (!CanEnterTile(rUnit, rDest))
    {
        result.failReason = AirdropFailReason_t::CannotEnter;
        return result;
    }

    for (Unit* pUnit : rWorldMap.GetUnitsOnTile(rDest))
    {
        if (pUnit && OccupantBlocksAirdrop(rUnit, *pUnit))
        {
            result.failReason = AirdropFailReason_t::EnemyOccupied;
            return result;
        }
    }
    if (rDest.HasImprovement(ImprovementIds::k_Base))
    {
        for (Unit* pCargo : rWorldMap.GetCargoOnTile(rDest))
        {
            if (pCargo && OccupantBlocksAirdrop(rUnit, *pCargo))
            {
                result.failReason = AirdropFailReason_t::EnemyOccupied;
                return result;
            }
        }
    }

    if (rWorldMap.GetUnitPositions().IsSingleUnitPerTile())
    {
        for (Unit* pUnit : rWorldMap.GetUnitsOnTile(rDest))
        {
            if (pUnit && pUnit->GetFaction().GetFactionId() == rFaction.GetFactionId())
            {
                result.failReason = AirdropFailReason_t::CannotEnter;
                return result;
            }
        }
    }

    if (IsAirdropInterdicted(rUnit, rDest))
    {
        result.failReason = AirdropFailReason_t::Interdicted;
        return result;
    }

    return result;
}

} // namespace ac
