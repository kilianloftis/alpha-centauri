#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "lib/LuaRuntime.h"
#include <stdexcept>

namespace ac
{

PopCompositionConfig PopCompositionConfigParser::ParseConfig(const std::string& scriptPath,
                                                             LuaRuntime& rLua)
{
    PopCompositionConfig config;

    sol::state& lua = rLua.GetState();

    sol::protected_function_result result =
        lua.safe_script_file(scriptPath, sol::script_pass_on_error);

    if (!result.valid())
    {
        sol::error err = result;
        throw std::runtime_error("Failed to load pop composition script '" + scriptPath
                                 + "': " + err.what());
    }

    sol::table tbl = result;

    config.droneFormula  = tbl.get_or("drone_formula",  std::string(""));
    config.talentFormula = tbl.get_or("talent_formula", std::string(""));

    sol::optional<sol::table> precedence = tbl["precedence"];
    if (precedence)
    {
        for (const auto& [key, val] : *precedence)
        {
            if (val.is<std::string>())
            {
                config.precedence.push_back(val.as<std::string>());
            }
        }
    }

    return config;
}

} // namespace ac
