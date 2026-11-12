#pragma once

#include "lib/Formula.h"
#include "lib/Rational.h"
#include "lib/config/EnumNames.h"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ac
{

// One JSON object of a config file. Every error names the context (the file or entry) and the
// key's path from there: "<context>: '<path.key>' <message>".
class ConfigReader
{
public:
    ConfigReader(const nlohmann::json& rJson, std::string context);

    // A number, a fraction string ("1/3") or a decimal string ("0.5"). Otherwise throws a
    // message starting "must be", for a caller to prefix with what it was reading.
    static double NumberFromJson(const nlohmann::json& rValue);

    const nlohmann::json& Json() const;
    const std::string& Context() const;
    bool Has(const std::string& rKey) const;

    [[noreturn]] void Fail(const std::string& rMessage) const;
    [[noreturn]] void FailKey(const std::string& rKey, const std::string& rMessage) const;

    void RejectUnknownKeys(const std::vector<std::string>& rKnown) const;

    const nlohmann::json& Require(const std::string& rKey) const;
    ConfigReader Object(const std::string& rKey) const;
    const nlohmann::json& Array(const std::string& rKey) const;
    const nlohmann::json& NonEmptyArray(const std::string& rKey) const;
    // Each entry of the array at rKey, which must be an object. Errors name it "key[<id>]", or
    // "key[<index>]" when it has no string id.
    std::vector<ConfigReader> Objects(const std::string& rKey) const;

    int Int(const std::string& rKey) const;
    int IntAtLeast(const std::string& rKey, int minimum) const;
    int IntInRange(const std::string& rKey, int minimum, int maximum) const;
    int IntOr(const std::string& rKey, int fallback) const;

    double Number(const std::string& rKey) const;
    double NumberAtLeast(const std::string& rKey, double minimum) const;
    double NumberAbove(const std::string& rKey, double bound) const;
    double NumberInRange(const std::string& rKey, double minimum, double maximum) const;
    double NumberOr(const std::string& rKey, double fallback) const;
    float Float(const std::string& rKey) const;
    float FloatOr(const std::string& rKey, float fallback) const;
    Rational_t Rational(const std::string& rKey) const;

    bool Bool(const std::string& rKey) const;
    bool BoolOr(const std::string& rKey, bool fallback) const;

    std::string String(const std::string& rKey) const;
    std::string NonEmptyString(const std::string& rKey) const;
    std::string StringOr(const std::string& rKey, const std::string& rFallback) const;
    // Absent is an empty list.
    std::vector<std::string> StringArray(const std::string& rKey) const;
    // A Lua expression reading only rVariables, checked by LuaRuntime::CheckFormula. It can
    // use Lua's libraries but nothing a config script defines.
    Formula_t Formula(const std::string& rKey, std::vector<std::string> variables) const;

    // Case-insensitive by enumerator name; rWhat names the enum in the error.
    template <typename Enum_t>
    Enum_t Enum(const std::string& rKey, std::string_view what) const
    {
        const std::string name = String(rKey);
        try
        {
            return EnumFromName<Enum_t>(name, what);
        }
        catch (const std::runtime_error&)
        {
            FailKey(rKey, "is not a known " + std::string(what) + ": '" + name + "'");
        }
    }

private:
    ConfigReader(const nlohmann::json& rJson, std::string context, std::string keyPrefix);

    std::string Quoted_(const std::string& rKey) const;

    const nlohmann::json& m_rJson;
    std::string m_context;
    std::string m_keyPrefix;
};

} // namespace ac
