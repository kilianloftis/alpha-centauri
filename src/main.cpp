#include <exception>
#include <iostream>
#include "game/Engine.h"

// Top-level error boundary. Config loading, world generation and faction construction all throw
// with a diagnostic naming the file/id/key at fault (that is the project's error policy), so the
// message is the useful artifact — letting it escape main would print an unhandled-exception
// abort and lose it. Nothing here attempts recovery: startup failures are not recoverable, they
// are reportable.
int main()
{
    try
    {
        ac::Engine engine;
        engine.Run();
    }
    catch (const std::exception& rError)
    {
        std::cerr << "Fatal error: " << rError.what() << "\n";
        return 1;
    }
    catch (...)
    {
        std::cerr << "Fatal error: unknown exception\n";
        return 1;
    }
    return 0;
}
