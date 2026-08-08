// SocialEngineeringManager's config-integrity checks: every category must declare exactly one
// starting policy (`"default": true`) and SetActivePolicy must reject a policy that is not in
// the registry. The starting ids used to be compiled in, so a mod shipping its own policy set
// hard-failed every faction constructor.

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
    { "id": "frontier",     "name": "Frontier",     "category": "politics",       "default": true, "effects": [] },
    { "id": "simple",       "name": "Simple",       "category": "economics",      "default": true, "effects": [] },
    { "id": "survival",     "name": "Survival",     "category": "values",         "default": true, "effects": [] },
    { "id": "none_future",  "name": "None",         "category": "future_society", "default": true, "effects": [] },
    { "id": "police_state", "name": "Police State", "category": "politics",       "effects": [] }
])";

} // namespace

TEST_CASE("SocialEngineeringManager: starts on the policies config marks as default",
          "[social-engineering][validation]")
{
    const std::filesystem::path path = WriteTempJson("ac_sem_valid.json", k_ValidDefaults);
    SocialPolicyRegistry registry;
    registry.Load(path.string());

    SocialEngineeringManager manager(registry);
    CHECK(manager.GetActivePolicy(SocialCategory_t::Politics)->id == "frontier");
    CHECK(manager.GetActivePolicy(SocialCategory_t::Economics)->id == "simple");
    CHECK(manager.GetActivePolicy(SocialCategory_t::Values)->id == "survival");
    CHECK(manager.GetActivePolicy(SocialCategory_t::FutureSociety)->id == "none_future");
    std::filesystem::remove(path);
}

TEST_CASE("SocialEngineeringManager: a mod's own ids work as long as each category has a default",
          "[social-engineering][validation]")
{
    // The point of the config flag: nothing here shares an id with the shipped policy set.
    const std::filesystem::path path = WriteTempJson("ac_sem_modded.json", R"([
        { "id": "clan_rule",  "name": "Clan Rule",  "category": "politics",       "default": true, "effects": [] },
        { "id": "barter",     "name": "Barter",     "category": "economics",      "default": true, "effects": [] },
        { "id": "ancestral",  "name": "Ancestral",  "category": "values",         "default": true, "effects": [] },
        { "id": "no_future",  "name": "No Future",  "category": "future_society", "default": true, "effects": [] }
    ])");
    SocialPolicyRegistry registry;
    registry.Load(path.string());

    SocialEngineeringManager manager(registry);
    CHECK(manager.GetActivePolicy(SocialCategory_t::Politics)->id == "clan_rule");
    std::filesystem::remove(path);
}

TEST_CASE("SocialEngineeringManager: throws naming the category that declares no default",
          "[social-engineering][validation]")
{
    const std::filesystem::path path = WriteTempJson("ac_sem_missing.json", R"([
        { "id": "frontier",    "name": "Frontier",    "category": "politics",       "default": true, "effects": [] },
        { "id": "simple",      "name": "Simple",      "category": "economics",      "default": true, "effects": [] },
        { "id": "survival",    "name": "Survival",    "category": "values",         "default": true, "effects": [] },
        { "id": "none_future", "name": "None",        "category": "future_society", "effects": [] }
    ])");
    SocialPolicyRegistry registry;
    registry.Load(path.string());

    CHECK_THROWS_WITH(SocialEngineeringManager(registry),
                      Catch::Matchers::ContainsSubstring("FutureSociety"));
    std::filesystem::remove(path);
}

TEST_CASE("SocialEngineeringManager: throws when a category declares two defaults",
          "[social-engineering][validation]")
{
    // Ambiguity is a config error, not a coin flip over which policy a faction starts on.
    const std::filesystem::path path = WriteTempJson("ac_sem_two_defaults.json", R"([
        { "id": "frontier",     "name": "Frontier",     "category": "politics",       "default": true, "effects": [] },
        { "id": "police_state", "name": "Police State", "category": "politics",       "default": true, "effects": [] },
        { "id": "simple",       "name": "Simple",       "category": "economics",      "default": true, "effects": [] },
        { "id": "survival",     "name": "Survival",     "category": "values",         "default": true, "effects": [] },
        { "id": "none_future",  "name": "None",         "category": "future_society", "default": true, "effects": [] }
    ])");
    SocialPolicyRegistry registry;
    registry.Load(path.string());

    CHECK_THROWS_WITH(SocialEngineeringManager(registry),
                      Catch::Matchers::ContainsSubstring("Politics"));
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
        { "id": "frontier", "name": "Frontier", "category": "politics", "default": true, "effects": [
            { "type": "SocialRatingModifier", "scope": "FactionGlobal",
              "parameters": { "rating": "growth", "amount": 2 } }
        ]},
        { "id": "simple", "name": "Simple", "category": "economics", "default": true, "effects": [
            { "type": "SocialRatingModifier", "scope": "FactionGlobal",
              "parameters": { "rating": "growth", "amount": 1 } }
        ]},
        { "id": "police_state", "name": "Police State", "category": "politics", "effects": [
            { "type": "SocialRatingModifier", "scope": "FactionGlobal",
              "parameters": { "rating": "police", "amount": 3 } }
        ]},
        { "id": "survival", "name": "Survival", "category": "values", "default": true, "effects": [] },
        { "id": "none_future", "name": "None", "category": "future_society", "default": true, "effects": [] }
    ])");
    SocialPolicyRegistry registry;
    registry.Load(path.string());

    SocialEngineeringManager manager(registry);

    CHECK(manager.GetSocialRating(SocialRatingId_t::Growth) == 3);
    CHECK(manager.GetSocialRating(SocialRatingId_t::Police) == 0);
    // Repeated reads of an unchanged selection agree (the cached path).
    CHECK(manager.GetSocialRating(SocialRatingId_t::Growth) == 3);

    // Changing the selection must move the cached map, not keep answering from it.
    manager.SetActivePolicy(registry.Get("police_state"));
    CHECK(manager.GetSocialRating(SocialRatingId_t::Growth) == 1);
    CHECK(manager.GetSocialRating(SocialRatingId_t::Police) == 3);

    std::filesystem::remove(path);
}
