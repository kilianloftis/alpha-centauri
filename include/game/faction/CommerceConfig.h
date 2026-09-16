#pragma once

namespace ac
{

// Tunables for planetary commerce income. Loaded from config/commerce.json.
struct CommerceConfig_t
{
    // Multiplied by paired bases' combined pre-commerce energy, then ceiled.
    double pairMultiplier = 0.125;
    // Applied (then floored) when the pair has Friendship but not Pact.
    double treatyMultiplier = 0.5;
};

} // namespace ac
