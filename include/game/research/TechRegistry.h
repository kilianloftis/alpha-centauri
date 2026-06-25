#pragma once

#include "lib/Registry.h"
#include "game/research/TechConfigParser.h"

namespace ac
{

class TechRegistry : public Registry<TechConfig_t, TechConfigParser>
{
public:
    TechRegistry() = default;
    ~TechRegistry() = default;

    // Load all techs from a config file. Throws on failure.
    void Load(const std::string& configPath);

private:
    // Validation functions for tech configuration
    void ValidatePrerequisites_(const TechConfig_t& config, const std::vector<TechConfig_t>& configs);
    void ValidateUniqueIds_(const TechConfig_t& config, const std::vector<TechConfig_t>& configs);
};

} // namespace ac
