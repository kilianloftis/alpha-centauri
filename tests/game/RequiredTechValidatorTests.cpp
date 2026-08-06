// Tests for post-load required_tech validation (RequiredTechValidator) — the same
// fail-at-startup standard ValidateEffectReferences applies to effect lists, applied to the
// separate requiredTech field buildings/improvements/unit components/unit slots/social
// policies/pop types/council proposals/probe actions all carry.

#include "game/RequiredTechValidator.h"
#include "game/GameDataContext.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/map/ImprovementRegistry.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/research/TechRegistry.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/units/ProbeActionConfig.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitSlotRegistry.h"

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

// Empty instances of every source registry/config LoadGameData installs before validation.
void FillRequiredTechSources(GameDataContext& rData)
{
    rData.buildingRegistry = std::make_unique<BuildingRegistry>();
    rData.improvementRegistry = std::make_unique<ImprovementRegistry>();
    rData.unitComponentRegistry = std::make_unique<UnitComponentRegistry>();
    rData.unitSlotRegistry = std::make_unique<UnitSlotRegistry>();
    rData.socialPolicyRegistry = std::make_unique<SocialPolicyRegistry>();
    rData.popTypeRegistry = std::make_unique<PopTypeRegistry>();
    rData.councilProposalRegistry = std::make_unique<CouncilProposalRegistry>();
    rData.probeActionsConfig = std::make_unique<ProbeActionsConfig_t>();
}

} // namespace

TEST_CASE("ValidateRequiredTechReferences: building required_tech must name a known tech",
          "[validation][required-tech]")
{
    const std::filesystem::path techsPath = WriteTempJson("ac_required_tech_techs.json", R"([
        { "id": "ecology", "name": "Ecology", "category": "build", "cost": 10 }
    ])");

    GameDataContext data;
    data.techRegistry = std::make_unique<TechRegistry>();
    data.techRegistry->Load(techsPath.string());
    FillRequiredTechSources(data);

    SECTION("known required_tech does not throw")
    {
        const std::filesystem::path buildingsPath = WriteTempJson("ac_required_tech_ok.json", R"([
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
        const std::filesystem::path buildingsPath = WriteTempJson("ac_required_tech_bad.json", R"([
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
        const std::filesystem::path buildingsPath = WriteTempJson("ac_required_tech_empty.json", R"([
            { "id": "starter_hall", "name": "Starter Hall", "mineral_cost": 5, "category": "grow" }
        ])");
        data.buildingRegistry = std::make_unique<BuildingRegistry>();
        data.buildingRegistry->Load(buildingsPath.string());

        CHECK_NOTHROW(ValidateRequiredTechReferences(data));
        std::filesystem::remove(buildingsPath);
    }

    std::filesystem::remove(techsPath);
}

TEST_CASE("ValidateRequiredTechReferences: null techRegistry or source registry throws",
          "[validation][required-tech]")
{
    GameDataContext empty;
    CHECK_THROWS_WITH(ValidateRequiredTechReferences(empty),
                      Catch::Matchers::ContainsSubstring("techRegistry"));

    const std::filesystem::path techsPath = WriteTempJson("ac_required_tech_only.json", R"([
        { "id": "ecology", "name": "Ecology", "category": "build", "cost": 10 }
    ])");
    GameDataContext techOnly;
    techOnly.techRegistry = std::make_unique<TechRegistry>();
    techOnly.techRegistry->Load(techsPath.string());
    // buildingRegistry (and other sources) left null: production path must not skip.
    CHECK_THROWS_WITH(ValidateRequiredTechReferences(techOnly),
                      Catch::Matchers::ContainsSubstring("buildingRegistry"));

    GameDataContext complete;
    complete.techRegistry = std::make_unique<TechRegistry>();
    complete.techRegistry->Load(techsPath.string());
    FillRequiredTechSources(complete);
    CHECK_NOTHROW(ValidateRequiredTechReferences(complete));

    std::filesystem::remove(techsPath);
}

TEST_CASE("ValidateRequiredTechReferences: probe action required_tech must name a known tech",
          "[validation][required-tech][probe]")
{
    const std::filesystem::path techsPath = WriteTempJson("ac_probe_required_techs.json", R"([
        { "id": "gene_splicing", "name": "Gene Splicing", "category": "conquer", "cost": 10 }
    ])");

    GameDataContext data;
    data.techRegistry = std::make_unique<TechRegistry>();
    data.techRegistry->Load(techsPath.string());
    FillRequiredTechSources(data);

    SECTION("known required_tech does not throw")
    {
        ProbeActionConfig_t action;
        action.id = ProbeActionId_t::GeneticPlague;
        action.requiredTech = "gene_splicing";
        data.probeActionsConfig->actions = {action};

        CHECK_NOTHROW(ValidateRequiredTechReferences(data));
    }

    SECTION("unknown required_tech throws naming the probe action and the tech")
    {
        ProbeActionConfig_t action;
        action.id = ProbeActionId_t::GeneticPlague;
        action.requiredTech = "not_a_tech";
        data.probeActionsConfig->actions = {action};

        CHECK_THROWS_WITH(ValidateRequiredTechReferences(data),
                          Catch::Matchers::ContainsSubstring("genetic_plague")
                              && Catch::Matchers::ContainsSubstring("not_a_tech"));
    }

    std::filesystem::remove(techsPath);
}
