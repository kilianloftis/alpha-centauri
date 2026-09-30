#pragma once

#include "TestHelpers.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/CouncilRulesConfigParser.h"
#include "game/stockpiles/StockpileRegistry.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/SocialEngineeringManager.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/HurryProductionCalculator.h"
#include "game/faction/base/production/ScrapRefundCalculator.h"
#include "game/faction/base/production/ProductionConfigParser.h"
#include "game/faction/CommerceConfigParser.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapOccupantLoad.h"
#include "game/map/TerrainConfig.h"
#include "game/map/TerrainOperationRegistry.h"
#include "game/research/TechCostCalculator.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechRegistry.h"
#include "lib/LuaRuntime.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/population/calculators/DroneCalculator.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "game/population/pop-types/GrowthConfigParser.h"
#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitSlotConfig.h"
#include "game/units/MovementRules.h"
#include "game/units/MoraleCalculator.h"
#include "game/units/MoraleConfig.h"
#include "game/units/MoraleConfigParser.h"
#include "game/units/BaseConquestConfig.h"
#include "game/units/NativeUnitConfig.h"
#include "game/units/NativeUnitRegistry.h"
#include "game/units/ProbeActionConfigParser.h"
#include "game/faction/FactionConfig.h"
#include "game/effects/TileEffectsContext.h"
#include "game/effects/TileYieldRulesConfigParser.h"
#include "game/map/ElevationRulesConfigParser.h"
#include "game/effects/InteractionGridsConfigParser.h"
#include "game/effects/PoliceRulesConfigParser.h"
#include "game/effects/WorldRulesConfigParser.h"
#include "game/DifficultyConfigParser.h"
#include "game/atrocities/AtrocityConfigParser.h"
#include "game/NativeLifeLevelConfig.h"
#include "game/ecology/EcoDamageCalculator.h"
#include "game/ecology/EcoDamageConfig.h"
#include "game/world-events/WorldEventConfig.h"

