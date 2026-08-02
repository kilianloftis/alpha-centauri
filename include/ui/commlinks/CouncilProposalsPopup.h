#pragma once

#include "ui/UIElement.h"
#include "input/Input.h"
#include <functional>
#include <vector>

namespace ac
{

struct CouncilProposalConfig_t;

// Lists proposals the player can currently put before the Planetary Council.
class CouncilProposalsPopup : public UIElement
{
public:
    CouncilProposalsPopup(
        std::vector<const CouncilProposalConfig_t*> proposals,
        WindowLayout_t layout,
        std::function<void(const CouncilProposalConfig_t&)> onProposalSelected
    );

    ~CouncilProposalsPopup() override = default;

    void Render(Graphics& rGraphics) override;

    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    std::vector<const CouncilProposalConfig_t*> m_proposals;
    std::vector<Rectangle_t> m_entryRects;
    std::function<void(const CouncilProposalConfig_t&)> m_onProposalSelected;

    void CacheEntryRects_();
};

} // namespace ac
