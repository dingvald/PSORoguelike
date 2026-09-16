---
name: test-core
description: Build and run the Core-Test Catch2 suite (engine-level tests). Use when the user asks to run Core tests, engine tests, or "test Core".
model: haiku
---

Build and run the `Core-Test` suite via the existing helper script — do not hand-roll an MSBuild/exe invocation, `Scripts/Run-Tests.ps1` already handles MSBuild discovery, building the right `.vcxproj`, and locating the output exe.

```powershell
./Scripts/Run-Tests.ps1 -Suite Core
```

- Default configuration is `Debug`; pass `-Configuration Release` or `-Configuration Dist` if the user asks for a different one.
- Forward any tag filter or Catch2 flag the user gives verbatim as trailing args, e.g.:
  ```powershell
  ./Scripts/Run-Tests.ps1 -Suite Core "[MessageBus]" -s
  ```
- Run from the repository root.
- On failure, report the failing test case names/assertions from the Catch2 output, not the full log. On success, a one-line pass summary is enough.
