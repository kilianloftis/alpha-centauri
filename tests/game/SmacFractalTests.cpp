#include "game/map/SmacFractal.h"

#include <catch2/catch_test_macros.hpp>

#include <array>

using namespace ac;

namespace
{

std::array<std::array<int, SmacFractal::k_LatticeSize>, SmacFractal::k_LatticeSize>
ConstantLattice_(int value)
{
    std::array<std::array<int, SmacFractal::k_LatticeSize>, SmacFractal::k_LatticeSize> lattice{};
    for (std::array<int, SmacFractal::k_LatticeSize>& rRow : lattice)
    {
        rRow.fill(value);
    }
    return lattice;
}

} // namespace

TEST_CASE("SmacFractal samples lattice points and midpoints", "[worldgen][fractal]")
{
    auto lattice = ConstantLattice_(0);
    lattice[0][0] = 4;
    lattice[0][1] = 8;
    lattice[1][0] = 12;
    const SmacFractal fractal(lattice);

    // Sample space: cell every 256 units; (128, 128) sits on cell (0,0) after the −128 offset.
    CHECK(fractal.Sample(128, 128) == 4 * 32);
    // Halfway to the next cell in x: mean of 4 and 8.
    CHECK(fractal.Sample(128 + 128, 128) == ((4 + 8) / 2) * 32);
    // Lattice wraps: cell 16 is cell 0.
    CHECK(fractal.Sample(128 + 16 * 256, 128) == 4 * 32);
}

TEST_CASE("SmacFractal Value matches the SMAC fungus formula on constant lattices",
          "[worldgen][fractal]")
{
    CHECK(SmacFractal(ConstantLattice_(4)).Value(0, 0, 4) == 24);
    CHECK(SmacFractal(ConstantLattice_(7)).Value(10, 20, 4) == 42);
    CHECK(SmacFractal(ConstantLattice_(11)).Value(3, 5, 4) == 67);
    CHECK(SmacFractal(ConstantLattice_(3)).Value(1, 1, 4) == 18);
}
