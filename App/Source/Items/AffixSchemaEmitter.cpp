#include "Items/AffixSchemaEmitter.h"

#include "Engine/ECS/JsonSchemaBuilders.h"
#include "Items/AffixError.h"
#include "Items/AffixLibraryFile.h" // kAffixLibraryVersion

#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

#include <string>

namespace psr {

namespace {

    using psr::jsonschema::Allocator;
    using psr::jsonschema::FieldSchemaFor;
    using psr::jsonschema::Key;
    using psr::jsonschema::Value;

} // namespace

rapidjson::Document BuildAffixJsonSchema(const AffixSchemaModel& model)
{
    rapidjson::Document schema{rapidjson::kObjectType};
    Allocator& alloc = schema.GetAllocator();

    schema.AddMember("$schema", Value{}.SetString("http://json-schema.org/draft-04/schema#", alloc), alloc);
    schema.AddMember("type", "object", alloc);

    Value version_enum{rapidjson::kArrayType};
    version_enum.PushBack(kAffixLibraryVersion, alloc);
    Value version{rapidjson::kObjectType};
    version.AddMember("enum", version_enum, alloc);

    Value properties{rapidjson::kObjectType};
    for (const FieldSchema& field : model.fields)
        properties.AddMember(Key(field.name, alloc), FieldSchemaFor(field, alloc), alloc);
    properties.AddMember("schema_version", version, alloc);
    schema.AddMember("properties", properties, alloc);

    schema.AddMember("additionalProperties", false, alloc);
    return schema;
}

void ValidateAffixDocument(const rapidjson::Document& document, const AffixSchemaModel& model)
{
    // schema_doc must outlive the SchemaDocument, which must outlive the
    // validator -- all local here, so they do.
    rapidjson::Document schema_doc = BuildAffixJsonSchema(model);
    rapidjson::SchemaDocument schema{schema_doc};
    rapidjson::SchemaValidator validator{schema};

    if (!document.Accept(validator))
    {
        rapidjson::StringBuffer pointer;
        validator.GetInvalidDocumentPointer().StringifyUriFragment(pointer);
        throw AffixError(std::string("affix file failed schema validation: keyword '") +
                         validator.GetInvalidSchemaKeyword() + "' at '" + pointer.GetString() + "'");
    }
}

} // namespace psr
