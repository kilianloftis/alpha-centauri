#include "ui/TextWrap.h"

#include "graphics/Graphics.h"

namespace ac
{

namespace
{

struct Word_t
{
    std::string text;
    bool bHighlight = false;
    bool bLineBreak = false;
};

// Words in reading order, a line break as a word of its own; spaces only separate.
std::vector<Word_t> WordsOf_(const std::vector<TextRun_t>& rRuns)
{
    std::vector<Word_t> words;
    for (const TextRun_t& rRun : rRuns)
    {
        std::string word;
        for (const char c : rRun.text + ' ')
        {
            if (c != ' ' && c != '\n')
            {
                word += c;
                continue;
            }
            if (!word.empty())
            {
                words.push_back({word, rRun.bHighlight, false});
                word.clear();
            }
            if (c == '\n')
            {
                words.push_back({"", false, true});
            }
        }
    }
    return words;
}

std::string PlainText_(const TextLine_t& rLine)
{
    std::string text;
    for (const TextRun_t& rRun : rLine)
    {
        text += rRun.text;
    }
    return text;
}

void Append_(TextLine_t& rLine, const Word_t& rWord)
{
    const std::string text = rLine.empty() ? rWord.text : " " + rWord.text;
    if (!rLine.empty() && rLine.back().bHighlight == rWord.bHighlight)
    {
        rLine.back().text += text;
        return;
    }
    rLine.push_back({text, rWord.bHighlight});
}

} // namespace

std::vector<TextLine_t> WrapText(const Graphics& rGraphics, const std::string& rText,
                                 unsigned int fontSize, float width)
{
    std::vector<TextLine_t> lines(1);
    for (const Word_t& rWord : WordsOf_(HighlightRuns(rText)))
    {
        if (rWord.bLineBreak)
        {
            lines.emplace_back();
            continue;
        }
        TextLine_t candidate = lines.back();
        Append_(candidate, rWord);
        const bool bFits = rGraphics.MeasureTextWidth(PlainText_(candidate), fontSize) <= width;
        if (bFits || lines.back().empty())
        {
            lines.back() = std::move(candidate);
            continue;
        }
        lines.push_back({{rWord.text, rWord.bHighlight}});
    }
    return lines;
}

} // namespace ac
