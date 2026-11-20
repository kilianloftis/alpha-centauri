#pragma once

#include <span>
#include <string>

namespace ac
{

// "Founding a base here will destroy the Bunker. Proceed?" for the improvements founding removes.
inline std::string FormatFoundBaseConfirm(std::span<const std::string> names)
{
    std::string listed;
    for (size_t i = 0; i < names.size(); ++i)
    {
        if (i > 0)
        {
            listed += i + 1 == names.size() ? " and " : ", ";
        }
        listed += "the " + names[i];
    }
    return "Founding a base here will destroy " + listed + ". Proceed?";
}

} // namespace ac
