#include "game/faction/DiplomaticStatus.h"

namespace ac
{

std::string ToString(DiplomaticStatus_t status)
{
    return std::string(magic_enum::enum_name(status));
}

} // namespace ac
