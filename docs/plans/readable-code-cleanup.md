# Readable-code cleanup: bring `src/` and `include/` in line with the hard rules

Applies the seven readable-code rules in `.devin/rules/coding-guidelines.mdc` to existing code.
For this cleanup, the line "existing code is not rewritten unless a change touches it" is
suspended: every file in `src/` and `include/` is in scope.

## Scope

- **In:** every `.h` and `.cpp` under `src/` and `include/` (about 79k lines, 734 files).
- **Not reviewed:** `tests/`, `config/`, `docs/`, `tools/`. A test or config is edited only when a
  source change requires it: a caller of a removed function, or a key that becomes required.
- No behavior changes except where rule 3 turns a silent skip into a throw.

## Baseline

Rough counts from grep, to size the work and track progress. The scanner below replaces them.
The function measures come from lizard over 3,209 functions.

| Rule | Pattern | Count |
|---|---|---|
| 2 | `//` blocks longer than two lines | 819 |
| 2 | "rather than" / "instead of" in comments | 190 |
| 2 | history words ("used to", "previously", "no longer"…) | ~50 |
| 2 | plan / review / finding references | ~10 |
| 1 | nested casts on one line | 41 |
| 3 | `if (!pX)` followed by `return` / `continue` | 123 |
| 3 | `.value("key", fallback)` | 158 |
| 4 | single-statement forwarding bodies (includes legitimate getters) | ~190 |
| 5 | `bool b…` parameters in headers | 50 |
| 5 | literal `true` / `false` call arguments | 80 |
| 5 | default arguments in declarations | ~30 |
| 7 | functions over 50 / over 100 lines | 93 / 19 |
| 7 | functions with cyclomatic complexity over 10 / over 20 | 100 / 31 |
| 7 | functions with more than 5 parameters | 59 |

Function length: median 9 lines, 90th percentile 30. Complexity: median 2, 90th percentile 6.

## Step 1: `tools/slop_scan.py`

A heuristic scanner that lists candidates by file, rule and line. It finds candidates; a person
or agent decides each one.

- `slop_scan.py <paths…>` prints candidates. `--summary` prints counts per rule and directory.
- **Rule 1:** a call or cast nested two deep inside another call's arguments.
- **Rule 2:** comment blocks over two lines, and blocks above a function longer than one line;
  the banned phrases from rule 2; label comments that repeat the next declaration's name;
  `ALL-CAPS` emphasis.
- **Rule 3:** `if (!pX)` / `pX == nullptr` followed by `return` or `continue`; `.value("…", …)`;
  `catch` blocks that neither rethrow nor convert.
- **Rule 4:** function bodies that are one statement forwarding to another call. Bodies that
  return a member are skipped.
- **Rule 5:** `bool` parameters, literal `true` / `false` arguments, default arguments.
- **Rule 6:** functions declared under `include/` with no reference outside their own declaration
  and definition; those referenced only from `tests/`; interfaces with one implementation.
- **Rule 7:** `--functions` runs lizard (`pip install lizard`) and lists functions over 40 lines,
  over three levels of nesting, or with complexity over 10. Switches that only map a value are
  marked as tables.
- `--comment-only <rev>` checks that every changed line since `<rev>` is a comment or blank.

## Step 2: slices

Each slice is a set of domain directories, taken from both `src/` and `include/`. They are worked
in this order, so lower layers settle their API before their callers are reviewed.

| # | Slice | Directories | Lines |
|---|---|---|---|
| 1 | Pilot: units | `game/units` | 3.5k |
| 2 | Library | `lib`, `lib/config`, `input`, `graphics`, `main.cpp` | 2.4k |
| 3 | Game root | `game/` (files directly in it: `GameState`, `GameDataContext`, `Engine`, validators) | 4.8k |
| 4 | Effects: definitions | `game/effects` headers, enums, config and parsers | ~4.4k |
| 5 | Effects: resolution and dispatch | `game/effects` remaining files | ~4.4k |
| 6 | Map | `game/map` | 3.7k |
| 7 | Terraform and ecology | `game/terraform`, `game/ecology` | 2.0k |
| 8 | World generation | `game/worldgen`, `game/natives`, `game/world-events` | 3.4k |
| 9 | Movement and visibility | `game/movement`, `game/visibility` | 3.1k |
| 10 | Combat and probes | `game/combat`, `game/probes` | 4.5k |
| 11 | Faction | `game/faction`, `game/social-engineering`, `game/research` | 4.4k |
| 12 | Base | `game/base`, `game/buildings` | 4.1k |
| 13 | Population | `game/population` | 3.8k |
| 14 | Production and economy | `game/production`, `game/economy` | 4.5k |
| 15 | Diplomacy | `game/diplomacy`, `game/council`, `game/atrocities` | 4.7k |
| 16 | Turn stages | `game/stages`, `game/display` | 3.2k |
| 17 | UI core | `ui/` (files directly in it) | 4.4k |
| 18 | UI world | `ui/world` | 5.4k |
| 19 | UI base and designer | `ui/base`, `ui/unit-designer` | 3.2k |
| 20 | UI panels | `ui/style`, `ui/settings`, `ui/research`, `ui/commlinks`, `ui/council`, `ui/satellite`, `ui/social-engineering` | 5.5k |

