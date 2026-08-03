#include "game/units/BaseConquestRules.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/FactionConfig.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"

namespace ac
{

bool HasBaseGarrison(const BaseManager& rBase, const WorldMap& rWorldMap)
{
    const FactionId_t ownerId = rBase.GetFactionId();
    for (const Unit* pUnit : rWorldMap.GetUnitsOnTile(rBase.GetTile()))
    {
        // Embarked units count here, unlike everywhere else. Cargo sitting in a base is
        // still a garrison: it survives its carrier being sunk in port (SurvivesCarrierLoss)
        // and holds the base against capture. The carrier is what an attacker can target —
        // see FindVisibleHostileOnTile — so this does not make the cargo fight, only hold.
        if (pUnit && pUnit->GetFaction().GetFactionId() == ownerId)
        {
            return true;
        }
    }
    return false;
}

bool CanAttackIntoBaseTile(const Unit& rAttacker, const Tile& rBaseTile)
{
    // Only the land → sea-base case is gated; ships and aircraft keep coastal options open.
    if (rAttacker.GetDomain() != UnitDomain_t::Land)
    {
        return true;
    }
    if (!rBaseTile.IsWater() || !rBaseTile.HasImprovement("Base"))
    {
        return true;
    }
    return ResolveFlag(rAttacker, RuleFlagId_t::Amphibious);
}

bool CanCaptureBase(const Unit& rCapturer)
{
    return !ResolveFlag(rCapturer, RuleFlagId_t::CannotCaptureBases);
}

bool IsCrossSpeciesConquest(FactionSpecies_t attacker, FactionSpecies_t defender)
{
    return (attacker == FactionSpecies_t::Human && defender == FactionSpecies_t::Progenitor)
        || (attacker == FactionSpecies_t::Progenitor && defender == FactionSpecies_t::Human);
}

bool IsNativeLifeFaction(FactionSpecies_t species)
{
    return species == FactionSpecies_t::NativeLife;
}

} // namespace ac
