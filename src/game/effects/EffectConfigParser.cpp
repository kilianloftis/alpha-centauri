#include "game/effects/EffectConfigParser.h"

#include "game/effects/TriggeredEffectParser.h"

#include <magic_enum.hpp>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ac
{
namespace EffectConfigParser
{

namespace
{

bool IsTileResourceStat_(StatId_t stat)
{
    return stat == StatId_t::Nutrients || stat == StatId_t::Minerals || stat == StatId_t::Energy;
}

void RequireScope_(EffectScope_t scope,
                   std::initializer_list<EffectScope_t> allowed,
                   const std::string& rErrorMessage)
{
    for (const EffectScope_t allowedScope : allowed)
    {
        if (scope == allowedScope)
        {
            return;
        }
    }
    throw std::runtime_error(rErrorMessage);
}

// Option A: amount_source legality is independent of DomainFor(stat). Subject domain is
// what AmountSourceValue needs; allowed stats/scopes are per-source (Energy stays Base
// while ElevationEnergySeed still requires a Tile subject at eval time).
void ValidateAmountSourceLegality_(const StatModifierEffect_t& rMod,
                                   const EffectConfig_t& rEffect,
                                   const std::string& rStatWire)
{
    const StatId_t stat = rMod.stat;
    const EffectScope_t scope = rEffect.scope;
    const double amount = rMod.amount;
    switch (*rMod.amountSource)
    {
        case StatModifierEffect_t::AmountSource_t::ElevationEnergy:
            // Required subject: Tile (+ the world's tile_yield_rules). Allowed: energy +
            // ThisTile. A radius makes no sense here — targetTile is the receiving tile, so an
            // aura would read the elevation of whatever it lands on, not its own host.
            if (stat != StatId_t::Energy)
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' ElevationEnergy is only valid on the energy "
                    "stat, got '"
                    + rStatWire + "'");
            }
            if (scope != EffectScope_t::ThisTile)
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' ElevationEnergy requires scope ThisTile");
            }
            if (rEffect.radius != 0)
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' ElevationEnergy cannot carry a radius: the "
                    "contribution is read from the receiving tile, not the aura's host");
            }
            break;
        case StatModifierEffect_t::AmountSource_t::MineralsConverted:
            // Required subject: Stockpile conversion. Allowed: stockpile-output stats +
            // ThisBase + Continuous (source-kind Stockpile checked in ValidateEffectForSource).
            if (!IsStockpileOutputStat(stat))
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' MineralsConverted is only valid on a stockpile "
                    "output stat (nutrients, energy, econ, labs, psych), got '"
                    + rStatWire
                    + "'. Minerals are the conversion input, so converting to them is a loop");
            }
            if (scope != EffectScope_t::ThisBase)
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' MineralsConverted requires scope ThisBase");
            }
            if (amount <= 0.0 || !std::isfinite(amount))
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' MineralsConverted requires amount > 0");
            }
            break;
        case StatModifierEffect_t::AmountSource_t::BaseSize:
            // Required subject: Base. Allowed on Base-domain stats with base/faction scopes.
            if (DomainFor(stat) != ResolveDomain_t::Base)
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' BaseSize is only valid on a Base-domain stat, got '"
                    + rStatWire + "'");
            }
            if (scope != EffectScope_t::ThisBase
                && scope != EffectScope_t::AllOwnerBases
                && scope != EffectScope_t::FactionGlobal)
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' BaseSize requires scope ThisBase, AllOwnerBases, "
                    "or FactionGlobal");
            }
            if (!std::isfinite(amount))
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' BaseSize requires a finite amount (per-pop scale)");
            }
            break;
        case StatModifierEffect_t::AmountSource_t::BasesOwned:
            // Required subject: Faction. Allowed on Unit-domain stats with ThisUnit
            // (weapon / chassis components that scale with empire size).
            if (DomainFor(stat) != ResolveDomain_t::Unit)
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' BasesOwned is only valid on a Unit-domain stat, got '"
                    + rStatWire + "'");
            }
            if (scope != EffectScope_t::ThisUnit)
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' BasesOwned requires scope ThisUnit");
            }
            if (!std::isfinite(amount))
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' BasesOwned requires a finite amount "
                    "(per-base scale)");
            }
            break;
        case StatModifierEffect_t::AmountSource_t::IntrinsicXp:
            // Required subject: Unit. Allowed on Unit-domain stats with ThisUnit
            // (e.g. Isle of the Deep cargo = 1 × intrinsic XP).
            if (DomainFor(stat) != ResolveDomain_t::Unit)
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' IntrinsicXp is only valid on a Unit-domain "
                    "stat, got '"
                    + rStatWire + "'");
            }
            if (scope != EffectScope_t::ThisUnit)
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' IntrinsicXp requires scope ThisUnit");
            }
            if (rMod.op != ModifierOp_t::Add)
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' IntrinsicXp requires op Add");
            }
            if (!std::isfinite(amount))
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' IntrinsicXp requires a finite amount "
                    "(per-XP scale)");
            }
            break;
        case StatModifierEffect_t::AmountSource_t::BuildingUpkeep:
            // Required subject: Base. Allowed: Continuous MaxClamp on econ (cap energy
            // commerce at facility upkeep). Base-level scopes only.
            if (stat != StatId_t::Econ)
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' BuildingUpkeep is only valid on the econ "
                    "stat, got '"
                    + rStatWire + "'");
            }
            if (rMod.op != ModifierOp_t::MaxClamp)
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' BuildingUpkeep requires op MaxClamp");
            }
            if (scope != EffectScope_t::ThisBase
                && scope != EffectScope_t::AllOwnerBases
                && scope != EffectScope_t::FactionGlobal)
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' BuildingUpkeep requires scope ThisBase, "
                    "AllOwnerBases, or FactionGlobal");
            }
            if (!std::isfinite(amount))
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' BuildingUpkeep requires a finite amount "
                    "(upkeep scale)");
            }
            break;
    }
}

