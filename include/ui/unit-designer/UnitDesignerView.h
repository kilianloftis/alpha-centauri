#pragma once

#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitSlotConfig.h"
#include "ui/IGameView.h"
#include "ui/unit-designer/SlotColumnPanel.h"
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
    // availableSlots and availableComponents are already tech-gated, so no locked option is
    // offered.
    UnitDesignerView(
        Military& rMilitary,
        std::vector<UnitSlotConfig_t> availableSlots,
        std::vector<const UnitComponentConfig_t*> availableComponents,
        const UnitManager& rUnitManager,
        WindowLayout_t layout
    );
    ~UnitDesignerView() override = default;

    bool HandleKey(const KeyEvent_t& rEvent) override;

private:
    void BuildUnitStatusPanel_(const UnitManager& rUnitManager);
    void BuildTopPanelElements_();
    SlotColumnPanel::SlotEntry_t MakeSlotEntry_(const UnitSlotConfig_t& rSlot);
    void BuildDesignListPanel_();
    void OnDesignSelected_(const UnitDesign* pDesign);
    // Editing a slot makes the draft something other than the design the list is highlighting.
    void ClearDesignSelection_();

    void ShowComponentSelector_(
        const std::string& rComponentType,
        const std::string& rSlotDisplayName,
        std::function<void(const UnitComponentConfig_t&)> onSelected
    );
    // Drops drafted components whose requires_chassis list excludes rChassisId.
    void ClearIncompatibleComponents_(const std::string& rChassisId);
    void HandleSaveDesign_();

    Military& m_rMilitary;
    // The unlocked slots, which the columns, the save gate and the saved design all share; a
    // hidden required slot would make Save dead.
    std::vector<UnitSlotConfig_t> m_availableSlots;
    // Unlocked components across all types; ShowComponentSelector_ filters by slot type.
    std::vector<const UnitComponentConfig_t*> m_availableComponents;
    const UnitDesign* m_pSelectedDesign = nullptr;
    // Owned by m_elements; built once in the constructor and never replaced.
    DesignListPanel* m_pDesignList = nullptr;
    UnitDesignerState_t m_state;
};

} // namespace ac
