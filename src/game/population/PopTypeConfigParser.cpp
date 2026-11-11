#include "game/population/PopTypeConfigParser.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/EffectConfigParser.h"

namespace ac
{

namespace
{

char ParseDisplayGlyph_(const ConfigReader& rReader)
{
    const std::string glyph = rReader.String("display_glyph");
    if (glyph.size() != 1)
    {
        rReader.FailKey("display_glyph", "must be exactly one character, got '" + glyph + "'");
    }
    return glyph[0];
}

PopTypeConfig_t ParsePopType_(const ConfigReader& rReader)
{
    PopTypeConfig_t config;
    config.id = rReader.NonEmptyString("id");
    config.name = rReader.String("name");
    config.displayGlyph = ParseDisplayGlyph_(rReader);
    config.bIsDefault = rReader.BoolOr("is_default", false);
    config.bCanWorkTile = rReader.BoolOr("can_work_tile", false);
    config.bPlayerAssignable = rReader.BoolOr("player_assignable", false);
    config.requiredTech = rReader.StringOr("required_tech", "");
    config.fallbackPopTypeId = rReader.StringOr("fallback_pop_type", "");
    config.obsoletes = rReader.StringArray("obsoletes");
    config.effects = EffectConfigParser::ParseEffects(rReader.Json(), EffectSourceKind_t::PopType,
                                                      config.id);

    config.psychToPromote = rReader.Has("psych_to_promote")
                                ? rReader.IntAtLeast("psych_to_promote", 0)
                                : 0;
    config.promotesToId = rReader.StringOr("promotes_to", "");
    if (config.promotesToId.empty() != (config.psychToPromote == 0))
    {
        rReader.Fail("psych_to_promote and promotes_to must be set together");
    }
    config.droneWeight = rReader.Has("drone_weight") ? rReader.IntAtLeast("drone_weight", 0) : 0;
    return config;
}

} // namespace

std::vector<PopTypeConfig_t> PopTypeConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadFile<PopTypeConfig_t>(configPath, "pop type", ParsePopType_);
}

} // namespace ac
