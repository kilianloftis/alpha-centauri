#pragma once

#include "game/ConstructableKind.h"

#include <string>

namespace ac
{

// Abstract interface for any entity that can be constructed in a base.
class IConstructable
{
public:
    virtual ~IConstructable() = default;

    // Unique identifier string for this constructable (e.g. "Nutrient_Bank").
    virtual const std::string& GetId() const = 0;

    // Display name of this constructable.
    virtual const std::string& GetName() const = 0;

    // Base mineral cost to construct this entity (before industry modifiers).
    virtual int GetBaseCost() const = 0;

    // What this item is. Hurry, completion, and the build menu all key off this rather than
    // RTTI. Every constructable answers — there is no "none".
    virtual ConstructableKind_t GetConstructableKind() const = 0;

    // Stockpile items convert minerals rather than finishing at a cost. Cost is 0 and
    // IsReadyToComplete is always false.
    bool IsStockpile() const
    {
        return GetConstructableKind() == ConstructableKind_t::Stockpile;
    }
};

} // namespace ac
