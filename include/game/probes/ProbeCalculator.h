#pragma once

namespace ac
{

struct ProbeCostConfig_t;
struct DrainFormulas_t;
struct PlagueFormulas_t;
class LuaRuntime;

// The variables a base mind-control cost formula reads.
struct MindControlCostInputs_t
{
    // Sums of garrison_probe_weight and garrison_probe_multiplier over the base tile.
    int garrisonWeight = 0;
    int garrisonMultiplier = 0;
    int mindControlTotal = 0;
    int population = 0;
    int energy = 0;
    // HQ distance after probe_distance_scale.
    int distance = 0;
};

// The variables a unit subversion cost formula reads.
struct SubvertCostInputs_t
{
    int mineralCost = 0;
    int energy = 0;
    int distance = 0;
};

// The variables a drain's share formula reads.
struct DrainShareInputs_t
{
    int treasury = 0;
    int population = 0;
    int factionPopulation = 0;
};

// The variables a plague's kill formula reads.
struct PlagueKillInputs_t
{
    int population = 0;
    // Techs carrying inc_gene_warfare_defense, and how many of them the target knows.
    int defenseTechs = 0;
    int knownDefenseTechs = 0;
    // The base has had a plague before.
    bool bRepeated = false;
};

// Evaluates probe_actions.json's formulas. Gathering the inputs, the cost multipliers and every
// refusal are ProbeRules' job.
class ProbeCalculator
{
public:
    explicit ProbeCalculator(LuaRuntime& rLua);

    int MindControlCost(const ProbeCostConfig_t& rCost,
                        const MindControlCostInputs_t& rInputs) const;
    int SubvertCost(const ProbeCostConfig_t& rCost, const SubvertCostInputs_t& rInputs) const;

    int DrainShare(const DrainFormulas_t& rDrain, const DrainShareInputs_t& rInputs) const;
    int DrainFloor(const DrainFormulas_t& rDrain, int share, int morale) const;
    int DrainAmount(const DrainFormulas_t& rDrain, int floorAmount, int draw) const;

    int PlagueKills(const PlagueFormulas_t& rPlague, const PlagueKillInputs_t& rInputs) const;
    int PlagueGarrisonDamageBound(const PlagueFormulas_t& rPlague, int hp) const;
    int PlagueGarrisonDamage(const PlagueFormulas_t& rPlague, int hp, int draw,
                             bool bRepeated) const;

private:
    // Not const: EvalInt mutates interpreter globals.
    LuaRuntime& m_rLua;
};

} // namespace ac
