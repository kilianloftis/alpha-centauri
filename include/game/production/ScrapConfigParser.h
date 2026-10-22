#pragma once

#include "game/production/ScrapConfig.h"

#include <nlohmann/json.hpp>
#include <string>

namespace ac
{

// The kind default_scrap and the partial override; failures throw prefixed with rPath.
class ScrapConfigParser
{
public:
    // formula, refund_type, and refund_ceiling_percent are all required.
    static ScrapKindConfig_t ParseKindConfig(const nlohmann::json& rValue,
                                             const std::string& rPath);

    // formula or refund_type, at least one. `"formula": null` denies scrap and stands alone.
    static ScrapOverride_t ParseOverride(const nlohmann::json& rValue, const std::string& rPath);
};

} // namespace ac
