#include "game/units/UnitSlotConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include <nlohmann/json.hpp>

namespace ac
{

std::vector<UnitSlotConfig_t> UnitSlotConfigParser::ParseConfig(const std::string& rConfigPath) const
{
    return JsonConfigLoader::LoadFile<UnitSlotConfig_t>(
        rConfigPath, "unit slot",
        [](const nlohmann::json& rSlotJson)
        {
            UnitSlotConfig_t slot;
            slot.id            = ConfigFields::ParseId(rSlotJson);
            slot.displayName   = ConfigFields::ParseName(rSlotJson, slot.id, "display_name");
            slot.componentType = rSlotJson.at("component_type").get<std::string>();
            slot.required      = rSlotJson.value("required", true);
            slot.column        = rSlotJson.value("column", std::string("left"));
            slot.costModifier  = rSlotJson.value("cost_modifier", 1.0f);
            slot.requiredTech  = ConfigFields::ParseRequiredTech(rSlotJson);
            return slot;
        });
}

} // namespace ac
