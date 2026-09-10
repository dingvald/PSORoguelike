#pragma once

#include "Engine/ECS/Entity.h"

#include <entt/entt.hpp>

#include <vector>

namespace psr {

// Merges `incoming`'s ItemComponent::quantity into matching entries of
// `slots` (same PrefabIdComponent, own ItemComponent with spare room),
// mutating quantities in place. Never touches `slots` or destroys entities --
// that's the caller's job based on incoming's resulting quantity. A no-op
// (returns false) if `incoming` has no ItemComponent or its max_stack <= 1
// (not stackable) -- this is what keeps two ordinary non-stackable items
// sharing a prefab id (e.g. two identical basic swords) from being treated
// as "an existing matching slot with 0 room".
//
// `stop_after_first_match`: true = only ever consider the first matching
// slot (found or not) -- Inventory's "exactly one slot per stackable type"
// rule, where a full match must block rather than fall through to a new
// slot (see PickupAction). false = keep spilling into every matching slot
// with room -- Storage's "multiple full stacks" rule (see Storage.cpp).
//
// Returns whether a matching slot was found at all (regardless of room) --
// callers use this to decide whether opening a brand new slot is allowed.
bool MergeIntoMatchingStacks(Registry& registry, entt::entity incoming, const std::vector<entt::entity>& slots,
                              bool stop_after_first_match);

} // namespace psr
