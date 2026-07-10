// Tests for post-load required_tech validation (RequiredTechValidator) — the same
// fail-at-startup standard ValidateEffectReferences applies to effect lists, applied to the
// separate requiredTech field buildings/improvements/unit components/unit slots/social
// policies/pop types all carry.

#include "game/RequiredTechValidator.h"
#include "game/GameDataContext.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/research/TechRegistry.h"

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

} // namespace

TEST_CASE("ValidateRequiredTechReferences: building required_tech must name a known tech",
          "[validation][required-tech]")
{
    const std::filesystem::path techsPath = WriteTempJson_("ac_required_tech_techs.json", R"([
        { "id": "ecology", "name": "Ecology", "category": "build", "cost": 10 }
    ])");

    GameDataContext data;
    data.techRegistry = std::make_unique<TechRegistry>();
    data.techRegistry->Load(techsPath.string());

    SECTION("known required_tech does not throw")
    {
        const std::filesystem::path buildingsPath = WriteTempJson_("ac_required_tech_ok.json", R"([
            { "id": "gated_hall", "name": "Gated Hall", "mineral_cost": 10,
              "required_tech": "ecology", "category": "grow" }
        ])");
        data.buildingRegistry = std::make_unique<BuildingRegistry>();
        data.buildingRegistry->Load(buildingsPath.string());

        CHECK_NOTHROW(ValidateRequiredTechReferences(data));
        std::filesystem::remove(buildingsPath);
    }

    SECTION("unknown required_tech throws naming the building and the tech")
    {
        const std::filesystem::path buildingsPath = WriteTempJson_("ac_required_tech_bad.json", R"([
            { "id": "gated_hall", "name": "Gated Hall", "mineral_cost": 10,
              "required_tech": "not_a_tech", "category": "grow" }
        ])");
        data.buildingRegistry = std::make_unique<BuildingRegistry>();
        data.buildingRegistry->Load(buildingsPath.string());

        CHECK_THROWS_WITH(ValidateRequiredTechReferences(data),
                          Catch::Matchers::ContainsSubstring("gated_hall")
                              && Catch::Matchers::ContainsSubstring("not_a_tech"));
        std::filesystem::remove(buildingsPath);
    }

    SECTION("omitted required_tech is always fine")
    {
        const std::filesystem::path buildingsPath = WriteTempJson_("ac_required_tech_empty.json", R"([
            { "id": "starter_hall", "name": "Starter Hall", "mineral_cost": 5, "category": "grow" }
        ])");
        data.buildingRegistry = std::make_unique<BuildingRegistry>();
        data.buildingRegistry->Load(buildingsPath.string());

        CHECK_NOTHROW(ValidateRequiredTechReferences(data));
        std::filesystem::remove(buildingsPath);
    }

    std::filesystem::remove(techsPath);
}

TEST_CASE("ValidateRequiredTechReferences: a null registry is skipped, a null tech registry is a no-op",
          "[validation][required-tech]")
{
    GameDataContext data;
    CHECK_NOTHROW(ValidateRequiredTechReferences(data));

    const std::filesystem::path techsPath = WriteTempJson_("ac_required_tech_only.json", R"([
        { "id": "ecology", "name": "Ecology", "category": "build", "cost": 10 }
    ])");
    data.techRegistry = std::make_unique<TechRegistry>();
    data.techRegistry->Load(techsPath.string());

    // buildingRegistry left null: nothing to check, must not throw.
    CHECK_NOTHROW(ValidateRequiredTechReferences(data));

    std::filesystem::remove(techsPath);
}