void ParseGrantBuilding_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    GrantBuildingEffect_t grantBuilding;
    grantBuilding.buildingId = parameters.value("building_id", "");
    if (grantBuilding.buildingId.empty())
    {
        throw std::runtime_error("GrantBuilding effect missing required 'building_id'");
    }
    rEffect.effect = grantBuilding;
}

void ParseInfiltration_(const nlohmann::json& /*parameters*/, EffectConfig_t& rEffect)
{
    RequireScope_(
        rEffect.scope,
        {EffectScope_t::FactionGlobal, EffectScope_t::WorldGlobal},
        "Infiltration requires scope FactionGlobal or WorldGlobal");
    if (rEffect.scope == EffectScope_t::FactionGlobal && !rEffect.factionFilter)
    {
        throw std::runtime_error(
            "FactionGlobal Infiltration requires a factionFilter "
            "(WorldGlobal without a filter means all other factions)");
    }
    if (rEffect.factionFilter
        && rEffect.factionFilter->kind == FactionFilterKind_t::ActionTarget)
    {
        // Only a probe mission supplies an action target, and a mission fires a triggered
        // list — there is no continuous context that could resolve this filter.
        throw std::runtime_error(
            "factionFilter ActionTarget requires the triggered 'SetInfiltration' effect, not "
            "the continuous 'Infiltration'");
    }
    rEffect.effect = InfiltrationEffect_t{};
}

void ParseStatModifier_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    StatModifierEffect_t statModifier;
    statModifier.stat = ParseStatId(parameters.value("stat", ""));
    statModifier.op = ParseModifierOp(parameters.value("op", "Add"));
    if (parameters.contains("amount_source"))
    {
        // Any op. An amount source computes the modifier's *amount*; ResolveStatModifiers calls
        // AmountSourceValue for every contribution and hands (amount, op) to ApplyModifierStack,
        // which is op-agnostic. So "MaxClamp at base size" resolves exactly like "Add 0.25 per
        // pop" — the per-source legality rules below (stat domain, scope, finiteness) are what
        // actually constrain this, not the op.
        statModifier.amountSource =
            ParseAmountSource(parameters.at("amount_source").get<std::string>());
        // amount is the per-source scale when amount_source is set (default 1).
        statModifier.amount = ParseNumber(parameters, "amount", 1.0);
        ValidateAmountSourceLegality_(statModifier, rEffect, parameters.value("stat", ""));
    }
    else
    {
        statModifier.amount = ParseNumber(parameters, "amount", 0.0);
    }
    // Optional per-tile selector: when present, this modifier applies to each worked
    // tile satisfying the selector instead of once at the base level. Selectors are
    // resolved only during tile-yield resolution, so they are rejected on any stat
    // that isn't a tile resource — such a modifier would silently never apply.
    if (parameters.contains("selector"))
    {
        if (!IsTileResourceStat_(statModifier.stat))
        {
            throw std::runtime_error(
                "StatModifier 'selector' is only valid on tile resource "
                "stats (nutrients/minerals/energy), got '" + parameters.value("stat", "") + "'");
        }
        // A selector routes the modifier through tile-yield resolution, which supplies only a
        // tile subject and never the base / faction / stockpile ones. Rather than enumerate
        // which pairings are impossible (and silently miss the next amount_source added), the
        // combination is rejected outright.
        if (statModifier.amountSource)
        {
            throw std::runtime_error(
                "StatModifier 'selector' cannot be combined with 'amount_source'");
        }
        statModifier.selector = ParseTileSelector(parameters.at("selector"));
    }
    // MaxClamp / MinClamp on tile resources must carry a selector so they apply per-tile
    // (via ResolveTileYield) rather than clamping the base-wide production total.
    if ((statModifier.op == ModifierOp_t::MaxClamp || statModifier.op == ModifierOp_t::MinClamp)
        && IsTileResourceStat_(statModifier.stat) && !statModifier.selector)
    {
        throw std::runtime_error(
            "StatModifier MaxClamp/MinClamp on tile resource stats requires a 'selector' "
            "(e.g. AnyTile) so the clamp applies per tile, not to base-level totals");
    }
    statModifier.bypassClamp = parameters.value("bypass_clamp", false);
    if (statModifier.bypassClamp && !IsTileResourceStat_(statModifier.stat))
    {
        throw std::runtime_error(
            "StatModifier 'bypass_clamp' is only valid on tile resource "
            "stats (nutrients/minerals/energy), got '" + parameters.value("stat", "") + "'");
    }
    if (statModifier.bypassClamp && statModifier.op != ModifierOp_t::Add)
    {
        throw std::runtime_error("StatModifier 'bypass_clamp' requires op Add");
    }
    rEffect.effect = statModifier;
}

