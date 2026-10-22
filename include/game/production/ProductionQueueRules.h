#pragma once

#include "game/production/IConstructable.h"

#include <vector>

namespace ac
{

class BaseManager;
class ProductionQueue;

enum class QueueEditAction_t
{
    Refuse,
    Apply,
};

// The upcoming list after Replace / Insert / Delete / pull. Refuse leaves items empty.
struct QueueEditPlan_t
{
    QueueEditAction_t action = QueueEditAction_t::Refuse;
    std::vector<const IConstructable*> items;
};

QueueEditPlan_t PlanReplaceQueueItem(const ProductionQueue& rQueue, int slot,
                                     const IConstructable& rItem);
QueueEditPlan_t PlanInsertQueueItem(const ProductionQueue& rQueue, int slot,
                                    const IConstructable& rItem);
QueueEditPlan_t PlanDeleteQueueItem(const ProductionQueue& rQueue, int slot);
// Drop a once-only building already on the list so SetProduction can take it as current.
QueueEditPlan_t PlanRemoveOnceOnlyQueueItem(const ProductionQueue& rQueue,
                                            const IConstructable& rItem);

// Items that may stay, in order: CanProduceItem, and at most one of each once-only building.
std::vector<const IConstructable*> ValidQueueItems(const BaseManager& rBase,
                                                   const ProductionQueue& rQueue);

} // namespace ac
