#pragma once

#include <string>
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

struct LandmarkShape_t
{
    LandmarkShapeKind_t kind = LandmarkShapeKind_t::Disk;
    int radius = 0;
    int outerRadius = 0;
    int innerRadius = 0;
    std::vector<std::string> maskRows;
    std::string sculptorId;
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
