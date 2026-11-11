#include "lib/config/JsonConfigLoader.h"

#include <fstream>

namespace ac
{
namespace JsonConfigLoader
{

nlohmann::json ReadFile(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open " + filePath);
    }
    try
    {
        return nlohmann::json::parse(file);
    }
    catch (const nlohmann::json::parse_error& rError)
    {
        throw std::runtime_error("Could not parse " + filePath + ": " + rError.what());
    }
}

std::string EntryContext(const char* noun, const std::string& filePath,
                         const nlohmann::json& rEntry, std::size_t index)
{
    const std::string file = "'" + filePath + "'";
    if (rEntry.is_object() && rEntry.contains("id") && rEntry.at("id").is_string())
    {
        return std::string(noun) + " '" + rEntry.at("id").get<std::string>() + "' in " + file;
    }
    return std::string(noun) + " entry " + std::to_string(index) + " in " + file;
}

} // namespace JsonConfigLoader
} // namespace ac
