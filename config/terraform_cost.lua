-- Raise/lower energy quote (docs/thinker/terranx-map.md `terraform_cost`).
--
-- Variables set by the engine before energy_cost:
--   elev_band              : |elev| / reference_level_meters
--   fungus                 : 1 when the tile has fungus, else 0
--   at_sea                 : 1 on water, else 0
--   sea_cost_scale         : resolved SeaRaiseLowerCostScale (seed 1; Doc Air MaxClamp 0.5)
--   nearest_base_distance  : own-base map range clamped 1..100 (1 with no base)
--   own_pop, enemy_pop     : nearest own / non-Pact enemy base population (0 when N/A)
--   own_range, enemy_range : map range to those bases (0 when N/A)

-- SMAC ×4 at sea before Doctrine: Air Power halves it.
local sea_cost_seed = 4

local function enemy_scale()
    local enemy_val = math.floor((enemy_pop + 2) * own_range / 3)
    local own_val = math.floor((own_pop + 2) * enemy_range / 3)
    if own_val <= 0 or enemy_val <= 0 or own_val >= enemy_val then
        return 1
    end
    return enemy_val / own_val
end

function energy_cost_formula()
    local fungus_scale = fungus > 0 and 6 or 1
    local sea_scale = at_sea > 0 and (sea_cost_seed * sea_cost_scale) or 1
    return math.max(1, math.min(30000, math.floor(
        ((elev_band + 2) ^ 2) * fungus_scale * sea_scale * nearest_base_distance
            * enemy_scale() / 2)))
end

return {
    energy_cost = "energy_cost_formula()",
}
