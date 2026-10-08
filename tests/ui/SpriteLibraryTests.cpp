#include "RecordingGraphics.h"

#include "ui/SpriteLibrary.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>
#include <stdexcept>
#include <string>

using namespace ac;

namespace
{

class CountingGraphics_ : public actest::RecordingGraphics
{
public:
    bool LoadTexture(const std::string&, const std::string& path) override
    {
        ++loadCounts[path];
        return !failing.contains(path);
    }

    std::map<std::string, int> loadCounts;
    std::set<std::string> failing;
};

} // namespace

TEST_CASE("SpriteLibrary loads an existing path into the backend once", "[ui][sprites]")
{
    CountingGraphics_ graphics;
    SpriteLibrary library(graphics, [](const std::string& rPath) { return rPath == "a.png"; });

    CHECK(library.Ensure("a.png"));
    CHECK(library.Ensure("a.png"));
    CHECK(library.Ensure("a.png"));

    CHECK(graphics.loadCounts.at("a.png") == 1);
}

TEST_CASE("SpriteLibrary reports a path with no file as missing without loading it",
          "[ui][sprites]")
{
    CountingGraphics_ graphics;
    SpriteLibrary library(graphics, [](const std::string&) { return false; });

    CHECK_FALSE(library.Ensure("gone.png"));
    CHECK_FALSE(library.Ensure("gone.png"));

    CHECK(graphics.loadCounts.empty());
}

TEST_CASE("SpriteLibrary remembers a path the backend failed to load", "[ui][sprites]")
{
    CountingGraphics_ graphics;
    graphics.failing.insert("broken.png");
    SpriteLibrary library(graphics, [](const std::string&) { return true; });

    CHECK_FALSE(library.Ensure("broken.png"));
    CHECK_FALSE(library.Ensure("broken.png"));

    CHECK(graphics.loadCounts.at("broken.png") == 1);
}

TEST_CASE("SpriteLibrary treats an empty path as missing", "[ui][sprites]")
{
    CountingGraphics_ graphics;
    SpriteLibrary library(graphics, [](const std::string&) { return true; });

    CHECK_FALSE(library.Ensure(""));
    CHECK(graphics.loadCounts.empty());
}

TEST_CASE("SpriteLibrary requires a file-exists callback", "[ui][sprites]")
{
    actest::RecordingGraphics graphics;
    CHECK_THROWS_AS(SpriteLibrary(graphics, SpriteLibrary::FileExists_t{}),
                    std::invalid_argument);
}
