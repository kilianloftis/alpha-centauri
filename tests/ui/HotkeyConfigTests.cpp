#include "TestHelpers.h"

#include "ui/HotkeyConfig.h"

#include "game/map/ImprovementRegistry.h"
#include "game/map/MapOccupantLoad.h"
#include "game/map/TerrainOperationRegistry.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace ac;

namespace
{

struct OccupantRegistries
{
    ImprovementRegistry improvements;
    TerrainOperationRegistry operations;

    OccupantRegistries()
    {
        LoadMapOccupants(actest::FixturePath("improvements.json"),
                         actest::FixturePath("terrain.json"), improvements, operations);
    }
};

class TempHotkeys
{
public:
    TempHotkeys(const std::string& name, const std::string& body)
        : m_path(std::filesystem::temp_directory_path() / name)
    {
        std::ofstream out(m_path);
        out << body;
    }

    ~TempHotkeys() { std::filesystem::remove(m_path); }

    std::string Path() const { return m_path.string(); }

private:
    std::filesystem::path m_path;
};

std::vector<std::string> CompleteBindings()
{
    return {
        R"({"action":"Hold","key":"H"})",
        R"({"action":"SkipTurn","key":"Space"})",
        R"({"action":"AttachTransport","key":"L"})",
        R"({"action":"UnloadTransport","key":"U","shift":true})",
        R"({"action":"Disband","key":"D","shift":true})",
        R"({"action":"SupplyCrawl","key":"O"})",
        R"({"action":"FoundBase","key":"B"})",
        R"({"action":"Detonate","key":"X","shift":true})",
        R"({"action":"Airdrop","key":"I"})",
        R"({"action":"Bombard","key":"F"})",
        R"({"action":"Farm","key":"F"})",
        R"({"action":"PanLeft","key":"ArrowLeft"})",
        R"({"action":"PanRight","key":"ArrowRight"})",
        R"({"action":"PanUp","key":"ArrowUp"})",
        R"({"action":"PanDown","key":"ArrowDown"})",
        R"({"action":"Cancel","key":"Escape"})",
        R"({"action":"EndTurn","key":"Enter"})",
        R"({"action":"NextUnit","key":"V"})",
        R"({"action":"Research","key":"F2"})",
        R"({"action":"SocialEngineering","key":"E"})",
        R"({"action":"UnitDesigner","key":"U"})",
        R"({"action":"Settings","key":"O"})",
        R"({"action":"Satellites","key":"F6"})",
    };
}

std::string BindingsDocument(const std::vector<std::string>& rEntries)
{
    std::string body = "{\n  \"bindings\": [\n";
    for (std::size_t i = 0; i < rEntries.size(); ++i)
    {
        body += "    ";
        body += rEntries[i];
        if (i + 1 != rEntries.size())
        {
            body += ",";
        }
        body += "\n";
    }
    body += "  ]\n}\n";
    return body;
}

const TerraformHotkey_t* FindProject(const HotkeyConfig& rConfig, const std::string& rProjectId)
{
    for (const TerraformHotkey_t& rBinding : rConfig.Terraform())
    {
        if (rBinding.projectId == rProjectId)
        {
            return &rBinding;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("A hotkeys file loads, with Bombard and Farm both on F", "[ui][hotkeys]")
{
    OccupantRegistries registries;
    const HotkeyConfig config = HotkeyConfig::Load(
        actest::FixturePath("hotkeys.json"), registries.improvements, registries.operations);

    const std::optional<HotkeyChord_t> bombard = config.Find(HotkeyAction_t::Bombard);
    REQUIRE(bombard.has_value());
    CHECK(bombard->key == Key_t::F);
    CHECK_FALSE(bombard->bCtrl);
    CHECK_FALSE(bombard->bAlt);
    CHECK_FALSE(bombard->bShift);
    const TerraformHotkey_t* pFarm = FindProject(config, "Farm");
    REQUIRE(pFarm != nullptr);
    CHECK(pFarm->chord.key == Key_t::F);
    const std::optional<HotkeyChord_t> unload = config.Find(HotkeyAction_t::UnloadTransport);
    REQUIRE(unload.has_value());
    CHECK(unload->bShift);
    const std::optional<HotkeyChord_t> research = config.Find(HotkeyAction_t::Research);
    REQUIRE(research.has_value());
    CHECK(research->key == Key_t::F2);
}

TEST_CASE("A built-in action with no hotkey stays unbound", "[ui][hotkeys]")
{
    OccupantRegistries registries;
    std::vector<std::string> entries = CompleteBindings();
    entries.erase(entries.begin());
    const TempHotkeys file("ac_hotkeys_missing.json", BindingsDocument(entries));

    const HotkeyConfig config = HotkeyConfig::Load(
        file.Path(), registries.improvements, registries.operations);
    CHECK_FALSE(config.Find(HotkeyAction_t::Hold).has_value());
    CHECK(config.Find(HotkeyAction_t::SkipTurn).has_value());
}

TEST_CASE("Two order actions may share a chord", "[ui][hotkeys]")
{
    OccupantRegistries registries;
    std::vector<std::string> entries = CompleteBindings();
    entries[1] = R"({"action":"SkipTurn","key":"H"})";
    const TempHotkeys file("ac_hotkeys_shared_order.json", BindingsDocument(entries));

    const HotkeyConfig config = HotkeyConfig::Load(
        file.Path(), registries.improvements, registries.operations);
    const std::optional<HotkeyChord_t> hold = config.Find(HotkeyAction_t::Hold);
    const std::optional<HotkeyChord_t> skip = config.Find(HotkeyAction_t::SkipTurn);
    REQUIRE(hold.has_value());
    REQUIRE(skip.has_value());
    CHECK(*hold == *skip);
}

TEST_CASE("Ctrl and Alt distinguish chords on the same key", "[ui][hotkeys]")
{
    OccupantRegistries registries;
    std::vector<std::string> entries = CompleteBindings();
    entries[0] = R"({"action":"Hold","key":"H","ctrl":true})";
    entries[1] = R"({"action":"SkipTurn","key":"H","alt":true})";
    const TempHotkeys file("ac_hotkeys_modifiers.json", BindingsDocument(entries));

    const HotkeyConfig config = HotkeyConfig::Load(
        file.Path(), registries.improvements, registries.operations);
    const HotkeyChord_t hold = *config.Find(HotkeyAction_t::Hold);
    const HotkeyChord_t skip = *config.Find(HotkeyAction_t::SkipTurn);
    CHECK(hold.bCtrl);
    CHECK(skip.bAlt);

    const KeyEvent_t ctrlH{Key_t::H, ModifierState_t{/*bCtrl=*/true, false, false}};
    const KeyEvent_t altH{Key_t::H, ModifierState_t{false, /*bAlt=*/true, false}};
    CHECK(hold.Matches(ctrlH));
    CHECK_FALSE(hold.Matches(altH));
    CHECK(skip.Matches(altH));
    CHECK_FALSE(skip.Matches(ctrlH));
}

TEST_CASE("Hotkeys reject an unknown action", "[ui][hotkeys]")
{
    OccupantRegistries registries;
    std::vector<std::string> entries = CompleteBindings();
    entries.push_back(R"({"action":"NotAProject","key":"M"})");
    const TempHotkeys file("ac_hotkeys_unknown.json", BindingsDocument(entries));

    CHECK_THROWS_WITH(
        HotkeyConfig::Load(file.Path(), registries.improvements, registries.operations),
        Catch::Matchers::ContainsSubstring("NotAProject"));
}

TEST_CASE("Hotkeys load when Bombard and Farm share F", "[ui][hotkeys]")
{
    OccupantRegistries registries;
    const TempHotkeys file("ac_hotkeys_shared_f.json", BindingsDocument(CompleteBindings()));

    const HotkeyConfig config = HotkeyConfig::Load(
        file.Path(), registries.improvements, registries.operations);
    const std::optional<HotkeyChord_t> bombard = config.Find(HotkeyAction_t::Bombard);
    REQUIRE(bombard.has_value());
    CHECK(bombard->key == Key_t::F);
    const TerraformHotkey_t* pFarm = FindProject(config, "Farm");
    REQUIRE(pFarm != nullptr);
    CHECK(pFarm->chord.key == Key_t::F);
}

TEST_CASE("Hotkeys reject two terraform projects on the same chord", "[ui][hotkeys]")
{
    OccupantRegistries registries;
    std::vector<std::string> entries = CompleteBindings();
    entries.insert(entries.begin() + 11, R"({"action":"Road","key":"F"})");
    const TempHotkeys file("ac_hotkeys_two_projects.json", BindingsDocument(entries));

    CHECK_THROWS_WITH(
        HotkeyConfig::Load(file.Path(), registries.improvements, registries.operations),
        Catch::Matchers::ContainsSubstring("share"));
}
