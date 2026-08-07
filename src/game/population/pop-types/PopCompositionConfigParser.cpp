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

    const auto requireString = [&](const char* key) {
        const std::string value = tbl.get_or(key, std::string(""));
        if (value.empty())
        {
            throw std::runtime_error("pop composition script '" + scriptPath + "' must set "
                                     + key + " to a non-empty value");
        }
        return value;
    };

    config.droneFormula = requireString("drone_formula");
    config.talentFormula = requireString("talent_formula");
    config.droneTypeId = requireString("drone_type");
    config.talentTypeId = requireString("talent_type");

    // TODO: recalculation order is not implemented. Rejected rather than ignored so a modder
    // who sets it learns it does nothing.
    if (tbl["precedence"].valid())
    {
        throw std::runtime_error("pop composition script '" + scriptPath
                                 + "' sets 'precedence', which is not implemented; remove it");
    }

    return config;
}

} // namespace ac
