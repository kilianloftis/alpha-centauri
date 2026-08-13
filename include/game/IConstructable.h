#pragma once

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

    // True for stockpile production items that convert minerals forever rather than
    // finishing at a cost. Cost is 0 and IsReadyToComplete is always false.
    virtual bool NeverCompletes() const { return false; }
};

} // namespace ac
