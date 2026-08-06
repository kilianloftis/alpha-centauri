#pragma once

#include "game/units/ProbeActionConfig.h"
#include "ui/UIElement.h"
#include "input/Input.h"
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace ac
{

// Popup listing available probe actions for the selected probe team.
class ProbeActionPopup : public UIElement
{
public:
    ProbeActionPopup(
        WindowLayout_t layout,
        std::vector<std::pair<ProbeActionId_t, std::string>> actions,
        std::function<void(ProbeActionId_t actionId)> onActionSelected
    );

    ~ProbeActionPopup() override = default;

    void Render(Graphics& rGraphics) override;
    bool IsModal() const override { return true; }

    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    std::vector<std::pair<ProbeActionId_t, std::string>> m_actions; // id, label
    std::vector<Rectangle_t> m_entryRects;
    std::function<void(ProbeActionId_t)> m_onActionSelected;

    void CacheEntryRects_();
};

} // namespace ac
