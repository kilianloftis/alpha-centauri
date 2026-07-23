#pragma once

#include "game/map/WorldGenPresetConfig.h"
#include <memory>

namespace ac
{

// Thin FastNoiseLite wrapper configured for 2D fractional Brownian motion.
class FbmNoise
{
public:
    FbmNoise(int seed, const WorldGenPresetConfig_t& rPreset);
    ~FbmNoise();

    FbmNoise(const FbmNoise&) = delete;
    FbmNoise& operator=(const FbmNoise&) = delete;
    FbmNoise(FbmNoise&&) noexcept;
    FbmNoise& operator=(FbmNoise&&) noexcept;

    // Returns a fractal noise sample, typically in roughly [-1, 1].
    float Sample(float x, float y) const;
    float Sample(float x, float y, float z) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
};

} // namespace ac
