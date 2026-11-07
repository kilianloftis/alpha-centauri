-- Planet fauna turn formulas (SMAC alien_fauna).
--
-- Variables set by the engine before evaluating each formula:
--   turn         : current turn number
--   fauna_forms  : session native-life abundance (0/1/2)
--   fauna_diff   : difficulty planet_fauna_diff
--   cargo_scale  : resolved planet_fauna_cargo_scale (1 normally, 2 under Perihelion)

function boat_period_formula()
    local span = 5 - fauna_forms
    if span <= 0 then return 0 end
    return 2 * span
end

function early_limit_threshold_formula()
    return 2 * (10 * (3 - fauna_forms) - fauna_diff)
end

function boat_cargo_formula()
    local inner = 4 * (8 - fauna_forms) - fauna_diff
    local divisor = math.max(1, 4 * inner)
    return math.floor(turn * cargo_scale / divisor) + fauna_forms + 1
end

function ambush_attempts_formula()
    return 2 * fauna_forms + 2
end

function swarm_year_threshold_formula()
    local span = 5 - fauna_forms
    if span <= 0 then return 0 end
    return 10 * span
end

return {
    boat_period_formula = "boat_period_formula()",
    early_limit_threshold_formula = "early_limit_threshold_formula()",
    boat_cargo_formula = "boat_cargo_formula()",
    ambush_attempts_formula = "ambush_attempts_formula()",
    swarm_year_threshold_formula = "swarm_year_threshold_formula()",
}