void ParseRuleFlag_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    RuleFlagEffect_t ruleFlag;
    const std::string flagStr = parameters.value("flag", "");
    if (flagStr.empty())
    {
        throw std::runtime_error("RuleFlag effect missing required 'flag'");
    }
    ruleFlag.flag = ParseRuleFlagId(flagStr);
    if (ruleFlag.flag == RuleFlagId_t::Harbors)
    {
        const std::string domainStr = parameters.value("domain", "");
        if (domainStr.empty())
        {
            throw std::runtime_error("RuleFlag 'harbors' requires a 'domain' string");
        }
        ruleFlag.domain = ParseUnitDomain(domainStr);
    }
    else if (parameters.contains("domain"))
    {
        throw std::runtime_error(
            "RuleFlag 'domain' is only valid with flag 'harbors'");
    }
    rEffect.effect = ruleFlag;
}

void ParseInteractionOverride_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    if (rEffect.scope != EffectScope_t::ThisUnit
        && rEffect.scope != EffectScope_t::FactionUnits)
    {
        throw std::runtime_error(
            "InteractionOverride scope must be ThisUnit or FactionUnits");
    }

    InteractionOverrideEffect_t overrideFx;
    const std::string gridStr = parameters.value("grid", "");
    if (gridStr.empty())
    {
        throw std::runtime_error("InteractionOverride missing required 'grid'");
    }
    if (gridStr == "enter")
    {
        overrideFx.grid = InteractionGridId_t::Enter;
    }
    else if (gridStr == "attack_tile")
    {
        overrideFx.grid = InteractionGridId_t::AttackTile;
    }
    else if (gridStr == "attack_unit")
    {
        overrideFx.grid = InteractionGridId_t::AttackUnit;
    }
    else if (gridStr == "zoc")
    {
        overrideFx.grid = InteractionGridId_t::Zoc;
    }
    else
    {
        throw std::runtime_error("Unknown InteractionOverride grid: '" + gridStr + "'");
    }

    // cell is required. Either polarity is legal; resolve skips overrides that restate the
    // stock cell (non-default only), so first-match order among overrides never matters.
    const std::string cellStr = parameters.value("cell", "");
    if (cellStr.empty())
    {
        throw std::runtime_error("InteractionOverride missing required 'cell'");
    }
    if (cellStr == "allow")
    {
        overrideFx.cell = InteractionCell_t::Allow;
    }
    else if (cellStr == "deny")
    {
        overrideFx.cell = InteractionCell_t::Deny;
    }
    else
    {
        throw std::runtime_error("Unknown InteractionOverride cell: '" + cellStr + "'");
    }

    auto parseDomainOpt = [&](const char* key) -> std::optional<UnitDomain_t> {
        if (!parameters.contains(key))
        {
            return std::nullopt;
        }
        if (!parameters.at(key).is_string())
        {
            throw std::runtime_error(std::string("InteractionOverride '") + key
                                     + "' must be a string");
        }
        return ParseUnitDomain(parameters.at(key).get<std::string>());
    };

    auto parseSurfaceOpt = [&]() -> std::optional<InteractionSurface_t> {
        if (!parameters.contains("surface"))
        {
            return std::nullopt;
        }
        if (!parameters.at("surface").is_string())
        {
            throw std::runtime_error("InteractionOverride 'surface' must be a string");
        }
        const std::string surfaceStr = parameters.at("surface").get<std::string>();
        if (surfaceStr == "land")
        {
            return InteractionSurface_t::Land;
        }
        if (surfaceStr == "water")
        {
            return InteractionSurface_t::Water;
        }
        throw std::runtime_error("Unknown InteractionOverride surface: '" + surfaceStr + "'");
    };

    auto parseFootingOpt = [&]() -> std::optional<InteractionFooting_t> {
        if (!parameters.contains("footing"))
        {
            return std::nullopt;
        }
        if (!parameters.at("footing").is_string())
        {
            throw std::runtime_error("InteractionOverride 'footing' must be a string");
        }
        const std::string footingStr = parameters.at("footing").get<std::string>();
        if (footingStr == "land")
        {
            return InteractionFooting_t::Land;
        }
        if (footingStr == "water")
        {
            return InteractionFooting_t::Water;
        }
        if (footingStr == "embarked")
        {
            return InteractionFooting_t::Embarked;
        }
        throw std::runtime_error("Unknown InteractionOverride footing: '" + footingStr + "'");
    };

    overrideFx.actorDomain = parseDomainOpt("actor_domain");
    overrideFx.surface = parseSurfaceOpt();
    overrideFx.footing = parseFootingOpt();
    overrideFx.targetDomain = parseDomainOpt("target_domain");

    // actor_domain is the row on every grid, so only the column needs checking: reject the
    // two columns that do not belong to the selected grid, and typos fail at load rather
    // than silently never matching.
    const bool bSurface = overrideFx.surface.has_value();
    const bool bFooting = overrideFx.footing.has_value();
    const bool bTarget = overrideFx.targetDomain.has_value();
    switch (overrideFx.grid)
    {
    case InteractionGridId_t::Enter:
        if (bFooting || bTarget)
        {
            throw std::runtime_error(
                "InteractionOverride enter only accepts actor_domain / surface axes");
        }
        break;
    case InteractionGridId_t::AttackTile:
        if (bSurface || bTarget)
        {
            throw std::runtime_error(
                "InteractionOverride attack_tile only accepts actor_domain / footing axes");
        }
        break;
    case InteractionGridId_t::AttackUnit:
    case InteractionGridId_t::Zoc:
        if (bSurface || bFooting)
        {
            throw std::runtime_error(
                "InteractionOverride attack_unit / zoc only accept actor_domain / "
                "target_domain axes");
        }
        break;
    }

    rEffect.effect = overrideFx;
}

