#include "game/faction/DiplomaticStatus.h"

namespace ac
{

std::string ToString(DiplomaticStatus_t status)
{
    if (status == DiplomaticStatus_t::Neutral)
    {
        return {};
    }
    return std::string(magic_enum::enum_name(status));
}

} // namespace ac
