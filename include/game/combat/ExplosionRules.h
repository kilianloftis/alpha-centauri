#pragma once

#include <optional>

namespace ac
{

class Unit;
struct ExplosionEffect_t;

// The effect's fixed radius, or the detonating unit's radius stat. Empty when the radius comes
// from a unit and none detonated.
std::optional<int> ExplosionRadius(const ExplosionEffect_t& rConfig, const Unit* pDetonator);

} // namespace ac