void ParseSocialEngineeringOverride_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    // TODO: define parsing when social engineering rules are finalized
    SocialEngineeringOverrideEffect_t seOverride;
    seOverride.category = parameters.value("category", "");
    seOverride.choice = parameters.value("choice", "");
    rEffect.effect = seOverride;
}

void ParseDiplomaticModifier_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    // TODO: define parsing when diplomatic modifier rules are finalized
    DiplomaticModifierEffect_t diplomatic;
    diplomatic.targetFactionId = parameters.value("target_faction_id", "");
    diplomatic.value = static_cast<int>(ParseNumber(parameters, "value", 0.0));
    rEffect.effect = diplomatic;
}

void ParseSocialRatingModifier_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    SocialRatingModifierEffect_t ratingMod;
    const std::string ratingStr = parameters.value("rating", "");
    if (ratingStr.empty())
    {
        throw std::runtime_error("SocialRatingModifier effect missing required 'rating'");
    }
    ratingMod.rating = ParseSocialRatingId(ratingStr);
    ratingMod.amount = static_cast<int>(ParseNumber(parameters, "amount", 0.0));
    rEffect.effect = ratingMod;
}

void ParseConceal_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    ConcealEffect_t conceal;
    conceal.channel = parameters.value("channel", "");
    if (conceal.channel.empty())
    {
        throw std::runtime_error("Conceal effect missing required 'channel'");
    }
    rEffect.effect = conceal;
}

void ParseDetect_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    DetectEffect_t detect;
    detect.channel = parameters.value("channel", "");
    if (detect.channel.empty())
    {
        throw std::runtime_error("Detect effect missing required 'channel'");
    }
    rEffect.effect = detect;
}

void ParseOrbitalAttack_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    RequireScope_(
        rEffect.scope,
        {EffectScope_t::FactionGlobal, EffectScope_t::AllOwnerBases},
        "OrbitalAttack requires scope FactionGlobal or AllOwnerBases");
    OrbitalAttackEffect_t orbitalAttack;
    orbitalAttack.chance = static_cast<int>(RequireNumber(parameters, "chance"));
    if (orbitalAttack.chance < 0 || orbitalAttack.chance > 100)
    {
        throw std::runtime_error("OrbitalAttack 'chance' must be in [0, 100]");
    }
    orbitalAttack.cooldownTurns = static_cast<int>(RequireNumber(parameters, "cooldown_turns"));
    if (orbitalAttack.cooldownTurns < 0)
    {
        throw std::runtime_error("OrbitalAttack 'cooldown_turns' must be >= 0");
    }
    orbitalAttack.chanceOfDestructionOnFail =
        static_cast<int>(ParseNumber(parameters, "chance_of_destruction_on_fail", 0.0));
    if (orbitalAttack.chanceOfDestructionOnFail < 0
        || orbitalAttack.chanceOfDestructionOnFail > 100)
    {
        throw std::runtime_error(
            "OrbitalAttack 'chance_of_destruction_on_fail' must be in [0, 100]");
    }
    rEffect.effect = orbitalAttack;
}

void ParseIntercept_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    if (!rEffect.condition)
    {
        throw std::runtime_error("Intercept requires a condition");
    }
    InterceptEffect_t intercept;
    intercept.chance = static_cast<int>(RequireNumber(parameters, "chance"));
    if (intercept.chance < 0 || intercept.chance > 100)
    {
        throw std::runtime_error("Intercept 'chance' must be in [0, 100]");
    }
    if (parameters.contains("cooldown_turns"))
    {
        intercept.cooldownTurns = static_cast<int>(RequireNumber(parameters, "cooldown_turns"));
        if (intercept.cooldownTurns < 0)
        {
            throw std::runtime_error("Intercept 'cooldown_turns' must be >= 0");
        }
    }
    intercept.chanceOfDestructionOnFail =
        static_cast<int>(ParseNumber(parameters, "chance_of_destruction_on_fail", 0.0));
    if (intercept.chanceOfDestructionOnFail < 0 || intercept.chanceOfDestructionOnFail > 100)
    {
        throw std::runtime_error(
            "Intercept 'chance_of_destruction_on_fail' must be in [0, 100]");
    }
    rEffect.effect = intercept;
}

void ParseScramble_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    if (!rEffect.condition)
    {
        throw std::runtime_error("Scramble requires a condition");
    }
    ScrambleEffect_t scramble;
    scramble.range = static_cast<int>(RequireNumber(parameters, "range"));
    if (scramble.range <= 0)
    {
        throw std::runtime_error("Scramble 'range' must be > 0");
    }
    rEffect.effect = scramble;
}

