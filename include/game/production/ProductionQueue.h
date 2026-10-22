#pragma once

#include "game/production/IConstructable.h"

#include <vector>

namespace ac
{

// Upcoming items after the one the base is building. Current production is not a slot.
// List surgery only — minerals, retool, and signals stay on ProductionManager.
class ProductionQueue
{
public:
    const std::vector<const IConstructable*>& Items() const;
    const IConstructable* Head() const;
    bool Empty() const;

    // Caller decided the contents (an edit plan, validate, or rebind).
    void Assign(std::vector<const IConstructable*> items);
    void PopFront();

private:
    std::vector<const IConstructable*> m_items;
};

} // namespace ac
