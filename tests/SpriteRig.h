#pragma once

#include "graphics/Graphics.h"
#include "ui/SpriteLibrary.h"
#include "ui/TileRenderer.h"
#include "ui/style/UiStyle.h"

#include <set>
#include <string>

namespace actest
{

// A SpriteLibrary whose "files" are the paths in `existing`, and a TileRenderer over it. Add a
// path before anything draws it: the library remembers a path it found missing.
struct SpriteRig
{
    std::set<std::string> existing;
    ac::SpriteLibrary sprites;
    ac::TileRenderer renderer;

    SpriteRig(ac::Graphics& rGraphics, const ac::TileRendererStyle_t& rStyle)
        : sprites(rGraphics, [this](const std::string& rPath) { return existing.contains(rPath); })
        , renderer(sprites, rStyle)
    {
        existing.insert(rStyle.palettePath);
    }

    SpriteRig(const SpriteRig&) = delete;
    SpriteRig& operator=(const SpriteRig&) = delete;
};

} // namespace actest
