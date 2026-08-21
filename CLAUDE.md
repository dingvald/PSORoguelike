# Coding Standard

Follow the [C++ Core Guidelines](https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md) — the source of truth for design/correctness (RAII, ownership, `const`-correctness, no raw `new`/`delete`, prefer std). Cite rule numbers when relevant (`R.1`, `ES.20`). When editing existing code, match surrounding style over the rules below if they conflict.

## Conventions

- **Language:** C++23. All engine code in `namespace psr`.
- **Headers:** `#pragma once`, no include guards.
- **Types / functions / methods:** `PascalCase`.
- **Members:** `m_snake_case`; plain `snake_case` for local/`Impl` struct fields.
- **Constants / `constexpr`:** `kPascalCase`.
- **`const`:** west const (`const Application&`). **Braces:** Allman. **Indent:** 4 spaces, no tabs.
- **File-local constants/helpers:** unnamed `namespace {}`.
- **`I` prefix** (e.g. `IChunkGenerator`) is reserved for pure abstract interfaces (all-pure-virtual, no default method bodies). A base class with default bodies (e.g. `Layer`) stays unprefixed.

## Ownership

- RAII + smart pointers (`R.1`, `R.20`–`R.23`); pimpl uses `unique_ptr`. Raw `new`/`delete` only for RmlUi interfaces tied to `Rml::Shutdown()` ordering — document each with a comment.
- Explicitly delete copy/move on non-copyable resource owners (`C.21`, `C.81`).

## Enforcement

`.clang-format` + `.clang-tidy` at repo root (clang 20, bundled with VS). Run from workspace root:

```powershell
./Scripts/Run-ClangFormat.ps1        # -Check to verify only
./Scripts/Run-ClangTidy.ps1          # -Fix to auto-apply
```

Not gated on build; run manually. Tooling details in [Scripts/README.md](Scripts/README.md).

## Collaboration & Design Principles

Forward-looking rules for new work — not a mandate to retroactively refactor existing code, though flag violations if directly encountered.

1. **Division of labor.** Claude implements engine systems and editor features that support data-driven content; the user owns game content creation via the editor tools and C++ API. Claude does not author real game content (areas, enemies, items, balance, etc.) on its own initiative. Exceptions: minimal throwaway test fixtures to exercise a new system in automated tests are fine — that's scaffolding to prove the engine works, not content authoring — and if the user explicitly requests content, Claude should write it rather than defer.

2. **Every feature needs a UI/editor answer.** When implementing an engine feature, explicitly consider both the internal components/systems *and* what UI or editor support it needs to be usable by the content creator. Decide and state whether the feature needs new editor tooling — don't leave content-facing features editor-less by default.

3. **Data-driven where it pays off; `Core` stays theme-agnostic.** `Core` (the engine) should not bake in assumptions about game theme — it drives arbitrary content, the same way UnnamedRoguelike's engine does. `App`, unlike that sibling project, is explicitly PSO-inspired by design (see [docs/GDD.md](docs/GDD.md)) and is allowed to encode PSO-flavored vocabulary (class names, area themes, item types) directly. Still prefer data-driven authoring (JSON schemas, config-authored behavior) over hard-coded values wherever it lets content be added without a code change.
