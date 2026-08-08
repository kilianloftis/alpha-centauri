#pragma once

#include "game/units/UnitSlotConfig.h"
#include "ui/IGameView.h"
#include "ui/unit-designer/UnitDesignerState.h"

#include <functional>

namespace ac
{

struct UnitComponentConfig_t;
class Military;
class UnitComponentRegistry;
class UnitSlotRegistry;
class ResearchManager;
class UnitManager;
class UnitDesign;

class UnitDesignerView : public IGameView
{
public:
    // rResearch gates both the component list and the slots: config already locks components
    // behind techs, and offering one the faction has not researched lets a player assemble and
    // save a design that should not exist.
    UnitDesignerView(
        Military& rMilitary,
        const UnitComponentRegistry& rComponentRegistry,
        const UnitSlotRegistry& rSlotRegistry,
        const ResearchManager& rResearch,
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

    void ShowComponentSelector_(
        const std::string& rComponentType,
        const std::string& rSlotDisplayName,
        std::function<void(const UnitComponentConfig_t&)> onSelected
    );
    bool IsUnlocked_(const std::string& rRequiredTech) const;
    void HandleSaveDesign_();

    Military& m_rMilitary;
    const UnitComponentRegistry& m_rComponentRegistry;
    const ResearchManager& m_rResearch;
    // The slots this faction has unlocked. Every consumer — the columns, the save gate, the
    // saved design — must agree on this list: a required slot the player cannot see and cannot
    // fill would otherwise make Save permanently dead, and UnitDesign's constructor throws on
    // a required slot with no component.
    std::vector<UnitSlotConfig_t> m_availableSlots;
    const UnitDesign* m_pSelectedDesign = nullptr;
    UnitDesignerState_t m_state;
};

} // namespace ac
