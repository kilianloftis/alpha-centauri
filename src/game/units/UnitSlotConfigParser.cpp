#include "game/units/UnitSlotConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

namespace ac
{

namespace
{

UnitSlotConfig_t ParseSlot_(const ConfigReader& rReader)
{
    UnitSlotConfig_t slot;
    slot.id = rReader.NonEmptyString("id");
    slot.displayName = rReader.String("display_name");
    slot.componentType = rReader.String("component_type");
    slot.required = rReader.BoolOr("required", true);
    slot.column = rReader.Has("column") ? rReader.Enum<SlotColumn_t>("column", "slot column")
                                        : SlotColumn_t::Left;
    slot.costModifier = rReader.FloatOr("cost_modifier", 1.0f);
    slot.requiredTech = rReader.StringOr("required_tech", "");
    return slot;
}

} // namespace

std::vector<UnitSlotConfig_t> UnitSlotConfigParser::ParseConfig(const std::string& rConfigPath) const
{
    return JsonConfigLoader::LoadFile<UnitSlotConfig_t>(rConfigPath, "unit slot", ParseSlot_);
}

} // namespace ac
