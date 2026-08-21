#pragma once

#include "Engine/ECS/ComponentSchema.h"
#include "Engine/ECS/Registry.h"

namespace psr {

// Registers every component this game defines (Core's and App's) against
// registry's entt::meta context, returning the assembled schema model for
// callers that need to introspect/validate content against it (e.g. a
// future JSON entity loader). Must run once, before any content is loaded.
EntitySchemaModel RegisterComponents(Registry& registry);

} // namespace psr
