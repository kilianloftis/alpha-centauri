#include "ui/social-engineering/SocialEngineeringView.h"

#include "ui/ListSelectorPopup.h"
#include "ui/social-engineering/SocialEngineeringDisplay.h"
#include "ui/social-engineering/SocialEngineeringBottomPanel.h"
#include "ui/style/UiStyle.h"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace ac
{

SocialEngineeringView::SocialEngineeringView(
    Faction& rFaction,
    const SocialPolicyRegistry& rPolicyRegistry,
    const SocialRatingRegistry& rRatingRegistry,
    WindowLayout_t layout
)
    : IGameView(layout)
{
    auto pDisplay = std::make_unique<SocialEngineeringDisplay>(
        rFaction,
        rPolicyRegistry,
        rRatingRegistry,
        ResolveLayout(m_layout, Style().layouts.topPanel),
        [this](std::string message, std::function<void()> onOk) {
            Confirm_(std::move(message), std::move(onOk));
        },
        [this](std::string message) { Notice_(std::move(message)); },
        [this]() { m_bShouldClose = true; }
    );
    m_pDisplay = pDisplay.get();
    m_elements.push_back(std::move(pDisplay));
    m_elements.push_back(std::make_unique<SocialEngineeringBottomPanel>(
        rFaction,
        ResolveLayout(m_layout, Style().layouts.bottomPanel)
    ));
}

bool SocialEngineeringView::HandleKey(const KeyEvent_t& rEvent)
{
    if (IGameView::HandleKey(rEvent))
    {
        return true;
    }

    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }

    if (rEvent.key == Key_t::Enter)
    {
        m_pDisplay->TryCommitDraft();
        return true;
    }

    return false;
}

void SocialEngineeringView::Notice_(std::string message)
{
    DismissOpenModals_();
    std::vector<PopupChoice_t> choices;
    choices.push_back({"OK", [] {}});
    m_elements.push_back(std::make_unique<ListSelectorPopup>(
        std::move(message), "", std::move(choices), GetPopupLayout(), Style().listSelectorPopup,
        nullptr));
}

} // namespace ac
