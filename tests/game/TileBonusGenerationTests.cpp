#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"
#include "game/map/TileBonusGeneration.h"
#include "game/map/WorldGenDecorationConfigParser.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <random>

using namespace ac;
using Catch::Approx;

namespace
{

void FillLand_(WorldMap& rWorld)
{
    for (auto& pTile : rWorld.GetTiles())
    {
        pTile->SetElevation(1000);
    }
}

} // namespace

TEST_CASE("WorldGenDecorationConfigParser loads tile_bonuses knobs",
          "[worldgen][tile-bonus][parser]")
{
    WorldGenDecorationConfigParser parser;
    const WorldGenDecorationConfig_t config =
        parser.ParseConfig(std::string(AC_TEST_FIXTURES_DIR) + "/../../config/worldGen/decoration.json");

    CHECK(config.tileBonuses.landFraction == Approx(0.04f));
}

TEST_CASE("PlaceTileBonuses stamps frequency-weighted improvements on land",
          "[worldgen][tile-bonus]")
{
    ImprovementRegistry improvements;
    improvements.Load(std::string(AC_TEST_FIXTURES_DIR) + "/improvements.json");

    WorldMap world(24, 24);
    FillLand_(world);

    TileBonusDecorationConfig_t cfg;
    cfg.landFraction = 0.2f;

    std::mt19937 rng(21);
    const int placed = PlaceTileBonuses(world, cfg, improvements, rng);
    REQUIRE(placed > 0);

    int bonusTiles = 0;
    int monoliths = 0;
    for (const auto& pTile : world.GetTiles())
    {
        if (!pTile)
        {
            continue;
        }
        const bool hasBonus =
            pTile->HasImprovement("Nutrients") || pTile->HasImprovement("Minerals")
            || pTile->HasImprovement("Energy") || pTile->HasImprovement("Monolith");
        if (hasBonus)
        {
            ++bonusTiles;
        }
        if (pTile->HasImprovement("Monolith"))
        {
            ++monoliths;
        }
    }

    CHECK(bonusTiles == placed);
    CHECK(monoliths > 0);
    CHECK(bonusTiles >= static_cast<int>(0.1f * 24 * 24));
    CHECK(bonusTiles <= static_cast<int>(0.3f * 24 * 24));
}
