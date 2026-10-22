#!/usr/bin/env python3
"""List candidate violations of the readable-code rules in coding-guidelines.mdc.

Every hit is a candidate for a person or agent to judge, not a verdict.

  python tools/slop_scan.py game/units            # src/game/units + include/game/units
  python tools/slop_scan.py src include --summary
  python tools/slop_scan.py game/units --functions  # rule 7, needs `pip install lizard`
  python tools/slop_scan.py --comment-only HEAD~1

A domain name such as `game/units` covers the files directly in `src/<domain>` and
`include/<domain>`. An existing path is scanned recursively.
"""

from __future__ import annotations

import argparse
import collections
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIRS = ("src", "include")
SUFFIXES = (".h", ".cpp")

CONTROL_KEYWORDS = {
    "if", "for", "while", "switch", "catch", "return", "sizeof", "alignof", "decltype",
    "noexcept", "throw", "co_return", "static_assert", "alignas", "requires", "defined",
}
HISTORY_PHRASES = re.compile(
    r"\b(used to|no longer|previously|formerly|was a bug|replaces|replaced|legacy|"
    r"backwards?[- ]compat\w*|old code|anymore|any more|now (?:uses|reads|takes|lives|goes|"
    r"calls|returns|throws|happens|runs))\b",
    re.IGNORECASE,
)
ALTERNATIVE_PHRASES = re.compile(r"\b(rather than|instead of|as opposed to)\b", re.IGNORECASE)
REFERENCE_PHRASES = re.compile(
    r"\b(code[- ]review|review finding|finding|plan(?:s)?/|docs/plans|phase \d|step \d|"
    r"§\s*\d|package \d+)\b",
    re.IGNORECASE,
)
CAPS_WORD = re.compile(r"\b[A-Z]{4,}\b")
CAPS_ALLOWED = {
    "TODO", "SMAC", "JSON", "SFML", "FIXME", "ASCII", "UTF8", "HTTP", "WAVE", "RGBA", "BGRA",
    "PCX", "AABB", "UUID", "MSVC", "GCC", "POSIX", "LIFO", "FIFO", "NULL", "CAPS",
}
LABEL_COMMENT = re.compile(r"^[A-Z][a-z]+(?: [a-z/&]+){0,2}$")


@dataclass
class Hit:
    path: Path
    line: int
    rule: str
    message: str


# ---------------------------------------------------------------------------
# Source handling


def strip_source(text: str) -> tuple[str, list[tuple[int, str, bool]]]:
    """Blank out comments and string contents, keeping offsets and newlines.

    Returns the code text and a list of (line, comment text, is_full_line) for `//` comments.
    """
    out = []
    comments: list[tuple[int, str, bool]] = []
    i = 0
    line = 1
    line_has_code = False
    n = len(text)
    while i < n:
        c = text[i]
        if c == "\n":
            out.append(c)
            line += 1
            line_has_code = False
            i += 1
        elif text.startswith("//", i):
            end = text.find("\n", i)
            end = n if end == -1 else end
            comments.append((line, text[i + 2:end].strip(), not line_has_code))
            out.append(" " * (end - i))
            i = end
        elif text.startswith("/*", i):
            end = text.find("*/", i + 2)
            end = n if end == -1 else end + 2
            chunk = text[i:end]
            out.append(re.sub(r"[^\n]", " ", chunk))
            line += chunk.count("\n")
            i = end
        elif c == "R" and text.startswith('R"', i) and (i == 0 or not text[i - 1].isalnum()):
            delim_end = text.find("(", i + 2)
            delim = text[i + 2:delim_end]
            end = text.find(")" + delim + '"', delim_end)
            end = n if end == -1 else end + len(delim) + 2
            chunk = text[i:end]
            out.append('""' + re.sub(r"[^\n]", " ", chunk[2:]))
            line += chunk.count("\n")
            line_has_code = True
            i = end
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            out.append(c + " " * (j - i - 1) + (c if j < n else ""))
            line_has_code = True
            i = j + 1
        else:
            if not c.isspace():
                line_has_code = True
            out.append(c)
            i += 1
    return "".join(out), comments


