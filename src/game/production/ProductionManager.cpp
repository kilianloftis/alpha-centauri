#include "game/production/ProductionManager.h"
#include "game/production/ProductionRules.h"
#include "game/production/ProductionPauseGates.h"
#include "game/base/BaseManager.h"
#include "game/faction/Faction.h"
#include "game/effects/EffectEnums.h"
#include "game/units/Military.h"
#include "game/production/StockpileConfig.h"
#include "game/production/StockpileConversion.h"
#include "game/production/StockpileRegistry.h"
#include "game/units/IDesign.h"

#include <optional>
#include <stdexcept>

namespace ac
{

ProductionManager::ProductionManager(const ProductionConfig_t& rConfig,
                                     std::function<const IConstructable*()> defaultItemProvider,
                                     BaseManager& rBase)
    : m_rConfig(rConfig)
    , m_defaultItemProvider(std::move(defaultItemProvider))
    , m_rBase(rBase)
    , m_mineralStockpile(0)
{
    // A base starts on the default rather than idle. Completion is not yet listening, so this
    // seeds current production without clearing any confirmation state.
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
    const QueueEditPlan_t pull = PlanRemoveOnceOnlyQueueItem(m_queue, *pItem);
    ApplyQueuePlan_(pull);
    m_pCurrentItem = pItem;
    OnProductionChanged.Emit();
}

void ProductionManager::Replace(int slot, const IConstructable& rItem)
{
    const QueueEditPlan_t plan = PlanReplaceQueueItem(m_queue, slot, rItem);
    ApplyQueuePlan_(plan);
}

void ProductionManager::Insert(int slot, const IConstructable& rItem)
{
    const QueueEditPlan_t plan = PlanInsertQueueItem(m_queue, slot, rItem);
    ApplyQueuePlan_(plan);
}

void ProductionManager::Delete(int slot)
{
    const QueueEditPlan_t plan = PlanDeleteQueueItem(m_queue, slot);
    ApplyQueuePlan_(plan);
}

std::optional<std::string> ProductionManager::ValidateQueue()
{
    const IConstructable* pOldCurrent = m_pCurrentItem;
    std::optional<std::string> droppedCurrent;
    if (m_pCurrentItem && !CanProduceItem(m_rBase, *m_pCurrentItem))
    {
        droppedCurrent = m_pCurrentItem->GetName();
        m_pCurrentItem = nullptr;
    }

    const std::vector<const IConstructable*> kept = ValidQueueItems(m_rBase, m_queue);
    m_queue.Assign(kept);

    if (!m_pCurrentItem)
    {
        if (!m_queue.Empty())
        {
            m_pCurrentItem = m_queue.Head();
            m_queue.PopFront();
        }
        else
        {
            m_pCurrentItem = DefaultItem_();
        }
    }

    if (m_pCurrentItem != pOldCurrent)
    {
        OnProductionChanged.Emit();
    }
    return droppedCurrent;
}

void ProductionManager::RebindCurrent(const IConstructable* pItem)
{
    m_pTurnOriginalItem = nullptr;
    m_pCurrentItem = pItem ? pItem : DefaultItem_();
}

void ProductionManager::RebindQueue(std::vector<const IConstructable*> items)
{
    m_queue.Assign(std::move(items));
}

void ProductionManager::ApplyRetoolPenalty_(const IConstructable* pNewItem,
                                            const BaseEffects_t& rBaseEffects)
{
    const bool bHasTurnOriginal = m_pTurnOriginalItem != nullptr;
    const bool bReturning = pNewItem == m_pTurnOriginalItem;
    m_mineralStockpile -= RetoolForfeit(m_mineralStockpile, bHasTurnOriginal, bReturning,
                                        m_rConfig, rBaseEffects);
}

void ProductionManager::ApplyQueuePlan_(const QueueEditPlan_t& rPlan)
{
    if (rPlan.action != QueueEditAction_t::Apply)
    {
        throw std::invalid_argument("ProductionManager: queue edit refused");
    }
    if (rPlan.items == m_queue.Items())
    {
        return;
    }
    m_queue.Assign(rPlan.items);
}

const std::vector<const IConstructable*>& ProductionManager::GetQueue() const
{
    return m_queue.Items();
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

    // Whatever is current once leftover minerals are claimed is what the player sees when
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
    const IConstructable* pItem = GetCurrentProduction();
    if (!pItem)
    {
        throw std::logic_error("ProductionManager::FinishReady_: nothing current");
    }
    const IConstructable& rItem = *pItem;
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
    const IConstructable* pItem = GetCurrentProduction();
    if (!pItem)
    {
        return ProductionApplyResult_t{ProductionApplyKind_t::Idle, {}};
    }
    if (pItem->IsStockpile())
    {
        return ConvertStockpile_();
    }
    return ProductionApplyResult_t{ProductionApplyKind_t::InProgress, {}};
}

ProductionApplyResult_t ProductionManager::ConvertStockpile_()
{
    const IConstructable* pItem = GetCurrentProduction();
    const int toConvert = m_mineralStockpile;
    m_mineralStockpile = 0;
    if (toConvert > 0)
    {
        const StockpileConfig_t& rStockpile = m_rBase.GetStockpileRegistry().Get(pItem->GetId());
        ApplyStockpileConversionAtBase(m_rBase, rStockpile, toConvert);
    }
    m_pTurnOriginalItem = pItem;
    return ProductionApplyResult_t{ProductionApplyKind_t::InProgress, {}};
}

ProductionApplyResult_t ProductionManager::TryCompleteReady(bool bNewTurn)
{
    return ApplyCompletionOutcome_(
        m_completion.TryCompleteReady(BuildCompletionProbe_(), bNewTurn));
}

bool ProductionManager::IsReadyToComplete() const
{
    const IConstructable* pItem = GetCurrentProduction();
    if (!pItem || pItem->IsStockpile())
    {
        return false;
    }
    if (!CanProduceItem(m_rBase, *pItem))
    {
        return false;
    }
    return m_mineralStockpile >= ItemMineralCost(m_rBase, *pItem);
}

std::string ProductionManager::CompleteProduction_()
{
    const IConstructable* pItem = GetCurrentProduction();
    if (!pItem)
    {
        return std::string();
    }

    const int cost = ItemMineralCost(m_rBase, *pItem);
    const int leftover = std::max(0, m_mineralStockpile - cost);
    m_mineralStockpile = OverflowCarried(leftover, m_rConfig);

    std::string completed = pItem->GetId();
    m_pCurrentItem = nullptr;
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
    const IDesign* pDesign = dynamic_cast<const IDesign*>(GetCurrentProduction());
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

const IConstructable* ProductionManager::DefaultItem_() const
{
    return m_defaultItemProvider ? m_defaultItemProvider() : nullptr;
}

void ProductionManager::ResetProduction_()
{
    const IConstructable* pDefault = DefaultItem_();
    if (m_pCurrentItem == pDefault)
    {
        return;
    }
    m_pCurrentItem = pDefault;
    OnProductionChanged.Emit();
}

} // namespace ac