void ParseTransportParams_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    RequireScope_(
        rEffect.scope,
        {EffectScope_t::ThisUnit},
        "TransportParams requires scope ThisUnit");
    if (parameters.contains("passenger_domains"))
    {
        throw std::runtime_error(
            "TransportParams no longer accepts 'passenger_domains'; use 'carries'");
    }
    if (parameters.contains("load_site_flags"))
    {
        throw std::runtime_error(
            "TransportParams no longer accepts 'load_site_flags'; use 'requires_harbor'");
    }
    if (parameters.contains("refuels_cargo"))
    {
        throw std::runtime_error(
            "TransportParams no longer accepts 'refuels_cargo'; carrying a domain refuels "
            "that cargo");
    }
    TransportParamsEffect_t transport;
    if (parameters.contains("carries"))
    {
        if (!parameters.at("carries").is_array())
        {
            throw std::runtime_error("TransportParams 'carries' must be an array");
        }
        for (const auto& rDomainJson : parameters.at("carries"))
        {
            if (!rDomainJson.is_string())
            {
                throw std::runtime_error(
                    "TransportParams carries entries must be strings");
            }
            transport.carries.push_back(ParseUnitDomain(rDomainJson.get<std::string>()));
        }
    }
    transport.requiresHarbor = parameters.value("requires_harbor", false);
    if (transport.carries.empty() && !transport.requiresHarbor)
    {
        throw std::runtime_error(
            "TransportParams requires non-empty 'carries' or 'requires_harbor': true");
    }
    rEffect.effect = transport;
}

using ParseEffectFn_ = void (*)(const nlohmann::json& parameters, EffectConfig_t& rEffect);

const std::unordered_map<std::string, ParseEffectFn_>& EffectTypeParsers_()
{
    static const std::unordered_map<std::string, ParseEffectFn_> k_Parsers = {
        {"GrantBuilding", ParseGrantBuilding_},
        {"Infiltration", ParseInfiltration_},
        {"StatModifier", ParseStatModifier_},
        {"RuleFlag", ParseRuleFlag_},
        {"InteractionOverride", ParseInteractionOverride_},
        {"SocialEngineeringOverride", ParseSocialEngineeringOverride_},
        {"DiplomaticModifier", ParseDiplomaticModifier_},
        {"SocialRatingModifier", ParseSocialRatingModifier_},
        {"Conceal", ParseConceal_},
        {"Detect", ParseDetect_},
        {"OrbitalAttack", ParseOrbitalAttack_},
        {"Intercept", ParseIntercept_},
        {"Scramble", ParseScramble_},
        {"TransportParams", ParseTransportParams_},
    };
    return k_Parsers;
}

} // namespace

ModifierOp_t ParseModifierOp(const std::string& rOp)
{
    const auto op = magic_enum::enum_cast<ModifierOp_t>(rOp);
    if (!op.has_value())
    {
        throw std::runtime_error("Unknown modifier op: '" + rOp + "'");
    }
    return *op;
}

StatModifierEffect_t::AmountSource_t ParseAmountSource(const std::string& rSource)
{
    const auto source = magic_enum::enum_cast<StatModifierEffect_t::AmountSource_t>(rSource);
    if (!source.has_value())
    {
        throw std::runtime_error("Unknown amount_source: '" + rSource + "'");
    }
    return *source;
}

EffectScope_t ParseEffectScope(const std::string& rScope)
{
    const auto scope = magic_enum::enum_cast<EffectScope_t>(rScope);
    if (!scope.has_value())
    {
        throw std::runtime_error("Unknown effect scope: '" + rScope + "'");
    }
    return *scope;
}

double ParseNumber(const nlohmann::json& parameters, const std::string& key, double defaultValue)
{
    const auto it = parameters.find(key);
    if (it == parameters.end())
    {
        return defaultValue;
    }
    if (it->is_number())
    {
        return it->get<double>();
    }
    if (it->is_string())
    {
        const std::string& rStr = it->get_ref<const std::string&>();
        try
        {
            std::size_t idx = 0;
            const double value = std::stod(rStr, &idx);
            if (idx != rStr.size())
            {
                throw std::runtime_error(
                    "Invalid numeric string for parameter '" + key
                    + "': trailing characters");
            }
            return value;
        }
        catch (const std::invalid_argument&)
        {
            throw std::runtime_error("Invalid numeric string for parameter '" + key + "'");
        }
        catch (const std::out_of_range&)
        {
            throw std::runtime_error(
                "Numeric string out of range for parameter '" + key + "'");
        }
    }
    throw std::runtime_error("Expected a number or numeric string for parameter '" + key + "'");
}

double RequireNumber(const nlohmann::json& parameters, const std::string& key)
{
    if (!parameters.contains(key))
    {
        throw std::runtime_error("Missing required parameter '" + key + "'");
    }
    return ParseNumber(parameters, key, 0.0);
}

