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

// Shared file-loading boilerplate for the JSON config parsers. Every config type
// (buildings, improvements, techs, ...) stores an array of entries in one or more JSON
// files and parses them the same way: open, validate that the top level is an array, then
// map each element through a per-entry parse function. These templates capture that shape
// so each parser only has to supply the element type and its own ParseX function.
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

// Like LoadFile, but if `path` is a directory it loads every *.json file inside it (sorted
// by name for deterministic ordering) and concatenates the results. Otherwise `path` is
// treated as a single file.
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