#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace actest
{

// Fixed seed for test factions. Faction seeds drive base-name and starting-research-target
// picks; drawing them from std::random_device (as the sub-objects used to) makes any test that
// touches research or base names order-dependent and intermittently red. Tests that care about
// a specific draw pass their own value.
constexpr uint32_t k_TestFactionSeed = 1234u;

// Fixed seed for a test GameState's roll RNG (combat, promotion, probes). Same reasoning:
// std::random_device made every roll-dependent test a coin flip across runs.
constexpr uint32_t k_TestRngSeed = 4321u;

// Tech cost formula for fixtures that do not care about research cost: a fixed positive
// value. A formula is required config, so "no formula" is not expressible. Per-tech cost
// lives in ThisTech TechCost effects (or the shipping Lua formula), not a base_cost field.
inline const std::string k_TestTechCostFormula = "10";

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

// A small world with the fixture improvement and unit-component registries loaded and a
// TileEffectsContext over them. Note: TileEffectsContext caches the max effect radius at
// construction, so the registries must be loaded first — hence the unique_ptr.
// Every base_conquest tunable is a baseline Add in its effects list, so tests set one by
// rewriting that Add's amount in place. Replacing BaseConquestConfig_t wholesale would
// dangle the reference FactionEffectsPool holds into `effects`, so never do that; editing an
// existing entry is safe because the pool resolves through a pointer to it.
inline void SetBaseConquestStat(ac::BaseConquestConfig_t& rConfig, ac::StatId_t stat, double amount)
{
    for (ac::EffectConfig_t& rEffect : rConfig.effects)
    {
        auto* pMod = std::get_if<ac::StatModifierEffect_t>(&rEffect.effect);
        if (pMod && pMod->stat == stat)
        {
            pMod->amount = amount;
            return;
        }
    }
    ac::EffectConfig_t effect;
    effect.scope = ac::EffectScope_t::FactionGlobal;
    ac::StatModifierEffect_t modifier;
    modifier.stat = stat;
    modifier.amount = amount;
    modifier.op = ac::ModifierOp_t::Add;
    effect.effect = modifier;
    rConfig.effects.push_back(effect);
}

// The improvement and terrain files, under tests/fixtures/, a fixture session loads.
struct OccupantFiles_t
{
    std::string improvements = "improvements.json";
    std::string terrain = "terrain.json";
};

// The fixture occupants with the Farm, Forest, Mine and Fungus excludes rules.
inline const OccupantFiles_t k_ExcludesOccupantFiles{"improvements_excludes.json",
                                                     "terrain_excludes.json"};

struct WorldFixture
{
    // Declared first so it is destroyed last: factions, bases, and units created by the
    // derived fixtures hold references into it, exactly as in Engine.
    ac::GameDataContext dataContext;
    ac::GameSettings settings; // session prefs; a Faction constructor dependency
    ac::WorldMap map;
    // The context's own registries, so a session built on dataContext sees the same ones.
    ac::ImprovementRegistry& improvements;
    ac::UnitComponentRegistry& unitComponents;
    std::unique_ptr<ac::TileEffectsContext> ctx;

    template <typename T>
    static T& CreateInContext(std::unique_ptr<T>& pSlot)
    {
        pSlot = std::make_unique<T>();
        return *pSlot;
    }

    // Every dependency a Faction or BaseManager takes is loaded here rather than in the
    // derived fixtures, because those dependencies are references now: there is no
    // "half-loaded context" for a fixture to construct against. That is deliberate — fixture
    // bases used to be built with a null rating registry and null research manager, so they
    // resolved social ratings to nothing while the real game resolved them, with no
    // diagnostic. A fixture that diverges from Engine is not a fixture.
    explicit WorldFixture(int width = 9, int height = 9, const OccupantFiles_t& rOccupants = {})
        : map(width, height, TestMapRules())
        , improvements(CreateInContext(dataContext.improvementRegistry))
        , unitComponents(CreateInContext(dataContext.unitComponentRegistry))
    {
        // As LoadGameData leaves it: the preset's elevation range belongs to the map.
        dataContext.elevationRules =
            ac::ElevationRulesConfigParser{}.ParseConfig(FixturePath("map_rules.json"));
        // The same call production uses, so the fixture cannot drift from it or skip the
        // improvement-shadows-operation check that only this path performs.
        dataContext.terrainOperationRegistry = std::make_unique<ac::TerrainOperationRegistry>();
        ac::LoadMapOccupants(FixturePath(rOccupants.improvements), FixturePath(rOccupants.terrain),
                             improvements, *dataContext.terrainOperationRegistry);
        unitComponents.Load(FixturePath("unit_components.json"));
        // Before the TileEffectsContext: it holds the yield rules by reference and stamps
        // them into every per-tile resolve (ElevationEnergy reads the step from here).
        dataContext.tileYieldRules = ac::TileYieldRulesConfigParser{}.ParseConfig(
            FixturePath("tile_yield_rules.json"));
        dataContext.interactionGrids = ac::InteractionGridsConfigParser{}.ParseConfig(
            FixturePath("interaction_grids.json"));
        ctx = std::make_unique<ac::TileEffectsContext>(map, improvements, &unitComponents,
                                                       dataContext.tileYieldRules,
                                                       dataContext.interactionGrids);
        dataContext.luaRuntime = std::make_unique<ac::LuaRuntime>();
        // Same morale table Engine loads, and the one calculator built from it.
        dataContext.moraleConfig = std::make_unique<ac::MoraleConfig_t>(
            ac::MoraleConfigParser{}.ParseConfig(FixturePath("morale_levels.json")));
        dataContext.moraleCalculator = std::make_unique<ac::MoraleCalculator>(
            *dataContext.moraleConfig, *dataContext.luaRuntime);

        dataContext.buildingRegistry = std::make_unique<ac::BuildingRegistry>();
        dataContext.buildingRegistry->Load(FixturePath("buildings.json"));
        dataContext.stockpileRegistry = std::make_unique<ac::StockpileRegistry>();
        dataContext.stockpileRegistry->Load(FixturePath("stockpiles.json"));
        dataContext.popTypeRegistry = std::make_unique<ac::PopTypeRegistry>();
        dataContext.popTypeRegistry->Load(FixturePath("pop_types.json"));
        dataContext.growthConfig = std::make_unique<ac::GrowthConfig_t>();
        dataContext.productionConfig = std::make_unique<ac::ProductionConfig_t>(
            ac::ProductionConfigParser{}.ParseConfig(FixturePath("production.json")));
        dataContext.commerceConfig = std::make_unique<ac::CommerceConfig_t>(
            ac::CommerceConfigParser{}.ParseConfig(FixturePath("commerce.json")));
        dataContext.hurryProductionCalculator =
            std::make_unique<ac::HurryProductionCalculator>(*dataContext.productionConfig,
                                                           *dataContext.luaRuntime);
        dataContext.scrapRefundCalculator =
            std::make_unique<ac::ScrapRefundCalculator>(*dataContext.productionConfig,
                                                        *dataContext.luaRuntime);
        dataContext.techRegistry = std::make_unique<ac::TechRegistry>();
        dataContext.techRegistry->Load(FixturePath("techs.json"));
        // Trivial formula; tests that care about tech cost supply their own.
        dataContext.techCostConfig =
            std::make_unique<ac::TechCostConfig_t>(ac::TechCostConfig_t{k_TestTechCostFormula});
        dataContext.techCostCalculator = std::make_unique<ac::TechCostCalculator>(
            *dataContext.techCostConfig, *dataContext.luaRuntime);
        dataContext.socialPolicyRegistry = std::make_unique<ac::SocialPolicyRegistry>();
        dataContext.socialPolicyRegistry->Load(FixturePath("social_policies.json"));
        dataContext.socialRatingRegistry = std::make_unique<ac::SocialRatingRegistry>();
        dataContext.socialRatingRegistry->Load(FixturePath("social_rating_effects.json"));
        dataContext.policeRules = ac::PoliceRulesConfigParser{}.ParseConfig(
            FixturePath("police_rules.json"));
        dataContext.popTypeAvailabilityCalculator =
            std::make_unique<ac::PopTypeAvailabilityCalculator>(*dataContext.popTypeRegistry);
        dataContext.popCompositionConfig = std::make_unique<ac::PopCompositionConfig_t>(
            ac::PopCompositionConfigParser{}.ParseConfig(FixturePath("pop_composition.json")));
        dataContext.droneCalculator = std::make_unique<ac::DroneCalculator>(
            *dataContext.popCompositionConfig, *dataContext.luaRuntime);
        dataContext.popCompositionCalculator = std::make_unique<ac::PopCompositionCalculator>(
            *dataContext.popCompositionConfig, *dataContext.popTypeRegistry);
        dataContext.difficultyConfig = std::make_unique<ac::DifficultyConfig_t>(
            ac::DifficultyConfigParser{}.ParseConfig(FixturePath("difficulty.json")));
        dataContext.atrocitiesConfig = std::make_unique<ac::AtrocitiesConfig_t>(
            ac::AtrocityConfigParser{}.ParseConfig(FixturePath("atrocities.json")));
        dataContext.nativeLifeLevelConfig = std::make_unique<ac::NativeLifeLevelConfig_t>(
            ac::NativeLifeLevelConfigParser{}.ParseConfig(FixturePath("native_life_levels.json")));
        dataContext.worldEventsConfig = std::make_unique<ac::WorldEventsConfig_t>(
            ac::WorldEventsConfigParser{}.ParseConfig(FixturePath("world_events.json")));
        dataContext.probeActionsConfig = std::make_unique<ac::ProbeActionsConfig_t>(
            ac::ProbeActionConfigParser{}.ParseConfig(FixturePath("probe_actions.json")));
        // Built before any Faction: the pool holds a reference into `effects`. Tests that want
        // a different formula edit damageFormula in place.
        dataContext.ecoDamageConfig = std::make_unique<ac::EcoDamageConfig_t>(
            ac::EcoDamageConfigParser{}.ParseConfig(FixturePath("eco_damage.json"),
                                                    FixturePath("eco_damage.lua"),
                                                    *dataContext.luaRuntime));
        dataContext.ecoDamageCalculator = std::make_unique<ac::EcoDamageCalculator>(
            *dataContext.ecoDamageConfig, *dataContext.luaRuntime);
        // Built here, before any Faction exists: FactionEffectsPool holds a reference into
        // `effects`, so a test replacing this config later would dangle it.
        dataContext.baseConquestConfig = std::make_unique<ac::BaseConquestConfig_t>();
        SetBaseConquestStat(*dataContext.baseConquestConfig,
                            ac::StatId_t::LastDefenderPopLoss, 1);
        SetBaseConquestStat(*dataContext.baseConquestConfig, ac::StatId_t::CapturePopLoss, 1);
        SetBaseConquestStat(*dataContext.baseConquestConfig,
                            ac::StatId_t::CaptureFacilitiesDestroyedMin, 0);
        SetBaseConquestStat(*dataContext.baseConquestConfig,
                            ac::StatId_t::CaptureFacilitiesDestroyedMaxPercent, 50);
        SetBaseConquestStat(*dataContext.baseConquestConfig, ac::StatId_t::ConqueredDroneCap, -0.5);
    }

    ac::MoraleCalculator& morale() const { return *dataContext.moraleCalculator; }

    ac::Tile& At(int x, int y) { return *map.GetTile(x, y); }
};

// WorldFixture plus the ability to found real bases (the registries themselves are loaded by
// WorldFixture, since Faction now requires them). Each base starts with 3 default Worker pops
// when MakeBase / MakeFactionBase omit an override (shipping CreateBase resolves StartingSize,
// currently 1). Registers the "Base" improvement on its tile, exactly as in the game.
struct BaseFixture : WorldFixture
{
    ac::EconomyManager economy; // default 40/50/10 energy split
    // Stub owner for MakeBase standalone bases (declared before bases so bases destroy first).
    ac::FactionConfig_t ownerDefinition;
    std::unique_ptr<ac::Faction> pOwnerFaction;
    std::vector<std::unique_ptr<ac::BaseManager>> bases;
    int nextBaseId = 1;

    BaseFixture()
        : BaseFixture(9, 9)
    {
    }

    explicit BaseFixture(int width, int height, const OccupantFiles_t& rOccupants = {})
        : WorldFixture(width, height, rOccupants)
    {
        ownerDefinition.id = "test_base_owner";
        pOwnerFaction = std::make_unique<ac::Faction>(
            /*factionId*/ 1, /*bIsPlayerControlled*/ true, ownerDefinition, dataContext,
            map, settings, k_TestFactionSeed);
    }

    ac::BuildingRegistry& buildings() { return *dataContext.buildingRegistry; }
    const ac::BuildingRegistry& buildings() const { return *dataContext.buildingRegistry; }
    ac::StockpileRegistry& stockpiles() { return *dataContext.stockpileRegistry; }
    const ac::StockpileRegistry& stockpiles() const { return *dataContext.stockpileRegistry; }
    ac::PopTypeRegistry& popTypes() { return *dataContext.popTypeRegistry; }
    const ac::PopTypeRegistry& popTypes() const { return *dataContext.popTypeRegistry; }

    // std::nullopt exercises the shipping founding path (resolved StartingSize); the 3 default
    // keeps the pre-existing fixture size for tests that only care about having pops.
    ac::BaseManager& MakeBase(int x, int y, std::optional<int> initialPopulation = 3)
    {
        bases.push_back(std::make_unique<ac::BaseManager>(
            *pOwnerFaction, nextBaseId++, "TestBase", At(x, y),
            *dataContext.buildingRegistry,
            *dataContext.stockpileRegistry,
            *dataContext.socialRatingRegistry,
            *dataContext.popTypeRegistry,
            *dataContext.popTypeAvailabilityCalculator,
            *dataContext.growthConfig,
            *dataContext.productionConfig,
            *dataContext.hurryProductionCalculator,
            *dataContext.scrapRefundCalculator,
            *dataContext.popCompositionCalculator,
            // The one optional dependency: it needs a GameState, which this fixture has no
            // reason to build. Only GetBuildingsAvailableForConstruction requires it.
            /*secretProjectCalculator*/ nullptr,
            *ctx,
            initialPopulation));
        return *bases.back();
    }
};

// BaseFixture plus real Factions and unit designs — for the universal-routing lanes
// (FactionUnits, WorldGlobal, two-level ratings, unit auras).
struct FactionFixture : BaseFixture
{
    ac::FactionConfig_t factionDefinition; // minimal shared definition for test factions
    // designs must outlive factions: units hold UnitDesign& into this deque.
    // Units are destroyed before bases (~Faction member order), so crawler claim
    // release can reach WorkerAssignmentManager while the home base is still alive.
    std::deque<ac::UnitDesign> designs;
    // Heap copies so a faction can outlive a caller-supplied definition. Declared before
    // factions: ~Faction must not outlive the config it references.
    std::vector<std::unique_ptr<ac::FactionConfig_t>> extraDefinitions;
    std::vector<std::unique_ptr<ac::Faction>> factions;
    // Session back-pointer for CreateUnit's on_unit_produced_effects (GrantXp). Owns a
    // throwaway 1×1 map so it does not steal FactionFixture::map; GrantXp never touches it.
    std::unique_ptr<ac::WorldMap> pBindMap;
    std::unique_ptr<ac::GameState> pBindState;
    int nextFactionId = 1;
    int nextUnitId = 1;

    FactionFixture()
        : FactionFixture(9, 9)
    {
    }

    explicit FactionFixture(int width, int height,
                            std::vector<ac::EffectConfig_t> worldRules = {},
                            const OccupantFiles_t& rOccupants = {})
        : BaseFixture(width, height, rOccupants)
    {
        factionDefinition.id = "test_faction";
        // NOTE: do not (re)load dataContext.tileYieldRules here. WorldFixture already parsed
        // it, and BaseFixture's pOwnerFaction has by now cached ActiveEffect_t entries holding
        // EffectConfig_t* into that vector's buffer. Reassigning it frees the buffer those
        // pointers reference, and no revision the effects pool samples changes, so the stale
        // cache is never rebuilt — a use-after-free on the next base-effects query.
        // Mirror GameState: index emits OnUnitMoved; faction rebuilds visibility.
        map.GetUnitPositions().OnUnitMoved.Connect([](ac::Unit& rMoved)
        {
            rMoved.GetFaction().RebuildVisibility();
        });
        dataContext.worldRules = std::move(worldRules);
        pBindMap = std::make_unique<ac::WorldMap>(1, 1, actest::TestMapRules());
        pBindState = std::make_unique<ac::GameState>(std::move(pBindMap), dataContext, settings,
                                                     k_TestRngSeed);
    }

    ac::SocialPolicyRegistry& socialPolicies() { return *dataContext.socialPolicyRegistry; }
    const ac::SocialPolicyRegistry& socialPolicies() const { return *dataContext.socialPolicyRegistry; }
    ac::SocialRatingRegistry& socialRatings() { return *dataContext.socialRatingRegistry; }
    const ac::SocialRatingRegistry& socialRatings() const { return *dataContext.socialRatingRegistry; }

    ac::Faction& MakeFaction()
    {
        return MakeFaction_(factionDefinition);
    }

    // Copies rDefinition onto the fixture so the faction's reference stays valid.
    ac::Faction& MakeFaction(const ac::FactionConfig_t& rDefinition)
    {
        extraDefinitions.push_back(std::make_unique<ac::FactionConfig_t>(rDefinition));
        return MakeFaction_(*extraDefinitions.back());
    }

    ac::Faction& MakeFaction_(const ac::FactionConfig_t& rDefinition)
    {
        // Only the first fixture faction is player-controlled; tests needing a second
        // faction (e.g. WorldGlobal routing between two factions) get an AI one.
        const bool bIsPlayerControlled = factions.empty();
        // Distinct per faction so two fixture factions do not make identical picks.
        const uint32_t seed = k_TestFactionSeed + static_cast<uint32_t>(nextFactionId);
        factions.push_back(std::make_unique<ac::Faction>(
            nextFactionId++, bIsPlayerControlled, rDefinition, dataContext,
            map, settings, seed));
        ac::Faction& rFaction = *factions.back();
        // Drop destroyed units from every faction's contact-reveal set (address reuse safety).
        rFaction.GetUnitManager().OnUnitDestroyed.Connect([this](ac::Unit& rDestroyed)
        {
            for (auto& pFaction : factions)
            {
                pFaction->GetRevealedUnits().Forget(rDestroyed);
            }
        });
        // Keep territory current when bases are founded (same hook GameState uses).
        rFaction.OnBaseListChanged.Connect([this]()
        {
            std::vector<const ac::BaseManager*> bases;
            for (const auto& pFaction : factions)
            {
                for (const ac::BaseManager& rBase : pFaction->Bases())
                {
                    bases.push_back(&rBase);
                }
            }
            map.GetTerritory().Rebuild(map, bases);
        });
        rFaction.BindWorldEffects(*pBindState);
        rFaction.BindGameState(*pBindState);
        rFaction.OnSecretProjectDestroyed.Connect([this](const ac::BuildingId_t& rId)
        {
            pBindState->MarkSecretProjectDestroyed(rId);
        });
        // Mirror GameState::AttachToSession_: CreateUnit no longer applies produced triggers.
        rFaction.GetUnitManager().OnUnitCreated.Connect(
            [this](ac::Unit& rCreated, ac::BaseManager* pBuiltAt)
            {
                if (pBuiltAt)
                {
                    ac::ApplyUnitProducedTriggers(*pBindState, rCreated, *pBuiltAt);
                }
            });
        return rFaction;
    }

    ac::BaseManager& MakeFactionBase(ac::Faction& rFaction, int x, int y,
                                     std::optional<int> initialPopulation = 3)
    {
        auto pBase = std::make_unique<ac::BaseManager>(
            rFaction, nextBaseId++, "TestBase", At(x, y),
            *dataContext.buildingRegistry,
            *dataContext.stockpileRegistry,
            *dataContext.socialRatingRegistry,
            *dataContext.popTypeRegistry,
            *dataContext.popTypeAvailabilityCalculator,
            *dataContext.growthConfig,
            *dataContext.productionConfig,
            *dataContext.hurryProductionCalculator,
            *dataContext.scrapRefundCalculator,
            *dataContext.popCompositionCalculator,
            /*secretProjectCalculator*/ nullptr,
            *ctx,
            initialPopulation);
        ac::BaseManager& rBase = *pBase;
        rFaction.AddBase(std::move(pBase));
        return rBase;
    }

    // Builds a design from fixture components (one synthetic slot per component) and
    // creates a live unit at (x, y), registered on the world map for aura/position queries.
    ac::Unit& MakeUnit(ac::Faction& rFaction, int x, int y,
                       const std::vector<std::string>& rComponentIds,
                       ac::BaseManager* pHomeBase = nullptr,
                       ac::BaseManager* pProducedAt = nullptr)
    {
        std::vector<ac::UnitSlotConfig_t> slots;
        std::unordered_map<std::string, const ac::UnitComponentConfig_t*> assigned;
        int slotIndex = 0;
        for (const std::string& rId : rComponentIds)
        {
            const ac::UnitComponentConfig_t* pComponent = unitComponents.Find(rId);
            if (!pComponent)
            {
                throw std::runtime_error("Fixture component not found: " + rId);
            }
            ac::UnitSlotConfig_t slot;
            slot.id = "slot_" + std::to_string(slotIndex++);
            slot.displayName = slot.id;
            slot.componentType = pComponent->type;
            slot.required = true;
            assigned[slot.id] = pComponent;
            slots.push_back(slot);
        }
        designs.emplace_back(slots, assigned);

        // The unit registers itself in the map's position index for its lifetime.
        return rFaction.GetUnitManager().CreateUnit(nextUnitId++, designs.back(),
                                                    map.GetUnitPositions(), At(x, y),
                                                    pHomeBase, pProducedAt);
    }

    void MoveUnit(ac::Unit& rUnit, int x, int y)
    {
        // No pre-check here: UnitPositionIndex::MoveUnit enforces the stacking rule itself now.
        // The guard this used to duplicate threw a different exception type than production,
        // so a fixture move and a real move failed differently.
        map.GetUnitPositions().MoveUnit(rUnit, At(x, y));
    }
};

// The fixture native units (one land and one sea lifeform), with every fungal bloom spawning
// between lifeformsMin and lifeformsMax of them.
inline void InstallNativeUnits(ac::GameDataContext& rData, int lifeformsMin, int lifeformsMax)
{
    rData.nativeUnitRegistry = std::make_unique<ac::NativeUnitRegistry>();
    rData.nativeUnitRegistry->Load(FixturePath("native_units.json"));
    ac::NativeLifeConfig_t life;
    life.fungalBloomNativeLifeformsMin = lifeformsMin;
    life.fungalBloomNativeLifeformsMax = lifeformsMax;
    rData.nativeUnitRegistry->SetFungalBloomLifeforms(life);
}

// The fixture council rules plus the proposals in rProposalsFile. Call before
// GameState::CreatePlanetaryCouncil.
inline void InstallCouncil(ac::GameDataContext& rData,
                           const std::string& rProposalsFile = "council/proposals.json")
{
    rData.councilProposalRegistry = std::make_unique<ac::CouncilProposalRegistry>();
    rData.councilProposalRegistry->Load(FixturePath(rProposalsFile));
    rData.councilRules = std::make_unique<ac::CouncilRulesConfig_t>(
        ac::CouncilRulesConfigParser{}.ParseConfig(FixturePath("council/rules.json")));
}

// A live GameState on its own 9x9 all-land map over rFixtures' data, for tests that need
// session factions rather than FactionFixture's bind state.
inline std::unique_ptr<ac::GameState> MakeLandSession(FactionFixture& rFixtures)
{
    auto pMap = std::make_unique<ac::WorldMap>(9, 9, TestMapRules());
    for (auto& pTile : pMap->GetTiles())
    {
        pTile->SetElevation(100);
    }
    return std::make_unique<ac::GameState>(std::move(pMap), rFixtures.dataContext,
                                           rFixtures.settings, k_TestRngSeed);
}

// rDefinition must outlive the session.
inline ac::Faction& AddSessionFaction(FactionFixture& rFixtures, ac::GameState& rState,
                                      const ac::FactionConfig_t& rDefinition,
                                      bool bIsPlayerControlled)
{
    return rState.AddFaction(std::make_unique<ac::Faction>(
        rState.AllocateFactionId(), bIsPlayerControlled, rDefinition, rFixtures.dataContext,
        rState.GetWorldMap(), rFixtures.settings, k_TestFactionSeed));
}

// The native-life faction a fungal bloom spawns lifeforms for.
inline ac::Faction& AddNativeLifeFaction(FactionFixture& rFixtures, ac::GameState& rState)
{
    rFixtures.extraDefinitions.push_back(
        std::make_unique<ac::FactionConfig_t>(rFixtures.factionDefinition));
    ac::FactionConfig_t& rPlanet = *rFixtures.extraDefinitions.back();
    rPlanet.id = "planet";
    rPlanet.identity.species = ac::FactionSpecies_t::NativeLife;
    rPlanet.identity.participatesInCouncil = false;
    return AddSessionFaction(rFixtures, rState, rPlanet, /*bIsPlayerControlled=*/false);
}

inline ac::BaseManager& MakeSessionBase(FactionFixture& rFixtures, ac::GameState& rState,
                                        ac::Faction& rFaction, int x, int y)
{
    ac::BaseManager* pBase = rFaction.CreateBase(
        rState.AllocateBaseId(), "TestBase", rState.GetWorldMap().GetTile(x, y),
        rState.GetTileEffects(), rState.GetSecretProjectAvailability());
    if (!pBase)
    {
        throw std::runtime_error("MakeSessionBase: CreateBase failed");
    }
    return *pBase;
}

} // namespace actest
