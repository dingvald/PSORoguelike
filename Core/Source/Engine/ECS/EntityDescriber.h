#pragma once

#include "Engine/ECS/ComponentSchema.h"

#include <entt/entt.hpp>

#include <string>
#include <vector>

namespace psr {

// One formatted field value read off a live component instance -- the
// read-direction mirror of the FieldSchema it was built from (see
// JsonEntityLoader.cpp's JsonToMeta for the write direction this undoes).
// text is already the final display string for leaf kinds; Array/Object
// kinds additionally populate children (their elements / nested fields, in
// schema order) and leave text as a generic joined summary, since neither
// Core nor this reader knows how a caller wants to lay a nested value out.
struct FieldValue
{
    std::string name;
    FieldKind kind = FieldKind::Number;
    std::string text;
    std::vector<FieldValue> children; // Array elements / Object fields (empty for leaves)
};

// One component's worth of formatted fields (empty for a tag component).
struct ComponentValue
{
    std::string component_id;
    std::vector<FieldValue> fields;
};

// Reads schema's fields off instance (a meta_any of meta type type) into
// formatted FieldValues, mirroring JsonToMeta's per-FieldKind dispatch in
// reverse. instance must be a valid, non-empty meta_any of type -- callers
// check component presence beforehand (see Registry::DescribeEntity, which
// uses each component's "describe_fields"_hs meta func for that check).
ComponentValue DescribeComponentValue(const ComponentSchema& schema, const entt::meta_type& type,
                                      const entt::meta_any& instance);

} // namespace psr
