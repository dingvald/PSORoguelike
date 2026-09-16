#pragma once

#include <string>

namespace psr {

struct MagComponent;

// Evaluates one MagEvolutionRule::condition against mag's current stat
// levels -- a small comparison expression such as "POW + DEX > MIND + DEF",
// authored per rule (see MagComponent.h). Grammar: a single top-level
// comparison (> < >= <= == !=) of two arithmetic expressions (+ - * /,
// parentheses, unary minus) over the identifiers POW/DEF/DEX/MIND (this
// mag's four stat levels), LEVEL (MagLevel(mag)), and IQ. Case-insensitive
// identifiers, integer or decimal literals.
//
// Returns false (rather than throwing) for a malformed condition -- a
// content-authoring mistake should mean "this evolution never triggers", not
// a runtime crash. No expression-parsing library exists in this repo
// (checked vcpkg.json), so this is a small, self-contained parser scoped
// deliberately to Mag evolution -- nothing else in the engine needs one yet.
bool EvaluateMagEvolutionCondition(const std::string& condition, const MagComponent& mag);

} // namespace psr
