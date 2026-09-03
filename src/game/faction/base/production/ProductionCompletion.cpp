#include "game/faction/base/production/ProductionCompletion.h"

#include "game/Faction.h"
#include "game/IConstructable.h"
#include "game/PauseOnEventsConfig.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Military.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/units/UnitDesign.h"

#include <stdexcept>

namespace ac
{

namespace
{

PauseOnEventId_t ClassifyCompletedItem_(const IConstructable& rItem)
{
    switch (rItem.GetConstructableKind())
    {
    case ConstructableKind_t::Building:
    case ConstructableKind_t::SecretProject:
        return PauseOnEventId_t::NewFacilityBuilt;
    case ConstructableKind_t::Unit:
    {
        const auto* pDesign = dynamic_cast<const UnitDesign*>(&rItem);
        if (pDesign && (pDesign->GetStat(StatId_t::Attack) > 0
                        || pDesign->GetFlag(RuleFlagId_t::ForcesPsiCombat)))
        {
            return PauseOnEventId_t::CombatUnitBuilt;
        }
        return PauseOnEventId_t::NonCombatUnitBuilt;
    }
    case ConstructableKind_t::Stockpile:
        throw std::logic_error("ClassifyCompletedItem_: a stockpile cannot complete");
    }
    throw std::logic_error("ClassifyCompletedItem_: unhandled constructable kind");
}

} // namespace

ProductionCompletion::ProductionCompletion(BaseManager& rBase,
                                           const BuildingRegistry& rBuildings)
    : m_rBase(rBase)
    , m_rBuildings(rBuildings)
{
}

bool ProductionCompletion::IsCurrentPrototype() const
{
    // Same test ClassifyCompletedItem_ uses. Resolving the design by id instead would scan
    // every design of the faction on a call GetMineralCost makes from render paths, and would
    // mistake a building for a unit if the two ever shared an id.
    const UnitDesign* pDesign =
        dynamic_cast<const UnitDesign*>(m_rBase.GetProduction().GetCurrentProduction());
    return pDesign && m_rBase.GetFaction().GetMilitary().IsPrototype(*pDesign);
}

bool ProductionCompletion::WouldEmptyBase_() const
{
    const IConstructable* pItem = m_rBase.GetProduction().GetCurrentProduction();
    if (!pItem)
    {
        return false;
    }

    const int size = m_rBase.GetPopulation().GetSize();
    if (const BuildingConfig_t* pBuilding = m_rBuildings.Find(pItem->GetId()))
    {
        return PredictInstantaneousPopulationSize(pBuilding->effects, size) <= 0;
    }
    if (const UnitDesign* pDesign = m_rBase.GetFaction().GetMilitary().GetDesign(pItem->GetId()))
    {
        return PredictUnitProductionPopulationSize(*pDesign, size) <= 0;
    }
    return false;
}

ProductionApplyResult_t ProductionCompletion::Apply()
{
    if (m_bPendingAbandonConfirm)
    {
        // ConvertMinerals already claimed the bank; wait for Confirm / Defer.
        return ProductionApplyResult_t{ProductionApplyKind_t::AwaitingAbandonConfirm, {}};
    }

    // Stamp the turn original without adding minerals: ConvertMinerals already moved this
    // turn's leftover bank onto a real item (or converted / wasted it).
    m_rBase.GetProduction().BankProduction(0);
    return TryCompleteReady();
}

ProductionApplyResult_t ProductionCompletion::TryCompleteReady()
{
    if (m_bPendingAbandonConfirm)
    {
        return ProductionApplyResult_t{ProductionApplyKind_t::AwaitingAbandonConfirm, {}};
    }
    if (m_bAbandonDeferred || ResolveFlag(m_rBase, RuleFlagId_t::DisableProduction))
    {
        // Stockpile is preserved; completion (and re-prompting) stays blocked until the
        // queue changes or the flag lifts.
        return ProductionApplyResult_t{ProductionApplyKind_t::InProgress, {}};
    }

    ProductionManager& rProduction = m_rBase.GetProduction();
    if (!rProduction.HasProduction())
    {
        return ProductionApplyResult_t{ProductionApplyKind_t::Idle, {}};
    }

    const BaseEffects_t& rEffects = m_rBase.GetBaseEffects();
    const bool bPrototype = IsCurrentPrototype();
    if (!rProduction.IsReadyToComplete(rEffects, bPrototype))
    {
        return ProductionApplyResult_t{ProductionApplyKind_t::InProgress, {}};
    }

    if (WouldEmptyBase_())
    {
        m_bPendingAbandonConfirm = true;
        return ProductionApplyResult_t{ProductionApplyKind_t::AwaitingAbandonConfirm, {}};
    }

    const IConstructable& rItem = *rProduction.GetCurrentProduction();
    // TODO: a prototype reports PrototypeBuilt instead of CombatUnitBuilt / NonCombatUnitBuilt,
    // so a player who wants combat-unit pauses but not prototype pauses gets no prompt at all
    // for a prototype combat unit. Whether prototype overrides the item classification or the
    // two gates should both be consulted is an unrecorded UI rules decision.
    const PauseOnEventId_t completedEvent =
        bPrototype ? PauseOnEventId_t::PrototypeBuilt : ClassifyCompletedItem_(rItem);
    const std::string completedName = rItem.GetName();
    return ProductionApplyResult_t{ProductionApplyKind_t::Completed,
                                   rProduction.CompleteProduction(rEffects, bPrototype),
                                   completedEvent, completedName};
}

bool ProductionCompletion::HasPendingAbandonConfirm() const
{
    return m_bPendingAbandonConfirm;
}

std::string ProductionCompletion::ConfirmAbandon()
{
    if (!m_bPendingAbandonConfirm)
    {
        throw std::runtime_error(
            "ProductionCompletion::ConfirmAbandon: no pending abandon confirmation");
    }
    // Clear before CompleteProduction: ResetProduction_ emits OnProductionChanged which would
    // also clear the flag, but Confirm must own the transition explicitly.
    m_bPendingAbandonConfirm = false;
    return m_rBase.GetProduction().CompleteProduction(m_rBase.GetBaseEffects(),
                                                      IsCurrentPrototype());
}

void ProductionCompletion::DeferAbandon()
{
    if (!m_bPendingAbandonConfirm)
    {
        throw std::runtime_error(
            "ProductionCompletion::DeferAbandon: no pending abandon confirmation");
    }
    m_bPendingAbandonConfirm = false;
    m_bAbandonDeferred = true;
}

void ProductionCompletion::NotifyProductionChanged()
{
    m_bPendingAbandonConfirm = false;
    m_bAbandonDeferred = false;
}

} // namespace ac
