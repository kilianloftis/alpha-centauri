-- Same shape as config/unit_upgrade.lua; fixture-owned so shipping can change freely.
local energy_per_mineral = 1

function upgrade_energy_cost_formula()
    local minerals = new_base_cost
    if target_unprototyped ~= 0 then
        local half_up = (new_base_cost + 1) // 2
        local t = new_base_cost + half_up
        minerals = t * (t + 1)
    end
    minerals = minerals + math.max(0, new_weapon_mineral_cost - old_weapon_mineral_cost)
    minerals = minerals + math.max(0, new_armor_mineral_cost - old_armor_mineral_cost)
    return minerals * energy_per_mineral
end

return {
    energy_cost_formula = "upgrade_energy_cost_formula()",
}
