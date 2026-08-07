#include "lib/Rational.h"

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace ac
{

namespace
{

int RequirePositiveDenominator_(int denominator)
{
    if (denominator <= 0)
    {
        throw std::runtime_error("Rational denominator must be positive");
    }
    return denominator;
}

} // namespace

Rational_t Rational_t::FromInt(int value)
{
    return Rational_t{value, 1};
}

Rational_t Rational_t::Parse(const std::string& rText)
{
    const auto slash = rText.find('/');
    if (slash == std::string::npos)
    {
        return FromInt(std::stoi(rText));
    }

    const int numerator = std::stoi(rText.substr(0, slash));
    const int denominator = RequirePositiveDenominator_(std::stoi(rText.substr(slash + 1)));
    return Rational_t{numerator, denominator};
}

Rational_t Rational_t::ParseJson(const nlohmann::json& rJson)
{
    if (rJson.is_number_integer())
    {
        return FromInt(rJson.get<int>());
    }
    if (rJson.is_number_unsigned())
    {
        return FromInt(static_cast<int>(rJson.get<unsigned>()));
    }
    if (rJson.is_string())
    {
        return Parse(rJson.get<std::string>());
    }
    throw std::runtime_error("Expected an integer or rational string (e.g. \"1/3\")");
}

int Rational_t::ScaledInt(int scale) const
{
    RequirePositiveDenominator_(denominator);

    const int gcd = std::gcd(std::abs(numerator), denominator);
    const int num = numerator / gcd;
    const int den = denominator / gcd;
    if ((scale % den) != 0)
    {
        throw std::runtime_error(
            "Rational " + std::to_string(numerator) + "/" + std::to_string(denominator)
            + " does not scale to an integer at scale=" + std::to_string(scale));
    }

    // Widened before multiplying: the divisibility check above says nothing about whether the
    // product fits, and signed overflow is UB rather than a detectable wrong answer.
    const int64_t scaled = static_cast<int64_t>(num) * static_cast<int64_t>(scale / den);
    if (scaled < std::numeric_limits<int>::min() || scaled > std::numeric_limits<int>::max())
    {
        throw std::runtime_error(
            "Rational " + std::to_string(numerator) + "/" + std::to_string(denominator)
            + " scaled at scale=" + std::to_string(scale) + " does not fit in an int");
    }
    return static_cast<int>(scaled);
}

} // namespace ac
