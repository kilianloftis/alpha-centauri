#pragma once

#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitSlotConfig.h"
#include "ui/IGameView.h"
#include "ui/unit-designer/UnitDesignerState.h"

#include <functional>
#include <vector>

namespace ac
{

class Military;
class UnitManager;
class UnitDesign;
class DesignListPanel;

class UnitDesignerView : public IGameView
{
public:
    // availableSlots / availableComponents are already tech-gated (see GetAvailableUnitSlots /
    // GetAvailableUnitComponents). Offering a locked option here lets a player assemble and
    // save a design that should not exist.
    UnitDesignerView(
        Military& rMilitary,
        std::vector<UnitSlotConfig_t> availableSlots,
        std::vector<const UnitComponentConfig_t*> availableComponents,
        const UnitManager* pUnitManager,
        WindowLayout_t layout
    );
    ~UnitDesignerView() override = default;

    bool HandleKey(const KeyEvent_t& rEvent) override;

private:
    void BuildUnitStatusPanel_(const UnitManager* pUnitManager);
    void BuildTopPanelElements_();
    void BuildDesignListPanel_();
    void OnDesignSelected_(const UnitDesign* pDesign);
    // Editing a slot makes the draft something other than the design the list is highlighting.
    void ClearDesignSelection_();

    void ShowComponentSelector_(
        const std::string& rComponentType,
        const std::string& rSlotDisplayName,
        std::function<void(const UnitComponentConfig_t&)> onSelected
    );
    void HandleSaveDesign_();

    Military& m_rMilitary;
    // The slots this faction has unlocked. Every consumer — the columns, the save gate, the
    // saved design — must agree on this list: a required slot the player cannot see and cannot
    // fill would otherwise make Save permanently dead, and UnitDesign's constructor throws on
    // a required slot with no component.
    std::vector<UnitSlotConfig_t> m_availableSlots;
    // Unlocked components across all types; ShowComponentSelector_ filters by slot type.
    std::vector<const UnitComponentConfig_t*> m_availableComponents;
    const UnitDesign* m_pSelectedDesign = nullptr;
    // Owned by m_elements; built once in the constructor and never replaced.
    DesignListPanel* m_pDesignList = nullptr;
    UnitDesignerState_t m_state;
};

} // namespace ac
