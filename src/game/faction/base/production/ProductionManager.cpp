#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/production/ProductionCostCalculator.h"
#include "game/effects/EffectEnums.h"

#include <algorithm>
#include <cmath>

namespace ac
{

ProductionManager::ProductionManager(const ProductionConfig_t& rConfig,
                                     std::function<const IConstructable*()> defaultItemProvider)
    : m_rConfig(rConfig)
    , m_defaultItemProvider(std::move(defaultItemProvider))
    , m_pCurrentItem(nullptr)
    , m_mineralStockpile(0)
{
    // A base starts on the default rather than idle. Nothing is connected to
    // OnProductionChanged yet, so this seeds the queue without announcing anything.
    ResetProduction_();
}

ProductionManager::~ProductionManager() = default;

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
    // No turn original yet
    if (!m_pTurnOriginalItem)
    {
        return;
    }
    // Free to go back to what the base was building when the turn handed over.
    if (pNewItem == m_pTurnOriginalItem)
    {
        return;
    }
    if (m_mineralStockpile <= m_rConfig.retoolPenaltyThreshold)
    {
        return;
    }

    // Integer division rounds the loss down, so the remainder favours the player. Scale
    // afterward so RetoolPenaltyScale 0 (Skunkworks) cancels the forfeit without changing
    // threshold / percent semantics when scale is 1.
    const int baseForfeit = m_mineralStockpile * m_rConfig.retoolPenaltyPercent / 100;
    const double scale = ResolveBaseStat(rBaseEffects, StatId_t::RetoolPenaltyScale,
                                         SeedFor(StatId_t::RetoolPenaltyScale));
    const int forfeited = std::max(0, static_cast<int>(std::lround(baseForfeit * scale)));
    m_mineralStockpile -= forfeited;
}

const IConstructable* ProductionManager::GetCurrentProduction() const
{
    return m_pCurrentItem;
}

bool ProductionManager::HasProduction() const
{
    return m_pCurrentItem != nullptr;
}

int ProductionManager::GetMineralCost(const BaseEffects_t& rBaseEffects, bool bPrototype) const
{
    if (!m_pCurrentItem || m_pCurrentItem->NeverCompletes())
    {
        return 0;
    }
    const int surcharge = bPrototype ? m_rConfig.prototypeSurchargePercent : 0;
    return ProductionCostCalculator::ComputeCost(m_pCurrentItem->GetBaseCost(), rBaseEffects,
                                                 surcharge);
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

    // Whatever is queued once leftover minerals are claimed (and again when ApplyProduction
    // stamps 0) is what the player sees when PlayerActions hands over, so it is the item a
    // retool this turn is measured against.
    m_pTurnOriginalItem = m_pCurrentItem;
}

bool ProductionManager::IsReadyToComplete(const BaseEffects_t& rBaseEffects, bool bPrototype) const
{
    return HasProduction() && !m_pCurrentItem->NeverCompletes()
        && m_mineralStockpile >= GetMineralCost(rBaseEffects, bPrototype);
}

std::string ProductionManager::CompleteProduction(const BaseEffects_t& rBaseEffects,
                                                  bool bPrototype)
{
    if (!HasProduction())
    {
        return std::string();
    }

    // Cost first: ResetProduction_ replaces the item, after which GetMineralCost is 0.
    const int cost = GetMineralCost(rBaseEffects, bPrototype);
    const int leftover = std::max(0, m_mineralStockpile - cost);
    m_mineralStockpile = std::min(leftover, m_rConfig.retoolPenaltyThreshold);

    std::string completed = m_pCurrentItem->GetId();
    ResetProduction_();
    // The default fallback is not a player choice, so there is no turn original until the
    // next BankProduction with something queued.
    m_pTurnOriginalItem = nullptr;
    OnProductionCompleted.Emit(completed);
    return completed;
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
