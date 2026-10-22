#pragma once

#include "input/Input.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

class Graphics;
struct KeyEvent_t;

struct Rectangle_t
{
    float x;
    float y;
    float width;
    float height;
};

using RatioLayout_t = Rectangle_t;
using WindowLayout_t = Rectangle_t;

// Native-size chrome: pixel size plus alignment inside a parent rect. Variants are
// alternatives for different parent widths (widest that fits, else the narrowest).
struct FixedLayoutVariant_t
{
    float width{};
    float height{};
    // Map (or other content above) extends this many pixels into the fixed rect from its
    // top — the top-silhouette valley of the console art (corner towers sit above it).
    float mapOverlap{};
    // Draw the sprite at layout.x + spriteOffsetX (negative when the PNG has side bars).
    float spriteOffsetX{};
    std::string sprite{};
};

struct FixedLayout_t
{
    float alignX{};
    float alignY{};
    std::vector<FixedLayoutVariant_t> variants{};
};

struct FixedPlacement_t
{
    WindowLayout_t layout{};
    std::string sprite{};
    float mapOverlap{};
    float spriteOffsetX{};
};

inline bool ContainsMouseCoord(const Rectangle_t& rRect, float x, float y)
{
    return x >= rRect.x && x < rRect.x + rRect.width
        && y >= rRect.y && y < rRect.y + rRect.height;
}

inline bool ContainsMouseCoord(const Rectangle_t& rRect, const MouseEvent_t& rEvent)
{
    return ContainsMouseCoord(rRect, static_cast<float>(rEvent.x), static_cast<float>(rEvent.y));
}

inline WindowLayout_t ResolveLayout(const WindowLayout_t& windowLayout, const RatioLayout_t& ratioLayout)
{
    if (ratioLayout.width > 1.0f || ratioLayout.height > 1.0f || ratioLayout.x > 1.0f || ratioLayout.y > 1.0f) {
        throw std::runtime_error("Invalid ratio layout");
    }
    return {
            windowLayout.x + ratioLayout.x * windowLayout.width,
            windowLayout.y + ratioLayout.y * windowLayout.height,
            ratioLayout.width  * windowLayout.width,
            ratioLayout.height * windowLayout.height
        };
}

inline FixedPlacement_t PlaceFixedLayout(const WindowLayout_t& rParent, const FixedLayout_t& rSpec)
{
    if (rSpec.variants.empty())
    {
        throw std::runtime_error("Fixed layout has no variants");
    }

    const FixedLayoutVariant_t* pChosen = nullptr;
    for (const FixedLayoutVariant_t& rVariant : rSpec.variants)
    {
        if (rVariant.width <= rParent.width
            && (!pChosen || rVariant.width > pChosen->width))
        {
            pChosen = &rVariant;
        }
    }
    if (!pChosen)
    {
        pChosen = &rSpec.variants.front();
        for (const FixedLayoutVariant_t& rVariant : rSpec.variants)
        {
            if (rVariant.width < pChosen->width)
            {
                pChosen = &rVariant;
            }
        }
    }

    return {
        WindowLayout_t{
            rParent.x + rSpec.alignX * (rParent.width - pChosen->width),
            rParent.y + rSpec.alignY * (rParent.height - pChosen->height),
            pChosen->width,
            pChosen->height},
        pChosen->sprite,
        pChosen->mapOverlap,
        pChosen->spriteOffsetX};
}

// A full-width band from the parent top down to rBelow.y + overlapIntoBelow.
inline WindowLayout_t MapBandAbove(const WindowLayout_t& rParent,
                                   const WindowLayout_t& rBelow,
                                   float overlapIntoBelow)
{
    const float bottom = rBelow.y + overlapIntoBelow;
    const float height = bottom - rParent.y;
    return {rParent.x, rParent.y, rParent.width, height > 0.0f ? height : 0.0f};
}

inline WindowLayout_t MapBandAbove(const WindowLayout_t& rParent, const FixedLayout_t& rSpec)
{
    const FixedPlacement_t placed = PlaceFixedLayout(rParent, rSpec);
    return MapBandAbove(rParent, placed.layout, placed.mapOverlap);
}

// Shared chrome layouts (map, dashboard columns, popups) live in config/ui/style.json
// under "layouts" and are accessed via Style().layouts.* after UiStyle::Load.

class UIElement
{
public:
    explicit UIElement(WindowLayout_t layout)
        : m_layout(layout)
    {}
    virtual ~UIElement() = default;

    virtual void Render(Graphics& rGraphics) = 0;

    bool Contains(float x, float y) const
    {
        return x >= m_layout.x && x < m_layout.x + m_layout.width
            && y >= m_layout.y && y < m_layout.y + m_layout.height;
    }

    virtual void HandleMouseClick(const MouseEvent_t& rEvent) {}
    virtual bool HandleKey(const KeyEvent_t& rEvent) { return false; }

    bool ShouldClose() const { return m_bShouldClose; }
    void RequestClose() { m_bShouldClose = true; }

    // A modal element captures every key and mouse press while open, and blocks turn advance.
    virtual bool IsModal() const { return false; }

protected:
    const WindowLayout_t m_layout;
    bool m_bShouldClose = false;
};

} // namespace ac
