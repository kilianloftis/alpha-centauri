#include "game/faction/base/production/ProductionCompletion.h"

#include "game/Faction.h"
#include "game/IConstructable.h"
#include "game/PauseOnEventsConfig.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Military.h"
#include "game/faction/base/BaseManager.h"
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

ProductionCompletion::ProductionCompletion(BaseManager& rBase)
    : m_rBase(rBase)
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

ProductionApplyResult_t ProductionCompletion::Apply()
{
    // Stamp the turn original without adding minerals: ConvertMinerals already moved this
    // turn's leftover bank onto a real item (or converted / wasted it). Stamped before any
    // early-out, so an item left funded by a deferral is still a turn original and switching
    // away from it charges retool like any other switch.
    m_rBase.GetProduction().BankProduction(0);
    // Last turn's "not this turn" does not answer for this turn.
    m_bDeferredThisTurn = false;
    return TryCompleteReady();
}

ProductionApplyResult_t ProductionCompletion::TryCompleteReady()
{
    if (m_bPendingConfirmation)
    {
        return ProductionApplyResult_t{ProductionApplyKind_t::AwaitingConfirmation, {}};
    }

    ProductionManager& rProduction = m_rBase.GetProduction();
    if (!rProduction.HasProduction())
    {
        return ProductionApplyResult_t{ProductionApplyKind_t::Idle, {}};
    }
    if (m_rBase.IsProductionDisabled())
    {
        // Riot: the base produces nothing this turn. The stockpile is untouched and completion
        // stays blocked until the DisableProduction RuleFlag lifts.
        return ProductionApplyResult_t{ProductionApplyKind_t::InProgress, {}};
    }

    const BaseEffects_t& rEffects = m_rBase.GetBaseEffects();
    const bool bPrototype = IsCurrentPrototype();
    if (!rProduction.IsReadyToComplete(rEffects, bPrototype))
    {
        return ProductionApplyResult_t{ProductionApplyKind_t::InProgress, {}};
    }

    if (m_rBase.WouldCompletionAbandonBase())
    {
        if (m_bDeferredThisTurn)
        {
            return ProductionApplyResult_t{ProductionApplyKind_t::InProgress, {}};
        }
        m_bPendingConfirmation = true;
        return ProductionApplyResult_t{ProductionApplyKind_t::AwaitingConfirmation, {}};
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

bool ProductionCompletion::HasPendingConfirmation() const
{
    return m_bPendingConfirmation;
}

bool ProductionCompletion::IsCompletionBlocked() const
{
    return m_bPendingConfirmation || m_bDeferredThisTurn;
}

std::string ProductionCompletion::CompletePending()
{
    if (!m_bPendingConfirmation)
    {
        throw std::runtime_error(
            "ProductionCompletion::CompletePending: no answer is outstanding");
    }
    // Clear before CompleteProduction: ResetProduction_ emits OnProductionChanged which would
    // also clear the flag, but CompletePending must own the transition explicitly.
    m_bPendingConfirmation = false;
    return m_rBase.GetProduction().CompleteProduction(m_rBase.GetBaseEffects(),
                                                      IsCurrentPrototype());
}

void ProductionCompletion::DeferCompletion()
{
    if (!m_bPendingConfirmation)
    {
        throw std::runtime_error(
            "ProductionCompletion::DeferCompletion: no answer is outstanding");
    }
    m_bPendingConfirmation = false;
    m_bDeferredThisTurn = true;
}

void ProductionCompletion::NotifyProductionChanged()
{
    m_bPendingConfirmation = false;
    m_bDeferredThisTurn = false;
}

} // namespace ac
