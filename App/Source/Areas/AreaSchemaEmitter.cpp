#include "Areas/AreaSchemaEmitter.h"

#include "Areas/AreaError.h"
#include "Areas/AreaLibraryFile.h" // kAreaLibraryVersion
#include "Engine/ECS/JsonSchemaBuilders.h"

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

rapidjson::Document BuildAreaJsonSchema(const AreaSchemaModel& model)
{
    rapidjson::Document schema{rapidjson::kObjectType};
    Allocator& alloc = schema.GetAllocator();

    schema.AddMember("$schema", Value{}.SetString("http://json-schema.org/draft-04/schema#", alloc), alloc);
    schema.AddMember("type", "object", alloc);

    Value version_enum{rapidjson::kArrayType};
    version_enum.PushBack(kAreaLibraryVersion, alloc);
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

void ValidateAreaDocument(const rapidjson::Document& document, const AreaSchemaModel& model)
{
    // schema_doc must outlive the SchemaDocument, which must outlive the
    // validator -- all local here, so they do.
    rapidjson::Document schema_doc = BuildAreaJsonSchema(model);
    rapidjson::SchemaDocument schema{schema_doc};
    rapidjson::SchemaValidator validator{schema};

    if (!document.Accept(validator))
    {
        rapidjson::StringBuffer pointer;
        validator.GetInvalidDocumentPointer().StringifyUriFragment(pointer);
        throw AreaError(std::string("area file failed schema validation: keyword '") +
                        validator.GetInvalidSchemaKeyword() + "' at '" + pointer.GetString() + "'");
    }
}

} // namespace psr
