#pragma once

namespace ac
{

// Movement / ZOC domain. Required on chassis components; land = neither sea nor air.
enum class UnitDomain_t
{
    Land,
    Sea,
    Air,
};

} // namespace ac
