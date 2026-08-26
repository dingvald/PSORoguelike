#pragma once

#include <memory>

namespace psr {

// Forward-declared, not #included: IAction.h includes ActionResult.h (its
// Perform() returns ActionResult by value), so the reverse include would
// cycle. ActionResult.cpp #includes IAction.h so the out-of-line
// destructor/move-assignment below are defined somewhere IAction is a
// complete type.
class IAction;

// What performing an IAction produced: the turn cost to apply (the
// executor's job, not the action's -- see IAction.h), and optionally a
// fallback action to try instead when this one couldn't be completed.
struct ActionResult
{
    int cost = 0;
    std::unique_ptr<IAction> fallback; // null => nothing more to attempt

    // A user-declared constructor here (even one that's `= default`) makes
    // this a non-aggregate under C++20's aggregate rules, so brace-init with
    // designated initializers isn't available -- construct via this
    // constructor instead (fallback defaults to "none").
    explicit ActionResult(int cost, std::unique_ptr<IAction> fallback = nullptr)
        : cost(cost), fallback(std::move(fallback))
    {
    }

    ActionResult(ActionResult&&) = default;  // unique_ptr<Incomplete> move-ctor never deletes -- fine as-is
    ~ActionResult();                         // defined in ActionResult.cpp, where IAction is complete
    ActionResult& operator=(ActionResult&&); // move-assign can delete the replaced fallback -- same reason
};

} // namespace psr
