#ifdef USE_SFML

#include "graphics/Graphics.h"
#include "input/KeyMapping.h"
#include "input/SFMLKeyEventQueue.h"
#include "input/SFMLMouseEventQueue.h"
#include <SFML/Graphics.hpp>
#include <iostream>
#include <memory>
#include <optional>
#include <unordered_map>

namespace ac
{

static const std::string k_fontPath1 = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
static const std::string k_fontPath2 = "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf";

class SFMLGraphics : public Graphics
{
public:
    SFMLGraphics()
: m_window(sf::VideoMode(sf::Vector2u(800, 600)), "Alpha Centauri")
        {
        std::cout << "[SFMLGraphics] Creating window...\n";
        m_window.setFramerateLimit(60);
        m_window.setKeyRepeatEnabled(false);
        m_window.setVisible(true);
        m_window.requestFocus();
        std::cout << "[SFMLGraphics] Window created, isOpen=" << m_window.isOpen() << "\n";
if (!m_font.openFromFile(k_fontPath1))
        {
if (!m_font.openFromFile(k_fontPath2))
            {
                std::cerr << "[SFMLGraphics] Font loading failed.\n";
            }
        }
    }

bool Initialize() override
    {
        std::cout << "[SFMLGraphics] Initialize() called, isOpen=" << m_window.isOpen() << "\n";
if (!m_window.isOpen())
        {
            std::cerr << "[Graphics] Failed to create SFML render window.\n";
            return false;
        }
        return true;
    }

void Clear() override
    {
        m_window.clear(sf::Color::Black);
    }

void Display() override
    {
        ProcessEvents_();
        m_window.display();
    }

bool LoadTexture(const std::string& id, const std::string& path) override
    {
        sf::Texture texture;
if (!texture.loadFromFile(path))
        {
            std::cerr << "[Graphics] Failed to load texture '" << path << "'.\n";
            return false;
        }
        m_textures.emplace(id, std::move(texture));
        return true;
    }

bool DrawSprite(const std::string& textureId, float x, float y) override
    {
        auto it = m_textures.find(textureId);
if (it == m_textures.end())
        {
            std::cerr << "[Graphics] Texture '" << textureId << "' is not loaded.\n";
            return false;
        }

        sf::Sprite sprite(it->second);
        sprite.setPosition({x, y});
        m_window.draw(sprite);
        return true;
    }

void DrawText(const std::string& text, float x, float y, unsigned int size = 24, const Color& color = Color::White()) override
    {
if (m_font.getInfo().family.empty())
        {
            return;
        }
        sf::Text drawable(m_font, text, size);
        drawable.setFillColor(sf::Color(color.r, color.g, color.b, color.a));
        drawable.setPosition({x, y});
        m_window.draw(drawable);
    }

void DrawRect(float x, float y, float width, float height, const Color& color = Color::White(), float thickness = 1.0f) override
    {
        sf::RectangleShape rect(sf::Vector2f(width, height));
        rect.setPosition(sf::Vector2f(x, y));
        rect.setFillColor(sf::Color::Transparent);
        rect.setOutlineColor(sf::Color(color.r, color.g, color.b, color.a));
        rect.setOutlineThickness(thickness);
        m_window.draw(rect);
    }

private:
void ProcessEvents_()
    {
while (auto event = m_window.pollEvent())
        {
if (event->is<sf::Event::Closed>())
            {
                // Ignore the close button: only Enter should close the window.
                continue;
            }

if (auto keyEvent = event->getIf<sf::Event::KeyPressed>())
            {
if (auto mapped = KeyFromSfKey(keyEvent->code))
                {
                    PushPendingKeyEvent(*mapped);
                }
            }

if (auto mouseEvent = event->getIf<sf::Event::MouseButtonPressed>())
            {
                MouseButton mb = MouseButton::None;
                if (mouseEvent->button == sf::Mouse::Button::Left) mb = MouseButton::Left;
                else if (mouseEvent->button == sf::Mouse::Button::Right) mb = MouseButton::Right;
                else if (mouseEvent->button == sf::Mouse::Button::Middle) mb = MouseButton::Middle;
                PushPendingMouseEvent(MouseEvent{mb, static_cast<int>(mouseEvent->position.x), static_cast<int>(mouseEvent->position.y)});
            }
        }
    }

    sf::RenderWindow m_window;
    sf::Font m_font;
    std::unordered_map<std::string, sf::Texture> m_textures;
};

std::unique_ptr<Graphics> CreateGraphics()
{
    return std::make_unique<SFMLGraphics>();
}

} // namespace ac

#endif // USE_SFML
