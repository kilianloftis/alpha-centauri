#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/Faction.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "ui/world/MapAppearance.h"

#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <string>

using namespace ac;

namespace
{

bool Shows_(const MapAppearance& rAppearance, const Tile& rTile, const std::string& rId)
{
    return rAppearance.OccupantsOf(rTile).ForEach(
        [&rId](const ImprovementConfig_t& rOccupant) { return rOccupant.id == rId; });
}

} // namespace

TEST_CASE("An appearance covers each tile by what its viewer knows of it", "[ui][map][appearance]")
{
    actest::FactionFixture fixture;
    Faction& viewer = fixture.MakeFaction();
    const ImprovementConfig_t& rCondenser = fixture.improvements.Get("Condenser");

    Unit& rUnit = fixture.MakeUnit(viewer, 8, 8, {"test_chassis"});
    Tile& rRemembered = fixture.At(8, 8);
    fixture.MoveUnit(rUnit, 12, 12);
    const Tile& rVisible = fixture.At(12, 12);
    const Tile& rUnexplored = fixture.At(30, 14);
    REQUIRE(viewer.GetExploredMap().IsExplored(rRemembered));
    REQUIRE_FALSE(viewer.GetVisibleMap().IsVisible(rRemembered));
    REQUIRE(viewer.GetVisibleMap().IsVisible(rVisible));
    REQUIRE_FALSE(viewer.GetExploredMap().IsExplored(rUnexplored));

    // Built out of the viewer's sight, so only the live tile has it.
    rRemembered.AddImprovement(rCondenser);

    SECTION("a fogged appearance shrouds unexplored tiles and fogs remembered ones")
    {
        const MapAppearance appearance = MapAppearance::Fogged(fixture.map, &viewer);
        CHECK(appearance.CoverOf(rUnexplored) == TileCover_t::Shroud);
        CHECK(appearance.CoverOf(rRemembered) == TileCover_t::Fog);
        CHECK(appearance.CoverOf(rVisible) == TileCover_t::None);
        CHECK_FALSE(Shows_(appearance, rRemembered, rCondenser.id));
    }

    SECTION("a clear appearance draws remembered tiles clear, still as the viewer remembers them")
    {
        const MapAppearance appearance = MapAppearance::Clear(fixture.map, &viewer);
        CHECK(appearance.CoverOf(rUnexplored) == TileCover_t::Shroud);
        CHECK(appearance.CoverOf(rRemembered) == TileCover_t::None);
        CHECK(appearance.CoverOf(rVisible) == TileCover_t::None);
        CHECK_FALSE(Shows_(appearance, rRemembered, rCondenser.id));
    }

    SECTION("without a viewer every tile is live and clear")
    {
        for (const MapAppearance& rAppearance : {MapAppearance::Fogged(fixture.map, nullptr),
                                                 MapAppearance::Clear(fixture.map, nullptr)})
        {
            CHECK(rAppearance.CoverOf(rUnexplored) == TileCover_t::None);
            CHECK(rAppearance.CoverOf(rRemembered) == TileCover_t::None);
            CHECK(Shows_(rAppearance, rRemembered, rCondenser.id));
        }
    }
}
