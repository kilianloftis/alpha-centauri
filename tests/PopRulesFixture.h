#pragma once

#include "GameFixtures.h"

#include "game/population/PopTypeAvailabilityCalculator.h"
#include "game/population/PopTypeRegistry.h"
#include "game/research/ResearchManager.h"
#include "game/research/TechCostCalculator.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechRegistry.h"
#include "lib/LuaRuntime.h"

#include <memory>

namespace actest
{

// The pop-type registry plus the two rules services PopContainer / PopulationManager require
// (availability resolution and the discovered-tech source behind it). Small enough for
// population unit tests that do not want a whole world, but real enough that those tests
// exercise the same conversion rules the game does — they used to pass nulls and silently skip
// the tech gate entirely.
struct PopRulesFixture
{
    ac::PopTypeRegistry popTypes;
    ac::TechRegistry techs;
    ac::LuaRuntime lua;
    ac::TechCostConfig_t techCostConfig{k_TestTechCostFormula};
    std::unique_ptr<ac::TechCostCalculator> techCost;
    std::unique_ptr<ac::PopTypeAvailabilityCalculator> availability;
    std::unique_ptr<ac::ResearchManager> research;

    PopRulesFixture()
    {
        popTypes.Load(FixturePath("pop_types.json"));
        techs.Load(FixturePath("techs.json"));
        techCost = std::make_unique<ac::TechCostCalculator>(techCostConfig, lua);
        availability = std::make_unique<ac::PopTypeAvailabilityCalculator>(popTypes);
        research = std::make_unique<ac::ResearchManager>(techs, *techCost,
                                                        /*pEffectsProvider*/ nullptr);
    }
};

// Just the pop-type registry, for tests that only need PopTypeConfig_t entries.
struct PopTypeRegistryOnly
{
    ac::PopTypeRegistry popTypes;

    PopTypeRegistryOnly() { popTypes.Load(FixturePath("pop_types.json")); }
};

} // namespace actest
