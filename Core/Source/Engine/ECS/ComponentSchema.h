#pragma once

#include <string>
#include <vector>

namespace psr {

// The value shape a component data member accepts in entity JSON. Mirrors,
// one-for-one, the cases JsonToMeta special-cases when it deserialises a field
// (see Core/Source/Engine/ECS/JsonEntityLoader.cpp): the schema is the inverse
// of that same contract. New leaf kinds slot in here as the component set grows
// -- add a case to BuildFieldSchema (ComponentSchemaRegistrar.h) and a sub-schema
// to EntitySchemaEmitter. The last three are the recursive kinds that carry a
// nested shape in FieldSchema::children / ::enum_values.
enum class FieldKind
{
    Integer, // a plain int: a JSON number (strict -- see NameId for the string case)
    Number,  // a float/double: any JSON number (no integer restriction)
    Boolean, // a bool: a JSON true/false
    String,  // a plain std::string: any JSON string
    NameId,  // a hashed-string id (std::uint32_t): a JSON number OR a name string
    Vec2,    // { "x", "y" } ints
    Color,   // "#RRGGBB[AA]" string OR { "r","g","b","a" } channels 0-255
    Enum,    // a C++ enum authored as one of `enum_values` (a JSON string)
    Array,   // a std::vector: `children` holds exactly one entry, the element schema
    Object,  // a nested struct: `children` are its fields
};

// One authorable field. For leaf kinds only `name`/`kind` are meaningful; the
// recursive kinds also carry a nested shape:
//   Array  -> children == { <element schema> } (exactly one)
//   Object -> children == the nested struct's fields
//   Enum   -> enum_values == the authorable names, in declaration order
struct FieldSchema
{
    std::string name; // the human-readable member name, captured at registration
    FieldKind kind = FieldKind::Number;
    std::vector<FieldSchema> children;    // Array element / Object fields (empty for leaves)
    std::vector<std::string> enum_values; // Enum: allowed authorable names (empty otherwise)

    // The element schema of an Array field (children always has exactly one).
    const FieldSchema& ElementSchema() const { return children.front(); }
};

// One registered component's authorable surface. `is_tag` marks an empty
// component (registered with a type id but no data members); its JSON body must
// be `{}`. `authorable` gates whether the component may appear in entity JSON
// / editor "add component" pickers at all -- false for engine-derived-only
// components (e.g. Position, stamped at spawn time, never authored) even if
// they have real data members.
struct ComponentSchema
{
    std::string id;
    bool is_tag = false;
    bool authorable = true;
    std::vector<FieldSchema> fields;
};

// The whole registered component set, in registration order. Built by
// ComponentSchemaRegistrar as components register, then consumed by
// EntitySchemaEmitter to emit / validate against a JSON Schema.
struct EntitySchemaModel
{
    std::vector<ComponentSchema> components;
};

} // namespace psr
