#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ac
{

enum class LandmarkDomain_t
{
    Land,
    Water,
    Any,
};

enum class LandmarkShapeKind_t
{
    Disk,      // Euclidean disk, radius
    Ring,      // Euclidean annulus: outer_radius inclusive, inner_radius exclusive
    Mask,      // ASCII rows; 'X'/'x' = footprint cell, other = empty
    Sculptor,  // named procedural footprint + optional terrain sculpt
};

// The one sculptor algorithm C++ implements: a radial peak that raises elevation towards the
// anchor and roughens the slopes. Knobs are data so a modded volcano needs no rebuild.
inline constexpr std::string_view k_MountPlanetSculptor = "mount_planet";

struct LandmarkSculpt_t
{
    int peakElevation = 3500;      // elevation at the anchor, in meters
    int baseElevation = 1000;      // elevation at the rim; tiles already higher are left alone
    float rockyCoreRadius = 1.5f;  // within this distance the tile becomes Rocky, else Rolling
};

struct LandmarkShape_t
{
    LandmarkShapeKind_t kind = LandmarkShapeKind_t::Disk;
    int radius = 0;
    int outerRadius = 0;
    int innerRadius = 0;
    std::vector<std::string> maskRows;
    std::string sculptorId;
    LandmarkSculpt_t sculpt;
};

// Placement recipe for one landmark type. Effects live on ImprovementConfig_t
// (improvementId); this struct only describes where/how to stamp it.
struct LandmarkConfig_t
{
    std::string id;
    std::string improvementId; // stamped on every footprint tile (unless overridden)
    LandmarkDomain_t domain = LandmarkDomain_t::Land;
    int maxCount = 1;
    int minSpacing = 16; // Chebyshev distance between anchors of any landmarks
    LandmarkShape_t shape;
    bool setFungus = false; // also SetHasFungus on footprint tiles (The Ruins)
};

} // namespace ac
