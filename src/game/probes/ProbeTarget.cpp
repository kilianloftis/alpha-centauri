#include "game/probes/ProbeTarget.h"

#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/visibility/VisibilityRules.h"
#include "game/base/BaseManager.h"
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
        BaseManager* pBase = rGameState.FindBaseAt(rTile);
        if (!pBase || &pBase->GetFaction() == &rProbeFaction)
        {
            return std::nullopt;
        }
        // Explored is enough: bases stay drawn from explored memory, the standard the player sees.
        if (!rProbeFaction.GetExploredMap().IsExplored(rTile))
        {
            return std::nullopt;
        }
        return ProbeTarget_t{ProbeBaseTarget_t{*pBase}, pBase->GetFaction(), rTile};
    }

    // Units must be visible, as for TryAttack; a concealed occupant is no target, so the order
    // stays a move.
    for (Unit* pUnit : rGameState.GetWorldMap().GetAllUnitsOnTile(rTile))
    {
        if (pUnit && &pUnit->GetFaction() != &rProbeFaction
            && IsUnitVisibleTo(rProbeFaction, *pUnit))
        {
            return ProbeTarget_t{ProbeUnitTarget_t{*pUnit}, pUnit->GetFaction(), rTile};
        }
    }
    return std::nullopt;
}

} // namespace ac
