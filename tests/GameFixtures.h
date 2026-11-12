#pragma once

#include "TestHelpers.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/economy/EconomyManager.h"
#include "game/faction/FactionConfig.h"
#include "game/units/UnitDesign.h"
#include "lib/Formula.h"

#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ac
{
class BaseManager;
class BuildingRegistry;
class CombatResolver;
class ImprovementRegistry;
class MoraleCalculator;
class PopTypeRegistry;
class SocialPolicyRegistry;
class SocialRatingRegistry;
class StockpileRegistry;
class Tile;
class TileEffectsContext;
class Unit;
class UnitAttackExecutor;
class UnitComponentRegistry;
class WorldMap;
struct BaseConquestConfig_t;
struct CombatResult_t;
} // namespace ac

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

// expression as a tech cost formula, over the variables TechCostCalculator passes.
inline ac::Formula_t TestTechCostFormula(std::string expression)
{
    return ac::Formula_t{std::move(expression),
                         {"techs", "most_techs", "diff", "catch_up_diff", "turns", "is_ai",
                          "turn_lag", "research", "map_area_sq_root"}};
}

// Tech cost formula for fixtures that do not care about research cost: a fixed positive
// value. A formula is required config, so "no formula" is not expressible. Per-tech cost
// lives in ThisTech TechCost effects (or the shipping Lua formula), not a base_cost field.
inline const ac::Formula_t k_TestTechCostFormula = TestTechCostFormula("10");

// Additive base_conquest baselines: tests set one by rewriting that Add's amount in place.
// CaptureFacilitiesDestroyed is RawScaled (MultiplyGeometric); edit its amount the same way.
// Replacing BaseConquestConfig_t wholesale would dangle FactionEffectsPool's reference into
// `effects`, so never do that; editing an existing entry is safe.
void SetBaseConquestStat(ac::BaseConquestConfig_t& rConfig, ac::StatId_t stat, double amount);

// The improvement and terrain files, under tests/fixtures/, a fixture session loads.
struct OccupantFiles_t
{
    std::string improvements = "improvements.json";
    std::string terrain = "terrain.json";
};

// The fixture occupants with the Farm, Forest, Mine and Fungus excludes rules.
inline const OccupantFiles_t k_ExcludesOccupantFiles{"improvements_excludes.json",
                                                     "terrain_excludes.json"};

// Everything a session's factions and units reference, so it outlives the session.
// TileEffectsContext caches the max effect radius at construction, so the registries must
// be loaded here before WorldFixture builds the GameState that owns the context.
struct SessionInputs
{
    ac::GameDataContext dataContext;
    ac::GameSettings settings;
    ac::ImprovementRegistry& improvements;
    ac::UnitComponentRegistry& unitComponents;
    ac::FactionConfig_t factionDefinition;
    ac::FactionConfig_t ownerDefinition;
    // designs must outlive factions: units hold UnitDesign& into this deque.
    std::deque<ac::UnitDesign> designs;
    // Heap copies so a faction can outlive a caller-supplied definition.
    std::vector<std::unique_ptr<ac::FactionConfig_t>> extraDefinitions;

    // Every dependency a Faction or BaseManager takes is loaded here rather than in the
    // derived fixtures, because those dependencies are references now: there is no
    // "half-loaded context" for a fixture to construct against. That is deliberate — fixture
    // bases used to be built with a null rating registry and null research manager, so they
    // resolved social ratings to nothing while the real game resolved them, with no
    // diagnostic. A fixture that diverges from Engine is not a fixture.
    explicit SessionInputs(const OccupantFiles_t& rOccupants = {},
                           std::vector<ac::EffectConfig_t> worldRules = {});

    ac::MoraleCalculator& morale() const;
};

// One game session: GameState owns the map and tile effects; the scope is current for the
// fixture's lifetime. Declared so SessionInputs outlives state.
struct WorldFixture : SessionInputs
{
    ac::GameState state;
    ac::GameStateScope scope;
    ac::WorldMap& map;
    ac::TileEffectsContext& ctx;

    explicit WorldFixture(int width = k_TestMapWidth, int height = k_TestMapHeight,
                          const OccupantFiles_t& rOccupants = {},
                          std::vector<ac::EffectConfig_t> worldRules = {},
                          const ac::MapRulesConfig_t& rMapRules = TestMapRules());

    ac::Tile& At(int x, int y);
};

// WorldFixture plus the ability to found real bases (the registries themselves are loaded by
// SessionInputs, since Faction now requires them). Each base starts with 3 default Worker pops
// when MakeBase / MakeFactionBase omit an override (shipping CreateBase resolves StartingSize,
// currently 1). Registers the "Base" improvement on its tile, exactly as in the game.
struct BaseFixture : WorldFixture
{
    ac::EconomyManager economy; // default 40/50/10 energy split
    ac::Faction* pBaseOwner = nullptr;

