#pragma once

#include <functional>
#include <string>
#include <unordered_map>

namespace ac
{

class Graphics;

// Loads each sprite path into the Graphics backend once. A path that fails to load is logged
// once and reported missing from then on.
class SpriteLibrary
{
public:
    using FileExists_t = std::function<bool(const std::string&)>;

    SpriteLibrary(Graphics& rGraphics, FileExists_t fileExists);

    bool Ensure(const std::string& path);

private:
    Graphics& m_rGraphics;
    FileExists_t m_fileExists;
    std::unordered_map<std::string, bool> m_loaded;
};

} // namespace ac
