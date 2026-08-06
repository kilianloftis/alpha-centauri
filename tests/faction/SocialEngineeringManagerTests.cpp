// Tests for SocialEngineeringManager's config-integrity checks (code-review-findings.md
// 1.11): the constructor's hardcoded default policy ids must exist and be in their expected
// category, and SetActivePolicy must reject a policy from the wrong category slot.

#include "game/faction/SocialEngineeringManager.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/effects/EffectEnums.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>

using namespace ac;

namespace
{

std::filesystem::path WriteTempJson(const std::string& name, const std::string& body)
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
    const std::filesystem::path path = WriteTempJson("ac_sem_valid.json", k_ValidDefaults);
    SocialPolicyRegistry registry;
    registry.Load(path.string());

    CHECK_NOTHROW(SocialEngineeringManager(registry));
    std::filesystem::remove(path);
}

TEST_CASE("SocialEngineeringManager: throws when a default policy id is missing from config",
          "[social-engineering][validation]")
{
    const std::filesystem::path path = WriteTempJson("ac_sem_missing.json", R"([
        { "id": "frontier",    "name": "Frontier",    "category": "politics",       "effects": [] },
        { "id": "simple",      "name": "Simple",      "category": "economics",      "effects": [] },
        { "id": "survival",    "name": "Survival",    "category": "values",         "effects": [] }
    ])");
    SocialPolicyRegistry registry;
    registry.Load(path.string());

    CHECK_THROWS_WITH(SocialEngineeringManager(registry),
                      Catch::Matchers::ContainsSubstring("none_future"));
    std::filesystem::remove(path);
}

TEST_CASE("SocialEngineeringManager: throws when a default policy id is in the wrong category",
          "[social-engineering][validation]")
{
    const std::filesystem::path path = WriteTempJson("ac_sem_miscategorized.json", R"([
        { "id": "frontier",    "name": "Frontier",    "category": "economics",      "effects": [] },
        { "id": "simple",      "name": "Simple",      "category": "economics",      "effects": [] },
        { "id": "survival",    "name": "Survival",    "category": "values",         "effects": [] },
        { "id": "none_future", "name": "None",        "category": "future_society", "effects": [] }
    ])");
    SocialPolicyRegistry registry;
    registry.Load(path.string());

    CHECK_THROWS_WITH(SocialEngineeringManager(registry),
                      Catch::Matchers::ContainsSubstring("frontier"));
    std::filesystem::remove(path);
}

// (Removed: "a null registry skips default-policy validation". The registry is a constructor
// reference now, so skipping validation is unrepresentable — that test pinned the escape hatch
// that let a faction be built with no policies at all.)

TEST_CASE("SocialEngineeringManager: SetActivePolicy stores the policy under its own category",
          "[social-engineering][validation]")
{
    const std::filesystem::path path = WriteTempJson("ac_sem_set_active.json", k_ValidDefaults);
    SocialPolicyRegistry registry;
    registry.Load(path.string());

    SocialEngineeringManager manager(registry);

    const SocialPolicyConfig_t& rPoliceState = registry.Get("police_state");
    CHECK_NOTHROW(manager.SetActivePolicy(rPoliceState));
    CHECK(manager.GetActivePolicy(SocialCategory_t::Politics) == &rPoliceState);
    // Economics slot is unchanged (still the default "simple").
    CHECK(manager.GetActivePolicy(SocialCategory_t::Economics)->id == "simple");

    CHECK_THROWS_WITH(manager.SetActivePolicy(SocialPolicyConfig_t{
                          .id = "not_in_registry",
                          .name = "Missing",
                          .category = SocialCategory_t::Politics}),
                      Catch::Matchers::ContainsSubstring("not_in_registry"));

    std::filesystem::remove(path);
}

TEST_CASE("SocialEngineeringManager: GetSocialRating sums active policy modifiers only",
          "[social-engineering][rating]")
{
    const std::filesystem::path path = WriteTempJson("ac_sem_rating.json", R"([
        { "id": "frontier", "name": "Frontier", "category": "politics", "effects": [
            { "type": "SocialRatingModifier", "scope": "FactionGlobal",
              "parameters": { "rating": "growth", "amount": 2 } }
        ]},
        { "id": "simple", "name": "Simple", "category": "economics", "effects": [
            { "type": "SocialRatingModifier", "scope": "FactionGlobal",
              "parameters": { "rating": "growth", "amount": 1 } }
        ]},
        { "id": "survival", "name": "Survival", "category": "values", "effects": [] },
        { "id": "none_future", "name": "None", "category": "future_society", "effects": [] }
    ])");
    SocialPolicyRegistry registry;
    registry.Load(path.string());

    SocialEngineeringManager manager(registry);

    CHECK(manager.GetSocialRating(SocialRatingId_t::Growth) == 3);
    CHECK(manager.GetSocialRating(SocialRatingId_t::Police) == 0);

    std::filesystem::remove(path);
}
