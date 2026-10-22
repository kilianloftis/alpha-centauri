#include "game/production/ProductionQueue.h"

#include <stdexcept>

namespace ac
{

const std::vector<const IConstructable*>& ProductionQueue::Items() const
{
    return m_items;
}

const IConstructable* ProductionQueue::Head() const
{
    return m_items.empty() ? nullptr : m_items.front();
}

bool ProductionQueue::Empty() const
{
    return m_items.empty();
}

void ProductionQueue::Assign(std::vector<const IConstructable*> items)
{
    m_items = std::move(items);
}

void ProductionQueue::PopFront()
{
    if (m_items.empty())
    {
        throw std::logic_error("ProductionQueue::PopFront: queue is empty");
    }
    m_items.erase(m_items.begin());
}

} // namespace ac