def line_of(code: str, offset: int) -> int:
    return code.count("\n", 0, offset) + 1


def match_brace(code: str, open_index: int) -> int:
    depth = 0
    for k in range(open_index, len(code)):
        if code[k] == "{":
            depth += 1
        elif code[k] == "}":
            depth -= 1
            if depth == 0:
                return k
    return len(code) - 1


def preprocessor_blanked(code: str) -> str:
    return re.sub(r"(?m)^[ \t]*#.*$", lambda m: " " * len(m.group(0)), code)


# ---------------------------------------------------------------------------
# Rule 1: one call deep


def word_before(code: str, index: int) -> tuple[str, int]:
    k = index - 1
    while k >= 0 and code[k] in " \t":
        k -= 1
    end = k + 1
    while k >= 0 and (code[k].isalnum() or code[k] == "_"):
        k -= 1
    return code[k + 1:end], k


def is_call_paren(code: str, index: int) -> bool:
    prev = index - 1
    if prev >= 0 and code[prev] == ">":
        return True
    while prev >= 0 and code[prev] in " \t":
        prev -= 1
    if prev < 0:
        return False
    if code[prev] in ")]":
        return code[prev] == ")" and prev == index - 1
    word, _ = word_before(code, index)
    if not word or word[0].isdigit():
        return False
    return word not in CONTROL_KEYWORDS and word != "operator"


def check_call_depth(path: Path, code: str) -> list[Hit]:
    hits = []
    stack: list[list[bool]] = [[]]
    reported = set()
    for k, c in enumerate(code):
        if c == "{":
            stack.append([])
        elif c == "}":
            if len(stack) > 1:
                stack.pop()
        elif c == "(":
            parens = stack[-1]
            bCall = is_call_paren(code, k)
            if bCall and sum(parens) >= 2:
                line = line_of(code, k)
                if line not in reported:
                    reported.add(line)
                    hits.append(Hit(path, line, "1", "call nested two deep in arguments"))
            parens.append(bCall)
        elif c == ")":
            if stack[-1]:
                stack[-1].pop()
    return hits


# ---------------------------------------------------------------------------
# Rule 2: comments


def comment_blocks(comments: list[tuple[int, str, bool]]) -> list[list[tuple[int, str]]]:
    blocks: list[list[tuple[int, str]]] = []
    for line, text, bFullLine in comments:
        if not bFullLine:
            continue
        if blocks and blocks[-1][-1][0] == line - 1:
            blocks[-1].append((line, text))
        else:
            blocks.append([(line, text)])
    return blocks


def check_comments(path: Path, comments) -> list[Hit]:
    hits = []
    for line, text, _ in comments:
        for pattern, label in (
            (HISTORY_PHRASES, "history"),
            (ALTERNATIVE_PHRASES, "alternative not taken"),
            (REFERENCE_PHRASES, "plan or review reference"),
        ):
            m = pattern.search(text)
            if m:
                hits.append(Hit(path, line, "2", f"{label}: \"{m.group(0)}\""))
        caps = [w for w in CAPS_WORD.findall(text) if w not in CAPS_ALLOWED]
        if caps:
            hits.append(Hit(path, line, "2", f"caps emphasis: {', '.join(caps)}"))
    for block in comment_blocks(comments):
        first_line = block[0][0]
        if len(block) > 2:
            hits.append(Hit(path, first_line, "2", f"comment block of {len(block)} lines"))
        if len(block) == 1:
            text = block[0][1]
            if LABEL_COMMENT.match(text) and not text.startswith("TODO"):
                hits.append(Hit(path, first_line, "2", f"label comment: \"{text}\""))
    return hits


# ---------------------------------------------------------------------------
# Rule 3: guards and silent defaults

