#include "game/units/ProbeTarget.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/faction/UnitVisibility.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"

#include <type_traits>

namespace ac
{

ProbeTargetKind_t KindOf(const ProbeTargetRef_t& rRef)
{
    return std::visit(
        [](const auto& rConcrete) -> ProbeTargetKind_t
        {
            using T = std::decay_t<decltype(rConcrete)>;
            if constexpr (std::is_same_v<T, ProbeBaseTarget_t>)
            {
                return ProbeTargetKind_t::Base;
            }
            else
            {
                return ProbeTargetKind_t::Unit;
            }
        },
        rRef);
}

BaseManager* AsBase(const ProbeTarget_t& rTarget)
{
    const ProbeBaseTarget_t* pBaseTarget = std::get_if<ProbeBaseTarget_t>(&rTarget.ref);
    return pBaseTarget ? &pBaseTarget->rBase : nullptr;
}

const BaseManager* EffectSourceBase(const ProbeTarget_t& rTarget)
{
    if (const BaseManager* pBase = AsBase(rTarget))
    {
        return pBase;
    }

    const Unit& rTargetUnit = std::get<ProbeUnitTarget_t>(rTarget.ref).rUnit;
    if (const BaseManager* pHome = rTargetUnit.GetHomeBase())
    {
        return pHome;
    }
    for (const BaseManager& rBase : rTarget.rFaction.Bases())
    {
        return &rBase;
    }
    return nullptr;
}

std::optional<ProbeTarget_t> ResolveProbeTarget(const Unit& rProbe, const Tile& rTile,
                                                ProbeTargetKind_t kind, GameState& rGameState)
{
    const Faction& rProbeFaction = rProbe.GetFaction();

    if (kind == ProbeTargetKind_t::Base)
    {
        BaseManager* pBase = rGameState.FindBaseAt(rTile.GetX(), rTile.GetY());
        if (!pBase || &pBase->GetFaction() == &rProbeFaction)
        {
            return std::nullopt;
        }
        // Explored, not currently visible: bases are static and stay drawn from explored
        // memory (WorldDisplay: "fog still shows last-known bases"), so knowing the tile
        // is the same standard the player is held to on screen.
        if (!rProbeFaction.GetExploredMap().IsExplored(rTile))
        {
            return std::nullopt;
        }
        return ProbeTarget_t{ProbeBaseTarget_t{*pBase}, pBase->GetFaction(), rTile};
    }

    // Units must be visible, matching the gate TryAttack applies via
    // FindVisibleHostileOnTile_. A concealed occupant resolves to no target at all, so the
    // order stays a move; bumping into it reveals it through the blocked-step path.
    for (Unit* pUnit : rGameState.GetWorldMap().GetUnitsOnTile(rTile))
    {
        if (pUnit && &pUnit->GetFaction() != &rProbeFaction
            && IsUnitVisibleTo(rProbeFaction, *pUnit, rGameState.GetTileEffects()))
        {
            return ProbeTarget_t{ProbeUnitTarget_t{*pUnit}, pUnit->GetFaction(), rTile};
        }
    }
    return std::nullopt;
}

} // namespace ac
