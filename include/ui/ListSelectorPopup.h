#pragma once

#include "input/Input.h"
#include "ui/UIElement.h"
#include "ui/style/UiStyle.h"

#include <functional>
#include <string>
#include <vector>

namespace ac
{

class Graphics;
class SpriteLibrary;

// One row: what it reads as, and what it does. Callers that pick from a payload vector bind
// the payload into the action rather than keeping a parallel array and an index.
struct PopupChoice_t
{
    std::string label;
    std::function<void()> onChosen;
    // Optional content icon; drawn when a SpriteLibrary is supplied and Ensure succeeds.
    std::string iconPath;
};

// The one modal list selector (title, choices, click or Escape), so every "pick one" popup
// behaves the same.
class ListSelectorPopup : public UIElement
{
public:
    // rStyle must outlive the popup. Throws if a choice has no action. Without pSprites, choice
    // icons are ignored.
    ListSelectorPopup(std::string title,
                      std::string emptyMessage,
                      std::vector<PopupChoice_t> choices,
                      WindowLayout_t layout,
                      const ListSelectorPopupStyle_t& rStyle,
                      SpriteLibrary* pSprites = nullptr);

    ~ListSelectorPopup() override = default;

    void Render(Graphics& rGraphics) override;
    bool IsModal() const override { return true; }

    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    // Rows that fit the content area below the header. Bounded, so a long list cannot paint
    // past the chrome into space that Contains() does not cover.
    size_t VisibleRowCount_() const;
    void CacheEntryRects_();

    std::string m_title;
    std::string m_emptyMessage;
    std::vector<PopupChoice_t> m_choices;
    const ListSelectorPopupStyle_t& m_rStyle;
    SpriteLibrary* m_pSprites = nullptr;

    // Index of the first row drawn; the arrow keys move it.
    size_t m_scrollOffset = 0;
    std::vector<Rectangle_t> m_entryRects;
};

} // namespace ac
