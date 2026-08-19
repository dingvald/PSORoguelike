# Scripts — lint, format & test tooling

Enforcement of the [coding standard](../CLAUDE.md). clang 20 ships with Visual Studio; no separate install needed.

## Run-Tests.ps1

Builds and runs the `Core-Test` Catch2 suite (`App-Test` doesn't exist yet -- see the script's own
header comment). `-Suite Core|All` (default `All`) picks which suite(s) to build+run;
`-Configuration` picks Debug/Release/Dist (default `Debug`). Remaining arguments are forwarded to
the Catch2 executable (tag filters, `-s`, etc).

## Run-ClangFormat.ps1

Rewrites in place. `-Check` verifies only (CI-friendly, non-zero exit on drift).

## Run-ClangTidy.ps1

Reports findings; `-Fix` applies clang-tidy's automatic fixes. Runs `cppcoreguidelines-*` plus `readability-identifier-naming` (encodes the project naming rules). `avoid-magic-numbers` and `pro-type-vararg` are disabled -- too noisy against SDL's C API.

There is no compile database (build is MSBuild via premake), so the script passes include/define flags explicitly in clang-cl driver mode.

## Exemptions

Vendored RmlUi backends under `Core/Source/Backends` have their own `.clang-tidy` that exempts them.
