#pragma once

#include "game/faction/base/production/ScrapConfig.h"

#include <nlohmann/json.hpp>
#include <string>

namespace ac
{

// Reads the two scrap JSON shapes: the full kind default in production.json
// kinds.<kind>.default_scrap, and the partial override a building or unit component may
// carry under "scrap". Every failure throws with rPath as the prefix, so the caller does not
// have to restate where in the file the problem is.
class ScrapConfigParser
{
public:
    // formula, refund_type, and refund_ceiling_percent are all required.
    static ScrapKindConfig_t ParseKindConfig(const nlohmann::json& rValue,
                                             const std::string& rPath);

    // formula and refund_type are both optional, but at least one must be present — an empty
    // override is a config mistake, not a no-op.
    static ScrapOverride_t ParseOverride(const nlohmann::json& rValue, const std::string& rPath);
};

} // namespace ac
