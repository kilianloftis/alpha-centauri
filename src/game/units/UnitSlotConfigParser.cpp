#include "game/units/UnitSlotConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include <magic_enum.hpp>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ac
{

namespace
{

// Wire form differs from the enumerator only by case, but an unknown value is a config error
// rather than a silent left-column slot.
SlotColumn_t ParseColumn_(const std::string& rColumn, const std::string& rSlotId)
{
    const auto parsed = magic_enum::enum_cast<SlotColumn_t>(rColumn, magic_enum::case_insensitive);
    if (!parsed)
    {
        throw std::runtime_error("Unit slot '" + rSlotId + "': unknown column '" + rColumn
                                 + "'; expected 'left' or 'right'");
    }
    return *parsed;
}

} // namespace

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
            slot.column        = ParseColumn_(rSlotJson.value("column", std::string("left")),
                                              slot.id);
            slot.costModifier  = rSlotJson.value("cost_modifier", 1.0f);
            slot.requiredTech  = ConfigFields::ParseRequiredTech(rSlotJson);
            return slot;
        });
}

} // namespace ac
