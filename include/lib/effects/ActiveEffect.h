#pragma once

#include "lib/effects/BonusEffect.h"
#include <string>
#include <vector>

namespace ac
{

// Forward declarations
class BaseManager;
class BuildingRegistry;
class Faction;
struct UnitComponentConfig_t;

struct ActiveEffect_t
{
    const EffectConfig_t* config;   // non-owning, points into static config data
    std::string sourceId;           // "command_nexus", "free_market", etc — for breakdown/UI
    const BaseManager* originBase = nullptr; // only set for ThisBase-scoped effects
};

struct StatBreakdown_t
{
    double total = 0.0;

    struct Contribution
    {
        std::string sourceId;
        double amount;
        ModifierOp op;
    };

    std::vector<Contribution> contributions;
};

std::vector<ActiveEffect_t> CollectActiveEffects(const Faction& rFaction,
                                                  const BuildingRegistry& rBuildingRegistry);

// baseValue seeds the additive total before contributions are summed — required for stats
// that are resolved purely through multiplicative modifiers (e.g. CostMultiplier, which has
// no Add contributions of its own and needs a base of 1.0 for MultiplyGeometric to act on).
StatBreakdown_t ResolveStatModifiers(const std::vector<ActiveEffect_t>& matching, double baseValue = 0.0);

// Returns effects whose target stat matches the given StatId.
// Only includes StatModifierEffect_t instances.
std::vector<ActiveEffect_t> FilterByStatId(const std::vector<ActiveEffect_t>& effects, StatId statId);

// Returns effects that apply to the given base.
// Includes ThisBase effects originating from this base, plus all AllOwnerBases, FactionGlobal, and WorldGlobal effects.
std::vector<ActiveEffect_t> FilterForBase(const std::vector<ActiveEffect_t>& effects, const BaseManager& rBase);

// Collects all effects from a list of unit components as ActiveEffect_t instances.
std::vector<ActiveEffect_t> CollectUnitEffects(const std::vector<const UnitComponentConfig_t*>& components);

} // namespace ac
