#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace ac
{

// A configured Lua expression and the variables it may read. LuaRuntime evaluates it with
// exactly those variables; reading any other name is an error. When scriptPath is set, that
// script is loaded into the runtime before eval so helpers the expression calls are defined.
struct Formula_t
{
    std::string expression;
    std::vector<std::string> variables;
    std::string scriptPath;
};

using FormulaVars_t = std::unordered_map<std::string, double>;

} // namespace ac
