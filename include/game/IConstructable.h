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
    virtual const char* GetId() const = 0;

    // Display name of this constructable.
    virtual const std::string& GetName() const = 0;

    // Mineral cost to construct this entity.
    virtual int GetMineralCost() const = 0;
};

} // namespace ac
