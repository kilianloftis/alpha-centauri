#pragma once

#include "game/effects/EffectEnums.h"
#include "ui/UIElement.h"
#include "input/Input.h"
#include <functional>
#include <string>
#include <vector>

namespace ac
{

class Graphics;

// Small popup listing Nutrients / Minerals / Energy for a supply-crawler order.
class SupplyCrawlPopup : public UIElement
{
public:
    SupplyCrawlPopup(
        WindowLayout_t layout,
        std::function<void(StatId_t)> onResourceSelected
    );

    ~SupplyCrawlPopup() override = default;

    void Render(Graphics& rGraphics) override;

    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    struct Entry_t
    {
        StatId_t resource;
        std::string label;
    };

    std::vector<Entry_t> m_entries;
    std::vector<Rectangle_t> m_entryRects;
    std::function<void(StatId_t)> m_onResourceSelected;

    void CacheEntryRects_();
};

} // namespace ac
