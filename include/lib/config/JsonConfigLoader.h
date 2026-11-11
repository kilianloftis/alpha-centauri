#pragma once

#include "lib/config/ConfigReader.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

// Loads config files, handing the caller's parse function a ConfigReader that names the file.
namespace JsonConfigLoader
{

// The parsed contents of one JSON file. Throws naming the file if it cannot be opened or parsed.
nlohmann::json ReadFile(const std::string& filePath);

// "<noun> '<id>' in '<path>'" for an entry with a string id, else its index in the file.
std::string EntryContext(const char* noun, const std::string& filePath,
                         const nlohmann::json& rEntry, std::size_t index);

// Opens a single JSON file expected to contain a top-level array of objects and returns each
// element parsed through parseEntry(const ConfigReader&). `noun` names the entries in log and
// error text (e.g. "building").
template <typename T, typename ParseFn>
std::vector<T> LoadFile(const std::string& filePath, const char* noun, ParseFn parseEntry)
{
    std::cout << "Loading " << noun << " configuration from: " << filePath << "\n";

    const nlohmann::json configJson = ReadFile(filePath);
    if (!configJson.is_array())
    {
        throw std::runtime_error("Expected a JSON array of " + std::string(noun)
                                 + " entries in '" + filePath + "'");
    }

    std::vector<T> configs;
    for (std::size_t i = 0; i < configJson.size(); ++i)
    {
        const nlohmann::json& rEntryJson = configJson.at(i);
        const std::string context = EntryContext(noun, filePath, rEntryJson, i);
        if (!rEntryJson.is_object())
        {
            throw std::runtime_error(context + " must be an object");
        }
        configs.push_back(parseEntry(ConfigReader(rEntryJson, context)));
    }

    std::cout << "Loaded " << configs.size() << " " << noun << " configurations\n";
    return configs;
}

// Opens a single JSON file expected to contain a top-level object and returns it parsed through
// parseObject(const ConfigReader&). For config types that are one settings block rather than an
// array of entries.
template <typename T, typename ParseFn>
T LoadObjectFile(const std::string& filePath, const char* noun, ParseFn parseObject)
{
    std::cout << "Loading " << noun << " configuration from: " << filePath << "\n";

    const nlohmann::json configJson = ReadFile(filePath);
    const std::string context = std::string(noun) + " config '" + filePath + "'";
    if (!configJson.is_object())
    {
        throw std::runtime_error(context + " must be a JSON object");
    }
    return parseObject(ConfigReader(configJson, context));
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
