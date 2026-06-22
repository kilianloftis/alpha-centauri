-- Production cost formula.
--
-- Variables set by the engine before evaluating cost_formula:
--   base_cost       : mineral cost from the item's definition
--   industry_rating : the base's industry rating

return {
    cost_formula = "base_cost * (10 * industry_rating)",
}
