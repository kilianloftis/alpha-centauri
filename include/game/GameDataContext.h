#pragma once

#include "game/faction/base/production/ProductionConfigParser.h"
#include "game/GameDataPaths.h"
#include "game/effects/EffectConfig.h"

#include <memory>
#include <vector>

namespace ac
{

class BuildingRegistry;
class StockpileRegistry;
class UnitComponentRegistry;
class UnitSlotRegistry;
class PopTypeRegistry;
class TechRegistry;
class SocialPolicyRegistry;
class SocialRatingRegistry;
class FactionRegistry;
struct PopCompositionConfig_t;
class PopCompositionCalculator;
struct GrowthConfig_t;
class LuaRuntime;
struct TechCostConfig_t;
class TechCostCalculator;
class PopTypeAvailabilityCalculator;
class ImprovementRegistry;
class WorldGenPresetRegistry;
struct WorldGenDecorationConfig_t;
struct LandmarkConfig_t;
struct MoraleConfig_t;
class MoraleCalculator;
struct ProbeActionsConfig_t;
struct BaseConquestConfig_t;
class CouncilProposalRegistry;
struct CouncilRulesConfig_t;

// Owns the definition data loaded once at startup (registries and config structs, all
// reconstructible from config files) plus the calculators/services built from that data.
// Individual pieces are not mutated after loading, but this is not a blanket immutability
// guarantee: LuaRuntime carries mutable interpreter state used internally by every formula
// evaluation (see code-review-findings.md 3.6). Deliberately excludes anything that reads
// live save-game state — e.g. SecretProjectAvailabilityCalculator lives on GameState instead,
// since it queries the faction vector and would dangle if GameState were ever rebuilt.
struct GameDataContext
{
    GameDataContext();
    ~GameDataContext();

    // Move-only: LoadGameData returns one by value, and the composition root owns exactly one.
    // Defined out-of-line because the members are incomplete types here.
    GameDataContext(GameDataContext&&) noexcept;
    GameDataContext& operator=(GameDataContext&&) noexcept;
    GameDataContext(const GameDataContext&) = delete;
    GameDataContext& operator=(const GameDataContext&) = delete;

    // --- Registries and config structs (definition data) ---
    std::unique_ptr<BuildingRegistry> buildingRegistry;
    std::unique_ptr<StockpileRegistry> stockpileRegistry;
    std::unique_ptr<UnitComponentRegistry> unitComponentRegistry;
    std::unique_ptr<UnitSlotRegistry> unitSlotRegistry;
    std::unique_ptr<TechRegistry> techRegistry;
    std::unique_ptr<SocialPolicyRegistry> socialPolicyRegistry;
    std::unique_ptr<SocialRatingRegistry> socialRatingRegistry;
    std::unique_ptr<FactionRegistry> factionRegistry;
    std::unique_ptr<PopTypeRegistry> popTypeRegistry;
    std::unique_ptr<PopCompositionConfig_t> popCompositionConfig;
    std::unique_ptr<GrowthConfig_t> growthConfig;
    std::unique_ptr<ProductionConfig_t> productionConfig;
    std::unique_ptr<TechCostConfig_t> techCostConfig;
    std::unique_ptr<ImprovementRegistry> improvementRegistry;
    std::unique_ptr<WorldGenPresetRegistry> worldGenPresetRegistry;
    std::unique_ptr<WorldGenDecorationConfig_t> worldGenDecorationConfig;
    std::vector<LandmarkConfig_t> worldGenLandmarks;
    // FactionGlobal TileResourceCap rules (and any other universal tile-yield effects).
    std::vector<EffectConfig_t> tileYieldRules;
    std::unique_ptr<MoraleConfig_t> moraleConfig;
    std::unique_ptr<ProbeActionsConfig_t> probeActionsConfig;
    std::unique_ptr<BaseConquestConfig_t> baseConquestConfig;
    std::unique_ptr<CouncilProposalRegistry> councilProposalRegistry;
    std::unique_ptr<CouncilRulesConfig_t> councilRules;

    // --- Calculators / services (built from the data above) ---
    std::unique_ptr<LuaRuntime> luaRuntime;
    std::unique_ptr<PopCompositionCalculator> popCompositionCalculator;
    std::unique_ptr<TechCostCalculator> techCostCalculator;
    std::unique_ptr<PopTypeAvailabilityCalculator> popTypeAvailabilityCalculator;
    // Stateless view over moraleConfig — reads no live save-game state, so it belongs here
    // beside techCostCalculator rather than on GameState. Every faction, unit, and combat
    // path borrows this one instance.
    std::unique_ptr<MoraleCalculator> moraleCalculator;

    // The paths this context was loaded from, so a caller that needs a config file LoadGameData
    // does not parse (the UI's terraform bindings) resolves it the same way as everything else
    // rather than hardcoding a second root.
    GameDataPaths paths;
};

// Single entry point that runs every config parser and returns a *complete* context. Load
// order is deliberate: registries that are reference targets (techs, improvements, unit
// components) load before configs that may cite them; effect-declaring configs (including
// production) load next; then ValidateEffectReferences / ValidateRequiredTechReferences run
// once everything they walk is present — so a typo'd unitFilter HasComponent id (or
// grant/tech/selector id) fails here instead of becoming a silent no-op. Formula configs
// and the calculators built from them load last.
//
// Returns by value rather than filling a caller-supplied bag so there is no window in which a
// half-populated context is observable: every member is non-null when this returns, or it threw.
// ThrowIfIncomplete enforces that before returning, naming the first missing member — consumers
// may therefore dereference any member without checking. Call from Engine::Initialize_; do not
// re-order individual Load calls at other call sites.
GameDataContext LoadGameData(const GameDataPaths& rPaths = {});

// Throw naming the first null member. Called by LoadGameData; also the check a test fixture
// should run once it has finished assembling a context by hand.
void ThrowIfIncomplete(const GameDataContext& rData);

} // namespace ac
