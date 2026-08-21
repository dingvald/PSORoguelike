#pragma once

#include "Engine/ECS/ComponentSchema.h"

#include <rapidjson/document.h>

#include <string>
#include <vector>

namespace psr::jsonschema {

// Draft-04 sub-schema builders shared by every FieldKind, factored out of the
// entity emitter so any future schema emitter reuses one implementation of
// "what JSON shape does this FieldKind accept". Each function allocates into
// the caller's document allocator; keys coming from the model are copied (a
// StringRef to a temporary std::string would dangle -- see Key).

using Allocator = rapidjson::Document::AllocatorType;
using Value = rapidjson::Value;

// A dynamic (std::string) key copied into the document's allocator. Static
// string-literal keys stay GenericStringRef at the call site (static lifetime);
// only object *names* coming from the model go through this.
inline Value Key(const std::string& text, Allocator& alloc)
{
    return Value{text.c_str(), static_cast<rapidjson::SizeType>(text.size()), alloc};
}

inline Value TypedObject(const char* type, Allocator& alloc)
{
    Value node{rapidjson::kObjectType};
    node.AddMember("type", Value{}.SetString(type, alloc), alloc);
    return node;
}

// { "type": "integer" }, reused for Vec2 axes and standalone integer fields.
inline Value IntegerSchema(Allocator& alloc) { return TypedObject("integer", alloc); }

// { "type": "number" }, for float/double fields -- any JSON number.
inline Value NumberSchema(Allocator& alloc) { return TypedObject("number", alloc); }

// { "type": "boolean" }, for bool fields -- a JSON true/false.
inline Value BooleanSchema(Allocator& alloc) { return TypedObject("boolean", alloc); }

// { "type": "string" }, for plain std::string fields -- any JSON string.
inline Value StringSchema(Allocator& alloc) { return TypedObject("string", alloc); }

// An {x,y} object of integer axes, closed to other keys. Omitted axes are
// legal (loader defaults them to 0), so none are required.
inline Value Vec2Schema(Allocator& alloc)
{
    Value properties{rapidjson::kObjectType};
    properties.AddMember("x", IntegerSchema(alloc), alloc);
    properties.AddMember("y", IntegerSchema(alloc), alloc);

    Value node = TypedObject("object", alloc);
    node.AddMember("additionalProperties", false, alloc);
    node.AddMember("properties", properties, alloc);
    return node;
}

// A 0-255 integer colour channel.
inline Value ColorChannelSchema(Allocator& alloc)
{
    Value node = IntegerSchema(alloc);
    node.AddMember("minimum", 0, alloc);
    node.AddMember("maximum", 255, alloc);
    return node;
}

// Color := a "#RRGGBB[AA]" hex string OR an {r,g,b,a} object of 0-255 channels
// (a defaults to 255 when omitted), matching MakeColor/ReadChannel.
inline Value ColorSchema(Allocator& alloc)
{
    Value hex = TypedObject("string", alloc);
    hex.AddMember("pattern", Value{}.SetString("^#([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$", alloc), alloc);

    Value channels{rapidjson::kObjectType};
    channels.AddMember("r", ColorChannelSchema(alloc), alloc);
    channels.AddMember("g", ColorChannelSchema(alloc), alloc);
    channels.AddMember("b", ColorChannelSchema(alloc), alloc);
    channels.AddMember("a", ColorChannelSchema(alloc), alloc);

    Value object = TypedObject("object", alloc);
    object.AddMember("additionalProperties", false, alloc);
    object.AddMember("properties", channels, alloc);

    Value one_of{rapidjson::kArrayType};
    one_of.PushBack(hex, alloc);
    one_of.PushBack(object, alloc);

    Value node{rapidjson::kObjectType};
    node.AddMember("oneOf", one_of, alloc);
    return node;
}

// A hashed-string name-id field accepts either a JSON number or a name string
// (the loader hashes the string); mirrors JsonToMeta's arithmetic string path
// for fields like texture_id.
inline Value NameIdSchema(Allocator& alloc)
{
    Value one_of{rapidjson::kArrayType};
    one_of.PushBack(IntegerSchema(alloc), alloc);
    one_of.PushBack(TypedObject("string", alloc), alloc);

    Value node{rapidjson::kObjectType};
    node.AddMember("oneOf", one_of, alloc);
    return node;
}

// FieldSchemaFor and ObjectSchemaFromFields are mutually recursive: a nested
// Object field emits an object-of-fields, whose fields may themselves be
// arrays/objects/enums.
inline Value FieldSchemaFor(const FieldSchema& field, Allocator& alloc);

// A closed object of the given fields: `additionalProperties:false` (rejects a
// misspelled key) plus a recursive sub-schema per field. Shared by component
// bodies and nested Object fields.
inline Value ObjectSchemaFromFields(const std::vector<FieldSchema>& fields, Allocator& alloc)
{
    Value properties{rapidjson::kObjectType};
    for (const FieldSchema& field : fields)
        properties.AddMember(Key(field.name, alloc), FieldSchemaFor(field, alloc), alloc);

    Value node = TypedObject("object", alloc);
    node.AddMember("additionalProperties", false, alloc);
    node.AddMember("properties", properties, alloc);
    return node;
}

// A JSON array whose every element validates against the element sub-schema.
inline Value ArraySchema(const FieldSchema& element, Allocator& alloc)
{
    Value node = TypedObject("array", alloc);
    node.AddMember("items", FieldSchemaFor(element, alloc), alloc);
    return node;
}

// An enum: a string restricted to the registered constant names.
inline Value EnumSchema(const std::vector<std::string>& names, Allocator& alloc)
{
    Value values{rapidjson::kArrayType};
    for (const std::string& name : names)
        values.PushBack(Key(name, alloc), alloc);

    Value node = TypedObject("string", alloc);
    node.AddMember("enum", values, alloc);
    return node;
}

inline Value FieldSchemaFor(const FieldSchema& field, Allocator& alloc)
{
    switch (field.kind)
    {
    case FieldKind::Integer:
        return IntegerSchema(alloc);
    case FieldKind::Number:
        return NumberSchema(alloc);
    case FieldKind::Boolean:
        return BooleanSchema(alloc);
    case FieldKind::String:
        return StringSchema(alloc);
    case FieldKind::NameId:
        return NameIdSchema(alloc);
    case FieldKind::Vec2:
        return Vec2Schema(alloc);
    case FieldKind::Color:
        return ColorSchema(alloc);
    case FieldKind::Enum:
        return EnumSchema(field.enum_values, alloc);
    case FieldKind::Array:
        return ArraySchema(field.ElementSchema(), alloc);
    case FieldKind::Object:
        return ObjectSchemaFromFields(field.children, alloc);
    }
    return Value{rapidjson::kObjectType}; // unreachable; every kind handled above
}

} // namespace psr::jsonschema
