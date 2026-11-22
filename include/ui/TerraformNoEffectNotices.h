#pragma once

#include <string>

namespace ac
{

struct TerraformNoEffectNoticeCopy_t
{
    std::string title;
    std::string body;
};

// projectName is the config display name when known; otherwise the project id.
TerraformNoEffectNoticeCopy_t TerraformNoEffectNoticeCopy(const std::string& rProjectName);

} // namespace ac
