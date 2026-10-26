-- Tech cost formula matching terranx tech_rate (0x5BE6B0) through the map step.
-- Discovery rate, faction TECHCOST, ThisTech and stagnation are the TechCost product after this
-- function returns.
--
-- Variables set by the engine before evaluating cost_formula:
--   techs                : max(1, discovered count)
--   most_techs           : max discovered count across every session faction
--   diff                 : TechCostDiff (1=Citizen … 5=Transcend; Talent and Librarian are 3)
--   catch_up_diff        : TechCostCatchUp (0=Citizen … 5=Transcend); humans ignore this
--   turns                : terranx CurrentTurn (1 on the first playable year)
--   is_ai                : 1 if AI faction, 0 if human
--   turn_lag             : 1 if TechCostTurnLag is on
--   research             : TechCostResearch (inherent SOCIAL RESEARCH); subtracted from techs
--   map_area_sq_root     : integer sqrt of the map's tile count (56 on a standard map)

local map_area_divisor = 56
local human_catch_up_diff = 3

-- Toward-zero division (terranx idiv).
local function truncdiv(a, b)
    local ipart = math.modf(a / b)
    return math.tointeger(ipart)
end

function tech_cost_formula()
    local factor
    if is_ai ~= 0 then
        factor = 29 - (diff * 3)
    else
        factor = (diff * 4) + 8
    end
    factor = math.max(12 - techs, math.min(12 + techs, factor))

    local turns_divisor = turn_lag ~= 0 and 12 or 8
    local lag_scale = turn_lag ~= 0 and 3 or 2
    local turn_lag_amount = techs - turns // turns_divisor
    local lag_cap = factor * lag_scale // 2
    turn_lag_amount = math.max(0, math.min(turn_lag_amount, lag_cap))
    factor = factor + turn_lag_amount

    local used_catch_up = is_ai ~= 0 and catch_up_diff or human_catch_up_diff
    local reduction = (most_techs - used_catch_up - techs + 7) // (8 - used_catch_up)
    local max_reduction = used_catch_up * factor // 10 + 1
    if reduction < 0 or max_reduction < 0 then
        reduction = 0
    elseif reduction > max_reduction then
        reduction = max_reduction
    end
    factor = factor - reduction

    local tech_factor = math.max(1, techs - research)
    local cost = factor * tech_factor

    cost = truncdiv(cost * map_area_sq_root, map_area_divisor)

    return math.max(1, cost)
end

return {
    cost_formula = "tech_cost_formula()",
}
