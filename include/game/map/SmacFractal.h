#pragma once

#include <array>
#include <cstdint>
#include <random>

namespace ac
{

// 16 × 16 lattice fractal used by world-gen fungus (docs/thinker/smac-world-generation.md).
// Sample space has one lattice cell every 256 units and wraps on both axes.
class SmacFractal
{
public:
    static constexpr int k_LatticeSize = 16;

    // Copies a 16 × 16 lattice of cell values (typically 0–15).
    explicit SmacFractal(const std::array<std::array<int, k_LatticeSize>, k_LatticeSize>& rLattice);
    // Uniform 0–15 per cell.
    explicit SmacFractal(std::mt19937& rRng);

    // Bilinear sample in 32nds over the lattice; result runs 0–480 for cell values 0–15.
    int Sample(int x, int y) const;
    // Fungus and kin: clamp((7 · (5a + 2b)) >> 8, 0, 100) with a = Sample(16x, 16y) and
    // b = Sample((16x) << fineShift, (16y) << fineShift). Fungus uses fineShift 4.
    int Value(int x, int y, int fineShift) const;

private:
    // Extra row/column so +1 and +pitch neighbor reads wrap without masking.
    static constexpr int k_Pitch = k_LatticeSize + 3;
    std::array<int8_t, k_Pitch * k_Pitch> m_cells{};

    void FillFromLattice_(
        const std::array<std::array<int, k_LatticeSize>, k_LatticeSize>& rLattice);
    int Cell_(int cellX, int cellY) const;
};

} // namespace ac
