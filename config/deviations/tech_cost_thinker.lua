-- Thinker revised_tech_cost / tech_alt_cost (refs/thinker/src/tech.cpp).
-- Not on GameDataPaths. Shipping uses config/tech_cost.lua (terranx tech_rate at 0x5BE6B0).
-- Faction TECHCOST, Thinker's tech_rate_modifier (discovery rate) and tech_stagnate_rate are the
-- TechCost product after this function returns, as for the shipping formula.
--
-- Engine inputs it reads (see config/tech_cost.lua):
--   techs                : stands in for Thinker's our_techs (known techs with a real first
--                          prerequisite); floored at 1, where our_techs can be 0
--   is_ai                : 1 if AI faction, 0 if human
--   catch_up_diff        : TechCostCatchUp, raw DiffLevel (0=Citizen … 5=Transcend)
--   map_area_sq_root     : integer sqrt of the map's tile count (56 on a standard map)
--
-- Still needs per-target pricing (Thinker prices at selection; the shipping formula is fixed at
-- each breakthrough) and these bindings:
--   level                : prerequisite depth of the target tech (1 if unknown)
--   commlink_owners      : other live commlink owners of this tech; pact or infiltrator
--                          counts as two

local map_area_divisor = 56
-- Citizen … Transcend. Thinker conf.tech_cost_factor[MaxDiffNum].
local tech_cost_factor = {124, 116, 108, 100, 84, 76}

function tech_cost_formula()
    local cost_base = 5 * level * level * level + 25 * level * level + 20 * techs
    local diff_scale = 1.0
    if is_ai ~= 0 then
        diff_scale = tech_cost_factor[catch_up_diff + 1] / 100.0
    end
    local known = math.max(6, math.min(16, techs + 6))
    local cost_diff = diff_scale * known / 16.0

    local cost = cost_base * cost_diff * map_area_sq_root / map_area_divisor
    cost = cost * (1.0 - 0.05 * math.min(6, commlink_owners))

    return math.max(1, math.floor(cost))
end

return { cost_formula = "tech_cost_formula()" }
