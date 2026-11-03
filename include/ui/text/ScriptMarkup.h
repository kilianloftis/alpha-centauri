#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace ac
{

enum class GrammaticalGender_t
{
    Masculine,
    Feminine,
    Neuter
};

// A string slot's text and the grammar `$<n:…>` forms agree with.
struct ScriptValue_t
{
    std::string text;
    GrammaticalGender_t gender = GrammaticalGender_t::Masculine;
    bool bPlural = false;
};

// What a script line's slots hold, as SMAC's parse_says and parse_num leave them.
class IScriptSlots
{
public:
    virtual ~IScriptSlots() = default;

    virtual const ScriptValue_t& String(int slot) const = 0;
    virtual int Number(int slot) const = 0;
};

// Slots a caller fills; reading one nobody set throws.
class ScriptArgs : public IScriptSlots
{
public:
    void SetString(int slot, ScriptValue_t value);
    void SetNumber(int slot, int value);

    const ScriptValue_t& String(int slot) const override;
    int Number(int slot) const override;

private:
    std::map<int, ScriptValue_t> m_strings;
    std::map<int, int> m_numbers;
};

struct ScriptSlots_t
{
    std::set<int> strings;
    std::set<int> numbers;
};

struct TextRun_t
{
    std::string text;
    bool bHighlight = false;
};

// SMAC's parse_string (terranx-map.md, Talk): `$$`, `$NUMn` / `$NUMBERn` / `$HEXn`, `$LINK<`,
// `$<spec:forms>` and `$WORDn`. Braces pass through for HighlightRuns. Throws on a malformed slot
// or a form spec it does not know.
std::string FormatScript(const std::string& rTemplate, const IScriptSlots& rSlots);

// Every string and number slot FormatScript reads from rTemplate.
ScriptSlots_t SlotsOf(const std::string& rTemplate);

// `{…}` spans, which SMAC's renderer highlights.
std::vector<TextRun_t> HighlightRuns(const std::string& rText);
std::string WithoutHighlights(const std::string& rText);

// A faction-file phrase line as get_noun_phrase reads it: its first comma item, gendered by an
// `M1` / `F2` / `N1` tag in the second.
ScriptValue_t NounPhrase(const std::string& rLine);

} // namespace ac
