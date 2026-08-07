#pragma once

#include <magic_enum.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ac
{

inline std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

// Case-insensitive enum lookup by enumerator name, for wire formats whose values match the
// enumerator spelling. rWhat names the field in the error, e.g. "landmark domain".
template <typename Enum_t>
Enum_t EnumFromName(const std::string& rValue, std::string_view what)
{
    const std::string normalized = ToLowerAscii(rValue);
    for (const Enum_t value : magic_enum::enum_values<Enum_t>())
    {
        if (ToLowerAscii(std::string(magic_enum::enum_name(value))) == normalized)
        {
            return value;
        }
    }
    throw std::runtime_error("Unknown " + std::string(what) + ": '" + rValue + "'");
}

// The wire spelling of an enumerator: its name, lowercased.
template <typename Enum_t>
std::string EnumToLowerName(Enum_t value)
{
    const auto name = magic_enum::enum_name(value);
    if (name.empty())
    {
        throw std::runtime_error("Enum value has no name");
    }
    return ToLowerAscii(std::string(name));
}

} // namespace ac
