#include "game/faction/base/population/pop-types/PopCompositionConfigParser.h"
#include "lib/LuaRuntime.h"
#include <iostream>

namespace ac
{

PopCompositionConfig PopCompositionConfigParser::ParseConfig(const std::string& scriptPath,
                                                             LuaRuntime& rLua)
{
    PopCompositionConfig config;
    config.defaultType  = "Worker";
    config.precedence   = {"Talent", "Drone", "Worker"};

    sol::state& lua = rLua.GetState();

    sol::protected_function_result result =
        lua.safe_script_file(scriptPath, sol::script_pass_on_error);

    if (!result.valid())
    {
        sol::error err = result;
        std::cout << "Warning: Failed to load pop composition script '" << scriptPath
                  << "': " << err.what() << "\n";
        return config;
    }

    sol::table tbl = result;

    config.defaultType  = tbl.get_or("default_type",  std::string("Worker"));
    config.droneFormula  = tbl.get_or("drone_formula",  std::string(""));
    config.talentFormula = tbl.get_or("talent_formula", std::string(""));

    sol::optional<sol::table> precedence = tbl["precedence"];
    if (precedence)
    {
        config.precedence.clear();
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
