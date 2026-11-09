-- Slider-imbalance loss percents after facility modifiers (terranx base_energy 0x4EC16D).
--
-- Variables set by the engine before each *_loss_formula:
--   econ_percent, labs_percent, psych_percent : faction slider shares (0–100)
--   imbalance_factor                          : allocation_imbalance_factor from Efficiency SE
--
-- Shipping SMAC taxes only the labs↔econ pair; psych_loss_formula returns 0. Mods can tax
-- psych the same way without a C++ change.

-- |labs% − econ%| / divisor (tenths used |L − E| / 2 → percents / 20).
local imbalance_divisor = 20
-- Multiplies imbalance × imbalance_factor.
local imbalance_scale = 2
-- Caps on the single vs double cut in the shipping labs/econ formulas.
local single_penalty_cap = 40
local double_penalty_cap = 80

local function pair_penalty()
    local imb = math.abs(labs_percent - econ_percent) // imbalance_divisor
    if imb <= 0 or imbalance_factor <= 0 then
        return 0
    end
    return imb * imbalance_factor * imbalance_scale
end

function labs_loss_formula()
    local penalty = pair_penalty()
    if penalty <= 0 then
        return 0
    end
    -- Labs-heavy: milder cut on labs; on a tie or econ-heavy: double cut on labs.
    if labs_percent > econ_percent then
        return math.min(penalty, single_penalty_cap)
    end
    return math.min(penalty * 2, double_penalty_cap)
end

function econ_loss_formula()
    local penalty = pair_penalty()
    if penalty <= 0 then
        return 0
    end
    if labs_percent > econ_percent then
        return math.min(penalty * 2, double_penalty_cap)
    end
    return math.min(penalty, single_penalty_cap)
end

function psych_loss_formula()
    return 0
end

return {
    labs_loss_formula = "labs_loss_formula()",
    econ_loss_formula = "econ_loss_formula()",
    psych_loss_formula = "psych_loss_formula()",
}