Condition_t ParseCondition(const nlohmann::json& conditionJson)
{
    const std::string kindStr = conditionJson.value("kind", "");
    if (kindStr == "IsDefending")
    {
        return IsDefending_t{};
    }
    if (kindStr == "OriginBaseIsTargetBase")
    {
        return OriginBaseIsTargetBase_t{};
    }
    if (kindStr == "OriginBaseIsHomeBase")
    {
        return OriginBaseIsHomeBase_t{};
    }
    if (kindStr == "AttackerIsEmbarked")
    {
        return AttackerIsEmbarked_t{};
    }
    if (kindStr == "HasAirdroppedThisTurn")
    {
        return HasAirdroppedThisTurn_t{};
    }
    if (kindStr == "AttackerDomain")
    {
        if (!conditionJson.contains("domains") || !conditionJson.at("domains").is_array()
            || conditionJson.at("domains").empty())
        {
            throw std::runtime_error(
                "AttackerDomain condition requires a non-empty 'domains' array");
        }
        AttackerDomain_t attackerDomain;
        for (const auto& rDomain : conditionJson.at("domains"))
        {
            if (!rDomain.is_string())
            {
                throw std::runtime_error(
                    "AttackerDomain condition domains must be strings");
            }
            attackerDomain.domains.push_back(ParseUnitDomain(rDomain.get<std::string>()));
        }
        return attackerDomain;
    }
    if (kindStr == "DefenderDomain")
    {
        if (!conditionJson.contains("domains") || !conditionJson.at("domains").is_array()
            || conditionJson.at("domains").empty())
        {
            throw std::runtime_error(
                "DefenderDomain condition requires a non-empty 'domains' array");
        }
        DefenderDomain_t defenderDomain;
        for (const auto& rDomain : conditionJson.at("domains"))
        {
            if (!rDomain.is_string())
            {
                throw std::runtime_error(
                    "DefenderDomain condition domains must be strings");
            }
            defenderDomain.domains.push_back(ParseUnitDomain(rDomain.get<std::string>()));
        }
        return defenderDomain;
    }
    if (kindStr == "IsHeadquarters")
    {
        return IsHeadquarters_t{};
    }
    if (kindStr == "SubjectDomain")
    {
        const std::string domainStr = conditionJson.value("domain", "");
        if (domainStr.empty())
        {
            throw std::runtime_error("SubjectDomain condition requires a non-empty 'domain'");
        }
        return SubjectDomain_t{ParseUnitDomain(domainStr)};
    }
    if (kindStr == "HasComponent")
    {
        const std::string componentId = conditionJson.value("component", "");
        if (componentId.empty())
        {
            throw std::runtime_error(
                "HasComponent condition requires a non-empty 'component' id");
        }
        return HasComponent_t{componentId};
    }
    if (kindStr == "SubjectDesign")
    {
        const std::string designId = conditionJson.value("design", "");
        if (designId.empty())
        {
            throw std::runtime_error(
                "SubjectDesign condition requires a non-empty 'design' id");
        }
        return SubjectDesign_t{designId};
    }
    if (kindStr == "HasFlag")
    {
        const std::string flagId = conditionJson.value("flag", "");
        if (flagId.empty())
        {
            throw std::runtime_error("HasFlag condition requires a non-empty 'flag' id");
        }
        return HasFlag_t{ParseRuleFlagId(flagId)};
    }
    if (kindStr == "IsPrototype")
    {
        return IsPrototype_t{};
    }
    if (kindStr == "IsCombatUnit")
    {
        return IsCombatUnit_t{};
    }
    if (kindStr == "BaseHasBuilding")
    {
        const std::string buildingId = conditionJson.value("building", "");
        if (buildingId.empty())
        {
            throw std::runtime_error(
                "BaseHasBuilding condition requires a non-empty 'building' id");
        }
        return BaseHasBuilding_t{buildingId};
    }
    if (kindStr == "AllOf")
    {
        const bool bHasValues = conditionJson.contains("values")
            && conditionJson.at("values").is_array()
            && !conditionJson.at("values").empty();
        const bool bHasConditions = conditionJson.contains("conditions")
            && conditionJson.at("conditions").is_array()
            && !conditionJson.at("conditions").empty();
        if (!bHasValues && !bHasConditions)
        {
            throw std::runtime_error(
                "AllOf condition requires a non-empty 'values' and/or 'conditions' array");
        }

        AllOf_t allOf;
        if (bHasValues)
        {
            for (const auto& rValue : conditionJson.at("values"))
            {
                if (!rValue.is_string() || rValue.get<std::string>().empty())
                {
                    throw std::runtime_error("AllOf condition values must be non-empty strings");
                }
                allOf.conditions.push_back(TargetTileHas_t{rValue.get<std::string>()});
            }
        }
        if (bHasConditions)
        {
            for (const auto& rNested : conditionJson.at("conditions"))
            {
                allOf.conditions.push_back(ParseCondition(rNested));
            }
        }
        return allOf;
    }
    if (kindStr == "TargetTileHas")
    {
        const std::string featureId = conditionJson.value("value", "");
        if (featureId.empty())
        {
            throw std::runtime_error("Condition requires a non-empty 'value'");
        }
        return TargetTileHas_t{featureId};
    }

    throw std::runtime_error("Unknown condition kind: '" + kindStr + "'");
}

TileSelector_t ParseTileSelector(const nlohmann::json& selectorJson)
{
    const std::string kindStr = selectorJson.value("kind", "BaseTile");
    if (kindStr == "BaseTile")
    {
        return TileSelectorBaseTile_t{};
    }
    if (kindStr == "HasImprovement")
    {
        const std::string improvementId = selectorJson.value("improvement", "");
        if (improvementId.empty())
        {
            throw std::runtime_error("HasImprovement selector requires a non-empty 'improvement' id");
        }
        return TileSelectorHasImprovement_t{improvementId};
    }
    if (kindStr == "AnyTile")
    {
        return TileSelectorAnyTile_t{};
    }

    throw std::runtime_error("Unknown tile selector kind: '" + kindStr + "'");
}

UnitDomain_t ParseUnitDomain(const std::string& rDomain)
{
    if (rDomain == "land") return UnitDomain_t::Land;
    if (rDomain == "sea")  return UnitDomain_t::Sea;
    if (rDomain == "air")  return UnitDomain_t::Air;
    if (rDomain == "orbital") return UnitDomain_t::Orbital;
    throw std::runtime_error(
        "Unknown unit domain '" + rDomain + "' (expected land, sea, air, or orbital)");
}

