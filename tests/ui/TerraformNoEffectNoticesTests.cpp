#include "ui/TerraformNoEffectNotices.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

TEST_CASE("Terraform no-effect notice names the project", "[terraform][notice][ui]")
{
    const TerraformNoEffectNoticeCopy_t copy = TerraformNoEffectNoticeCopy("Raise Land");
    CHECK(copy.title == "Terraforming Failed");
    CHECK(copy.body
          == "Your former could not complete Raise Land — the terrain will no longer accept it.");
}
