#include "game/terraform/TerraformSpreadCalculator.h"

#include "lib/Formula.h"
#include "lib/LuaRuntime.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

namespace
{

Formula_t AttemptsFormula_()
{
    return Formula_t{"math.floor(map_tiles / (math.floor(turn / 4) + 32))",
                     {"map_tiles", "turn"}};
}

Formula_t ScoreFormula_()
{
    return Formula_t{"4 * (3 - moisture) - rockiness", {"moisture", "rockiness"}};
}

} // namespace

TEST_CASE("TerraformSpreadCalculator GrowthAttempts matches SMAC alien_fauna formula",
          "[terraform][spread][calculator]")
{
    LuaRuntime lua;
    const Formula_t attempts = AttemptsFormula_();
    const TerraformSpreadCalculator calculator(attempts, lua);

    CHECK(calculator.GrowthAttempts(3200, 0) == 100);   // 3200 / 32
    CHECK(calculator.GrowthAttempts(3200, 100) == 56);  // 3200 / (25 + 32)
    CHECK(calculator.GrowthAttempts(0, 0) == 0);
}

TEST_CASE("TerraformSpreadCalculator NeighborScore prefers arid flat tiles",
          "[terraform][spread][calculator]")
{
    LuaRuntime lua;
    const Formula_t attempts = AttemptsFormula_();
    const Formula_t score = ScoreFormula_();
    const TerraformSpreadCalculator calculator(attempts, lua);

    // Arid Flat = 4*(3-0)-0 = 12; Wet Rocky = 4*(3-2)-2 = 2.
    CHECK(calculator.NeighborScore(score, /*moisture=*/0, /*rockiness=*/0) == 12);
    CHECK(calculator.NeighborScore(score, /*moisture=*/2, /*rockiness=*/2) == 2);
}
