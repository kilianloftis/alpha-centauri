#include "ui/SpriteLibrary.h"

#include "graphics/Graphics.h"

#include <iostream>
#include <stdexcept>
#include <utility>

namespace ac
{

SpriteLibrary::SpriteLibrary(Graphics& rGraphics, FileExists_t fileExists)
    : m_rGraphics(rGraphics)
    , m_fileExists(std::move(fileExists))
{
    if (!m_fileExists)
    {
        throw std::invalid_argument("SpriteLibrary: fileExists callback is required");
    }
}

bool SpriteLibrary::Ensure(const std::string& path)
{
    if (path.empty())
    {
        return false;
    }

    const auto found = m_loaded.find(path);
    if (found != m_loaded.end())
    {
        return found->second;
    }

    const bool bLoaded = m_fileExists(path) && m_rGraphics.LoadTexture(path, path);
    if (!bLoaded)
    {
        std::cerr << "[SpriteLibrary] sprite '" << path << "' is missing or failed to load\n";
    }
    m_loaded.emplace(path, bLoaded);
    return bLoaded;
}

} // namespace ac
