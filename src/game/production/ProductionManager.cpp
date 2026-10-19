#include "game/production/ProductionManager.h"
#include "game/production/ProductionRules.h"
#include "game/production/ProductionPauseGates.h"
#include "game/base/BaseManager.h"
#include "game/faction/Faction.h"
#include "game/effects/EffectEnums.h"
#include "game/units/Military.h"
#include "game/production/StockpileConversion.h"
#include "game/production/StockpileRegistry.h"
#include "game/units/IDesign.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ac
{

ProductionManager::ProductionManager(const ProductionConfig_t& rConfig,
                                     std::function<const IConstructable*()> defaultItemProvider,
                                     BaseManager& rBase)
    : m_rConfig(rConfig)
    , m_defaultItemProvider(std::move(defaultItemProvider))
    , m_rBase(rBase)
    , m_pCurrentItem(nullptr)
    , m_mineralStockpile(0)
{
    // A base starts on the default rather than idle. Completion is not yet listening, so this
    // seeds the queue without clearing any confirmation state.
    ResetProduction_();
    OnProductionChanged.Connect([this]() { m_completion.NotifyProductionChanged(); });
}

void ProductionManager::SetProduction(const IConstructable* pItem,
                                      const BaseEffects_t& rBaseEffects)
{
    if (!pItem)
    {
        ResetProduction_();
        return;
    }

    if (pItem == m_pCurrentItem)
    {
        return;
    }

    ApplyRetoolPenalty_(pItem, rBaseEffects);
    m_pCurrentItem = pItem;
    OnProductionChanged.Emit();
}

void ProductionManager::RebindProductionItem(const IConstructable* pItem)
{
    if (pItem == m_pCurrentItem)
    {
        return;
    }

    if (!pItem)
    {
        m_pTurnOriginalItem = nullptr;
        ResetProduction_();
        return;
    }

    // Same logical item, new backing pointer: keep turn-original continuity for retool.
    if (m_pTurnOriginalItem == m_pCurrentItem)
    {
        m_pTurnOriginalItem = pItem;
    }
    m_pCurrentItem = pItem;
}

void ProductionManager::ApplyRetoolPenalty_(const IConstructable* pNewItem,
                                            const BaseEffects_t& rBaseEffects)
{
    const bool bHasTurnOriginal = m_pTurnOriginalItem != nullptr;
    const bool bReturning = pNewItem == m_pTurnOriginalItem;
    m_mineralStockpile -= RetoolForfeit(m_mineralStockpile, bHasTurnOriginal, bReturning,
                                        m_rConfig, rBaseEffects);
}

const IConstructable* ProductionManager::GetCurrentProduction() const
{
    return m_pCurrentItem;
}

bool ProductionManager::HasProduction() const
{
    return m_pCurrentItem != nullptr;
}

int ProductionManager::GetMineralStockpile() const
{
    return m_mineralStockpile;
}

void ProductionManager::SetMineralStockpile(int amount)
{
    m_mineralStockpile = amount;
}

void ProductionManager::BankProduction(int minerals)
{
    if (!HasProduction())
    {
        m_pTurnOriginalItem = nullptr;
        return;
    }

    m_mineralStockpile += minerals;

    // Whatever is queued once leftover minerals are claimed is what the player sees when
    // PlayerActions hands over, so it is the item a retool this turn is measured against.
    m_pTurnOriginalItem = m_pCurrentItem;
}

ProductionCompletionProbe_t ProductionManager::BuildCompletionProbe_() const
{
    ProductionCompletionProbe_t probe;
    probe.hasProduction = HasProduction();
    probe.productionDisabled = !CanCompleteProduction(m_rBase);
    probe.readyToComplete = IsReadyToComplete();
    probe.wouldAbandonBase = CompletionAbandonsBase(m_rBase);
    probe.isPrototype = IsCurrentPrototype();
    return probe;
}

ProductionApplyResult_t
ProductionManager::FinishReady_(const ProductionCompletion::EvaluateResult_t& ready)
{
    if (!m_pCurrentItem)
    {
        throw std::logic_error("ProductionManager::FinishReady_: nothing queued");
    }
    // Capture before CompleteProduction_ resets the queue.
    const IConstructable& rItem = *m_pCurrentItem;
    const std::string completedName = rItem.GetName();
    std::vector<PauseOnEventId_t> events =
        CollectProductionPauseGates(rItem, ready.isPrototype);
    const std::string completedId = CompleteProduction_();
    return ProductionApplyResult_t{ProductionApplyKind_t::Completed, completedId,
                                   std::move(events), completedName};
}

ProductionApplyResult_t
ProductionManager::ApplyCompletionOutcome_(const ProductionCompletion::EvaluateResult_t& outcome)
{
    using Outcome_t = ProductionCompletion::Outcome_t;
    switch (outcome.outcome)
    {
    case Outcome_t::Idle:
        return ProductionApplyResult_t{ProductionApplyKind_t::Idle, {}};
    case Outcome_t::InProgress:
        return ProductionApplyResult_t{ProductionApplyKind_t::InProgress, {}};
    case Outcome_t::AwaitingConfirmation:
        return ProductionApplyResult_t{ProductionApplyKind_t::AwaitingConfirmation, {}};
    case Outcome_t::ReadyToFinish:
        return FinishReady_(outcome);
    }
    throw std::logic_error("ProductionManager::ApplyCompletionOutcome_: unhandled outcome");
}

ProductionApplyResult_t ProductionManager::ApplyProduction()
{
    if (m_pCurrentItem && m_pCurrentItem->IsStockpile())
    {
        return ConvertStockpile_();
    }
    return ApplyCompletionOutcome_(
        m_completion.TryCompleteReady(BuildCompletionProbe_(), /*bNewTurn=*/true));
}

ProductionApplyResult_t ProductionManager::ConvertStockpile_()
{
    const int toConvert = m_mineralStockpile;
    m_mineralStockpile = 0;
    if (toConvert > 0)
    {
        ApplyStockpileConversionAtBase(
            m_rBase, m_rBase.GetStockpileRegistry().Get(m_pCurrentItem->GetId()), toConvert);
    }
    m_pTurnOriginalItem = m_pCurrentItem;
    return ProductionApplyResult_t{ProductionApplyKind_t::InProgress, {}};
}

ProductionApplyResult_t ProductionManager::TryCompleteReady(bool bNewTurn)
{
    return ApplyCompletionOutcome_(
        m_completion.TryCompleteReady(BuildCompletionProbe_(), bNewTurn));
}

bool ProductionManager::IsReadyToComplete() const
{
    return HasProduction() && !m_pCurrentItem->IsStockpile()
        && m_mineralStockpile >= ItemMineralCost(m_rBase, *m_pCurrentItem);
}

std::string ProductionManager::CompleteProduction_()
{
    if (!HasProduction())
    {
        return std::string();
    }

    // Cost first: ResetProduction_ replaces the item.
    const int cost = ItemMineralCost(m_rBase, *m_pCurrentItem);
    const int leftover = std::max(0, m_mineralStockpile - cost);
    m_mineralStockpile = OverflowCarried(leftover, m_rConfig);

    std::string completed = m_pCurrentItem->GetId();
    ResetProduction_();
    // The default fallback is not a player choice, so there is no turn original until the
    // next BankProduction with something queued.
    m_pTurnOriginalItem = nullptr;
    OnProductionCompleted.Emit(completed);
    return completed;
}

bool ProductionManager::HasPendingConfirmation() const
{
    return m_completion.HasPendingConfirmation();
}

bool ProductionManager::IsCompletionBlocked() const
{
    return m_completion.IsCompletionBlocked();
}

bool ProductionManager::IsCurrentPrototype() const
{
    // Same test ClassifyCompletedItem uses. Resolving the design by id instead would scan
    // every design of the faction on every render-path cost query, and would mistake a
    // building for a unit if the two ever shared an id.
    const IDesign* pDesign = dynamic_cast<const IDesign*>(m_pCurrentItem);
    return pDesign && IsPrototype(m_rBase.GetFaction(), *pDesign);
}

std::string ProductionManager::CompletePending()
{
    // Probe before AcceptPending clears confirmation; the item must still be the funded one.
    const ProductionCompletionProbe_t probe = BuildCompletionProbe_();
    const ProductionCompletion::EvaluateResult_t ready = m_completion.AcceptPending(probe);
    return FinishReady_(ready).completedId;
}

void ProductionManager::DeferCompletion()
{
    m_completion.DeferCompletion();
}

void ProductionManager::ResetProduction_()
{
    const IConstructable* pDefault = m_defaultItemProvider ? m_defaultItemProvider() : nullptr;
    if (pDefault == m_pCurrentItem)
    {
        return;
    }
    m_pCurrentItem = pDefault;
    OnProductionChanged.Emit();
}

} // namespace ac
