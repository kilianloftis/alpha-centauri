#pragma once

#include "game/production/ScrapConfig.h"
#include "lib/config/ConfigReader.h"


namespace ac
{

// The kind default_scrap and the partial override.
class ScrapConfigParser
{
public:
    // formula, refund_type, and refund_ceiling_percent are all required.
    static ScrapKindConfig_t ParseKindConfig(const ConfigReader& rReader);

    // formula or refund_type, at least one. `"formula": null` denies scrap and stands alone.
    static ScrapOverride_t ParseOverride(const ConfigReader& rReader);
};

} // namespace ac
