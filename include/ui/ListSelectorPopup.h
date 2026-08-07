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

// The one modal list-selector: a title, a column of rows, click or Escape.
//
// Rows are plain strings and selection reports an index, so callers keep their own payload
// vector and index into it. Every popup that offers "pick one of these" uses this, which is what
// keeps dismiss, hit-testing, clipping and the empty-list message from drifting apart.
class ListSelectorPopup : public UIElement
{
public:
    // rStyle lets a screen keep its own colours and metrics without a second widget; it must
    // outlive the popup (they are all members of the loaded UiStyle).
    // Throws if onSelected is empty: a selector whose click does nothing is a programmer error,
    // and silently swallowing the click leaves the popup open with no feedback.
    ListSelectorPopup(std::string title,
                      std::string emptyMessage,
                      std::vector<std::string> rows,
                      WindowLayout_t layout,
                      std::function<void(size_t)> onSelected,
                      const ListSelectorPopupStyle& rStyle);

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
    std::vector<std::string> m_rows;
    std::function<void(size_t)> m_onSelected;
    const ListSelectorPopupStyle& m_rStyle;

    // Index of the first row drawn; the arrow keys move it.
    size_t m_scrollOffset = 0;
    std::vector<Rectangle_t> m_entryRects;
};

} // namespace ac
