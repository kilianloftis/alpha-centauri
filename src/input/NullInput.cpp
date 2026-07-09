#include "input/Input.h"
#include "input/KeyMapping.h"
#include <iostream>
#include <limits>
#include <string>

namespace ac
{

class NullInput : public Input
{
public:
    NullInput()
    {
        std::cout << "[Input] Console input backend selected.\n";
    }

    std::optional<Key_t> CaptureKey() override
    {
        std::cout << "Press a key and press Enter: ";
        char raw = 0;
        if (!(std::cin >> raw))
        {
            return std::nullopt;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        return KeyFromAscii(raw);
    }

    void CaptureKeyAsync(std::function<void(KeyEvent_t)> callback) override
    {
        if (auto key = CaptureKey())
        {
            callback(KeyEvent_t{*key});
        }
        else
        {
            callback(KeyEvent_t{Key_t::Unknown});
        }
    }

    std::optional<MouseEvent_t> CaptureMouse() override
    {
        std::cout << "Enter mouse click as 'x y' (or 'n' to skip): ";
        std::string line;
        if (!std::getline(std::cin, line))
        {
            return std::nullopt;
        }
        if (line.empty())
        {
            // if previous >> left newline, try again
            if (!std::getline(std::cin, line))
            {
                return std::nullopt;
            }
        }
        if (!line.empty() && (line[0] == 'n' || line[0] == 'N'))
        {
            return std::nullopt;
        }
        int x = 0, y = 0;
        if (sscanf(line.c_str(), "%d %d", &x, &y) == 2)
        {
            return MouseEvent_t{MouseButton_t::Left, x, y};
        }
        return std::nullopt;
    }

    void CaptureMouseAsync(std::function<void(MouseEvent_t)> callback) override
    {
        if (auto m = CaptureMouse())
        {
            callback(*m);
        }
        else
        {
            callback(MouseEvent_t{MouseButton_t::None, 0, 0});
        }
    }

};

std::unique_ptr<Input> CreateInput()
{
    return std::make_unique<NullInput>();
}

} // namespace ac
