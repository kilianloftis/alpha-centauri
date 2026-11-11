#include "game/effects/InteractionGridsConfigParser.h"
#include "game/effects/EffectConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <string>
#include <vector>

namespace ac
{
namespace
{

InteractionCell_t ParseCell_(const ConfigReader& rRow, const char* pCol)
{
    const std::string cell = rRow.String(pCol);
    if (cell == "allow")
    {
        return InteractionCell_t::Allow;
    }
    if (cell == "deny")
    {
        return InteractionCell_t::Deny;
    }
    rRow.FailKey(pCol, "is not a known cell: '" + cell + "' (expected allow or deny)");
}

void RequireAxes_(const ConfigReader& rGrid, const char* pA, const char* pB)
{
    const std::vector<std::string> axes = rGrid.StringArray("axes");
    if (axes.size() != 2 || axes[0] != pA || axes[1] != pB)
    {
        rGrid.FailKey("axes", std::string("must be [\"") + pA + "\", \"" + pB + "\"]");
    }
}

const char* DomainKey_(UnitDomain_t domain)
{
    switch (domain)
    {
    case UnitDomain_t::Land:
        return "land";
    case UnitDomain_t::Sea:
        return "sea";
    case UnitDomain_t::Air:
        return "air";
    case UnitDomain_t::Orbital:
        return "orbital";
    }
    return "land";
}

const char* SurfaceKey_(InteractionSurface_t surface)
{
    return surface == InteractionSurface_t::Land ? "land" : "water";
}

const char* FootingKey_(InteractionFooting_t footing)
{
    switch (footing)
    {
    case InteractionFooting_t::Land:
        return "land";
    case InteractionFooting_t::Water:
        return "water";
    case InteractionFooting_t::Embarked:
        return "embarked";
    }
    return "land";
}

inline constexpr UnitDomain_t k_Domains[] = {UnitDomain_t::Land, UnitDomain_t::Sea,
                                            UnitDomain_t::Air, UnitDomain_t::Orbital};
inline constexpr InteractionSurface_t k_Surfaces[] = {InteractionSurface_t::Land,
                                                      InteractionSurface_t::Water};
inline constexpr InteractionFooting_t k_Footings[] = {InteractionFooting_t::Land,
                                                      InteractionFooting_t::Water,
                                                      InteractionFooting_t::Embarked};

// Every grid is rows-of-domains; only the column axis differs, so the column keys and
// index function are passed in. Adding a grid means adding a call, not another parser.
template <std::size_t ColCount, typename ColEnum, std::size_t ColN, typename KeyFn,
          typename IndexFn>
void ParseGrid_(const ConfigReader& rCells,
                std::array<std::array<InteractionCell_t, ColCount>, k_UnitDomainCount>& rOut,
                const ColEnum (&rCols)[ColN], KeyFn colKey, IndexFn colIndex)
{
    for (UnitDomain_t row : k_Domains)
    {
        const ConfigReader rowReader = rCells.Object(DomainKey_(row));
        for (ColEnum col : rCols)
        {
            rOut[UnitDomainIndex(row)][colIndex(col)] = ParseCell_(rowReader, colKey(col));
        }
    }
}

// Validates the grid block's shape and hands back its "cells" object.
ConfigReader RequireGrid_(const ConfigReader& rReader, const char* pGridName, const char* pA,
                          const char* pB)
{
    const ConfigReader grid = rReader.Object(pGridName);
    RequireAxes_(grid, pA, pB);
    return grid.Object("cells");
}

InteractionGridsConfig_t ParseGrids_(const ConfigReader& rReader)
{
    InteractionGridsConfig_t config;

    ParseGrid_(RequireGrid_(rReader, "enter", "actor_domain", "surface"), config.enter,
               k_Surfaces, SurfaceKey_, SurfaceIndex);

    ParseGrid_(RequireGrid_(rReader, "attack_tile", "actor_domain", "footing"),
               config.attackTile, k_Footings, FootingKey_, FootingIndex);

    ParseGrid_(RequireGrid_(rReader, "attack_unit", "actor_domain", "target_domain"),
               config.attackUnit, k_Domains, DomainKey_, UnitDomainIndex);

    ParseGrid_(RequireGrid_(rReader, "zoc", "actor_domain", "target_domain"), config.zoc,
               k_Domains, DomainKey_, UnitDomainIndex);

    return config;
}

} // namespace

InteractionGridsConfig_t InteractionGridsConfigParser::ParseConfig(
    const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<InteractionGridsConfig_t>(
        configPath, "interaction grids", ParseGrids_);
}

} // namespace ac
