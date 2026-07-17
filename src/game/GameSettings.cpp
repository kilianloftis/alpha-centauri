#include "game/GameSettings.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

namespace ac
{

void GameSettings::Load(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        // First run / missing prefs: keep member defaults.
        return;
    }

    const nlohmann::json json = nlohmann::json::parse(file);
    m_bPauseAtEndOfTurn = json.value("pause_at_end_of_turn", m_bPauseAtEndOfTurn);
}

void GameSettings::Save(const std::string& path) const
{
    nlohmann::json json;
    json["pause_at_end_of_turn"] = m_bPauseAtEndOfTurn;

    std::ofstream file(path);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not write game settings '" + path + "'");
    }
    file << json.dump(2) << '\n';
}

} // namespace ac
