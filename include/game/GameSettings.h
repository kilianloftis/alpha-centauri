#pragma once

#include <string>

namespace ac
{

// Cross-save player preferences (not part of GameState / save-game serialization).
// Owned by Engine; GameState holds a non-owning reference for stage and UI access.
class GameSettings
{
public:
    static constexpr const char* kDefaultPath = "user_settings.json";

    bool IsPauseAtEndOfTurn() const { return m_bPauseAtEndOfTurn; }
    void SetPauseAtEndOfTurn(bool value) { m_bPauseAtEndOfTurn = value; }

    // Missing file leaves defaults (pause off). Unreadable or corrupt file throws.
    void Load(const std::string& path = kDefaultPath);
    void Save(const std::string& path = kDefaultPath) const;

private:
    bool m_bPauseAtEndOfTurn = false;
};

} // namespace ac
