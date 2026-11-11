#include "game/map/SmacFractal.h"

#include <algorithm>

namespace ac
{

namespace
{

constexpr int k_FungusFineBaseShift = 4;

} // namespace

SmacFractal::SmacFractal(
    const std::array<std::array<int, k_LatticeSize>, k_LatticeSize>& rLattice)
{
    FillFromLattice_(rLattice);
}

SmacFractal::SmacFractal(std::mt19937& rRng)
{
    std::array<std::array<int, k_LatticeSize>, k_LatticeSize> lattice{};
    std::uniform_int_distribution<int> cell(0, 15);
    for (std::array<int, k_LatticeSize>& rRow : lattice)
    {
        for (int& rValue : rRow)
        {
            rValue = cell(rRng);
        }
    }
    FillFromLattice_(lattice);
}

void SmacFractal::FillFromLattice_(
    const std::array<std::array<int, k_LatticeSize>, k_LatticeSize>& rLattice)
{
    for (int y = 0; y < k_LatticeSize; ++y)
    {
        for (int x = 0; x < k_LatticeSize; ++x)
        {
            m_cells[static_cast<size_t>(y * k_Pitch + x)] =
                static_cast<int8_t>(rLattice[static_cast<size_t>(y)][static_cast<size_t>(x)]);
        }
        // Wrap column for Sample's +1 neighbor on the last cell.
        m_cells[static_cast<size_t>(y * k_Pitch + k_LatticeSize)] =
            m_cells[static_cast<size_t>(y * k_Pitch)];
    }
    // Wrap row for Sample's +pitch neighbor on the last cell.
    for (int x = 0; x <= k_LatticeSize; ++x)
    {
        m_cells[static_cast<size_t>(k_LatticeSize * k_Pitch + x)] =
            m_cells[static_cast<size_t>(x)];
    }
}

int SmacFractal::Cell_(int cellX, int cellY) const
{
    return m_cells[static_cast<size_t>(cellY * k_Pitch + cellX)];
}

int SmacFractal::Sample(int x, int y) const
{
    const int adjX = x - 128;
    const int adjY = y - 128;
    const int cellX = (adjX >> 8) & 15;
    const int cellY = (adjY >> 8) & 15;
    const int fracX = (adjX >> 3) & 31;
    const int fracY = (adjY >> 3) & 31;

    const int v00 = Cell_(cellX, cellY);
    const int v10 = Cell_(cellX + 1, cellY);
    const int v01 = Cell_(cellX, cellY + 1);
    const int v11 = Cell_(cellX + 1, cellY + 1);

    const int invX = 32 - fracX;
    const int invY = 32 - fracY;
    const int top = v00 * invX + v10 * fracX;
    const int bot = v01 * invX + v11 * fracX;
    return (top * invY + bot * fracY) >> 5;
}

int SmacFractal::Value(int x, int y, int fineShift) const
{
    const int coarseX = x << k_FungusFineBaseShift;
    const int coarseY = y << k_FungusFineBaseShift;
    const int a = Sample(coarseX, coarseY);
    const int b = Sample(coarseX << fineShift, coarseY << fineShift);
    const int mixed = 5 * a + 2 * b;
    const int scaled = (7 * mixed) >> 8;
    return std::clamp(scaled, 0, 100);
}

} // namespace ac
