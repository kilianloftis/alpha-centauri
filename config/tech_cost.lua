-- Tech cost formula for Alpha Centauri research.
--
-- Variables set by the engine before evaluating cost_formula:
--   techs                : TECHS (discovered techs excl. starting + varA - varB)
--   most_techs           : MOSTTECHS (max discovered by any faction + varA)
--   diff                 : 1=Citizen, 2=Specialist, 3=Talent/Librarian, 4=Thinker, 5=Transcend
--   turns                : number of turns elapsed
--   is_ai                : 1 if AI faction, 0 if human
--   tech_stagnation      : 1 if tech stagnation is active, 0 otherwise
--   research_modifier    : -1=natural bonus, 0=neutral, +1=natural penalty
--   world_size_modifier  : percentage modifier from world size (0 = no change)
--   faction_modifier     : percentage modifier from faction definition
--   alphax_modifier      : percentage modifier from alpha(x).txt
--   base_cost            : per-tech base cost scalar from Tech definition

function tech_cost_formula()
    -- Step 1: Difficulty-based base value, clamped to [-12, 12].
    local step1
    if is_ai ~= 0 then
        step1 = 29 - (diff * 3)
    else
        step1 = (diff * 4) + 8
    end
    step1 = math.max(-12, math.min(12, step1))

    -- Step 2: Clamp (TECHS - TURNS/divisor) to [0, step1 * max_multiplier].
    local turns_divisor   = tech_stagnation ~= 0 and 12  or 8
    local max_multiplier  = tech_stagnation ~= 0 and 1.5 or 1.0
    local turns_adj = techs - turns / turns_divisor
    local step2 = math.max(0, math.min(turns_adj, step1 * max_multiplier))

    -- Step 3: base + tech-lag adjustment.
    local step3 = step1 + step2

    -- Step 4: Subtract catch-up bonus, bounded by a max deduction.
    local catch_up
    if is_ai ~= 0 then
        -- TODO: Exact intermediate values for AI divisor and max % need verification.
        -- Divisor ranges from 8 (Citizen/diff=1) down to 3 (Transcend/diff=5).
        local ai_divisor = math.max(1, 8 - math.floor((diff - 1) * 5 / 4))
        -- Max deduction % ranges from 0 (Citizen) to 50 (Transcend).
        local ai_max_pct = math.floor((diff - 1) * 50 / 4)
        catch_up = math.ceil((most_techs - techs) / ai_divisor)
        local ai_max_deduction = math.floor(step3 * ai_max_pct / 100) + 1
        catch_up = math.min(catch_up, ai_max_deduction)
    else
        catch_up = math.ceil((most_techs - techs) / 5)
        local max_deduction = math.floor(step3 * 0.3) + 1
        catch_up = math.min(catch_up, max_deduction)
    end
    local step4 = step3 - catch_up

    -- Step 5: Multiply by tech count factor with research modifier applied.
    local tech_factor = math.max(1, techs + research_modifier)
    local step5 = tech_factor * step4

    -- Step 6: Apply world size, faction, and alpha(x).txt cost modifiers.
    -- TODO: Default formula (integration of base_cost) is TBD.
    local step6 = step5
    step6 = step6 + math.floor(step6 * world_size_modifier / 100)
    step6 = step6 + math.floor(step6 * faction_modifier     / 100)
    step6 = step6 + math.floor(step6 * alphax_modifier      / 100)
    if tech_stagnation ~= 0 then
        step6 = math.floor(step6 * 1.5)
    end

    return math.max(1, math.floor(step6))
end

return { cost_formula = "tech_cost_formula()" }
