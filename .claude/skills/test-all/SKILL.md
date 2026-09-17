---
name: test-all
description: Build and run both the Core-Test and App-Test Catch2 suites. Use when the user asks to run all tests, run the test suite, or just "run the tests" without specifying Core or App.
model: haiku
---

Build and run both suites via the existing helper script — do not hand-roll an MSBuild/exe invocation, `Scripts/Run-Tests.ps1` already handles MSBuild discovery, building each `.vcxproj`, and locating the output exes.

```powershell
./Scripts/Run-Tests.ps1
```

(`-Suite All` is the default, so it can be omitted; pass it explicitly if you prefer.)

- Default configuration is `Debug`; pass `-Configuration Release` or `-Configuration Dist` if the user asks for a different one.
- Forward any tag filter or Catch2 flag the user gives verbatim as trailing args — it applies to both suites.
- Run from the repository root.
- The script builds+runs Core-Test first, then App-Test, and continues to the second suite even if the first fails, exiting non-zero if either failed. Report per-suite pass/fail; on failure show the failing test case names/assertions, not the full log.
