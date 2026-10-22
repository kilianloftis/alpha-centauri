#pragma once

#include <cstddef>
#include <magic_enum.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace ac
{

enum class GameCategory_t
{
    Build,
    Grow,
    Discover,
    Conquer
};

// Derived from the enum: fixed-size arrays sized by this stay in step with the enumerators.
constexpr size_t k_GameCategoryCount = magic_enum::enum_count<GameCategory_t>();

std::string GameCategoryToString(GameCategory_t category);
GameCategory_t ParseGameCategory(const std::string& category);

// Named apart from ParseGameCategory(const std::string&): a string literal converts to both a
// string and a json, so an overload would be ambiguous.
GameCategory_t ParseGameCategoryField(const nlohmann::json& j, const char* key = "category");

} // namespace ac
