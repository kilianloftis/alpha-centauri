#!/usr/bin/env python3
"""Generate UiStyle C++ headers/sources from config/ui/style.json.

config/ui/style.json is the source of truth. Edit values (and add fields/sections)
there, then run this script to regenerate:

  include/ui/style/UiStyle.h
  src/ui/style/UiStyle.cpp

Type inference from JSON values:
  - [r,g,b] / [r,g,b,a] of integers           -> Color_t
  - [x,y,w,h] layout (layouts section, keys
    named layout / *_layout, or float elems) -> RatioLayout_t
  - JSON float                               -> float
  - JSON int, key font_size / *_font_size    -> unsigned int
  - other JSON int                           -> int

Layout arrays should use float literals (e.g. 1.0) so they are not mistaken
for colors when they are not under the layouts section / a *_layout key.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
STYLE_JSON = ROOT / "config" / "ui" / "style.json"
HEADER_PATH = ROOT / "include" / "ui" / "style" / "UiStyle.h"
SOURCE_PATH = ROOT / "src" / "ui" / "style" / "UiStyle.cpp"

# field: (cpp_name, json_key, kind)
Field = tuple[str, str, str]
# section: (cpp_member, json_key, fields)
Section = tuple[str, str, list[Field]]


def snake_to_camel(name: str) -> str:
    parts = name.split("_")
    return parts[0] + "".join(p.title() for p in parts[1:])


def struct_name(member: str) -> str:
    return member[0].upper() + member[1:] + "Style"


def cpp_type(kind: str) -> str:
    return {
        "color": "Color_t",
        "layout": "RatioLayout_t",
        "float": "float",
        "uint": "unsigned int",
        "int": "int",
    }[kind]


def infer_kind(section_key: str, field_key: str, value: Any) -> str:
    if isinstance(value, list):
        if not value:
            raise ValueError(f"{section_key}.{field_key}: empty array")
        if (
            field_key == "layout"
            or field_key.endswith("_layout")
            or section_key == "layouts"
        ):
            if len(value) != 4:
                raise ValueError(
                    f"{section_key}.{field_key}: layout must be [x,y,w,h]"
                )
            return "layout"
        if (
            len(value) in (3, 4)
            and all(isinstance(x, int) and not isinstance(x, bool) for x in value)
        ):
            return "color"
        if len(value) == 4 and all(
            isinstance(x, (int, float)) and not isinstance(x, bool) for x in value
        ):
            return "layout"
        raise ValueError(
            f"{section_key}.{field_key}: unsupported array value {value!r}"
        )

    if isinstance(value, bool):
        raise ValueError(f"{section_key}.{field_key}: bool is not supported")

    if isinstance(value, float):
        return "float"

    if isinstance(value, int):
        if field_key == "font_size" or field_key.endswith("_font_size"):
            return "uint"
        return "int"

    raise ValueError(
        f"{section_key}.{field_key}: unsupported type {type(value).__name__}"
    )


def load_sections(path: Path) -> list[Section]:
    root = json.loads(path.read_text())
    if not isinstance(root, dict):
        raise ValueError("UI style root must be a JSON object")

    sections: list[Section] = []
    for json_key, fields_obj in root.items():
        if not isinstance(fields_obj, dict):
            raise ValueError(f"Section '{json_key}' must be a JSON object")
        member = snake_to_camel(json_key)
        fields: list[Field] = []
        for field_key, value in fields_obj.items():
            kind = infer_kind(json_key, field_key, value)
            fields.append((snake_to_camel(field_key), field_key, kind))
        sections.append((member, json_key, fields))
    return sections


def write_header(sections: list[Section]) -> None:
    lines = [
        "#pragma once",
        "",
        '#include "graphics/Graphics.h"',
        '#include "ui/UIElement.h"',
        "",
        "#include <string>",
        "",
        "namespace ac",
        "{",
        "",
    ]
    for member, _jk, fields in sections:
        lines.append(f"struct {struct_name(member)}")
        lines.append("{")
        for cpp, _jk, kind in fields:
            lines.append(f"    {cpp_type(kind)} {cpp}{{}};")
        lines.append("};")
        lines.append("")

    lines += [
        "class UiStyle",
        "{",
        "public:",
        "    // Loads config/ui/style.json (or an absolute/relative path to that file).",
        "    static void Load(const std::string& filePath);",
        "    static const UiStyle& Get();",
        "    static bool IsLoaded();",
        "",
    ]
    for member, _jk, _fields in sections:
        lines.append(f"    {struct_name(member)} {member};")
    lines += [
        "};",
        "",
        "inline const UiStyle& Style()",
        "{",
        "    return UiStyle::Get();",
        "}",
        "",
        "} // namespace ac",
        "",
    ]
    HEADER_PATH.write_text("\n".join(lines))
    print(f"Wrote {HEADER_PATH}")


def write_parser(sections: list[Section]) -> None:
    lines = [
        '#include "ui/style/UiStyle.h"',
        "",
        "#include <nlohmann/json.hpp>",
        "",
        "#include <fstream>",
        "#include <iostream>",
        "#include <stdexcept>",
        "",
        "namespace ac",
        "{",
        "",
        "namespace",
        "{",
        "",
        "UiStyle g_style{};",
        "bool g_loaded = false;",
        "",
        "Color_t ParseColor_(const nlohmann::json& j, const char* key)",
        "{",
        "    const auto& arr = j.at(key);",
        "    if (!arr.is_array() || arr.size() < 3)",
        "    {",
        '        throw std::runtime_error(std::string("Expected RGBA array for \'") + key + "\'");',
        "    }",
        "    const uint8_t a = arr.size() >= 4 ? arr.at(3).get<uint8_t>() : 255;",
        "    return Color_t{",
        "        arr.at(0).get<uint8_t>(),",
        "        arr.at(1).get<uint8_t>(),",
        "        arr.at(2).get<uint8_t>(),",
        "        a};",
        "}",
        "",
        "RatioLayout_t ParseLayout_(const nlohmann::json& j, const char* key)",
        "{",
        "    const auto& arr = j.at(key);",
        "    if (!arr.is_array() || arr.size() != 4)",
        "    {",
        '        throw std::runtime_error(std::string("Expected [x,y,w,h] layout for \'") + key + "\'");',
        "    }",
        "    return RatioLayout_t{",
        "        arr.at(0).get<float>(),",
        "        arr.at(1).get<float>(),",
        "        arr.at(2).get<float>(),",
        "        arr.at(3).get<float>()};",
        "}",
        "",
    ]

    for member, _json_key, fields in sections:
        sn = struct_name(member)
        lines.append(f"{sn} Parse{sn}_(const nlohmann::json& j)")
        lines.append("{")
        lines.append(f"    {sn} s{{}};")
        for cpp, jk, kind in fields:
            if kind == "color":
                lines.append(f'    s.{cpp} = ParseColor_(j, "{jk}");')
            elif kind == "layout":
                lines.append(f'    s.{cpp} = ParseLayout_(j, "{jk}");')
            elif kind == "float":
                lines.append(f'    s.{cpp} = j.at("{jk}").get<float>();')
            elif kind == "uint":
                lines.append(f'    s.{cpp} = j.at("{jk}").get<unsigned int>();')
            elif kind == "int":
                lines.append(f'    s.{cpp} = j.at("{jk}").get<int>();')
        lines.append("    return s;")
        lines.append("}")
        lines.append("")

    lines += [
        "} // namespace",
        "",
        "void UiStyle::Load(const std::string& filePath)",
        "{",
        '    std::cout << "Loading UI style from: " << filePath << "\\n";',
        "",
        "    std::ifstream file(filePath);",
        "    if (!file.is_open())",
        "    {",
        '        throw std::runtime_error("Could not open UI style file: " + filePath);',
        "    }",
        "",
        "    nlohmann::json root;",
        "    file >> root;",
        "    if (!root.is_object())",
        "    {",
        '        throw std::runtime_error("UI style root must be a JSON object");',
        "    }",
        "",
        "    UiStyle style{};",
    ]
    for member, json_key, _fields in sections:
        sn = struct_name(member)
        lines.append(f'    style.{member} = Parse{sn}_(root.at("{json_key}"));')

    lines += [
        "",
        "    g_style = style;",
        "    g_loaded = true;",
        '    std::cout << "Loaded UI style\\n";',
        "}",
        "",
        "const UiStyle& UiStyle::Get()",
        "{",
        "    if (!g_loaded)",
        "    {",
        '        throw std::runtime_error("UiStyle::Get called before UiStyle::Load");',
        "    }",
        "    return g_style;",
        "}",
        "",
        "bool UiStyle::IsLoaded()",
        "{",
        "    return g_loaded;",
        "}",
        "",
        "} // namespace ac",
        "",
    ]
    SOURCE_PATH.write_text("\n".join(lines))
    print(f"Wrote {SOURCE_PATH}")


def main() -> None:
    if not STYLE_JSON.is_file():
        print(f"Missing style JSON: {STYLE_JSON}", file=sys.stderr)
        sys.exit(1)
    sections = load_sections(STYLE_JSON)
    write_header(sections)
    write_parser(sections)


if __name__ == "__main__":
    main()