### Pilot review

Before the pilot starts, ten functions in `game/units` are picked: a mix of long functions,
heavily commented ones and short ones. After the pilot, each before/after pair is shown to the
user side by side, in random order, without saying which is which. The user picks the more
readable version and says why in a few words.

- The cleaned version should win at least 8 of 10.
- Each loss, and each reason given, becomes an edit to the guidelines or to this procedure
  before slice 2 starts.

## Step 3: per-slice procedure

Every slice lands as two commits.

### Commit A: comments

Read every comment in the slice, not only the scanner's hits. For each one:

- **Restates the code, or a label for the next name:** delete.
- **History, bug narrative, rejected alternative, plan or review reference:** delete. The commit
  history already holds it.
- **States a current constraint the code does not enforce** (an invariant, a lifetime hazard, a
  SMAC rule): rewrite it as one line, two at most. If the constraint can be enforced with a
  throw, note it for commit B.
- **A paragraph covering several steps:** split it into one-line comments at the lines each part
  explains, keeping only the parts that pass the tests above. A part that names a step is noted
  for commit B, where the step becomes a function with that name.
- **Subsystem design a reader needs and the code cannot show:** move it to that subsystem's doc
  in `docs/architecture/` if the doc lacks it, and delete it from the source.
- **Confirmed SMAC behavior:** move it to `docs/thinker/terranx-map.md` with its address if the map
  lacks it, and leave a one-line comment citing it.
- **TODO for an unknown game rule:** keep, cut to one line that names the missing rule.

Check: `slop_scan.py --comment-only HEAD~1` passes, and `./bd build` succeeds.

### Commit B: code

Work through the scanner's rule 1 and 3–7 candidates, then read the slice for the misses.
Commit B makes local fixes only. A fix is local when it stays inside the function and its direct
callers, and needs no new type, no move between files, and no change to what a caller must
know or hold. Anything else goes into [`refactor-flags.md`](refactor-flags.md) and the code is
left as it is.

- **Rule 1:** name the intermediate values, with names that say what the value is.
- **Rule 3:** if a pointer cannot be null, make it a reference, or throw where it is read. If a
  `.value("key", fallback)` key has no meaning when absent, make the parser require it, and add
  the key to every shipping and fixture config that lacks it. A pointer that is nullable because
  of construction or lifetime order is flagged.
- **Rule 4:** delete the forwarder; its callers, including tests, call the target.
- **Rule 5:** remove default arguments, updating each caller. Split a function in two when every
  caller passes a literal. A flag that has to be derived from an object, or that callers compute,
  is flagged.
- **Rule 6:** delete code with no caller. A getter, setter or hook that only tests use is
  flagged, because removing it means rewriting those tests against public behavior. An interface
  with one implementation that neither narrows its consumer's reach nor breaks a header cycle is
  flagged.
- **Rule 7:** extract the steps of a long or deeply nested function into named functions in the
  same file, including the steps commit A noted. A function over 100 lines, or one whose steps
  share so much state that splitting it needs a new struct, is flagged.
- Changes reach callers outside the slice in the same commit.
- If a removed component appears in `docs/architecture/`, the doc is updated in the same commit.

Check: `./bd test` passes. A test that fails because a silent skip now throws means the case
does happen: the guard stays, written as a deliberate branch for a real case, and the slice
report says so.

### Slice report

Each slice ends with a short note to the user listing:

- entries it added to `refactor-flags.md`;
- config keys that became required;
- guards that turned out to be reachable;
- any test failure, and whether it was a requirement change or a bug.

## Step 4: final sweep

- `slop_scan.py --summary src include` reports no banned phrases. Every remaining comment block
  over two lines, and every remaining heuristic hit for rules 1, 3, 4, 5 and 7, has been checked.
- The baseline table above is replaced with the final counts.
- `refactor-flags.md` is grouped into themes, and each theme gets its own plan.
