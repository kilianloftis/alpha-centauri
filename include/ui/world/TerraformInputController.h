#pragma once

#include "input/Input.h"

#include <string>
#include <unordered_map>

namespace ac
{

class Unit;

// Former-only hotkeys that request a terraform project by improvement id.
// WorldView calls TryStartTerraform when WasTerraformRequested() is true.
class TerraformInputController
{
public:
    TerraformInputController() = default;

    bool HandleKey(const KeyEvent_t& rEvent, Unit* pSelectedUnit);

    bool WasTerraformRequested() const { return m_bTerraformRequested; }
    const std::string& GetRequestedImprovementId() const { return m_requestedImprovementId; }

private:
    bool m_bTerraformRequested = false;
    std::string m_requestedImprovementId;

    // Letter bindings (no modifier keys in KeyEvent_t). Only consumed when the unit has
    // RuleFlagId_t::Terraform so letters stay free on other units.
    const std::unordered_map<Key_t, std::string> m_bindings = {
        { Key_t::R, "Road" },
        { Key_t::F, "Farm" },
        { Key_t::I, "Mine" },
        { Key_t::O, "Forest" },
        { Key_t::S, "Sensor" },
        { Key_t::Y, "SolarCollector" },
        { Key_t::E, "SoilEnricher" },
        { Key_t::B, "Bunker" },
        { Key_t::A, "Airbase" },
        { Key_t::G, "MagTube" },
        { Key_t::C, "Condenser" },
        { Key_t::X, "Mirror" },
        { Key_t::Z, "ThermalBorehole" },
        { Key_t::P, "PlantFungus" },
        { Key_t::D, "RemoveFungus" },
        { Key_t::L, "LevelTerrain" },
        { Key_t::U, "RaiseLand" },
        { Key_t::J, "LowerLand" },
        { Key_t::Q, "Aquifer" },
        { Key_t::K, "KelpFarm" },
        { Key_t::W, "MiningPlatform" },
        { Key_t::T, "TidalHarness" },
    };
};

} // namespace ac
