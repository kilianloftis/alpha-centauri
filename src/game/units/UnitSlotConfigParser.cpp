#include "game/units/UnitSlotConfigParser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>

using json = nlohmann::json;

namespace ac
{

std::vector<UnitSlotConfig_t> UnitSlotConfigParser::ParseConfig(const std::string& rConfigPath) const
{
    std::cout << "Loading unit slot configuration from: " << rConfigPath << "\n";

    std::ifstream file(rConfigPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open " + rConfigPath);
    }

    json data;
    file >> data;

    if (!data.is_array())
    {
        throw std::runtime_error("Expected array in " + rConfigPath);
    }

    std::vector<UnitSlotConfig_t> slots;
    for (const auto& rSlotJson : data)
    {
        UnitSlotConfig_t slot;
        slot.id            = rSlotJson["id"].get<std::string>();
        slot.displayName   = rSlotJson.value("display_name", slot.id);
        slot.componentType = rSlotJson["component_type"].get<std::string>();
        slot.required      = rSlotJson.value("required", true);
        slot.column        = rSlotJson.value("column", std::string("left"));
        slot.costModifier  = rSlotJson.value("cost_modifier", 1.0f);
        slot.requiredTech  = rSlotJson.value("required_tech", std::string(""));
        slots.push_back(std::move(slot));
    }

    std::cout << "Loaded " << slots.size() << " unit slot configurations\n";
    return slots;
}

} // namespace ac
