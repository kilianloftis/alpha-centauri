#include "TestHelpers.h"
#include "game/map/MoistureGeneration.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace ac;
using namespace ac::moisture_gen;

namespace
{

MoistureDecorationConfig_t DefaultMoisture_()
{
    return MoistureDecorationConfig_t{};
}

} // namespace

TEST_CASE("TropicalMoistureBonus peaks at equator and falls off toward poles",
          "[worldgen][moisture]")
{
    const MoistureDecorationConfig_t cfg = DefaultMoisture_();
    constexpr int height = 21;
    const int equatorY = height / 2;
    const int polarY = 0;

    const float equator = TropicalMoistureBonus(equatorY, height, cfg);
    const float polar = TropicalMoistureBonus(polarY, height, cfg);
    const float midTropics = TropicalMoistureBonus(equatorY - 2, height, cfg);

    CHECK(equator == Catch::Approx(cfg.tropicalPeakBonus));
    CHECK(polar == Catch::Approx(0.0f));
    CHECK(midTropics > 0.0f);
    CHECK(midTropics < equator);
    CHECK(equator > polar);
}

TEST_CASE("CoastalMoistureBonus is higher near water than inland",
          "[worldgen][moisture]")
{
    const MoistureDecorationConfig_t cfg = DefaultMoisture_();
    // Wide enough that mid-map is outside coastal radius even with X-wrap.
    WorldMap world(11, 7, actest::TestMapRules());

    // Fill with land, then a water column on the left.
    for (auto& pTile : world.GetTiles())
    {
        pTile->SetElevation(100);
    }
    for (int y = 0; y < world.GetHeight(); ++y)
    {
        world.GetTile(0, y)->SetElevation(-100);
    }

    Tile& coast = *world.GetTile(1, 3);
    Tile& inland = *world.GetTile(5, 3);

    const float coastalBonus = CoastalMoistureBonus(coast, world, cfg);
    const float inlandBonus = CoastalMoistureBonus(inland, world, cfg);

    CHECK(coastalBonus == Catch::Approx(cfg.coastalPeakBonus));
    CHECK(inlandBonus == Catch::Approx(0.0f));
    CHECK(coastalBonus > inlandBonus);

    // Water itself gets no coastal bonus.
    CHECK(CoastalMoistureBonus(*world.GetTile(0, 3), world, cfg) == Catch::Approx(0.0f));
}

TEST_CASE("OrographicMoistureBias: western face wetter than eastern face of a ridge",
          "[worldgen][moisture]")
{
    const MoistureDecorationConfig_t cfg = DefaultMoisture_();
    // Ridge peak at high elev; west neighbor lower, east neighbor lower.
    // On the western face: elevWest < local < elevEast → positive grad → wetter.
    // On the eastern face: elevWest > local > elevEast → negative grad → drier.
    constexpr int peak = 3000;
    constexpr int mid = 2000;
    constexpr int low = 500;
    constexpr int maxElev = 3500;

    const float westFace = OrographicMoistureBias(mid, low, peak, cfg, maxElev);
    const float eastFace = OrographicMoistureBias(mid, peak, low, cfg, maxElev);
    const float flatLow = OrographicMoistureBias(100, 100, 100, cfg, maxElev);
    const float water = OrographicMoistureBias(-50, 0, 1000, cfg, maxElev);

    CHECK(westFace > 0.0f);
    CHECK(eastFace < 0.0f);
    CHECK(westFace > eastFace);
    CHECK(flatLow == Catch::Approx(0.0f));
    CHECK(water == Catch::Approx(0.0f));
}

TEST_CASE("OrographicMoistureBias reaches full strength at the preset elevation ceiling",
          "[worldgen][moisture]")
{
    const MoistureDecorationConfig_t cfg = DefaultMoisture_();
    constexpr int maxElev = 3500;
    const int saturatedEast = static_cast<int>(cfg.orographicElevScale);

    const float atCeiling = OrographicMoistureBias(maxElev, 0, saturatedEast, cfg, maxElev);
    const float aboveCeiling =
        OrographicMoistureBias(maxElev + 500, 0, saturatedEast, cfg, maxElev);
    const float halfHeight = OrographicMoistureBias(maxElev / 2, 0, saturatedEast, cfg, maxElev);

    CHECK(atCeiling == Catch::Approx(cfg.orographicStrength));
    CHECK(aboveCeiling == Catch::Approx(cfg.orographicStrength));
    CHECK(halfHeight == Catch::Approx(cfg.orographicStrength * 0.5f));
    CHECK_THROWS_AS(OrographicMoistureBias(100, 0, saturatedEast, cfg, 0), std::invalid_argument);
}

TEST_CASE("QuantizeMoistureScore maps bands to Arid/Moist/Wet",
          "[worldgen][moisture]")
{
    MoistureDecorationConfig_t cfg;
    cfg.aridThreshold = 0.4f;
    cfg.moistThreshold = 0.7f;
    CHECK(QuantizeMoistureScore(cfg.aridThreshold - 0.01f, cfg) == Moisture_t::Arid);
    CHECK(QuantizeMoistureScore(cfg.aridThreshold, cfg) == Moisture_t::Moist);
    CHECK(QuantizeMoistureScore(cfg.moistThreshold - 0.01f, cfg) == Moisture_t::Moist);
    CHECK(QuantizeMoistureScore(cfg.moistThreshold, cfg) == Moisture_t::Wet);
    CHECK(QuantizeMoistureScore(-1.0f, cfg) == QuantizeMoistureScore(0.0f, cfg));
    CHECK(QuantizeMoistureScore(2.0f, cfg) == QuantizeMoistureScore(1.0f, cfg));
}
