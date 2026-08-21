#include "Engine/ECS/EntitySchemaEmitter.h"

#include "Engine/ECS/JsonEntityLoader.h"   // EntityLoaderError
#include "Engine/ECS/JsonSchemaBuilders.h" // shared per-FieldKind sub-schemas

#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

#include <string>

namespace psr {

namespace {

    using psr::jsonschema::Allocator;
    using psr::jsonschema::Key;
    using psr::jsonschema::ObjectSchemaFromFields;
    using psr::jsonschema::StringSchema;
    using psr::jsonschema::TypedObject;
    using psr::jsonschema::Value;

    // A component's JSON body. Tags must be exactly {}; data components accept
    // only their registered fields (additionalProperties:false is what rejects a
    // misspelled field name).
    Value ComponentBodySchema(const ComponentSchema& component, Allocator& alloc)
    {
        if (component.is_tag)
        {
            Value node = TypedObject("object", alloc);
            node.AddMember("maxProperties", 0, alloc);
            return node;
        }
        return ObjectSchemaFromFields(component.fields, alloc);
    }

    // The "components" object: keyed strictly by the registered component ids,
    // each value validated against that component's body schema.
    Value ComponentsSchema(const EntitySchemaModel& model, Allocator& alloc)
    {
        Value properties{rapidjson::kObjectType};
        for (const ComponentSchema& component : model.components)
            properties.AddMember(Key(component.id, alloc), ComponentBodySchema(component, alloc), alloc);

        Value node = TypedObject("object", alloc);
        node.AddMember("additionalProperties", false, alloc);
        node.AddMember("properties", properties, alloc);
        return node;
    }

} // namespace

// One entity file: "schema_version" and a "components" object, nothing else --
// the entity's id comes from its path in the Entities directory, not from the
// document (see JsonEntityLoader.h).
rapidjson::Document BuildEntityJsonSchema(const EntitySchemaModel& model)
{
    rapidjson::Document schema{rapidjson::kObjectType};
    Allocator& alloc = schema.GetAllocator();

    schema.AddMember("$schema", Value{}.SetString("http://json-schema.org/draft-04/schema#", alloc), alloc);
    schema.AddMember("type", "object", alloc);

    // schema_version is pinned (M0 no-migration policy).
    Value version_enum{rapidjson::kArrayType};
    version_enum.PushBack(1, alloc);
    Value version{rapidjson::kObjectType};
    version.AddMember("enum", version_enum, alloc);

    Value properties{rapidjson::kObjectType};
    // Entity files may carry a "$schema" pointer back to this file for editor
    // validation/autocomplete; accepted but not required.
    properties.AddMember("$schema", StringSchema(alloc), alloc);
    properties.AddMember("schema_version", version, alloc);
    properties.AddMember("components", ComponentsSchema(model, alloc), alloc);
    schema.AddMember("properties", properties, alloc);

    schema.AddMember("additionalProperties", false, alloc);
    return schema;
}

void ValidateEntityDocument(const rapidjson::Document& document, const EntitySchemaModel& model)
{
    // schema_doc must outlive the SchemaDocument, which must outlive the
    // validator -- all local here, so they do.
    rapidjson::Document schema_doc = BuildEntityJsonSchema(model);
    rapidjson::SchemaDocument schema{schema_doc};
    rapidjson::SchemaValidator validator{schema};

    if (!document.Accept(validator))
    {
        rapidjson::StringBuffer pointer;
        validator.GetInvalidDocumentPointer().StringifyUriFragment(pointer);
        throw EntityLoaderError(std::string("entity file failed schema validation: keyword '") +
                                validator.GetInvalidSchemaKeyword() + "' at '" + pointer.GetString() + "'");
    }
}

} // namespace psr
