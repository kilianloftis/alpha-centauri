#pragma once

#include <sol/sol.hpp>

#include <string>

namespace ac
{

class LuaRuntime;

// Loads Lua config scripts that return a table of formula strings.
namespace LuaConfigLoader
{

// The table the script at rScriptPath returns. `noun` names the script in errors
// (e.g. "tech cost").
sol::table RunScript(LuaRuntime& rLua, const std::string& rScriptPath, const char* noun);

// The non-empty Lua expression the script set at pKey.
std::string RequireFormula(const sol::table& rTable, const char* pKey,
                           const std::string& rScriptPath, const char* noun);

} // namespace LuaConfigLoader
} // namespace ac
