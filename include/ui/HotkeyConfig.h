#pragma once

#include "input/Input.h"

#include <magic_enum.hpp>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace ac
{

class ImprovementRegistry;
class TerrainOperationRegistry;

// Built-in player actions. A hotkeys.json action that is not one of these is a former
// project id.
enum class HotkeyAction_t
{
    Hold,
    SkipTurn,
    AttachTransport,
    UnloadTransport,
    Disband,
    SupplyCrawl,
    FoundBase,
    Detonate,
    Airdrop,
    Bombard,
    PanLeft,
    PanRight,
    PanUp,
    PanDown,
    Cancel,
    EndTurn,
    NextUnit,
    Research,
    SocialEngineering,
    UnitDesigner,
    Settings,
    Satellites,
};

struct HotkeyChord_t
{
    Key_t key = Key_t::Unknown;
    bool bCtrl = false;
    bool bAlt = false;
    bool bShift = false;

    bool Matches(const KeyEvent_t& rEvent) const
    {
        return rEvent.key == key && rEvent.modifier.bCtrl == bCtrl && rEvent.modifier.bAlt == bAlt
               && rEvent.modifier.bShift == bShift;
    }

    bool operator==(const HotkeyChord_t& rOther) const
    {
        return key == rOther.key && bCtrl == rOther.bCtrl && bAlt == rOther.bAlt
               && bShift == rOther.bShift;
    }
};

struct HotkeyChordHash
{
    std::size_t operator()(const HotkeyChord_t& rChord) const
    {
        return (static_cast<std::size_t>(rChord.key) << 3)
               ^ (static_cast<std::size_t>(rChord.bCtrl) << 2)
               ^ (static_cast<std::size_t>(rChord.bAlt) << 1)
               ^ static_cast<std::size_t>(rChord.bShift);
    }
};

std::string DescribeHotkeyChord(const HotkeyChord_t& rChord);

// One former project bound in hotkeys.json. The action id is the project id.
struct TerraformHotkey_t
{
    HotkeyChord_t chord;
    std::string projectId;
};

// Every player chord, loaded from config/ui/hotkeys.json. Widget keys (list scroll, hurry
// digits, Escape-to-close) are not in this file.
class HotkeyConfig
{
public:
    static HotkeyConfig Load(const std::string& rConfigPath,
                             const ImprovementRegistry& rImprovements,
                             const TerrainOperationRegistry& rOperations);

    // Empty when this action has no entry. An unbound action is simply unavailable.
    std::optional<HotkeyChord_t> Find(HotkeyAction_t action) const;

    const std::vector<TerraformHotkey_t>& Terraform() const { return m_terraform; }

private:
    static constexpr std::size_t k_ActionCount = magic_enum::enum_count<HotkeyAction_t>();

    std::array<std::optional<HotkeyChord_t>, k_ActionCount> m_chords{};
    std::vector<TerraformHotkey_t> m_terraform;
};

} // namespace ac
