#include "lib/Rational.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>

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

// The whole of rPart, so "0.5" or "1/3x" is an error and not a truncated value.
int ParseWholeInt_(const std::string& rPart, const std::string& rText)
{
    int value = 0;
    const char* pEnd = rPart.data() + rPart.size();
    const auto [pParsed, error] = std::from_chars(rPart.data(), pEnd, value);
    if (rPart.empty() || error != std::errc() || pParsed != pEnd)
    {
        throw std::runtime_error("Expected an integer or a fraction like \"1/3\", got '" + rText
                                 + "'");
    }
    return value;
}

int RequireFitsInt_(int64_t scaled, const Rational_t& rRational, int scale)
{
    if (scaled < std::numeric_limits<int>::min() || scaled > std::numeric_limits<int>::max())
    {
        throw std::runtime_error(
            "Rational " + std::to_string(rRational.numerator) + "/"
            + std::to_string(rRational.denominator) + " scaled at scale=" + std::to_string(scale)
            + " does not fit in an int");
    }
    return static_cast<int>(scaled);
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
        return FromInt(ParseWholeInt_(rText, rText));
    }

    const int numerator = ParseWholeInt_(rText.substr(0, slash), rText);
    const int rawDenominator = ParseWholeInt_(rText.substr(slash + 1), rText);
    const int denominator = RequirePositiveDenominator_(rawDenominator);
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
        const int value = static_cast<int>(rJson.get<unsigned>());
        return FromInt(value);
    }
    if (rJson.is_number_float())
    {
        const double value = rJson.get<double>();
        if (value != std::trunc(value) || value < static_cast<double>(std::numeric_limits<int>::min())
            || value > static_cast<double>(std::numeric_limits<int>::max()))
        {
            throw std::runtime_error(
                "Expected an integer, whole-number float, or rational string (e.g. \"1/3\")");
        }
        return FromInt(static_cast<int>(value));
    }
    if (rJson.is_string())
    {
        return Parse(rJson.get<std::string>());
    }
    throw std::runtime_error(
        "Expected an integer, whole-number float, or rational string (e.g. \"1/3\")");
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
    return RequireFitsInt_(scaled, *this, scale);
}

int Rational_t::ScaledTruncated(int scale) const
{
    RequirePositiveDenominator_(denominator);
    const int64_t product = static_cast<int64_t>(numerator) * scale;
    return RequireFitsInt_(product / denominator, *this, scale);
}

int Rational_t::ScaledRoundedUp(int scale) const
{
    RequirePositiveDenominator_(denominator);
    const int64_t product = static_cast<int64_t>(numerator) * scale;
    const int64_t truncated = product / denominator;
    // Truncation already rounds a negative quotient up.
    const bool bRaise = product > 0 && product % denominator != 0;
    return RequireFitsInt_(bRaise ? truncated + 1 : truncated, *this, scale);
}

} // namespace ac
