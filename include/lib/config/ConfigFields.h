#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

// Shared parsing for the fields that nearly every config entry carries (id, display name,
// tech prerequisite, string lists). Keeps the string<->key conventions in one place so the
// individual ConfigParsers don't each re-implement the same nlohmann accessors.
namespace ConfigFields
{

// Required unique id. Reads j["id"], throwing if it is absent.
std::string ParseId(const nlohmann::json& j);

// Display name. Reads j[key] (default key "name"), falling back to `id` when absent so an
// entry never has an empty name.
std::string ParseName(const nlohmann::json& j, const std::string& id, const char* key = "name");

// Single tech prerequisite. Reads j["required_tech"], or "" when the entry is not tech-gated.
std::string ParseRequiredTech(const nlohmann::json& j);

// Reads a string array at j[key]. Returns {} if the key is absent (missing == empty list).
std::vector<std::string> ParseStringArray(const nlohmann::json& j, const std::string& key);

} // namespace ConfigFields

} // namespace ac
