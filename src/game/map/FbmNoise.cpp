#include "game/map/FbmNoise.h"

#include <FastNoiseLite.h>

namespace ac
{

struct FbmNoise::Impl
{
    FastNoiseLite noise;
};

FbmNoise::FbmNoise(int seed, const WorldGenPresetConfig_t& rPreset)
    : m_pImpl(std::make_unique<Impl>())
{
    m_pImpl->noise.SetSeed(seed);
    m_pImpl->noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_pImpl->noise.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_pImpl->noise.SetFrequency(rPreset.frequency);
    m_pImpl->noise.SetFractalOctaves(rPreset.octaves);
    m_pImpl->noise.SetFractalLacunarity(rPreset.lacunarity);
    m_pImpl->noise.SetFractalGain(rPreset.gain);
}

FbmNoise::~FbmNoise() = default;

FbmNoise::FbmNoise(FbmNoise&&) noexcept = default;
FbmNoise& FbmNoise::operator=(FbmNoise&&) noexcept = default;

float FbmNoise::Sample(float x, float y) const
{
    return m_pImpl->noise.GetNoise(x, y);
}

} // namespace ac
