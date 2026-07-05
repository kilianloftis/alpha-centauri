#pragma once

#include "ui/IGameView.h"
#include "ui/unit-designer/UnitDesignerState.h"

namespace ac
{

struct UnitComponentConfig_t;
class Military;
class UnitComponentRegistry;
class UnitSlotRegistry;
class UnitManager;
class UnitDesign;

class UnitDesignerView : public IGameView
{
public:
    UnitDesignerView(
        Military& rMilitary,
        const UnitComponentRegistry& rComponentRegistry,
        const UnitSlotRegistry& rSlotRegistry,
        const UnitManager* pUnitManager,
        WindowLayout_t layout
    );
    ~UnitDesignerView() override = default;

    bool HandleKey(const KeyEvent_t& rEvent) override;

private:
    void ShowComponentSelector_(
        const std::string& rComponentType,
        std::function<void(const UnitComponentConfig_t&)> onSelected
    );
    void HandleSaveDesign_();

    Military& m_rMilitary;
    const UnitComponentRegistry& m_rComponentRegistry;
    const UnitSlotRegistry& m_rSlotRegistry;
    const UnitDesign* m_pSelectedDesign = nullptr;
    UnitDesignerState_t m_state;
};

} // namespace ac
