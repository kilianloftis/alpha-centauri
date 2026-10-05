#pragma once

#include "graphics/Graphics.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace actest
{

// A Graphics that records what was drawn and *where*. The earlier stub discarded coordinates,
// which is why a popup drawing its overflow indicator on top of its last row passed every test.
class RecordingGraphics : public ac::Graphics
{
public:
    struct TextDraw_t
    {
        std::string text;
        float x = 0.0f;
        float y = 0.0f;
        unsigned int size = 0;
        ac::Color_t color{};
    };

    struct RectDraw_t
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        bool bFilled = false;
        ac::Color_t color{};
        // Position among every sprite and rect drawn, so the two lists can be ordered.
        std::size_t order = 0;
    };

    struct LineDraw_t
    {
        float x1 = 0.0f;
        float y1 = 0.0f;
        float x2 = 0.0f;
        float y2 = 0.0f;
        ac::Color_t color{};
    };

    struct SpriteDraw_t
    {
        std::string textureId;
        float x = 0.0f;
        float y = 0.0f;
        float destWidth = 0.0f;
        float destHeight = 0.0f;
        ac::Color_t tint = ac::Color_t::White();
        bool bScaled = false;
        std::optional<ac::DiamondTint_t> diamondTint;
        std::size_t order = 0;
    };

    void PumpEvents() override {}
    void Clear() override { ++clearCount; }
    void Display() override { ++displayCount; }
    void PaceFrame() override { ++paceCount; }

    bool LoadTexture(const std::string&, const std::string&) override { return true; }
    bool UpsertTextureRGBA(const std::string&, unsigned int, unsigned int,
                           const std::uint8_t*) override
    {
        ++upsertTextureCount;
        return true;
    }
    bool DrawSprite(const std::string& textureId, float x, float y) override
    {
        sprites.push_back(SpriteDraw_t{textureId, x, y, 0.0f, 0.0f, ac::Color_t::White(), false,
                                       std::nullopt, drawCount++});
        ++spriteCount;
        return true;
    }
    bool DrawSprite(const std::string& textureId, float x, float y, float destWidth,
                    float destHeight) override
    {
        return DrawSprite(textureId, x, y, destWidth, destHeight, ac::Color_t::White());
    }
    bool DrawSprite(const std::string& textureId, float x, float y, float destWidth,
                    float destHeight, const ac::Color_t& tint) override
    {
        sprites.push_back(SpriteDraw_t{textureId, x, y, destWidth, destHeight, tint, true,
                                       std::nullopt, drawCount++});
        ++spriteCount;
        return true;
    }
    bool DrawDiamondSprite(const std::string& textureId, float x, float y, float destWidth,
                           float destHeight, const ac::DiamondTint_t& rTint) override
    {
        sprites.push_back(SpriteDraw_t{textureId, x, y, destWidth, destHeight, rTint.center, true,
                                       rTint, drawCount++});
        ++spriteCount;
        return true;
    }

    void DrawText(const std::string& rText, float x, float y, unsigned int size,
                  const ac::Color_t& rColor) override
    {
        texts.push_back(TextDraw_t{rText, x, y, size, rColor});
    }

    void DrawRect(float x, float y, float width, float height, const ac::Color_t& rColor,
                  float) override
    {
        rects.push_back(RectDraw_t{x, y, width, height, false, rColor, drawCount++});
    }

    void DrawFilledRect(float x, float y, float width, float height,
                        const ac::Color_t& rColor) override
    {
        rects.push_back(RectDraw_t{x, y, width, height, true, rColor, drawCount++});
    }

    void DrawFilledDiamond(float x, float y, float width, float height,
                           const ac::Color_t& rColor) override
    {
        // Record as a filled rect of the diamond AABB so color-at helpers keep working.
        rects.push_back(RectDraw_t{x, y, width, height, true, rColor, drawCount++});
    }

    void DrawDiamond(float x, float y, float width, float height, const ac::Color_t& rColor,
                     float) override
    {
        rects.push_back(RectDraw_t{x, y, width, height, false, rColor, drawCount++});
    }

    void DrawLine(float x1, float y1, float x2, float y2, const ac::Color_t& rColor,
                  float) override
    {
        lines.push_back(LineDraw_t{x1, y1, x2, y2, rColor});
    }

    unsigned int GetWindowWidth() const override { return 1280; }
    unsigned int GetWindowHeight() const override { return 900; }

    bool SetMouseCursor(const std::string&, unsigned int, unsigned int) override { return false; }
    void ResetMouseCursor() override {}

    // Every y a given string was drawn at, in draw order.
    std::vector<float> TextYs(const std::string& rText) const
    {
        std::vector<float> ys;
        for (const TextDraw_t& rDraw : texts)
        {
            if (rDraw.text == rText)
            {
                ys.push_back(rDraw.y);
            }
        }
        return ys;
    }

    // True if any drawn string contains rNeedle.
    bool AnyTextContaining(const std::string& rNeedle) const
    {
        for (const TextDraw_t& rDraw : texts)
        {
            if (rDraw.text.find(rNeedle) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }

    // The fill colour of the filled rect at (x, y), if one was drawn there. Selection state is
    // often nothing but a fill colour, so a stub that dropped colours could not see it.
    std::optional<ac::Color_t> FillColorAt(float x, float y) const
    {
        for (const RectDraw_t& rDraw : rects)
        {
            if (rDraw.bFilled && rDraw.x == x && rDraw.y == y)
            {
                return rDraw.color;
            }
        }
        return std::nullopt;
    }

    // The y of the first drawn string containing rNeedle, or -1 if none.
    float FirstTextYContaining(const std::string& rNeedle) const
    {
        for (const TextDraw_t& rDraw : texts)
        {
            if (rDraw.text.find(rNeedle) != std::string::npos)
            {
                return rDraw.y;
            }
        }
        return -1.0f;
    }

    std::vector<TextDraw_t> texts;
    std::vector<RectDraw_t> rects;
    std::vector<SpriteDraw_t> sprites;
    std::vector<LineDraw_t> lines;
    int clearCount = 0;
    int displayCount = 0;
    int paceCount = 0;
    int upsertTextureCount = 0;
    int spriteCount = 0;
    std::size_t drawCount = 0;
};

} // namespace actest
