#include "game/population/pop-types/GrowthConfigParser.h"
#include "lib/LuaRuntime.h"
#include <iostream>

namespace ac
{

GrowthConfig_t GrowthConfig_tParser::ParseConfig(const std::string& scriptPath, LuaRuntime& rLua)
{
    GrowthConfig_t config;
    config.thresholdFormula = "base_size * 10";

    sol::state& lua = rLua.GetState();

    sol::protected_function_result result =
        lua.safe_script_file(scriptPath, sol::script_pass_on_error);

    if (!result.valid())
    {
        sol::error err = result;
        std::cout << "Warning: Failed to load pop growth script '" << scriptPath
                  << "': " << err.what() << "\n";
        return config;
    }

    sol::table tbl = result;
    config.thresholdFormula = tbl.get_or("threshold_formula", std::string("base_size * 10"));

    return config;
}

} // namespace ac
