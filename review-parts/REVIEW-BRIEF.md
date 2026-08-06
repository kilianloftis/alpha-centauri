# Shared brief: full-project code review (August 2026)

You are a senior C++ reviewer doing an in-depth code review of one slice of this project.
The repository is `/home/martok/alpha-centauri` — a C++ turn-based strategy game (an Alpha
Centauri rebuild), built with CMake via the root `bd` script.

Your slice (area name, output file, and exact file list) is given in the prompt that pointed
you here.

## Before you start

Read these, they define what "good" means for this project:

- `.cursor/rules/coding-guidelines.md` — mandatory conventions: SOLID, references over
  pointers, constructors must produce valid objects, classes own their resources, prefer
  throwing over returning defaults, throw on unexpected null, no legacy/back-compat code,
  moddability via config + Lua hooks, and the full naming/formatting scheme.
- `.cursor/rules/architecting.md` — architecture diagrams in `docs/architecture/` must be
  kept current.
- `docs/architecture/high-level.md` — the intended overall structure. Any relevant
  subsystem doc in `docs/architecture/` is also worth reading for your slice.

## What to review

Read **every** file in your assigned list, in full — implementation and headers. Then judge
them on:

1. **Clean code and simplicity** — is the code as simple as the problem allows? Dead code,
   speculative generality, needless indirection, duplicated logic, long functions doing
   several things, unclear names, comments that restate the code or hide a real constraint.
2. **SOLID** — single responsibility per class, extension without modification (especially
   for anything a modder should be able to extend), substitutable derived types, narrow
   interfaces, dependencies on abstractions rather than concretions.
3. **Clarity** — can a new maintainer read this and know what it does and why? Are
   invariants stated and enforced, or only implied?
4. **Robustness** — error handling consistency, null/bounds/overflow handling, integer and
   `Rational` arithmetic edge cases, iterator/reference/dangling-lifetime hazards,
   uninitialized members, silent fallbacks that swallow config or rule errors.
5. **Maintainability and modifiability** — what will hurt when this system grows? Hardcoded
   values or IDs that belong in config, multiple sources of truth that can desync,
   copy-paste that must be edited in N places, hidden coupling to other subsystems.
6. **Convention adherence** — naming, brace style, `_t` usage, `magic_enum` for enum/string
   conversion, member/pointer/reference prefixes. Report these as low severity, grouped.

You **may and should** read files outside your slice to understand interactions (callers,
collaborators, config JSON under `config/`, tests under `tests/`). But only report findings
whose fix lives in *your* files. If you find something serious that belongs to another
slice, note it in a short "Observed outside slice" list at the end instead.

## What is NOT a finding

- **Unimplemented or stubbed features.** This game is under construction; many systems are
  deliberately partial. A missing feature, an empty stage, or a TODO is not a finding.
  *Exception:* a stub wired in a way that silently produces wrong values, or that will be
  expensive to unwind later, is worth reporting — say explicitly why.
- **`src/game/Engine.cpp` / `include/game/Engine.h`** — excluded from this review by the
  project owner; it is an ad-hoc testing catchall. Do not review it, and do not report
  findings about it (you may read it for context).
- Missing tests for unimplemented behavior. Gaps in tests for *implemented* behavior are
  fair game, but keep them brief.
- Style preferences that contradict the project's own stated guidelines.

## Rules of engagement

- **Read-only.** Do not modify, create, or delete any source, header, config, or test file.
  The only file you write is your part file (path given in your prompt).
- **Do not run builds or tests.** Do not invoke `./bd`, `cmake`, `make`, or `ctest`.
- **Verify every claim against the code before you write it.** Cite `path:line` for each
  finding. No speculation — if you are unsure whether something is a bug, say what you
  checked and what remains unverified, or leave it out.
- **Do not pad.** A short list of real, specific problems is worth far more than a long list
  of generic observations. If a file is genuinely clean, say so and move on.
- A prior project-wide review exists at `docs/code-review-findings.md`, with status notes on
  what has since been fixed or deliberately deferred. Form your own conclusions **first**,
  then skim the parts relevant to your slice: do not re-report items recorded there as
  resolved, but *do* report cases where the recorded fix is incomplete or where the fix
  itself introduced a problem. Say so when a finding relates to a prior item.

## Output format

Write your part file at the path given in your prompt, in exactly this structure. Use `###`
for each finding — do not number findings (they are merged into a larger document later).
Order findings by severity: `[H]` first, then `[M]`, then `[L]`.

Severity meanings:
- `[H]` — structural problem, real defect, or design that will compound as the code grows.
- `[M]` — localized design flaw, trap for the next maintainer, or robustness gap.
- `[L]` — hygiene, naming, consistency, minor duplication.

```markdown
## <Area name>

**Files:** `path/one.cpp`, `path/one.h`, ...

**Assessment:** 2–4 sentences on the overall health of this slice — what is well built, what
the dominant weakness is.

### [H] Short imperative title
`src/path/File.cpp:120` — What is wrong, why it matters concretely (what breaks or what gets
harder), and the direction of a fix in one sentence. Include a snippet only if 1–5 lines are
essential to understanding.

### [M] Another title
...

### [L] Convention and hygiene items
Group all small style/naming items as bullets under a single heading like this, one line
each with a `path:line`.

**Observed outside slice:** (omit if empty) one line each, `path:line` + one sentence.
```

Keep the part file under about 200 lines. Do not add a document title (`#`), a date, or a
table of contents — the parent agent assembles those.

## What to return in your final response

Keep it to about 10 lines:

1. The path of the part file you wrote.
2. Counts: `H=<n> M=<n> L=<n>`.
3. Your three most important findings, one line each.
4. Anything that blocked you (a file you could not read, a slice list that did not match
   reality, etc.).
