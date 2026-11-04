#pragma once

#include "input/Input.h"
#include "ui/UIElement.h"
#include "ui/style/UiStyle.h"

#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

namespace ac
{

class Graphics;
class SpriteLibrary;

// Modal list that toggles up to maxSelections rows, then Confirm commits indices and Cancel
// dismisses. Distinct from ListSelectorPopup (one-click rows with per-choice actions).
class MultiSelectListPopup : public UIElement
{
public:
    // rStyle must outlive the popup. Throws if maxSelections is 0 or onConfirm is empty.
    MultiSelectListPopup(std::string title,
                         std::string emptyMessage,
                         std::vector<std::string> labels,
                         size_t maxSelections,
                         std::string confirmLabel,
                         std::function<void(std::vector<size_t>)> onConfirm,
                         WindowLayout_t layout,
                         const ListSelectorPopupStyle_t& rStyle,
                         SpriteLibrary* pSprites);

    ~MultiSelectListPopup() override = default;

    void Render(Graphics& rGraphics) override;
    bool IsModal() const override { return true; }

    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    size_t VisibleRowCount_() const;
    void CacheEntryRects_();
    float Padding_() const;
    unsigned int EntryFontSize_() const;
    float LineHeight_() const;
    void DrawEntries_(Graphics& rGraphics) const;
    void DrawScrollIndicator_(Graphics& rGraphics) const;
    void DrawFooter_(Graphics& rGraphics) const;
    void ToggleSelection_(size_t choiceIndex);
    void Confirm_();

    std::string m_title;
    std::string m_emptyMessage;
    std::vector<std::string> m_labels;
    size_t m_maxSelections = 1;
    std::string m_confirmLabel;
    std::function<void(std::vector<size_t>)> m_onConfirm;
    const ListSelectorPopupStyle_t& m_rStyle;
    SpriteLibrary* m_pSprites = nullptr;
    std::unordered_set<size_t> m_selected;

    size_t m_scrollOffset = 0;
    std::vector<Rectangle_t> m_entryRects;
    Rectangle_t m_confirmRect{};
    Rectangle_t m_cancelRect{};
};

} // namespace ac
