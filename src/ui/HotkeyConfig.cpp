#include "ui/HotkeyConfig.h"

#include "game/map/ImprovementRegistry.h"
#include "game/map/TerrainOperationRegistry.h"
#include "game/terraform/TerraformRules.h"
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
        const std::string name(magic_enum::enum_name(value));
        if (ToLowerAscii(name) == normalized)
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

[[noreturn]] void FailHotkeys_(const std::string& rConfigPath, const std::string& rMessage)
{
    throw std::runtime_error("Hotkeys '" + rConfigPath + "': " + rMessage);
}

// Returns the binding's action name.
std::string RequireBindingFields_(const nlohmann::json& rEntry, const std::string& rConfigPath)
{
    if (!rEntry.contains("action") || !rEntry.at("action").is_string())
    {
        FailHotkeys_(rConfigPath, "a binding is missing string 'action'");
    }
    if (!rEntry.contains("key") || !rEntry.at("key").is_string())
    {
        FailHotkeys_(rConfigPath, "a binding is missing string 'key'");
    }
    return rEntry.at("action").get<std::string>();
}

HotkeyChord_t ReadChord_(const nlohmann::json& rEntry, const std::string& rActionName,
                         const std::string& rConfigPath)
{
    const auto fail = [&rConfigPath](const std::string& rMessage) {
        FailHotkeys_(rConfigPath, rMessage);
    };
    const std::string keyName = rEntry.at("key").get<std::string>();
    return HotkeyChord_t{EnumFromName<Key_t>(keyName, "hotkey key"),
                         ReadModifier_(rEntry, "ctrl", rActionName, fail),
                         ReadModifier_(rEntry, "alt", rActionName, fail),
                         ReadModifier_(rEntry, "shift", rActionName, fail)};
}

// The built-in action, or nullopt for a terraform project; exactly one must match.
std::optional<HotkeyAction_t> ResolveAction_(const std::string& rActionName,
                                             const ImprovementRegistry& rImprovements,
                                             const TerrainOperationRegistry& rOperations,
                                             const std::string& rConfigPath)
{
    const std::optional<HotkeyAction_t> builtin = FindBuiltin(rActionName);
    const bool bProject =
        FindTerraformProject(rActionName, rImprovements, rOperations).has_value();
    if (builtin.has_value() && bProject)
    {
        FailHotkeys_(rConfigPath, "action '" + rActionName
                                      + "' is both a built-in action and a terraform project");
    }
    if (!builtin.has_value() && !bProject)
    {
        FailHotkeys_(rConfigPath, "unknown action '" + rActionName + "'");
    }
    return builtin;
}

// What one load has bound so far, to reject repeats.
class BindingLedger_
{
public:
    explicit BindingLedger_(const std::string& rConfigPath)
        : m_rConfigPath(rConfigPath)
    {
    }

    void RequireNewAction(const std::string& rActionName)
    {
        for (const std::string& rSeen : m_seenActions)
        {
            if (ToLowerAscii(rSeen) == ToLowerAscii(rActionName))
            {
                FailHotkeys_(m_rConfigPath, "action '" + rActionName + "' is bound more than once");
            }
        }
        m_seenActions.push_back(rActionName);
    }

    // Order actions may share a chord. The selected unit decides which one runs.
    void RequireChordFree(HotkeyAction_t action, const HotkeyChord_t& rChord,
                          const std::string& rActionName)
    {
        const HotkeyStage_t stage = StageFor(action);
        if (stage == HotkeyStage_t::Order)
        {
            return;
        }
        std::vector<SeenChord_t>& rStage = m_stageChords[static_cast<std::size_t>(stage)];
        for (const SeenChord_t& rSeen : rStage)
        {
            if (rSeen.chord == rChord)
            {
                FailHotkeys_(m_rConfigPath, "actions '" + rActionName + "' and '" + rSeen.action
                                                + "' share " + DescribeHotkeyChord(rChord));
            }
        }
        rStage.push_back(SeenChord_t{rChord, rActionName});
    }

private:
    const std::string& m_rConfigPath;
    std::vector<std::string> m_seenActions;
    std::array<std::vector<SeenChord_t>, magic_enum::enum_count<HotkeyStage_t>()> m_stageChords;
};

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
            return Parse_(rJson, rConfigPath, rImprovements, rOperations);
        });
}

HotkeyConfig HotkeyConfig::Parse_(const nlohmann::json& rJson, const std::string& rConfigPath,
                                  const ImprovementRegistry& rImprovements,
                                  const TerrainOperationRegistry& rOperations)
{
    if (!rJson.contains("bindings") || !rJson.at("bindings").is_array())
    {
        FailHotkeys_(rConfigPath, "missing required array 'bindings'");
    }
    HotkeyConfig config;
    BindingLedger_ ledger(rConfigPath);
    for (const nlohmann::json& rEntry : rJson.at("bindings"))
    {
        const std::string actionName = RequireBindingFields_(rEntry, rConfigPath);
        const HotkeyChord_t chord = ReadChord_(rEntry, actionName, rConfigPath);
        ledger.RequireNewAction(actionName);
        const std::optional<HotkeyAction_t> builtin =
            ResolveAction_(actionName, rImprovements, rOperations, rConfigPath);
        if (!builtin)
        {
            config.BindTerraform_(chord, actionName, rConfigPath);
            continue;
        }
        ledger.RequireChordFree(*builtin, chord, actionName);
        config.m_chords[ActionIndex(*builtin)] = chord;
    }
    return config;
}

void HotkeyConfig::BindTerraform_(const HotkeyChord_t& rChord, const std::string& rProjectId,
                                  const std::string& rConfigPath)
{
    for (const TerraformHotkey_t& rBound : m_terraform)
    {
        if (rBound.chord == rChord)
        {
            FailHotkeys_(rConfigPath, "actions '" + rProjectId + "' and '" + rBound.projectId
                                          + "' share " + DescribeHotkeyChord(rChord));
        }
    }
    m_terraform.push_back(TerraformHotkey_t{rChord, rProjectId});
}

std::optional<HotkeyChord_t> HotkeyConfig::Find(HotkeyAction_t action) const
{
    return m_chords[ActionIndex(action)];
}

} // namespace ac
