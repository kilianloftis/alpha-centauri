#include "game/units/FuelRules.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/UnitManager.h"
#include "game/map/WorldMap.h"
#include "game/units/TransportRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"

namespace ac
{

namespace
{

bool UnitProjectsRefuelsAir_(const Unit& rUnit)
{
    for (const ActiveEffect_t& rEffect : CollectLiveUnitEffects(rUnit))
    {
        const RuleFlagEffect_t* pFlag = std::get_if<RuleFlagEffect_t>(&rEffect.config->effect);
        if (pFlag && pFlag->flag == RuleFlagId_t::RefuelsAir
            && rEffect.config->scope == EffectScope_t::ThisTile
            && rEffect.config->radius == 0
            && !rEffect.config->condition.has_value())
        {
            return true;
        }
    }
    return false;
}

} // namespace

bool IsRefuelSite(const Unit& rUnit)
{
    // Pad improvements (Base, Airbase): any unit ending the turn on the tile.
    if (ResolveFlag(rUnit.GetTile(), RuleFlagId_t::RefuelsAir))
    {
        return true;
    }

    // Carrier decks (and similar unit-projected pads): only landed cargo.
    if (!rUnit.IsEmbarked())
    {
        return false;
    }
    const Unit* pCarrier = rUnit.GetCarrier();
    return pCarrier && UnitProjectsRefuelsAir_(*pCarrier);
}

void ProcessFuelAtTurnEnd(Unit& rUnit, const WorldMap& rWorldMap)
{
    if (!rUnit.GetDesign().UsesFuel())
    {
        return;
    }

    // Land on a carrier before fuel accounting so a free deck slot can save the unit.
    if (!rUnit.IsEmbarked() && rUnit.GetDomain() == UnitDomain_t::Air)
    {
        TryAttachToTransport(rUnit, rWorldMap);
    }
    
    if (IsRefuelSite(rUnit))
    {
        rUnit.SetCurrentFuel(rUnit.GetMaxFuel());
        return;
    }

    // Unused moves still consume fuel for the airborne turn.
    rUnit.SpendRemainingMoveFragments();

    if (rUnit.GetCurrentFuel() > 0)
    {
        return;
    }

    const int maxHp = ResolveStat(rUnit, StatId_t::HitPoints);
    const int damagePercent = ResolveStat(rUnit, StatId_t::DamageFromOutOfFuel);
    const int damage = FinalizeResolvedStat(maxHp * (damagePercent / 100.0));
    rUnit.SetCurrentHp(rUnit.GetCurrentHp() - damage);
    if (rUnit.GetCurrentHp() <= 0)
    {
        rUnit.GetFaction().GetUnitManager().DestroyUnit(rUnit);
    }
}

void ProcessAllFuelAtTurnEnd(GameState& rGameState)
{
    WorldMap& rWorldMap = rGameState.GetWorldMap();
    for (Faction& rFaction : rGameState.Factions())
    {
        UnitManager& rUnits = rFaction.GetUnitManager();
        const auto destructionScope = rUnits.DeferDestruction();
        for (Unit& rUnit : rUnits.Units())
        {
            ProcessFuelAtTurnEnd(rUnit, rWorldMap);
        }
    }
}

} // namespace ac
