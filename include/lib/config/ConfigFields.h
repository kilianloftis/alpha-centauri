#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

// Parsing for the fields most config entries carry: id, name, required tech, string lists,
// integers.
namespace ConfigFields
{

// Required unique id. Reads j["id"], throwing if it is absent.
std::string ParseId(const nlohmann::json& j);

// Required display name at j[key]. id names the entry in the error when it is absent.
std::string ParseName(const nlohmann::json& j, const std::string& id, const char* key);

// Single tech prerequisite. Reads j["required_tech"], or "" when the entry is not tech-gated.
std::string ParseRequiredTech(const nlohmann::json& j);

// Reads a string array at j[key]. Returns {} if the key is absent (missing == empty list).
std::vector<std::string> ParseStringArray(const nlohmann::json& j, const std::string& key);

// The integer at j[key]. Throws "<rContext>: '<key>' must be an integer" when it is absent or
// not one.
int RequireInt(const nlohmann::json& j, const std::string& key, const std::string& rContext);

// RequireInt, also throwing "<rContext>: '<key>' must be an integer >= <minimum>" below minimum.
int RequireIntAtLeast(const nlohmann::json& j, const std::string& key, int minimum,
                      const std::string& rContext);

// Throws "<rContext>: unknown key '<key>'" for the first key of j not in rKnown.
void RejectUnknownKeys(const nlohmann::json& j, const std::vector<std::string>& rKnown,
                       const std::string& rContext);

} // namespace ConfigFields

} // namespace ac