SILENT_NULL = re.compile(
    r"\bif\s*\(\s*(?:!\s*(?:m_)?p[A-Z]\w*|(?:m_)?p[A-Z]\w*\s*==\s*nullptr|nullptr\s*==\s*"
    r"(?:m_)?p[A-Z]\w*)\s*\)\s*\{?\s*(return\b[^;]*;|continue\s*;|break\s*;)"
)
REFERENCE_NULL = re.compile(r"&\s*(?:m_)?r[A-Z]\w*\s*[!=]=\s*nullptr|!\s*&\s*(?:m_)?r[A-Z]")
JSON_DEFAULT = re.compile(r"\.value\(\s*\"")
CATCH = re.compile(r"\bcatch\s*\(([^)]*)\)\s*\{")


def check_guards(path: Path, code: str) -> list[Hit]:
    hits = []
    for m in SILENT_NULL.finditer(code):
        hits.append(Hit(path, line_of(code, m.start()), "3",
                        f"silent null skip: {m.group(1).split()[0]}"))
    for m in REFERENCE_NULL.finditer(code):
        hits.append(Hit(path, line_of(code, m.start()), "3", "null check on a reference"))
    for m in JSON_DEFAULT.finditer(code):
        hits.append(Hit(path, line_of(code, m.start()), "3", "config key with a fallback"))
    for m in CATCH.finditer(code):
        body_end = match_brace(code, m.end() - 1)
        body = code[m.end():body_end]
        if "throw" not in body:
            hits.append(Hit(path, line_of(code, m.start()), "3",
                            f"catch ({m.group(1).strip()}) without throw"))
    return hits


# ---------------------------------------------------------------------------
# Function bodies (rules 4 and 6)

QUALIFIERS = re.compile(r"\)\s*(?:const|override|final|noexcept|mutable|&|&&|\s|->\s*[\w:<>,\*&\s]+)*$")


@dataclass
class FunctionDef:
    name: str
    signature: str
    body: str
    line: int


def function_definitions(code: str) -> list[FunctionDef]:
    defs = []
    code = preprocessor_blanked(code)
    for m in re.finditer(r"\{", code):
        start = m.start()
        k = start - 1
        while k >= 0 and code[k] not in ";{}":
            k -= 1
        head = code[k + 1:start]
        if not QUALIFIERS.search(head.rstrip() + " ") and not head.rstrip().endswith(")"):
            continue
        head_stripped = head.strip()
        if not head_stripped or "(" not in head_stripped:
            continue
        if re.search(r"\]\s*(\([^)]*\))?\s*(mutable\s*)?(->[^{]*)?$", head_stripped):
            continue
        if re.search(r"(?:^|\s):\s*[\w:]+[({]", head_stripped) and ")" not in head_stripped.split(":")[0]:
            continue
        name_part = head_stripped.split("(")[0]
        if re.search(r"\b(if|for|while|switch|catch|return|else|do|namespace|class|struct|enum)\b",
                     name_part):
            continue
        if "=" in name_part:
            continue
        name_match = re.search(r"(~?\w+)\s*$", name_part)
        if not name_match:
            continue
        end = match_brace(code, start)
        defs.append(FunctionDef(name_match.group(1), head_stripped, code[start + 1:end],
                                line_of(code, start)))
    return defs


MEMBER_GETTER = re.compile(r"^return\s+\*?(?:m_\w+|this)(?:\.get\(\))?\s*;$")


def check_pass_through(path: Path, code: str) -> list[Hit]:
    hits = []
    for fn in function_definitions(code):
        body = " ".join(fn.body.split())
        if "{" in body or body.count(";") != 1 or "(" not in body:
            continue
        if re.search(r"\boverride\b", fn.signature) or fn.name.startswith("~"):
            continue
        if MEMBER_GETTER.match(body):
            continue
        statement = re.sub(r"^return\s+", "", body).rstrip(";").strip()
        if re.match(r"^(?:std::|static_cast|throw\b|m_\w+(?:\.|->))", statement) or "=" in statement.split("(")[0]:
            continue
        if not re.match(r"^[\w:\.\->\*\[\]]+\s*(?:<[^()]*>)?\s*\(.*\)$", statement):
            continue
        if re.search(r"[+\-*/%<>]=?\s|&&|\|\||\?", statement.split("(")[0]):
            continue
        if re.search(r"&&|\|\||\?|==|!=", statement):
            continue
        hits.append(Hit(path, fn.line, "4", f"{fn.name} forwards to {statement.split('(')[0]}"))
    return hits


