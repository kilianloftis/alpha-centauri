#include "TestHelpers.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"
#include "game/map/TileBonusGeneration.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>
#include <random>

using namespace ac;

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

TEST_CASE("PlaceTileBonuses stamps frequency-weighted improvements on viable tiles",
          "[worldgen][tile-bonus]")
{
    ImprovementRegistry terrain;
    terrain.LoadOccupants(std::string(AC_TEST_FIXTURES_DIR) + "/improvements.json",
                          std::string(AC_TEST_FIXTURES_DIR) + "/terrain.json");

    WorldMap world(24, 24, actest::TestMapRules());
    FillLand_(world);

    TileBonusDecorationConfig_t cfg;
    cfg.fraction = 0.2f;

    std::mt19937 rng(21);
    for (auto& pTile : world.GetTiles())
    {
        pTile->BindOccupants(terrain);
    }
    const int placed = PlaceTileBonuses(world, cfg, terrain, rng);
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
            pTile->HasFeature("Nutrients") || pTile->HasFeature("Minerals")
            || pTile->HasImprovement("Energy") || pTile->HasFeature("Monolith");
        if (hasBonus)
        {
            ++bonusTiles;
        }
        if (pTile->HasFeature("Monolith"))
        {
            ++monoliths;
        }
    }

    CHECK(bonusTiles == placed);
    CHECK(monoliths > 0);
    const float tileCount = static_cast<float>(world.GetTiles().size());
    CHECK(bonusTiles >= static_cast<int>(0.1f * tileCount));
    CHECK(bonusTiles <= static_cast<int>(0.3f * tileCount));
}

TEST_CASE("PlaceTileBonuses stamps water tiles the bonus entry can occupy",
          "[worldgen][tile-bonus]")
{
    ImprovementRegistry terrain;
    terrain.LoadOccupants(std::string(AC_TEST_FIXTURES_DIR) + "/improvements.json",
                          std::string(AC_TEST_FIXTURES_DIR) + "/terrain.json");

    WorldMap world(16, 16, actest::TestMapRules());
    for (auto& pTile : world.GetTiles())
    {
        pTile->SetElevation(pTile->GetX() < 8 ? 1000 : -500);
        pTile->BindOccupants(terrain);
    }

    TileBonusDecorationConfig_t cfg;
    cfg.fraction = 0.5f;

    std::mt19937 rng(4);
    const int placed = PlaceTileBonuses(world, cfg, terrain, rng);
    REQUIRE(placed > 0);

    int landBonuses = 0;
    int waterBonuses = 0;
    for (const auto& pTile : world.GetTiles())
    {
        const bool hasBonus = pTile->HasFeature("Nutrients") || pTile->HasFeature("Monolith");
        if (!hasBonus)
        {
            continue;
        }
        if (pTile->IsLand())
        {
            ++landBonuses;
        }
        if (pTile->IsWater())
        {
            ++waterBonuses;
        }
    }
    CHECK(landBonuses > 0);
    CHECK(waterBonuses > 0);
    CHECK(landBonuses + waterBonuses == placed);
}
