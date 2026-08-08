#pragma once

#include "graphics/Graphics.h"

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
    };

    struct RectDraw_t
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        bool bFilled = false;
    };

    void PumpEvents() override {}
    void Clear() override { ++clearCount; }
    void Display() override { ++displayCount; }

    bool LoadTexture(const std::string&, const std::string&) override { return true; }
    bool DrawSprite(const std::string&, float, float) override { return true; }

    void DrawText(const std::string& rText, float x, float y, unsigned int size,
                  const ac::Color_t&) override
    {
        texts.push_back(TextDraw_t{rText, x, y, size});
    }

    void DrawRect(float x, float y, float width, float height, const ac::Color_t&, float) override
    {
        rects.push_back(RectDraw_t{x, y, width, height, false});
    }

    void DrawFilledRect(float x, float y, float width, float height, const ac::Color_t&) override
    {
        rects.push_back(RectDraw_t{x, y, width, height, true});
    }

    void DrawLine(float, float, float, float, const ac::Color_t&, float) override {}

    unsigned int GetWindowWidth() const override { return 1280; }
    unsigned int GetWindowHeight() const override { return 900; }

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
    int clearCount = 0;
    int displayCount = 0;
};

} // namespace actest
