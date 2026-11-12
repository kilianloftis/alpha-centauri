#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace ac
{

// A configured Lua expression and the variables it may read. LuaRuntime evaluates it with
// exactly those variables; reading any other name is an error.
struct Formula_t
{
    std::string expression;
    std::vector<std::string> variables;
};

using FormulaVars_t = std::unordered_map<std::string, double>;

} // namespace ac
