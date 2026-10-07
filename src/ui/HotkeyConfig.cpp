#include "ui/HotkeyConfig.h"

#include "game/map/ImprovementRegistry.h"
#include "game/map/TerrainOperationRegistry.h"
#include "game/units/TerraformRules.h"
#include "lib/config/EnumNames.h"
#include "lib/config/JsonConfigLoader.h"

#include <functional>
#include <magic_enum.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

namespace
{

enum class HotkeyStage_t
{
    Order,
    Camera,
    Chrome,
    View,
    Bombard,
};

struct SeenChord_t
{
    HotkeyChord_t chord;
    std::string action;
};

HotkeyStage_t StageFor(HotkeyAction_t action)
{
    switch (action)
    {
    case HotkeyAction_t::Hold:
    case HotkeyAction_t::SkipTurn:
    case HotkeyAction_t::AttachTransport:
    case HotkeyAction_t::UnloadTransport:
    case HotkeyAction_t::Disband:
    case HotkeyAction_t::SupplyCrawl:
    case HotkeyAction_t::FoundBase:
    case HotkeyAction_t::Detonate:
    case HotkeyAction_t::Airdrop:
        return HotkeyStage_t::Order;
    case HotkeyAction_t::PanLeft:
    case HotkeyAction_t::PanRight:
    case HotkeyAction_t::PanUp:
    case HotkeyAction_t::PanDown:
    case HotkeyAction_t::ZoomIn:
    case HotkeyAction_t::ZoomOut:
        return HotkeyStage_t::Camera;
    case HotkeyAction_t::Cancel:
    case HotkeyAction_t::EndTurn:
    case HotkeyAction_t::NextUnit:
        return HotkeyStage_t::Chrome;
    case HotkeyAction_t::Research:
    case HotkeyAction_t::SocialEngineering:
    case HotkeyAction_t::UnitDesigner:
    case HotkeyAction_t::Settings:
    case HotkeyAction_t::Satellites:
        return HotkeyStage_t::View;
    case HotkeyAction_t::Bombard:
        return HotkeyStage_t::Bombard;
    }
    return HotkeyStage_t::Order;
}

std::optional<HotkeyAction_t> FindBuiltin(const std::string& rName)
{
    const std::string normalized = ToLowerAscii(rName);
    for (const HotkeyAction_t value : magic_enum::enum_values<HotkeyAction_t>())
    {
        if (ToLowerAscii(std::string(magic_enum::enum_name(value))) == normalized)
        {
            return value;
        }
    }
    return std::nullopt;
}

std::size_t ActionIndex(HotkeyAction_t action)
{
    return static_cast<std::size_t>(*magic_enum::enum_index(action));
}

bool ReadModifier_(const nlohmann::json& rEntry, const char* name, const std::string& rAction,
                   const std::function<void(const std::string&)>& rFail)
{
    if (!rEntry.contains(name))
    {
        return false;
    }
    if (!rEntry.at(name).is_boolean())
    {
        rFail("action '" + rAction + "' has a non-boolean '" + name + "'");
    }
    return rEntry.at(name).get<bool>();
}

} // namespace

std::string DescribeHotkeyChord(const HotkeyChord_t& rChord)
{
    std::string text;
    if (rChord.bCtrl)
    {
        text += "Ctrl+";
    }
    if (rChord.bAlt)
    {
        text += "Alt+";
    }
    if (rChord.bShift)
    {
        text += "Shift+";
    }
    text += magic_enum::enum_name(rChord.key);
    return text;
}

HotkeyConfig HotkeyConfig::Load(const std::string& rConfigPath,
                                const ImprovementRegistry& rImprovements,
                                const TerrainOperationRegistry& rOperations)
{
    return JsonConfigLoader::LoadObjectFile<HotkeyConfig>(
        rConfigPath, "hotkey",
        [&rConfigPath, &rImprovements, &rOperations](const nlohmann::json& rJson) {
            const auto fail = [&rConfigPath](const std::string& rMessage) {
                throw std::runtime_error("Hotkeys '" + rConfigPath + "': " + rMessage);
            };

            if (!rJson.contains("bindings") || !rJson.at("bindings").is_array())
            {
                fail("missing required array 'bindings'");
            }

            HotkeyConfig config;
            std::vector<SeenChord_t> stageChords[5];
            std::vector<std::string> seenActions;

            for (const nlohmann::json& rEntry : rJson.at("bindings"))
            {
                if (!rEntry.contains("action") || !rEntry.at("action").is_string())
                {
                    fail("a binding is missing string 'action'");
                }
                if (!rEntry.contains("key") || !rEntry.at("key").is_string())
                {
                    fail("a binding is missing string 'key'");
                }

                const std::string actionName = rEntry.at("action").get<std::string>();
                const std::string keyName = rEntry.at("key").get<std::string>();
                const HotkeyChord_t chord{EnumFromName<Key_t>(keyName, "hotkey key"),
                                          ReadModifier_(rEntry, "ctrl", actionName, fail),
                                          ReadModifier_(rEntry, "alt", actionName, fail),
                                          ReadModifier_(rEntry, "shift", actionName, fail)};

                for (const std::string& rSeen : seenActions)
                {
                    if (ToLowerAscii(rSeen) == ToLowerAscii(actionName))
                    {
                        fail("action '" + actionName + "' is bound more than once");
                    }
                }
                seenActions.push_back(actionName);

                const std::optional<HotkeyAction_t> builtin = FindBuiltin(actionName);
                const bool bProject = FindTerraformProject(actionName, rImprovements, rOperations)
                                          .has_value();
                if (builtin.has_value() && bProject)
                {
                    fail("action '" + actionName
                         + "' is both a built-in action and a terraform project");
                }
                if (!builtin.has_value() && !bProject)
                {
                    fail("unknown action '" + actionName + "'");
                }

                if (bProject)
                {
                    for (const TerraformHotkey_t& rBound : config.m_terraform)
                    {
                        if (rBound.chord == chord)
                        {
                            fail("actions '" + actionName + "' and '" + rBound.projectId
                                 + "' share " + DescribeHotkeyChord(chord));
                        }
                    }
                    config.m_terraform.push_back(TerraformHotkey_t{chord, actionName});
                    continue;
                }

                const HotkeyAction_t action = *builtin;
                const HotkeyStage_t stage = StageFor(action);
                // Order actions may share a chord. The selected unit decides which one runs.
                if (stage != HotkeyStage_t::Order)
                {
                    std::vector<SeenChord_t>& rStage = stageChords[static_cast<int>(stage)];
                    for (const SeenChord_t& rSeen : rStage)
                    {
                        if (rSeen.chord == chord)
                        {
                            fail("actions '" + actionName + "' and '" + rSeen.action + "' share "
                                 + DescribeHotkeyChord(chord));
                        }
                    }
                    rStage.push_back(SeenChord_t{chord, actionName});
                }

                config.m_chords[ActionIndex(action)] = chord;
            }

            return config;
        });
}

std::optional<HotkeyChord_t> HotkeyConfig::Find(HotkeyAction_t action) const
{
    return m_chords[ActionIndex(action)];
}

} // namespace ac
