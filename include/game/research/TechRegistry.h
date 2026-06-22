#pragma once

#include "lib/Registry.h"
#include "game/research/TechConfigParser.h"
#include "game/research/Tech.h"
#include <map>
#include <memory>

namespace ac
{

using TechRegistryBase = Registry<TechConfig, TechConfigParser, Tech>;

class TechRegistry : public TechRegistryBase
{
public:
    TechRegistry() = default;
    ~TechRegistry() = default;

    // Load all techs from a config file. Throws on failure.
    void Load(const std::string& configPath);

private:
    // Validation functions for tech configuration
    void ValidatePrerequisites_(const TechConfig& config, const std::vector<TechConfig>& configs);
    void ValidateUniqueIds_(const TechConfig& config, const std::vector<TechConfig>& configs);
};

} // namespace ac
