#pragma once

#include <string>

namespace ac
{

class GameSettings;

// Controls whether a setting may be edited in the current session context.
enum class SettingScope_t
{
    Always,
    NewGameOnly,
};

enum class SettingRowKind_t
{
    Header,
    Bool,
    ReadOnlyValue,
    // Shows getValueText; a click calls cycle to move to the next value.
    Choice,
};

struct SettingDescriptor_t
{
    const char* label = nullptr;
    SettingRowKind_t kind = SettingRowKind_t::Header;
    SettingScope_t scope = SettingScope_t::Always;
    bool (*getBool)(const GameSettings&) = nullptr;
    void (*setBool)(GameSettings&, bool) = nullptr;
    std::string (*getValueText)(const GameSettings&) = nullptr;
    void (*cycle)(GameSettings&) = nullptr;
};

} // namespace ac
