#include "game/units/AirdropRules.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/units/MovementConstants.h"
#include "game/units/MovementRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"

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

bool TileIsAirdropLaunchPad(const Tile& rTile, const WorldMap& rWorldMap, int factionId)
{
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

int AirdropLandingDamageHp(const Unit& rUnit, const Tile& rDest, const WorldMap& rWorldMap)
{
    if (TileIsAirdropLaunchPad(rDest, rWorldMap, rUnit.GetFaction().GetFactionId()))
    {
        return 0;
    }
    const int percent = ResolveStat(rUnit, StatId_t::AirdropLandingDamage);
    if (percent <= 0)
    {
        return 0;
    }
    const int maxHp = ResolveStat(rUnit, StatId_t::HitPoints);
    return maxHp * percent / 100;
}

bool IsAirdropInterdicted(const Unit& rDropper, const Tile& rDest, const WorldMap& rWorldMap)
{
    const FactionId_t dropperId = rDropper.GetFaction().GetFactionId();
    const int mapWidth = rWorldMap.GetWidth();
    bool blocked = false;
    rWorldMap.GetUnitPositions().ForEachUnit([&](const Unit& rCandidate)
    {
        if (blocked)
        {
            return;
        }
        if (rCandidate.GetFaction().GetFactionId() == dropperId)
        {
            return;
        }
        if (rCandidate.GetDomain() != UnitDomain_t::Air)
        {
            return;
        }
        const int radius = ResolveStat(rCandidate, StatId_t::InterceptRadius);
        if (radius <= 0)
        {
            return;
        }
        if (!UnitHasFullMoves_(rCandidate))
        {
            return;
        }
        if (ChebyshevDistance(rCandidate.GetTile(), rDest, mapWidth) <= radius)
        {
            blocked = true;
        }
    });
    return blocked;
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
    if (!TileIsAirdropLaunchPad(rUnit.GetTile(), rUnit.GetFaction().GetWorldMap(), factionId))
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

AirdropEligibility_t CanAirdropTo(const Unit& rUnit, const Tile& rDest, const WorldMap& rWorldMap,
                                  const TileEffectsContext& rTileEffects)
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

    const InteractionGridsConfig_t& rGrids = rTileEffects.GetInteractionGrids();
    if (!CanEnterTile(rUnit, rDest, rWorldMap, rGrids))
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

    if (IsAirdropInterdicted(rUnit, rDest, rWorldMap))
    {
        result.failReason = AirdropFailReason_t::Interdicted;
        return result;
    }

    return result;
}

} // namespace ac
