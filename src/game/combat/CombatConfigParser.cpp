#include "game/combat/CombatConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "lib/config/EnumNames.h"
#include "lib/config/JsonConfigLoader.h"

#include <string>

namespace ac
{

namespace
{

void ParseDefenseSlot_(const ConfigReader& rSlot, CombatConfig_t& rConfig)
{
    rSlot.RejectUnknownKeys({"id", "kinds", "combine"});
    StatSlot_t slot;
    slot.id = rSlot.NonEmptyString("id");
    if (DeclaresDefenseSlot(rConfig, slot.id))
    {
        rSlot.FailKey("id", "is declared twice: '" + slot.id + "'");
    }
    if (rSlot.Has("combine"))
    {
        slot.combine = EnumFromName<SlotCombine_t>(rSlot.String("combine"), "slot combine");
    }
    const std::vector<std::string> kinds = rSlot.StringArray("kinds");
    if (kinds.empty())
    {
        rSlot.FailKey("kinds", "must name at least one kind of fight");
    }
    for (const std::string& rKind : kinds)
    {
        const CombatKindId_t kind = EnumFromName<CombatKindId_t>(rKind, "combat kind");
        rConfig.defenseSlotsByKind[kind].push_back(slot);
    }
}

} // namespace

CombatConfig_t CombatConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<CombatConfig_t>(
        configPath, "combat", [](const ConfigReader& rReader) {
            rReader.RejectUnknownKeys({"boarding_home_base_min_pop", "defense_slots", "effects"});
            CombatConfig_t config;
            config.boardingHomeBaseMinPop = rReader.IntAtLeast("boarding_home_base_min_pop", 0);
            for (const ConfigReader& rSlot : rReader.Objects("defense_slots"))
            {
                ParseDefenseSlot_(rSlot, config);
            }
            config.effects = EffectConfigParser::ParseEffects(
                rReader.Json(), EffectSourceKind_t::WorldRules, "combat");
            return config;
        });
}

} // namespace ac
