#pragma once

#include "game/map/TerrainConfig.h"
#include "lib/Registry.h"

namespace ac
{

// Former projects from terraform_projects.json.
class TerraformProjectRegistry
    : public Registry<TerraformProjectConfig_t, TerraformProjectConfigParser>
{
};

} // namespace ac
