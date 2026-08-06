#include "GameFixtures.h"

#include "game/faction/FactionConfig.h"
#include "game/faction/FactionFlavor.h"
#include "game/faction/FactionIdentity.h"

#include <catch2/catch_test_macros.hpp>
#include <string>

using namespace ac;

namespace
{

FactionIdentity MakeTestIdentity()
{
    FactionIdentityConfig identityConfig;
    identityConfig.name = "Gaian";
    identityConfig.descriptiveName = "Gaian High Command";
    identityConfig.noun = "Gaian";
    identityConfig.adjective = "Gaians";

    LeaderConfig leaderConfig;
    leaderConfig.name = "Deirdre";
    leaderConfig.title = "Lady Deirdre";

    return FactionIdentity(identityConfig, leaderConfig);
}

FactionFlavorConfig MakeTestFlavorConfig()
{
    FactionFlavorConfig flavor;
    flavor.baseNames = {"Alpha Base", "Beta Base"};
    flavor.phrases["greeting"] = {"{leader} of the {faction.noun} greets you."};
    return flavor;
}

} // namespace

TEST_CASE("FactionFlavor substitutes identity tokens in templates", "[faction][flavor]")
{
    const FactionIdentity identity = MakeTestIdentity();
    const FactionFlavorConfig flavorConfig = MakeTestFlavorConfig();
    FactionFlavor flavor(flavorConfig, identity, actest::k_TestFactionSeed);

    CHECK(flavor.Format("{leader}") == "Deirdre");
    CHECK(flavor.Format("{leader.title}") == "Lady Deirdre");
    CHECK(flavor.Format("{faction}") == "Gaian");
    CHECK(flavor.Format("{faction.noun}") == "Gaian");
    CHECK(flavor.Format("{faction.adjective}") == "Gaians");
    CHECK(flavor.Format("{faction.descriptive_name}") == "Gaian High Command");
    CHECK(flavor.Format("{leader.title} leads the {faction.adjective}.")
          == "Lady Deirdre leads the Gaians.");
}

TEST_CASE("FactionFlavor picks unique base names then falls back", "[faction][flavor]")
{
    const FactionIdentity identity = MakeTestIdentity();
    const FactionFlavorConfig flavorConfig = MakeTestFlavorConfig();
    FactionFlavor flavor(flavorConfig, identity, actest::k_TestFactionSeed);

    const std::string first = flavor.PickBaseName();
    const std::string second = flavor.PickBaseName();
    CHECK(first != second);
    CHECK((first == "Alpha Base" || first == "Beta Base"));
    CHECK((second == "Alpha Base" || second == "Beta Base"));

    const std::string fallback = flavor.PickBaseName();
    CHECK(fallback == "Gaians Base 1");
    CHECK(flavor.PickBaseName() == "Gaians Base 2");
}

TEST_CASE("FactionFlavor formats picked phrases", "[faction][flavor]")
{
    const FactionIdentity identity = MakeTestIdentity();
    const FactionFlavorConfig flavorConfig = MakeTestFlavorConfig();
    FactionFlavor flavor(flavorConfig, identity, actest::k_TestFactionSeed);

    CHECK(flavor.PickPhrase("greeting") == "Deirdre of the Gaian greets you.");
    CHECK(flavor.PickPhrase("missing_category").empty());
}
