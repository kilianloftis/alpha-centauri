#include "lib/config/ConfigReader.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <sstream>
#include <utility>

namespace ac
{

namespace
{

constexpr const char* k_NumberExpected = "must be a number or a fraction like \"1/3\"";

std::string FormatNumber_(double value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

double ParseDecimal_(const std::string& rText)
{
    double value = 0.0;
    const char* pEnd = rText.data() + rText.size();
    const auto [pParsed, error] = std::from_chars(rText.data(), pEnd, value);
    if (error != std::errc() || pParsed != pEnd || !std::isfinite(value))
    {
        throw std::runtime_error(std::string(k_NumberExpected) + ", got '" + rText + "'");
    }
    return value;
}

double ParseFraction_(const std::string& rText)
{
    try
    {
        const Rational_t fraction = Rational_t::Parse(rText);
        return static_cast<double>(fraction.numerator) / static_cast<double>(fraction.denominator);
    }
    catch (const std::runtime_error&)
    {
        throw std::runtime_error(std::string(k_NumberExpected) + ", got '" + rText + "'");
    }
}

// An entry's string id names it better than its position.
std::string EntryName_(const nlohmann::json& rEntry, std::size_t index)
{
    if (rEntry.is_object() && rEntry.contains("id") && rEntry.at("id").is_string())
    {
        return rEntry.at("id").get<std::string>();
    }
    return std::to_string(index);
}

} // namespace

ConfigReader::ConfigReader(const nlohmann::json& rJson, std::string context)
    : ConfigReader(rJson, std::move(context), std::string())
{
}

ConfigReader::ConfigReader(const nlohmann::json& rJson, std::string context,
                           std::string keyPrefix)
    : m_rJson(rJson)
    , m_context(std::move(context))
    , m_keyPrefix(std::move(keyPrefix))
{
}

double ConfigReader::NumberFromJson(const nlohmann::json& rValue)
{
    if (rValue.is_number())
    {
        return rValue.get<double>();
    }
    if (!rValue.is_string())
    {
        throw std::runtime_error(k_NumberExpected);
    }
    const std::string& rText = rValue.get_ref<const std::string&>();
    if (rText.find('/') != std::string::npos)
    {
        return ParseFraction_(rText);
    }
    return ParseDecimal_(rText);
}

const nlohmann::json& ConfigReader::Json() const
{
    return m_rJson;
}

const std::string& ConfigReader::Context() const
{
    return m_context;
}

bool ConfigReader::Has(const std::string& rKey) const
{
    return m_rJson.contains(rKey);
}

void ConfigReader::Fail(const std::string& rMessage) const
{
    if (m_keyPrefix.empty())
    {
        throw std::runtime_error(m_context + ": " + rMessage);
    }
    const std::string where = m_keyPrefix.substr(0, m_keyPrefix.size() - 1);
    throw std::runtime_error(m_context + " at '" + where + "': " + rMessage);
}

void ConfigReader::FailKey(const std::string& rKey, const std::string& rMessage) const
{
    throw std::runtime_error(m_context + ": " + Quoted_(rKey) + " " + rMessage);
}

void ConfigReader::RejectUnknownKeys(const std::vector<std::string>& rKnown) const
{
    for (const auto& [rKey, rUnused] : m_rJson.items())
    {
        if (std::find(rKnown.begin(), rKnown.end(), rKey) == rKnown.end())
        {
            throw std::runtime_error(m_context + ": unknown key " + Quoted_(rKey));
        }
    }
}

const nlohmann::json& ConfigReader::Require(const std::string& rKey) const
{
    if (!Has(rKey))
    {
        FailKey(rKey, "is required");
    }
    return m_rJson.at(rKey);
}

ConfigReader ConfigReader::Object(const std::string& rKey) const
{
    const nlohmann::json& rValue = Require(rKey);
    if (!rValue.is_object())
    {
        FailKey(rKey, "must be an object");
    }
    return ConfigReader(rValue, m_context, m_keyPrefix + rKey + ".");
}

const nlohmann::json& ConfigReader::Array(const std::string& rKey) const
{
    const nlohmann::json& rValue = Require(rKey);
    if (!rValue.is_array())
    {
        FailKey(rKey, "must be an array");
    }
    return rValue;
}

const nlohmann::json& ConfigReader::NonEmptyArray(const std::string& rKey) const
{
    const nlohmann::json& rValue = Array(rKey);
    if (rValue.empty())
    {
        FailKey(rKey, "must be a non-empty array");
    }
    return rValue;
}

std::vector<ConfigReader> ConfigReader::Objects(const std::string& rKey) const
{
    const nlohmann::json& rArray = Array(rKey);
    std::vector<ConfigReader> entries;
    entries.reserve(rArray.size());
    for (std::size_t i = 0; i < rArray.size(); ++i)
    {
        const std::string entryKey = rKey + "[" + EntryName_(rArray.at(i), i) + "]";
        if (!rArray.at(i).is_object())
        {
            FailKey(entryKey, "must be an object");
        }
        entries.push_back(ConfigReader(rArray.at(i), m_context, m_keyPrefix + entryKey + "."));
    }
    return entries;
}

int ConfigReader::Int(const std::string& rKey) const
{
    const nlohmann::json& rValue = Require(rKey);
    if (!rValue.is_number_integer())
    {
        FailKey(rKey, "must be an integer");
    }
    return rValue.get<int>();
}

int ConfigReader::IntAtLeast(const std::string& rKey, int minimum) const
{
    const int value = Int(rKey);
    if (value < minimum)
    {
        FailKey(rKey, "must be >= " + std::to_string(minimum) + ", got " + std::to_string(value));
    }
    return value;
}

int ConfigReader::IntInRange(const std::string& rKey, int minimum, int maximum) const
{
    const int value = Int(rKey);
    if (value < minimum || value > maximum)
    {
        FailKey(rKey, "must be in " + std::to_string(minimum) + ".." + std::to_string(maximum)
                          + ", got " + std::to_string(value));
    }
    return value;
}

int ConfigReader::IntOr(const std::string& rKey, int fallback) const
{
    return Has(rKey) ? Int(rKey) : fallback;
}

double ConfigReader::Number(const std::string& rKey) const
{
    const nlohmann::json& rValue = Require(rKey);
    try
    {
        return NumberFromJson(rValue);
    }
    catch (const std::runtime_error& rError)
    {
        FailKey(rKey, rError.what());
    }
}

double ConfigReader::NumberAtLeast(const std::string& rKey, double minimum) const
{
    const double value = Number(rKey);
    if (value < minimum)
    {
        FailKey(rKey, "must be >= " + FormatNumber_(minimum) + ", got " + FormatNumber_(value));
    }
    return value;
}

double ConfigReader::NumberAbove(const std::string& rKey, double bound) const
{
    const double value = Number(rKey);
    if (value <= bound)
    {
        FailKey(rKey, "must be > " + FormatNumber_(bound) + ", got " + FormatNumber_(value));
    }
    return value;
}

double ConfigReader::NumberInRange(const std::string& rKey, double minimum, double maximum) const
{
    const double value = Number(rKey);
    if (value < minimum || value > maximum)
    {
        FailKey(rKey, "must be in [" + FormatNumber_(minimum) + ", " + FormatNumber_(maximum)
                          + "], got " + FormatNumber_(value));
    }
    return value;
}

double ConfigReader::NumberOr(const std::string& rKey, double fallback) const
{
    return Has(rKey) ? Number(rKey) : fallback;
}

float ConfigReader::Float(const std::string& rKey) const
{
    return static_cast<float>(Number(rKey));
}

float ConfigReader::FloatOr(const std::string& rKey, float fallback) const
{
    return Has(rKey) ? Float(rKey) : fallback;
}

Rational_t ConfigReader::Rational(const std::string& rKey) const
{
    const nlohmann::json& rValue = Require(rKey);
    try
    {
        return Rational_t::ParseJson(rValue);
    }
    catch (const std::runtime_error&)
    {
        FailKey(rKey, "must be an integer or a fraction like \"1/3\"");
    }
}

bool ConfigReader::Bool(const std::string& rKey) const
{
    const nlohmann::json& rValue = Require(rKey);
    if (!rValue.is_boolean())
    {
        FailKey(rKey, "must be a boolean");
    }
    return rValue.get<bool>();
}

bool ConfigReader::BoolOr(const std::string& rKey, bool fallback) const
{
    return Has(rKey) ? Bool(rKey) : fallback;
}

std::string ConfigReader::String(const std::string& rKey) const
{
    const nlohmann::json& rValue = Require(rKey);
    if (!rValue.is_string())
    {
        FailKey(rKey, "must be a string");
    }
    return rValue.get<std::string>();
}

std::string ConfigReader::NonEmptyString(const std::string& rKey) const
{
    std::string value = String(rKey);
    if (value.empty())
    {
        FailKey(rKey, "must be a non-empty string");
    }
    return value;
}

std::string ConfigReader::StringOr(const std::string& rKey, const std::string& rFallback) const
{
    return Has(rKey) ? String(rKey) : rFallback;
}

std::vector<std::string> ConfigReader::StringArray(const std::string& rKey) const
{
    if (!Has(rKey))
    {
        return {};
    }
    std::vector<std::string> values;
    for (const nlohmann::json& rEntry : Array(rKey))
    {
        if (!rEntry.is_string())
        {
            FailKey(rKey, "must contain only strings");
        }
        values.push_back(rEntry.get<std::string>());
    }
    return values;
}

std::string ConfigReader::Quoted_(const std::string& rKey) const
{
    return "'" + m_keyPrefix + rKey + "'";
}

} // namespace ac
