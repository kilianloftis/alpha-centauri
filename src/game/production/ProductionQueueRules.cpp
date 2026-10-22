#include "game/production/ProductionQueueRules.h"

#include "game/buildings/BuildingConfig.h"
#include "game/production/ProductionQueue.h"
#include "game/production/ProductionRules.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace ac
{

namespace
{

bool IsOnceOnlyBuilding_(const IConstructable& rItem)
{
    const auto* pBuilding = dynamic_cast<const BuildingConfig_t*>(&rItem);
    return pBuilding && !pBuilding->allowMultiple;
}

std::optional<int> OnceOnlyIndex_(const std::vector<const IConstructable*>& rItems,
                                  const IConstructable& rItem)
{
    if (!IsOnceOnlyBuilding_(rItem))
    {
        return std::nullopt;
    }
    for (size_t i = 0; i < rItems.size(); ++i)
    {
        const IConstructable* pSlot = rItems[i];
        if (pSlot && pSlot->GetId() == rItem.GetId())
        {
            return static_cast<int>(i);
        }
    }
    return std::nullopt;
}

QueueEditPlan_t RefusePlan_()
{
    return QueueEditPlan_t{};
}

QueueEditPlan_t ApplyPlan_(std::vector<const IConstructable*> items)
{
    QueueEditPlan_t plan;
    plan.action = QueueEditAction_t::Apply;
    plan.items = std::move(items);
    return plan;
}

} // namespace

QueueEditPlan_t PlanReplaceQueueItem(const ProductionQueue& rQueue, int slot,
                                     const IConstructable& rItem)
{
    const std::vector<const IConstructable*>& rItems = rQueue.Items();
    const int size = static_cast<int>(rItems.size());
    if (slot < 0 || slot >= size)
    {
        return RefusePlan_();
    }

    std::vector<const IConstructable*> items = rItems;
    const std::optional<int> existing = OnceOnlyIndex_(rItems, rItem);
    if (!existing)
    {
        items[static_cast<size_t>(slot)] = &rItem;
        return ApplyPlan_(std::move(items));
    }
    if (*existing == slot)
    {
        items[static_cast<size_t>(slot)] = &rItem;
        return ApplyPlan_(std::move(items));
    }

    items.erase(items.begin() + *existing);
    int dest = slot;
    if (*existing < slot)
    {
        dest = slot - 1;
    }
    items[static_cast<size_t>(dest)] = &rItem;
    return ApplyPlan_(std::move(items));
}

QueueEditPlan_t PlanInsertQueueItem(const ProductionQueue& rQueue, int slot,
                                    const IConstructable& rItem)
{
    const std::vector<const IConstructable*>& rItems = rQueue.Items();
    const int size = static_cast<int>(rItems.size());
    if (slot < 0 || slot > size)
    {
        return RefusePlan_();
    }

    std::vector<const IConstructable*> items = rItems;
    const std::optional<int> existing = OnceOnlyIndex_(rItems, rItem);
    if (!existing)
    {
        items.insert(items.begin() + slot, &rItem);
        return ApplyPlan_(std::move(items));
    }
    if (*existing == slot)
    {
        return ApplyPlan_(std::move(items));
    }

    items.erase(items.begin() + *existing);
    int dest = slot;
    if (*existing < slot)
    {
        dest = slot - 1;
    }
    items.insert(items.begin() + dest, &rItem);
    return ApplyPlan_(std::move(items));
}

QueueEditPlan_t PlanDeleteQueueItem(const ProductionQueue& rQueue, int slot)
{
    const std::vector<const IConstructable*>& rItems = rQueue.Items();
    const int size = static_cast<int>(rItems.size());
    if (slot < 0 || slot >= size)
    {
        return RefusePlan_();
    }
    std::vector<const IConstructable*> items = rItems;
    items.erase(items.begin() + slot);
    return ApplyPlan_(std::move(items));
}

QueueEditPlan_t PlanRemoveOnceOnlyQueueItem(const ProductionQueue& rQueue,
                                            const IConstructable& rItem)
{
    const std::vector<const IConstructable*>& rItems = rQueue.Items();
    const std::optional<int> existing = OnceOnlyIndex_(rItems, rItem);
    if (!existing)
    {
        return ApplyPlan_(rItems);
    }
    std::vector<const IConstructable*> items = rItems;
    items.erase(items.begin() + *existing);
    return ApplyPlan_(std::move(items));
}

std::vector<const IConstructable*> ValidQueueItems(const BaseManager& rBase,
                                                   const ProductionQueue& rQueue)
{
    std::vector<const IConstructable*> kept;
    std::unordered_set<std::string> seenOnceOnly;
    for (const IConstructable* pItem : rQueue.Items())
    {
        if (!pItem)
        {
            throw std::logic_error("ValidQueueItems: queue slot is null");
        }
        if (!CanProduceItem(rBase, *pItem))
        {
            continue;
        }
        if (IsOnceOnlyBuilding_(*pItem))
        {
            if (!seenOnceOnly.insert(pItem->GetId()).second)
            {
                continue;
            }
        }
        kept.push_back(pItem);
    }
    return kept;
}

} // namespace ac