BuildingFilter_t ParseBuildingFilter(const nlohmann::json& filterJson)
{
    const std::string kindStr = filterJson.value("kind", "");
    if (kindStr == "All")
    {
        return BuildingFilterAll_t{};
    }
    if (kindStr == "BuildingId")
    {
        const std::string buildingId = filterJson.value("building", "");
        if (buildingId.empty())
        {
            throw std::runtime_error(
                "BuildingId buildingFilter requires a non-empty 'building' id");
        }
        return BuildingFilterId_t{buildingId};
    }
    if (kindStr == "Category")
    {
        return BuildingFilterCategory_t{ParseGameCategoryField(filterJson)};
    }

    throw std::runtime_error("Unknown buildingFilter kind: '" + kindStr + "'");
}

FactionFilter_t ParseFactionFilter(const nlohmann::json& filterJson)
{
    FactionFilter_t filter;
    const std::string kindStr = filterJson.value("kind", "");
    if (kindStr == "ActionTarget")
    {
        filter.kind = FactionFilterKind_t::ActionTarget;
    }
    else if (kindStr == "CouncilMembers")
    {
        filter.kind = FactionFilterKind_t::CouncilMembers;
    }
    else if (kindStr == "PlayerType")
    {
        filter.kind = FactionFilterKind_t::PlayerType;
        const std::string typeStr = filterJson.value("type", "");
        const auto playerType =
            magic_enum::enum_cast<PlayerType_t>(typeStr, magic_enum::case_insensitive);
        if (!playerType)
        {
            throw std::runtime_error("PlayerType factionFilter requires 'type' Player or AI, got '"
                                     + typeStr + "'");
        }
        filter.playerType = *playerType;
    }
    else
    {
        throw std::runtime_error("Unknown factionFilter kind: '" + kindStr + "'");
    }
    return filter;
}

bool IsEffectType(const std::string& rTypeName)
{
    return EffectTypeParsers_().count(rTypeName) != 0;
}

EffectConfig_t ParseEffectConfig(const nlohmann::json& effectJson)
{
    EffectConfig_t effect;

    const std::string typeStr = effectJson.at("type").get<std::string>();
    if (TriggeredEffectParser::IsTriggeredEffectType(typeStr))
    {
        throw std::runtime_error(
            "'" + typeStr + "' is a one-shot effect and belongs in a trigger-named list "
            "(on_complete_effects / on_enter_effects / on_visit_effects / …), not 'effects'");
    }
    const std::string scopeStr = effectJson.at("scope").get<std::string>();
    const auto& parameters = effectJson.value("parameters", nlohmann::json::object());

    effect.scope = ParseEffectScope(scopeStr);
    effect.radius = effectJson.value("radius", 0);
    if (effect.radius < 0)
    {
        throw std::runtime_error("Effect 'radius' must be >= 0");
    }
    if (effect.radius != 0 && effect.scope != EffectScope_t::ThisTile)
    {
        throw std::runtime_error("Effect 'radius' is only valid with scope ThisTile");
    }
    effect.minRadius = effectJson.value("min_radius", 0);
    if (effect.minRadius < 0)
    {
        throw std::runtime_error("Effect 'min_radius' must be >= 0");
    }
    if (effect.minRadius != 0 && effect.scope != EffectScope_t::ThisTile)
    {
        throw std::runtime_error("Effect 'min_radius' is only valid with scope ThisTile");
    }
    if (effect.minRadius > effect.radius)
    {
        throw std::runtime_error("Effect 'min_radius' (" + std::to_string(effect.minRadius)
                                 + ") must be <= 'radius' (" + std::to_string(effect.radius)
                                 + "); the effect would reach no tile at all");
    }
    if (effectJson.contains("condition"))
    {
        effect.condition = ParseCondition(effectJson.at("condition"));
    }
    if (effectJson.contains("buildingFilter"))
    {
        effect.buildingFilter = ParseBuildingFilter(effectJson.at("buildingFilter"));
    }
    if (effectJson.contains("factionFilter"))
    {
        effect.factionFilter = ParseFactionFilter(effectJson.at("factionFilter"));
    }
    effect.removedByTech = effectJson.value("removed_by_tech", "");

    const auto& parsers = EffectTypeParsers_();
    const auto it = parsers.find(typeStr);
    if (it == parsers.end())
    {
        throw std::runtime_error("Unknown effect type: '" + typeStr + "'");
    }
    it->second(parameters, effect);

    return effect;
}

