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
    return flavor;
}

} // namespace

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