# ---------------------------------------------------------------------------
# Rule 5: flags and default arguments

BOOL_PARAM = re.compile(r"\bbool\s+(b[A-Z]\w*)\s*(?:=\s*\w+\s*)?[,)]")


def check_flags(path: Path, code: str) -> list[Hit]:
    hits = []
    code = preprocessor_blanked(code)
    for m in BOOL_PARAM.finditer(code):
        hits.append(Hit(path, line_of(code, m.start()), "5", f"bool parameter {m.group(1)}"))
    for m in re.finditer(r"[(,]\s*(true|false)\s*(?=[,)])", code):
        open_index = enclosing_paren(code, m.start() + 1)
        if open_index is None or not is_call_paren(code, open_index):
            continue
        word, _ = word_before(code, open_index)
        if word.startswith("m_") or word in {"bool", "optional"}:
            continue
        hits.append(Hit(path, line_of(code, m.start()), "5",
                        f"literal {m.group(1)} passed to {word or 'call'}"))
    hits.extend(default_arguments(path, code))
    return hits


def enclosing_paren(code: str, index: int) -> int | None:
    depth = 0
    for k in range(index - 1, -1, -1):
        c = code[k]
        if c in ")]}":
            depth += 1
        elif c in "([{":
            if depth == 0:
                return k if c == "(" else None
            depth -= 1
        elif c == ";" and depth == 0:
            return None
    return None


def default_arguments(path: Path, code: str) -> list[Hit]:
    hits = []
    for m in re.finditer(r"(?<![=!<>+\-*/%&|^])=(?![=])", code):
        open_index = enclosing_paren(code, m.start())
        if open_index is None or not is_call_paren(code, open_index):
            continue
        word, _ = word_before(code, open_index)
        if word in {"for"} or word.startswith("m_"):
            continue
        close = match_paren(code, open_index)
        tail = code[close + 1:close + 80].lstrip()
        if not re.match(r"(?:const|override|noexcept|final|->|\s)*[;{]|=\s*0|=\s*default|=\s*delete",
                        tail):
            continue
        name = code[m.start() - 40:m.start()].split()[-1:] or ["?"]
        hits.append(Hit(path, line_of(code, m.start()), "5", f"default argument for {name[0]}"))
    return hits


def match_paren(code: str, open_index: int) -> int:
    depth = 0
    for k in range(open_index, len(code)):
        if code[k] == "(":
            depth += 1
        elif code[k] == ")":
            depth -= 1
            if depth == 0:
                return k
    return len(code) - 1


# ---------------------------------------------------------------------------
# Rule 6: speculative code

NOT_DECL_WORDS = {"return", "new", "throw", "else", "case", "delete", "co_await", "co_return",
                  "not", "and", "or", "sizeof", "decltype"}


def occurrence_is_declaration(line: str, start: int) -> bool:
    before = line[:start].rstrip()
    if not before:
        return False
    if before.endswith(("::", ".", "->", "(", ",", "=", "!", "&&", "||", "?", ":", "{", "[")):
        return before.endswith("::") and re.search(r"[\w>&\*]\s+(?:\w+::)+$", line[:start]) is not None
    last = re.search(r"(\w+)\s*[&\*]*\s*$", before)
    if last and last.group(1) in NOT_DECL_WORDS:
        return False
    return bool(re.search(r"[\w>&\*]$", before))


def index_identifiers(files: list[Path]) -> tuple[dict, dict]:
    uses: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    decls: dict[str, list[tuple[Path, int, str]]] = collections.defaultdict(list)
    for path in files:
        code, _ = strip_source(path.read_text(errors="ignore"))
        code = preprocessor_blanked(code)
        area = path.relative_to(ROOT).parts[0]
        for line_number, line in enumerate(code.splitlines(), 1):
            for m in re.finditer(r"\b([A-Z]\w*)\b", line):
                name = m.group(1)
                rest = line[m.end():].lstrip()
                qualified = re.search(r"((?:\w+::)+)$", line[:m.start(1)])
                decl_start = m.start(1) - (len(qualified.group(1)) if qualified else 0)
                bDecl = rest.startswith("(") and occurrence_is_declaration(line, decl_start)
                if bDecl:
                    decls[name].append((path, line_number, line))
                else:
                    uses[name][area] += 1
    return uses, decls


