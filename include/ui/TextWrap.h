#pragma once

#include "ui/text/ScriptMarkup.h"

#include <string>
#include <vector>

namespace ac
{

class Graphics;

using TextLine_t = std::vector<TextRun_t>;

// Script text in lines that fit width: SMAC's own line breaks stay, a longer line breaks at
// spaces, and a word wider than width gets a line to itself. Highlighted spans keep their runs.
std::vector<TextLine_t> WrapText(const Graphics& rGraphics, const std::string& rText,
                                 unsigned int fontSize, float width);

} // namespace ac
