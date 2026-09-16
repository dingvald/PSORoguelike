---
name: build-and-test-all
description: Build the full Windows solution, then run both the Core-Test and App-Test suites. Use for "build and test everything", a pre-PR sanity check, or when the user asks to build and run all tests in one go.
model: haiku
---

Two-stage sanity check: full solution build, then the full test suite. Stop after stage 1 if it fails — don't run tests against a solution that didn't build.

1. **Build.** Invoke the `build-windows` skill (or follow its steps directly): locate MSBuild via `vswhere`, then build `PSORoguelike.slnx` at `Debug|x64` (or the configuration the user asked for). Check `$LASTEXITCODE`; if non-zero, report the build errors and stop — do not proceed to testing.
2. **Test.** Invoke the `test-all` skill (or run directly):
   ```powershell
   ./Scripts/Run-Tests.ps1
   ```
   Pass `-Configuration` through if a non-default one was used for the build, so the test script builds/runs the matching test binaries.
3. Summarize both stages: build result, then per-suite (Core/App) pass/fail. On any test failure, report the failing test case names/assertions, not the full log.