CLASS_BASES = re.compile(r"\b(?:class|struct)\s+\w+(?:\s+final)?\s*:(?!:)\s*([^{;]+)\{")


def check_speculative(scan_files: list[Path]) -> list[Hit]:
    all_files = source_files([ROOT / d for d in (*SOURCE_DIRS, "tests")])
    uses, decls = index_identifiers(all_files)
    scanned = {p.resolve() for p in scan_files}
    classes = set()
    for path in all_files:
        classes.update(re.findall(r"\b(?:class|struct)\s+([A-Z]\w*)", path.read_text(errors="ignore")))
    hits = []
    for name, sites in decls.items():
        if name in classes or name == "main":
            continue
        header_sites = [s for s in sites if s[0].suffix == ".h" and s[0].resolve() in scanned
                        and "include" in s[0].relative_to(ROOT).parts[:1]]
        if not header_sites:
            continue
        if any(re.search(r"\b(override|virtual)\b", s[2]) for s in sites):
            continue
        source_uses = uses[name]["src"] + uses[name]["include"]
        test_uses = uses[name]["tests"]
        path, line, _ = header_sites[0]
        if source_uses == 0 and test_uses == 0:
            hits.append(Hit(path, line, "6", f"{name} has no caller"))
        elif source_uses == 0:
            hits.append(Hit(path, line, "6", f"{name} is used only by tests ({test_uses})"))
    for path in scan_files:
        text = path.read_text(errors="ignore")
        for m in re.finditer(r"\bclass\s+(I[A-Z]\w*)\b(?!\s*;)", text):
            interface = m.group(1)
            implementers = collections.Counter()
            for other in all_files:
                bases = CLASS_BASES.findall(other.read_text(errors="ignore"))
                count = sum(1 for b in bases if re.search(rf"\b{interface}\b", b))
                implementers[other.relative_to(ROOT).parts[0]] += count
            source_count = implementers["src"] + implementers["include"]
            if source_count <= 1:
                hits.append(Hit(path, line_of(text, m.start()), "6",
                                f"interface {interface} has {source_count} implementation(s)"
                                f" ({implementers['tests']} in tests)"))
    return hits


# ---------------------------------------------------------------------------
# Rule 7: function size, via lizard


def check_functions(path: Path) -> list[Hit]:
    try:
        import lizard
    except ImportError:
        sys.exit("--functions needs lizard: pip install lizard")
    analyzer = lizard.FileAnalyzer(lizard.get_extensions(["NS"]))
    text = path.read_text(errors="ignore")
    result = analyzer.analyze_source_code(str(path), text)
    lines = text.splitlines()
    hits = []
    for fn in result.function_list:
        nesting = getattr(fn, "max_nested_structures", 0)
        problems = []
        if fn.nloc > 40:
            problems.append(f"{fn.nloc} lines")
        if nesting > 3:
            problems.append(f"nesting {nesting}")
        if fn.cyclomatic_complexity > 10:
            problems.append(f"complexity {fn.cyclomatic_complexity}")
        if fn.parameter_count > 5:
            problems.append(f"{fn.parameter_count} parameters")
        if not problems:
            continue
        body = lines[fn.start_line - 1:fn.end_line]
        case_lines = sum(1 for b in body if re.match(r"\s*(case\b.*|default\s*:|return\b.*;)\s*$", b))
        table = " (table)" if case_lines >= 0.6 * max(1, fn.nloc) else ""
        hits.append(Hit(path, fn.start_line, "7", f"{fn.name}: {', '.join(problems)}{table}"))
    return hits


# ---------------------------------------------------------------------------
# Comment-only diff check