    BaseFixture();

    explicit BaseFixture(int width, int height, const OccupantFiles_t& rOccupants = {},
                         std::vector<ac::EffectConfig_t> worldRules = {},
                         const ac::MapRulesConfig_t& rMapRules = TestMapRules());

    ac::BuildingRegistry& buildings();
    const ac::BuildingRegistry& buildings() const;
    ac::StockpileRegistry& stockpiles();
    const ac::StockpileRegistry& stockpiles() const;
    ac::PopTypeRegistry& popTypes();
    const ac::PopTypeRegistry& popTypes() const;

    ac::Faction& MakeFaction();

    // Copies rDefinition onto the fixture so the faction's reference stays valid.
    ac::Faction& MakeFaction(const ac::FactionConfig_t& rDefinition);

    ac::Faction& MakeFaction_(const ac::FactionConfig_t& rDefinition);

    ac::Faction& BaseOwner();

    ac::BaseManager& MakeFactionBase(ac::Faction& rFaction, int x, int y,
                                     std::optional<int> initialPopulation = 3);

    // std::nullopt exercises the shipping founding path (resolved StartingSize); the 3 default
    // keeps the pre-existing fixture size for tests that only care about having pops.
    ac::BaseManager& MakeBase(int x, int y, std::optional<int> initialPopulation = 3);
};

// BaseFixture plus real Factions and unit designs — for the universal-routing lanes
// (FactionUnits, WorldGlobal, two-level ratings, unit auras).
struct FactionFixture : BaseFixture
{
    FactionFixture();

    explicit FactionFixture(int width, int height,
                            std::vector<ac::EffectConfig_t> worldRules = {},
                            const OccupantFiles_t& rOccupants = {},
                            const ac::MapRulesConfig_t& rMapRules = TestMapRules());

    ac::SocialPolicyRegistry& socialPolicies();
    const ac::SocialPolicyRegistry& socialPolicies() const;
    ac::SocialRatingRegistry& socialRatings();
    const ac::SocialRatingRegistry& socialRatings() const;

    // A design from fixture components, one synthetic slot per component.
    const ac::UnitDesign& MakeDesign(const std::vector<std::string>& rComponentIds);

    // A live unit at (x, y), homed at pHomeBase but built nowhere, so it carries no production
    // grants. It registers itself in the map's position index for its lifetime.
    ac::Unit& MakeUnit(ac::Faction& rFaction, int x, int y,
                       const std::vector<std::string>& rComponentIds = {"test_chassis"},
                       ac::BaseManager* pHomeBase = nullptr);

    // A unit rBuiltAt produced: homed there, with its production grants and prototype status.
    ac::Unit& MakeBuiltUnit(ac::Faction& rFaction, int x, int y,
                            const std::vector<std::string>& rComponentIds,
                            ac::BaseManager& rBuiltAt);

    void MoveUnit(ac::Unit& rUnit, int x, int y);
};

// The fixture native units (one land and one sea lifeform), with every fungal bloom spawning
// between lifeformsMin and lifeformsMax of them.
void InstallNativeUnits(ac::GameDataContext& rData, int lifeformsMin, int lifeformsMax);

// Advances rState's mission year one year at a time, as turns do, until it is year.
void AdvanceToYear(ac::GameState& rState, int year);

// Raise every tile so the map is all land. Call on the fixture's map before any AddFaction.
void FillLand(ac::WorldMap& rMap);

// rDefinition must outlive the session.
ac::Faction& AddSessionFaction(FactionFixture& rFixtures, const ac::FactionConfig_t& rDefinition,
                               bool bIsPlayerControlled);

// The native-life faction a fungal bloom spawns lifeforms for.
ac::Faction& AddNativeLifeFaction(FactionFixture& rFixtures);

ac::BaseManager& MakeSessionBase(FactionFixture& rFixtures, ac::Faction& rFaction, int x, int y);

// CombatResolver::Resolve, then the collateral UnitAttackExecutor lands on the tile a killed
// defender left. For tests that fight a defender the tile would not field.
ac::CombatResult_t ResolveWithCollateral(ac::CombatResolver& rCombat, ac::WorldMap& rMap,
                                         ac::Unit& rAttacker, ac::Unit& rDefender);

// The attack a player declares on rTargetTile: the defender FindAttackableHostileOnTile picks,
// then TryAttack against it. nullopt when the tile has no attackable hostile.
std::optional<ac::CombatResult_t> AttackTile(ac::UnitAttackExecutor& rAttacks, ac::Unit& rAttacker,
                                             const ac::Tile& rTargetTile);

} // namespace actest
