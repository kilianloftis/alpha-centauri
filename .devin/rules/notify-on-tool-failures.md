---
trigger: always_on
description: Notify the user when agent tools fail instead of silently working around them
---

# Notify on Tool Failures

When an agent tool fails or is unavailable (e.g. `Write`, `StrReplace`, `Read`, `AskQuestion`, permission/ownership errors, empty/`old_string` rejections), **tell the user immediately** before continuing.

Do not silently fall back to alternate methods (shell `python`/`sed`/`tee` rewrites, skipping the tool, etc.) without first reporting:

1. Which tool failed
2. The error or failure mode (briefly)
3. What workaround you intend to use, if any

Ask before applying a workaround when the failure is unexpected or may indicate a permissions/environment problem the user should fix (e.g. root-owned source files).
