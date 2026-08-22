#include "game/DifficultyConfig.h"

#include <stdexcept>

namespace ac
{

const DifficultyLevel_t* DifficultyConfig_t::FindById(const std::string& rId) const
{
    for (const DifficultyLevel_t& rLevel : levels)
    {
        if (rLevel.id == rId)
        {
            return &rLevel;
        }
    }
    return nullptr;
}

const DifficultyLevel_t& DifficultyConfig_t::RequireForSession(
    const std::string& rDifficultyId) const
{
    const std::string& rWanted = rDifficultyId.empty() ? defaultId : rDifficultyId;
    if (const DifficultyLevel_t* pLevel = FindById(rWanted))
    {
        return *pLevel;
    }
    throw std::runtime_error("Unknown difficulty id '" + rWanted + "'");
}

} // namespace ac
