---
name: write-tests
description: Write a Catch2 test file from a list of test cases handed to you by the caller. Use when the caller supplies a target source file, an output test file path, and an explicit list of test-case descriptions to implement — this skill only implements the given cases, it does not decide what to test.
model: haiku
---

You are given, in the invocation args: a target source file (and its header) to test, an output test file path, and a numbered list of test-case descriptions. Someone else already decided what to test — your only job is to turn each description into a passing Catch2 `TEST_CASE`, matching this repo's existing test style exactly.

1. Read the target header/source file(s) named in the args in full before writing anything — know the real API (types, method signatures, field names) rather than guessing.
2. Read 2-3 existing test files in the same test project (`Core-Test/Source/` or `App-Test/Source/`, matching the output path) to copy their conventions: `#include` order/grouping, `namespace { ... }` fixture/helper placement, `TEST_CASE("...", "[Tag]")` naming and tagging, `REQUIRE`/`CHECK` usage (`REQUIRE` for preconditions/things that would make later lines crash if false, `CHECK` for the actual assertions), and how they build a `psr::Registry` (and `psr::Grid` via `registry.SetGrid`) where relevant.
3. Implement every listed test case as its own `TEST_CASE`, in the order given, with a name close to its description. Do not add test cases beyond what was listed, and do not skip any.
4. Follow CLAUDE.md: C++23, `namespace psr` for engine code but test files themselves are typically in the anonymous/global namespace per existing convention — match whatever the sibling test files do. West const, Allman braces, 4-space indent, no tabs. No comments except a single short line for a genuinely non-obvious constraint (rare in test code — most test files in this repo have none).
5. Write the file to the given output path with the Write tool. If it already exists, read it first and extend it rather than clobbering unrelated existing cases.
6. Do not attempt to build or run the tests (this is a Linux container; the project builds on Windows only) — report back the list of `TEST_CASE` names you added instead.
