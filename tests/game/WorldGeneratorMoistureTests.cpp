#include "game/map/MoistureGeneration.h"
#include "game/map/Tile.h"
#include "game/map/WorldGenDecorationConfigParser.h"
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

TEST_CASE("WorldGenDecorationConfigParser loads moisture knobs from decoration.json",
          "[worldgen][moisture][parser]")
{
    WorldGenDecorationConfigParser parser;
    const WorldGenDecorationConfig_t config =
        parser.ParseConfig(std::string(AC_TEST_FIXTURES_DIR) + "/../../config/worldGen/decoration.json");

    const MoistureDecorationConfig_t& m = config.moisture;
    CHECK(m.baseMin == Catch::Approx(0.25f));
    CHECK(m.baseRange == Catch::Approx(0.5f));
    CHECK(m.coastalPeakBonus == Catch::Approx(0.12f));
    CHECK(m.coastalRadius == 2);
    CHECK(m.tropicalPeakBonus == Catch::Approx(0.1f));
    CHECK(m.tropicalHalfWidth == Catch::Approx(0.35f));
    CHECK(m.orographicStrength == Catch::Approx(0.45f));
    CHECK(m.orographicElevScale == Catch::Approx(1000.0f));
    CHECK(m.orographicMaxElev == Catch::Approx(4000.0f));
    CHECK(m.aridThreshold == Catch::Approx(0.4f));
    CHECK(m.moistThreshold == Catch::Approx(0.7f));
}

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
    WorldMap world(11, 7);

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

    const float westFace = OrographicMoistureBias(mid, low, peak, cfg);
    const float eastFace = OrographicMoistureBias(mid, peak, low, cfg);
    const float flatLow = OrographicMoistureBias(100, 100, 100, cfg);
    const float water = OrographicMoistureBias(-50, 0, 1000, cfg);

    CHECK(westFace > 0.0f);
    CHECK(eastFace < 0.0f);
    CHECK(westFace > eastFace);
    CHECK(flatLow == Catch::Approx(0.0f));
    CHECK(water == Catch::Approx(0.0f));
}

TEST_CASE("QuantizeMoistureScore maps bands to Arid/Moist/Wet",
          "[worldgen][moisture]")
{
    const MoistureDecorationConfig_t cfg = DefaultMoisture_();
    CHECK(QuantizeMoistureScore(0.0f, cfg) == Moisture_t::Arid);
    CHECK(QuantizeMoistureScore(0.39f, cfg) == Moisture_t::Arid);
    CHECK(QuantizeMoistureScore(0.40f, cfg) == Moisture_t::Moist);
    CHECK(QuantizeMoistureScore(0.69f, cfg) == Moisture_t::Moist);
    CHECK(QuantizeMoistureScore(0.70f, cfg) == Moisture_t::Wet);
    CHECK(QuantizeMoistureScore(1.0f, cfg) == Moisture_t::Wet);
    CHECK(QuantizeMoistureScore(-1.0f, cfg) == Moisture_t::Arid);
    CHECK(QuantizeMoistureScore(2.0f, cfg) == Moisture_t::Wet);
}
