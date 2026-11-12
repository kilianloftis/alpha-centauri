#pragma once

#include "lib/Formula.h"

#include <sol/sol.hpp>

#include <string>
#include <vector>

namespace ac
{

class LuaRuntime;

// Loads Lua config scripts that return a table of formula strings.
namespace LuaConfigLoader
{

// The table the script at rScriptPath returns. `noun` names the script in errors
// (e.g. "tech cost").
sol::table RunScript(LuaRuntime& rLua, const std::string& rScriptPath, const char* noun);

// The Lua expression the script set at pKey, reading only rVariables, checked by
// LuaRuntime::CheckFormula in rLua so it can call the script's functions.
Formula_t RequireFormula(LuaRuntime& rLua, const sol::table& rTable, const char* pKey,
                         std::vector<std::string> variables, const std::string& rScriptPath,
                         const char* noun);

} // namespace LuaConfigLoader
} // namespace ac
