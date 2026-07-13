#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace ac
{

// Exact rational for config values that may be ints (2) or fraction strings ("1/3").
struct Rational_t
{
    int numerator = 0;
    int denominator = 1;

    static Rational_t FromInt(int value);
    static Rational_t Parse(const std::string& rText);
    static Rational_t ParseJson(const nlohmann::json& rJson);

    // Returns (numerator * scale) / denominator. Throws if the product is not divisible
    // by the (reduced) denominator.
    int ScaledInt(int scale) const;
};

} // namespace ac