void ValidateEffectForSource(const EffectConfig_t& rEffect, EffectSourceKind_t sourceKind,
                             const std::string& rSourceId)
{
    const EffectScope_t scope = rEffect.scope;

    const auto* pStatModifier = std::get_if<StatModifierEffect_t>(&rEffect.effect);
    if (pStatModifier
        && pStatModifier->amountSource == StatModifierEffect_t::AmountSource_t::MineralsConverted
        && sourceKind != EffectSourceKind_t::Stockpile)
    {
        throw std::runtime_error(
            "Effect on '" + rSourceId
            + "': amount_source MineralsConverted is only valid on a stockpile config — nothing "
              "else converts minerals, so it would never resolve");
    }

    if (scope == EffectScope_t::ThisPop && sourceKind != EffectSourceKind_t::PopType)
    {
        throw std::runtime_error("Effect on '" + rSourceId
            + "': scope ThisPop is only meaningful on a pop type config");
    }

    // Mood weights are summed by walking the seated pops, never resolved through
    // ResolveBaseStat — a weight emitted anywhere but a pop type would silently never apply.
    if (pStatModifier
        && (pStatModifier->stat == StatId_t::RiotWeight
            || pStatModifier->stat == StatId_t::GoldenAgeWeight)
        && (sourceKind != EffectSourceKind_t::PopType || scope != EffectScope_t::ThisPop))
    {
        throw std::runtime_error(
            "Effect on '" + rSourceId
            + "': riot_weight / golden_age_weight are per-pop and must be ThisPop on a pop type "
              "config");
    }
    if (scope == EffectScope_t::ThisUnit
        && sourceKind != EffectSourceKind_t::UnitComponent
        && sourceKind != EffectSourceKind_t::MoraleLevel
        && sourceKind != EffectSourceKind_t::NativeUnit)
    {
        throw std::runtime_error("Effect on '" + rSourceId
            + "': scope ThisUnit is only meaningful on a unit component, morale level, "
              "or native unit config");
    }
    if (scope == EffectScope_t::ThisTech)
    {
        if (sourceKind != EffectSourceKind_t::Tech)
        {
            throw std::runtime_error("Effect on '" + rSourceId
                + "': scope ThisTech is only meaningful on a tech config");
        }
        if (pStatModifier == nullptr || pStatModifier->stat != StatId_t::TechCost)
        {
            throw std::runtime_error(
                "Effect on '" + rSourceId
                + "': scope ThisTech only accepts StatModifier tech_cost (research cost "
                  "modifiers for this tech as a research target)");
        }
    }
    if (scope == EffectScope_t::ThisBase || scope == EffectScope_t::ProducedAtThisBase)
    {
        // Listed exhaustively (no default:) so adding an EffectSourceKind_t forces a decision
        // here rather than silently inheriting the rejection.
        bool bCanSupplyOriginBase = false;
        switch (sourceKind)
        {
        case EffectSourceKind_t::Building:
        case EffectSourceKind_t::PopType:
        case EffectSourceKind_t::SocialPolicy:
        case EffectSourceKind_t::SocialRating:
        // Stockpile effects are stamped with the converting base at conversion time.
        case EffectSourceKind_t::Stockpile:
            bCanSupplyOriginBase = true;
            break;
        case EffectSourceKind_t::UnitComponent:
        case EffectSourceKind_t::ProbeAction:
            // A continuous ThisBase effect here has no pool origin to be stamped with: the
            // producing / mission-target base only exists at the moment the trigger fires, so
            // that behaviour belongs in a triggered list instead.
            bCanSupplyOriginBase = false;
            break;
        case EffectSourceKind_t::PopCompositionBaseLocal:
            // Mood arrays are collected per base, so ThisBase is stamped with the rioting base
            // by the effects pool / base cache. ProducedAtThisBase is not collected here.
            bCanSupplyOriginBase = scope == EffectScope_t::ThisBase;
            break;
        case EffectSourceKind_t::PopComposition:
        case EffectSourceKind_t::Improvement:
        case EffectSourceKind_t::Faction:
        case EffectSourceKind_t::CouncilProposal:
        case EffectSourceKind_t::CouncilRules:
        case EffectSourceKind_t::TileYieldRules:
        case EffectSourceKind_t::Tech:
        case EffectSourceKind_t::Production:
        case EffectSourceKind_t::Difficulty:
        case EffectSourceKind_t::Growth:
        case EffectSourceKind_t::BaseConquest:
        case EffectSourceKind_t::PoliceRules:
        case EffectSourceKind_t::MoraleLevel:
        case EffectSourceKind_t::NativeUnit:
            bCanSupplyOriginBase = false;
            break;
        }
        if (!bCanSupplyOriginBase)
        {
            const char* pScopeName =
                scope == EffectScope_t::ThisBase ? "ThisBase" : "ProducedAtThisBase";
            throw std::runtime_error(
                "Effect on '" + rSourceId + "': scope " + pScopeName
                + " requires a source that can supply an origin base "
                  "(Building, PopType, SocialPolicy, SocialRating, or "
                  "PopCompositionBaseLocal). A unit component or probe action that should act "
                  "on the base at production / mission time wants a triggered list instead.");
        }
    }
}

std::vector<EffectConfig_t> ParseEffects(const nlohmann::json& rContainerJson)
{
    std::vector<EffectConfig_t> effects;
    if (rContainerJson.contains("effects"))
    {
        const nlohmann::json& rEffectsJson = rContainerJson.at("effects");
        if (!rEffectsJson.is_array())
        {
            throw std::runtime_error("'effects' must be a JSON array");
        }
        for (const auto& rEffectJson : rEffectsJson)
        {
            effects.push_back(ParseEffectConfig(rEffectJson));
        }
    }
    return effects;
}

std::vector<EffectConfig_t> ParseEffects(const nlohmann::json& rContainerJson,
                                         EffectSourceKind_t sourceKind,
                                         const std::string& rSourceId)
{
    std::vector<EffectConfig_t> effects = ParseEffects(rContainerJson);
    for (const EffectConfig_t& rEffect : effects)
    {
        ValidateEffectForSource(rEffect, sourceKind, rSourceId);
    }
    return effects;
}

} // namespace EffectConfigParser
} // namespace ac
