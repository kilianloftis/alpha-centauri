#include "ui/TerraformNoEffectNotices.h"

namespace ac
{

TerraformNoEffectNoticeCopy_t TerraformNoEffectNoticeCopy(const std::string& rProjectName)
{
    TerraformNoEffectNoticeCopy_t copy;
    copy.title = "Terraforming Failed";
    copy.body = "Your former could not complete " + rProjectName
                + " — the terrain will no longer accept it.";
    return copy;
}

} // namespace ac
