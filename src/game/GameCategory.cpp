#include "game/GameCategory.h"

#include "lib/config/EnumNames.h"

namespace ac
{

std::string GameCategoryToString(GameCategory_t category)
{
    return EnumToLowerName(category);
}

GameCategory_t ParseGameCategory(const std::string& category)
{
    return EnumFromName<GameCategory_t>(category, "game category");
}

GameCategory_t ParseGameCategoryField(const nlohmann::json& j, const char* key)
{
    return ParseGameCategory(j.at(key).get<std::string>());
}

} // namespace ac
