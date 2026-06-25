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

protected:
    void Validate_() override;

private:
    void ValidatePrerequisites_(const TechConfig_t& rConfig, const std::vector<TechConfig_t>& rConfigs);
};

} // namespace ac
