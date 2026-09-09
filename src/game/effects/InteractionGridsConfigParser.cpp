#include "game/effects/InteractionGridsConfigParser.h"

#include "game/effects/EffectConfigParser.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace ac
{
namespace
{

InteractionCell_t ParseCell_(const nlohmann::json& rValue, const std::string& path)
{
    if (!rValue.is_string())
    {
        throw std::runtime_error("interaction_grids " + path + " must be a string");
    }
    const std::string s = rValue.get<std::string>();
    if (s == "allow")
    {
        return InteractionCell_t::Allow;
    }
    if (s == "deny")
    {
        return InteractionCell_t::Deny;
    }
    throw std::runtime_error("interaction_grids " + path + ": unknown cell '" + s + "'");
}

void RequireAxes_(const nlohmann::json& rGrid, const char* pGridName, const char* pA,
                  const char* pB)
{
    if (!rGrid.contains("axes") || !rGrid.at("axes").is_array()
        || rGrid.at("axes").size() != 2)
    {
        throw std::runtime_error(std::string("interaction_grids '") + pGridName
                                 + "' requires axes [a, b]");
    }
    if (rGrid.at("axes")[0].get<std::string>() != pA
        || rGrid.at("axes")[1].get<std::string>() != pB)
    {
        throw std::runtime_error(std::string("interaction_grids '") + pGridName
                                 + "' axes must be [\"" + pA + "\", \"" + pB + "\"]");
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
void ParseGrid_(const nlohmann::json& rCells,
                std::array<std::array<InteractionCell_t, ColCount>, k_UnitDomainCount>& rOut,
                const char* pGridName, const ColEnum (&rCols)[ColN], KeyFn colKey,
                IndexFn colIndex)
{
    for (UnitDomain_t row : k_Domains)
    {
        const char* pRow = DomainKey_(row);
        if (!rCells.contains(pRow) || !rCells.at(pRow).is_object())
        {
            throw std::runtime_error(std::string("interaction_grids '") + pGridName
                                     + "' missing row '" + pRow + "'");
        }
        const nlohmann::json& rRow = rCells.at(pRow);
        for (ColEnum col : rCols)
        {
            const char* pCol = colKey(col);
            if (!rRow.contains(pCol))
            {
                throw std::runtime_error(std::string("interaction_grids '") + pGridName
                                         + "' missing cell " + pRow + "/" + pCol);
            }
            rOut[UnitDomainIndex(row)][colIndex(col)] =
                ParseCell_(rRow.at(pCol), std::string(pGridName) + "." + pRow + "." + pCol);
        }
    }
}

// Validates the grid block's shape and hands back its "cells" object.
const nlohmann::json& RequireGrid_(const nlohmann::json& rJson, const char* pGridName,
                                   const char* pA, const char* pB)
{
    if (!rJson.contains(pGridName) || !rJson.at(pGridName).is_object())
    {
        throw std::runtime_error(std::string("interaction_grids missing object '") + pGridName
                                 + "'");
    }
    const nlohmann::json& rGrid = rJson.at(pGridName);
    RequireAxes_(rGrid, pGridName, pA, pB);
    if (!rGrid.contains("cells") || !rGrid.at("cells").is_object())
    {
        throw std::runtime_error(std::string("interaction_grids '") + pGridName
                                 + "' missing object 'cells'");
    }
    return rGrid.at("cells");
}

} // namespace

InteractionGridsConfig_t InteractionGridsConfigParser::ParseConfig(
    const std::string& configPath) const
{
    std::ifstream file(configPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open interaction grids '" + configPath + "'");
    }

    const nlohmann::json json = nlohmann::json::parse(file);
    InteractionGridsConfig_t config;

    ParseGrid_(RequireGrid_(json, "enter", "actor_domain", "surface"), config.enter, "enter",
               k_Surfaces, SurfaceKey_, SurfaceIndex);

    ParseGrid_(RequireGrid_(json, "attack_tile", "actor_domain", "footing"), config.attackTile,
               "attack_tile", k_Footings, FootingKey_, FootingIndex);

    ParseGrid_(RequireGrid_(json, "attack_unit", "actor_domain", "target_domain"),
               config.attackUnit, "attack_unit", k_Domains, DomainKey_, UnitDomainIndex);

    ParseGrid_(RequireGrid_(json, "zoc", "actor_domain", "target_domain"), config.zoc, "zoc",
               k_Domains, DomainKey_, UnitDomainIndex);

    return config;
}

} // namespace ac
