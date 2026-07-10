// Tests for SocialEngineeringManager's config-integrity checks (code-review-findings.md
// 1.11): the constructor's hardcoded default policy ids must exist and be in their expected
// category, and SetActivePolicy must reject a policy from the wrong category slot.

#include "game/faction/SocialEngineeringManager.h"
#include "game/social-engineering/SocialPolicyRegistry.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>

using namespace ac;

namespace
{

std::filesystem::path WriteTempJson_(const std::string& name, const std::string& body)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path);
    out << body;
    return path;
}

const char* k_ValidDefaults = R"([
    { "id": "frontier",     "name": "Frontier",     "category": "politics",       "effects": [] },
    { "id": "simple",       "name": "Simple",       "category": "economics",      "effects": [] },
    { "id": "survival",     "name": "Survival",     "category": "values",         "effects": [] },
    { "id": "none_future",  "name": "None",         "category": "future_society", "effects": [] },
    { "id": "police_state", "name": "Police State", "category": "politics",       "effects": [] }
])";

} // namespace

TEST_CASE("SocialEngineeringManager: constructs when all default policies exist in their category",
          "[social-engineering][validation]")
{
    const std::filesystem::path path = WriteTempJson_("ac_sem_valid.json", k_ValidDefaults);
    SocialPolicyRegistry registry;
    registry.Load(path.string());

    CHECK_NOTHROW(SocialEngineeringManager(&registry, nullptr));
    std::filesystem::remove(path);
}

TEST_CASE("SocialEngineeringManager: throws when a default policy id is missing from config",
          "[social-engineering][validation]")
{
    const std::filesystem::path path = WriteTempJson_("ac_sem_missing.json", R"([
        { "id": "frontier",    "name": "Frontier",    "category": "politics",       "effects": [] },
        { "id": "simple",      "name": "Simple",      "category": "economics",      "effects": [] },
        { "id": "survival",    "name": "Survival",    "category": "values",         "effects": [] }
    ])");
    SocialPolicyRegistry registry;
    registry.Load(path.string());

    CHECK_THROWS_WITH(SocialEngineeringManager(&registry, nullptr),
                      Catch::Matchers::ContainsSubstring("none_future"));
    std::filesystem::remove(path);
}

TEST_CASE("SocialEngineeringManager: throws when a default policy id is in the wrong category",
          "[social-engineering][validation]")
{
    const std::filesystem::path path = WriteTempJson_("ac_sem_miscategorized.json", R"([
        { "id": "frontier",    "name": "Frontier",    "category": "economics",      "effects": [] },
        { "id": "simple",      "name": "Simple",      "category": "economics",      "effects": [] },
        { "id": "survival",    "name": "Survival",    "category": "values",         "effects": [] },
        { "id": "none_future", "name": "None",        "category": "future_society", "effects": [] }
    ])");
    SocialPolicyRegistry registry;
    registry.Load(path.string());

    CHECK_THROWS_WITH(SocialEngineeringManager(&registry, nullptr),
                      Catch::Matchers::ContainsSubstring("frontier"));
    std::filesystem::remove(path);
}

TEST_CASE("SocialEngineeringManager: a null registry skips default-policy validation",
          "[social-engineering][validation]")
{
    CHECK_NOTHROW(SocialEngineeringManager(nullptr, nullptr));
}

TEST_CASE("SocialEngineeringManager: SetActivePolicy rejects a policy from another category",
          "[social-engineering][validation]")
{
    const std::filesystem::path path = WriteTempJson_("ac_sem_set_active.json", k_ValidDefaults);
    SocialPolicyRegistry registry;
    registry.Load(path.string());

    SocialEngineeringManager manager(&registry, nullptr);

    // "police_state" is Politics; assigning it into the Economics slot must be rejected.
    CHECK_THROWS_WITH(manager.SetActivePolicy(SocialCategory_t::Economics, "police_state"),
                      Catch::Matchers::ContainsSubstring("police_state"));

    // Same policy into its actual category succeeds.
    CHECK_NOTHROW(manager.SetActivePolicy(SocialCategory_t::Politics, "police_state"));

    std::filesystem::remove(path);
}
