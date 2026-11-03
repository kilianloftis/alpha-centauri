#include "ui/text/ScriptMarkup.h"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace ac
{

namespace
{

constexpr std::string_view k_Digits = "0123456789";

bool IsDigit_(char c)
{
    return c >= '0' && c <= '9';
}

std::size_t SkipSpaces_(std::string_view text, std::size_t pos)
{
    while (pos < text.size() && text[pos] == ' ')
    {
        ++pos;
    }
    return pos;
}

std::string_view Trim_(std::string_view text)
{
    const std::size_t first = text.find_first_not_of(" \t");
    if (first == std::string_view::npos)
    {
        return {};
    }
    const std::size_t last = text.find_last_not_of(" \t");
    return text.substr(first, last - first + 1);
}

struct FormGrammar_t
{
    GrammaticalGender_t gender = GrammaticalGender_t::Masculine;
    bool bPlural = false;
};

// parse_string 0x625BF9: two forms by number, three by gender, four by masculine-or-not and
// number, six by gender then number; any other count takes the last form.
int FormIndex_(int colons, const FormGrammar_t& rGrammar)
{
    const int gender = static_cast<int>(rGrammar.gender);
    const int plural = rGrammar.bPlural ? 1 : 0;
    switch (colons)
    {
    case 1:
        return plural;
    case 2:
        return gender;
    case 3:
        return (gender == 0 ? 0 : 1) + 2 * plural;
    case 5:
        return gender + 3 * plural;
    default:
        return colons;
    }
}

int ColonsIn_(std::string_view text)
{
    return static_cast<int>(std::ranges::count(text, ':'));
}

std::string_view FormAt_(std::string_view forms, int index)
{
    std::size_t pos = 0;
    while (index > 0 && pos < forms.size())
    {
        if (forms[pos] == ':')
        {
            --index;
        }
        ++pos;
    }
    const std::size_t end = forms.find_first_of(":>", pos);
    return forms.substr(pos, end == std::string_view::npos ? std::string_view::npos : end - pos);
}

// One pass of parse_string over a template.
class ScriptFormatter_
{
public:
    ScriptFormatter_(const std::string& rTemplate, const IScriptSlots& rSlots)
        : m_template(rTemplate)
        , m_rSlots(rSlots)
    {
    }

    std::string Run()
    {
        std::size_t from = 0;
        while (from != std::string_view::npos)
        {
            const std::size_t dollar = m_template.find('$', from);
            if (dollar == std::string_view::npos)
            {
                m_out += m_template.substr(from);
                break;
            }
            m_out += m_template.substr(from, dollar - from);
            from = Token_(dollar);
        }
        return std::move(m_out);
    }

private:
    // Returns where scanning resumes, or npos when the rest was copied as written.
    std::size_t Token_(std::size_t dollar)
    {
        const std::string_view rest = m_template.substr(dollar);
        const char next = rest.size() > 1 ? rest[1] : '\0';
        if (next == '$')
        {
            m_out += '$';
            return dollar + 2;
        }
        if (next == '<')
        {
            return Form_(dollar);
        }
        if (rest.starts_with("$NUMBER"))
        {
            return Number_(dollar, 7);
        }
        if (rest.starts_with("$NUM") || rest.starts_with("$HEX"))
        {
            return Number_(dollar, 4);
        }
        if (rest.starts_with("$LINK<"))
        {
            m_out += '$';
            return dollar + 1;
        }
        return Slot_(dollar);
    }

    std::size_t Number_(std::size_t dollar, std::size_t prefixLength)
    {
        const std::size_t digit = dollar + prefixLength;
        const int value = m_rSlots.Number(SlotDigit_(digit));
        m_out += std::to_string(value);
        return digit + 1;
    }

    // `$WORDn`: the slot is the first digit after the `$`, wherever it is (findnum).
    std::size_t Slot_(std::size_t dollar)
    {
        const std::size_t digit = m_template.find_first_of(k_Digits, dollar);
        if (digit == std::string_view::npos)
        {
            m_out += m_template.substr(dollar);
            return std::string_view::npos;
        }
        m_out += m_rSlots.String(m_template[digit] - '0').text;
        return digit + 1;
    }

    std::size_t Form_(std::size_t dollar)
    {
        const std::size_t close = m_template.find('>', dollar);
        if (close == std::string_view::npos)
        {
            return dollar + 1;
        }
        std::size_t pos = SkipSpaces_(m_template, dollar + 2);
        const FormGrammar_t grammar = FormSpec_(pos);
        pos = SkipSpaces_(m_template, pos);
        if (pos < m_template.size() && m_template[pos] == ':')
        {
            ++pos;
        }
        m_out += SelectForm_(pos, close, grammar);
        return close + 1;
    }

    // Reads the spec at rPos and leaves rPos after it.
    FormGrammar_t FormSpec_(std::size_t& rPos)
    {
        const char spec = rPos < m_template.size() ? m_template[rPos] : '\0';
        ++rPos;
        if (spec == '#')
        {
            const int slot = SlotDigit_(rPos);
            ++rPos;
            return FormGrammar_t{GrammaticalGender_t::Masculine, m_rSlots.Number(slot) != 1};
        }
        if (IsDigit_(spec))
        {
            const ScriptValue_t& rValue = m_rSlots.String(spec - '0');
            return FormGrammar_t{rValue.gender, rValue.bPlural};
        }
        FormGrammar_t grammar{ExplicitGender_(spec), false};
        if (rPos < m_template.size() && IsDigit_(m_template[rPos]))
        {
            grammar.bPlural = m_template[rPos] != '1';
            ++rPos;
        }
        return grammar;
    }

    GrammaticalGender_t ExplicitGender_(char spec) const
    {
        switch (spec)
        {
        case 'M':
        case 'm':
            return GrammaticalGender_t::Masculine;
        case 'F':
        case 'f':
            return GrammaticalGender_t::Feminine;
        case 'N':
        case 'n':
            return GrammaticalGender_t::Neuter;
        default:
            throw std::runtime_error("FormatScript: unknown form spec in '"
                                     + std::string(m_template) + "'");
        }
    }

    // With no ':' before the '>', the forms are the first named slot's own text.
    std::string SelectForm_(std::size_t pos, std::size_t close, const FormGrammar_t& rGrammar)
    {
        std::string_view forms = m_template.substr(pos);
        int colons = ColonsIn_(m_template.substr(pos, close - pos));
        if (colons == 0)
        {
            const std::size_t digit = m_template.find_first_of(k_Digits, pos);
            if (digit != std::string_view::npos)
            {
                forms = m_rSlots.String(m_template[digit] - '0').text;
                colons = ColonsIn_(forms);
            }
        }
        const int index = FormIndex_(colons, rGrammar);
        return std::string(FormAt_(forms, index));
    }

    int SlotDigit_(std::size_t pos) const
    {
        if (pos >= m_template.size() || !IsDigit_(m_template[pos]))
        {
            throw std::runtime_error("FormatScript: expected a slot digit in '"
                                     + std::string(m_template) + "'");
        }
        return m_template[pos] - '0';
    }

    std::string_view m_template;
    const IScriptSlots& m_rSlots;
    std::string m_out;
};

// Answers every slot with an empty value and remembers which were read.
class SlotRecorder_ : public IScriptSlots
{
public:
    const ScriptValue_t& String(int slot) const override
    {
        m_slots.strings.insert(slot);
        return m_empty;
    }

    int Number(int slot) const override
    {
        m_slots.numbers.insert(slot);
        return 0;
    }

    const ScriptSlots_t& Slots() const { return m_slots; }

private:
    mutable ScriptSlots_t m_slots;
    ScriptValue_t m_empty;
};

} // namespace

void ScriptArgs::SetString(int slot, ScriptValue_t value)
{
    m_strings[slot] = std::move(value);
}

void ScriptArgs::SetNumber(int slot, int value)
{
    m_numbers[slot] = value;
}

const ScriptValue_t& ScriptArgs::String(int slot) const
{
    const auto it = m_strings.find(slot);
    if (it == m_strings.end())
    {
        throw std::runtime_error("ScriptArgs: string slot " + std::to_string(slot)
                                 + " is not set");
    }
    return it->second;
}

int ScriptArgs::Number(int slot) const
{
    const auto it = m_numbers.find(slot);
    if (it == m_numbers.end())
    {
        throw std::runtime_error("ScriptArgs: number slot " + std::to_string(slot)
                                 + " is not set");
    }
    return it->second;
}

std::string FormatScript(const std::string& rTemplate, const IScriptSlots& rSlots)
{
    return ScriptFormatter_(rTemplate, rSlots).Run();
}

ScriptSlots_t SlotsOf(const std::string& rTemplate)
{
    const SlotRecorder_ recorder;
    FormatScript(rTemplate, recorder);
    return recorder.Slots();
}

std::vector<TextRun_t> HighlightRuns(const std::string& rText)
{
    std::vector<TextRun_t> runs;
    TextRun_t current;
    for (const char c : rText)
    {
        if (c == '{' || c == '}')
        {
            if (!current.text.empty())
            {
                runs.push_back(current);
            }
            current = TextRun_t{"", c == '{'};
            continue;
        }
        current.text += c;
    }
    if (!current.text.empty())
    {
        runs.push_back(current);
    }
    return runs;
}

std::string WithoutHighlights(const std::string& rText)
{
    std::string plain = rText;
    std::erase_if(plain, [](char c) { return c == '{' || c == '}'; });
    return plain;
}

ScriptValue_t NounPhrase(const std::string& rLine)
{
    const std::string_view line = rLine;
    const std::size_t comma = line.find(',');
    const std::string_view text = Trim_(line.substr(0, comma));
    const std::string_view tag =
        comma == std::string_view::npos ? std::string_view{} : Trim_(line.substr(comma + 1));
    ScriptValue_t value{std::string(text), GrammaticalGender_t::Masculine, false};
    if (!tag.empty() && (tag[0] == 'F' || tag[0] == 'f'))
    {
        value.gender = GrammaticalGender_t::Feminine;
    }
    if (!tag.empty() && (tag[0] == 'N' || tag[0] == 'n'))
    {
        value.gender = GrammaticalGender_t::Neuter;
    }
    value.bPlural = tag.size() > 1 && tag[1] == '2';
    return value;
}

} // namespace ac
