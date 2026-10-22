#pragma once

#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

// Loads config files holding an array of entries, parsing each with the caller's function.
namespace JsonConfigLoader
{

// Opens a single JSON file expected to contain a top-level array and returns each element
// parsed through parseEntry. `noun` appears only in log/error text (e.g. "building").
template <typename T, typename ParseFn>
std::vector<T> LoadFile(const std::string& filePath, const char* noun, ParseFn parseEntry)
{
    std::cout << "Loading " << noun << " configuration from: " << filePath << "\n";

    std::ifstream configFile(filePath);
    if (!configFile.is_open())
    {
        throw std::runtime_error("Could not open " + filePath);
    }

    nlohmann::json configJson;
    configFile >> configJson;

    if (!configJson.is_array())
    {
        throw std::runtime_error("Expected a JSON array of " + std::string(noun)
                                 + " entries in '" + filePath + "'");
    }

    std::vector<T> configs;
    for (const auto& rEntryJson : configJson)
    {
        configs.push_back(parseEntry(rEntryJson));
    }

    std::cout << "Loaded " << configs.size() << " " << noun << " configurations\n";
    return configs;
}

// Opens a single JSON file expected to contain a top-level object and returns it parsed through
// parseObject. For config types that are one settings block rather than an array of entries.
template <typename T, typename ParseFn>
T LoadObjectFile(const std::string& filePath, const char* noun, ParseFn parseObject)
{
    std::cout << "Loading " << noun << " configuration from: " << filePath << "\n";

    std::ifstream configFile(filePath);
    if (!configFile.is_open())
    {
        throw std::runtime_error("Could not open " + filePath);
    }

    nlohmann::json configJson;
    configFile >> configJson;

    if (!configJson.is_object())
    {
        throw std::runtime_error("Expected a JSON object of " + std::string(noun)
                                 + " settings in '" + filePath + "'");
    }

    return parseObject(configJson);
}

// LoadFile, or for a directory every *.json in it, sorted by name and concatenated.
template <typename T, typename ParseFn>
std::vector<T> LoadPath(const std::string& path, const char* noun, ParseFn parseEntry)
{
    namespace fs = std::filesystem;

    if (fs::is_directory(path))
    {
        std::vector<fs::path> files;
        for (const auto& rEntry : fs::directory_iterator(path))
        {
            if (rEntry.path().extension() == ".json")
                files.push_back(rEntry.path());
        }
        std::sort(files.begin(), files.end());

        std::vector<T> all;
        for (const auto& rFile : files)
        {
            auto configs = LoadFile<T>(rFile.string(), noun, parseEntry);
            all.insert(all.end(), configs.begin(), configs.end());
        }
        return all;
    }

    return LoadFile<T>(path, noun, parseEntry);
}

} // namespace JsonConfigLoader
} // namespace ac
