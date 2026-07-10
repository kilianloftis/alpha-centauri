#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "lib/LuaRuntime.h"
#include <stdexcept>

namespace ac
{

PopCompositionConfig_t PopCompositionConfigParser::ParseConfig(const std::string& scriptPath,
                                                             LuaRuntime& rLua)
{
    PopCompositionConfig_t config;

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
    config.droneTypeId   = tbl.get_or("drone_type",     std::string(""));
    config.talentTypeId  = tbl.get_or("talent_type",    std::string(""));

    if (config.droneTypeId.empty())
    {
        throw std::runtime_error("pop composition script '" + scriptPath
                                 + "' must set drone_type to a pop type id");
    }
    if (config.talentTypeId.empty())
    {
        throw std::runtime_error("pop composition script '" + scriptPath
                                 + "' must set talent_type to a pop type id");
    }

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
