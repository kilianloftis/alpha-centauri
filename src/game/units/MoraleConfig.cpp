#include "game/units/MoraleConfig.h"

#include <stdexcept>

namespace ac
{

int MoraleConfig_t::MinLevel() const
{
    if (levels.empty())
    {
        throw std::logic_error("MoraleConfig_t::MinLevel: no levels configured");
    }
    return levels.front().index;
}

int MoraleConfig_t::MaxLevel() const
{
    if (levels.empty())
    {
        throw std::logic_error("MoraleConfig_t::MaxLevel: no levels configured");
    }
    return levels.back().index;
}

const MoraleLevel_t* MoraleConfig_t::FindLevel(int index) const
{
    for (const MoraleLevel_t& rLevel : levels)
    {
        if (rLevel.index == index)
        {
            return &rLevel;
        }
    }
    return nullptr;
}

} // namespace ac