def code_part(line: str) -> str:
    quote = None
    k = 0
    while k < len(line):
        c = line[k]
        if quote:
            if c == "\\":
                k += 1
            elif c == quote:
                quote = None
        elif c in "\"'":
            quote = c
        elif line.startswith("//", k):
            line = line[:k]
            break
        k += 1
    return "".join(line.split())


def check_comment_only(rev: str) -> int:
    diff = subprocess.run(["git", "diff", "-U0", rev, "--", *SOURCE_DIRS], cwd=ROOT,
                          capture_output=True, text=True, check=True).stdout
    removed: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    added: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    current = None
    for line in diff.splitlines():
        if line.startswith("+++ "):
            current = line[6:]
        elif line.startswith("--- "):
            continue
        elif line.startswith("-") and current is not None:
            removed[current][code_part(line[1:])] += 1
        elif line.startswith("+") and current is not None:
            added[current][code_part(line[1:])] += 1
    bad = 0
    for path in sorted(set(removed) | set(added)):
        r = removed[path]
        a = added[path]
        r.pop("", None)
        a.pop("", None)
        if r != a:
            bad += 1
            changed = sorted(((r - a) + (a - r)).keys())[:3]
            print(f"{path}: code changed: {changed}")
    if bad == 0:
        print("comment-only: ok")
    return 1 if bad else 0


# ---------------------------------------------------------------------------
# Driver


def source_files(paths: list[Path]) -> list[Path]:
    files = []
    for p in paths:
        if p.is_file() and p.suffix in SUFFIXES:
            files.append(p)
        elif p.is_dir():
            files.extend(sorted(f for f in p.rglob("*") if f.suffix in SUFFIXES))
    return files


def expand(args: list[str]) -> list[Path]:
    files = []
    for arg in args:
        path = ROOT / arg
        if path.exists():
            files.extend(source_files([path]))
            continue
        domain_files = []
        for top in SOURCE_DIRS:
            folder = ROOT / top / arg
            if folder.is_dir():
                domain_files.extend(sorted(f for f in folder.iterdir() if f.suffix in SUFFIXES))
        if not domain_files:
            sys.exit(f"no such path or domain: {arg}")
        files.extend(domain_files)
    return files


def scan(files: list[Path], bFunctions: bool) -> list[Hit]:
    hits = []
    for path in files:
        text = path.read_text(errors="ignore")
        code, comments = strip_source(text)
        hits += check_call_depth(path, preprocessor_blanked(code))
        hits += check_comments(path, comments)
        hits += check_guards(path, code)
        hits += check_pass_through(path, code)
        hits += check_flags(path, code)
        if bFunctions:
            hits += check_functions(path)
    hits += check_speculative(files)
    return sorted(hits, key=lambda h: (str(h.path), h.line, h.rule))


def domain_of(path: Path) -> str:
    parts = path.relative_to(ROOT).parts
    return "/".join(parts[1:-1]) or "(root)"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("paths", nargs="*")
    parser.add_argument("--summary", action="store_true", help="counts per rule and domain")
    parser.add_argument("--functions", action="store_true", help="rule 7 checks via lizard")
    parser.add_argument("--rule", action="append", help="only report these rules")
    parser.add_argument("--comment-only", metavar="REV",
                        help="check that changes since REV touch only comments")
    args = parser.parse_args()
    if args.comment_only:
        return check_comment_only(args.comment_only)
    if not args.paths:
        parser.error("give paths or domains to scan")
    hits = scan(expand(args.paths), args.functions)
    if args.rule:
        hits = [h for h in hits if h.rule in args.rule]
    if args.summary:
        table: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
        for h in hits:
            table[domain_of(h.path)][h.rule] += 1
        rules = sorted({h.rule for h in hits})
        print(f"{'domain':32}" + "".join(f"{'r' + r:>7}" for r in rules))
        for domain in sorted(table):
            print(f"{domain:32}" + "".join(f"{table[domain][r]:>7}" for r in rules))
        totals = collections.Counter(h.rule for h in hits)
        print(f"{'total':32}" + "".join(f"{totals[r]:>7}" for r in rules))
        return 0
    for h in hits:
        print(f"{h.path.relative_to(ROOT)}:{h.line}: [{h.rule}] {h.message}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
