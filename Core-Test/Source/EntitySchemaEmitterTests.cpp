#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/EntitySchemaEmitter.h"
#include "Engine/ECS/JsonEntityLoader.h" // EntityLoaderError
#include "Engine/ECS/TypeReflection.h"
#include "Engine/Math/Color.h"
#include "Engine/Math/Vec2.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>
#include <rapidjson/document.h>

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace {

// A describable value type nested inside SampleComponent -- exercises the
// Object field kind and the array element type below.
struct SampleNested
{
    int a = 0;
    float b = 0.0f;

    template <typename TVisitor> static void Describe(TVisitor& visitor)
    {
        visitor.template Field<&SampleNested::a>("a");
        visitor.template Field<&SampleNested::b>("b");
    }
};

enum class SampleEnum
{
    Alpha,
    Beta,
};

} // namespace

// EnumNames lives in namespace psr; specialise it there for the test's enum.
template <> struct psr::EnumNames<SampleEnum>
{
    static constexpr std::array<std::pair<std::string_view, SampleEnum>, 2> kValues{{
        {"alpha", SampleEnum::Alpha},
        {"beta", SampleEnum::Beta},
    }};
};

namespace {

// One component exercising every FieldKind, plus an empty tag. The last
// three fields cover the recursive kinds (Enum / Object / Array).
struct SampleComponent
{
    int count = 0;
    std::uint32_t ref = 0;
    psr::Vec2 size;
    psr::Color tint;
    SampleEnum mode = SampleEnum::Alpha;
    SampleNested nested;
    std::vector<SampleNested> items;
    std::vector<std::uint32_t> tags; // Array of bare NameId -- a scalar-NameId array field shape
};

struct SampleTagComponent
{
};

psr::EntitySchemaModel RegisterSampleModel(entt::meta_ctx& ctx)
{
    psr::ComponentSchemaRegistrar reg{ctx};
    reg.Component<SampleComponent>("sample")
        .Data<&SampleComponent::count>("count")
        .Data<&SampleComponent::ref>("ref")
        .Data<&SampleComponent::size>("size")
        .Data<&SampleComponent::tint>("tint")
        .Data<&SampleComponent::mode>("mode")
        .Data<&SampleComponent::nested>("nested")
        .Data<&SampleComponent::items>("items")
        .Data<&SampleComponent::tags>("tags");
    reg.Component<SampleTagComponent>("sample_tag");
    return reg.Model();
}

// Walks root -> "components" to the object whose keys are the registered
// component ids.
const rapidjson::Value& ComponentsNode(const rapidjson::Document& schema) { return schema["properties"]["components"]; }

rapidjson::Document Parse(const char* json)
{
    rapidjson::Document doc;
    doc.Parse(json);
    REQUIRE_FALSE(doc.HasParseError());
    return doc;
}

} // namespace

TEST_CASE("ComponentSchemaRegistrar captures component ids, tag-ness, field names and kinds", "[EntitySchema]")
{
    entt::meta_ctx ctx;
    const psr::EntitySchemaModel model = RegisterSampleModel(ctx);

    REQUIRE(model.components.size() == 2);

    const psr::ComponentSchema& sample = model.components[0];
    CHECK(sample.id == "sample");
    CHECK_FALSE(sample.is_tag);
    REQUIRE(sample.fields.size() == 8);
    CHECK(sample.fields[0].name == "count");
    CHECK(sample.fields[0].kind == psr::FieldKind::Integer);
    CHECK(sample.fields[1].name == "ref");
    CHECK(sample.fields[1].kind == psr::FieldKind::NameId);
    CHECK(sample.fields[2].kind == psr::FieldKind::Vec2);
    CHECK(sample.fields[3].kind == psr::FieldKind::Color);

    // Enum: kind plus the authorable names in declaration order.
    CHECK(sample.fields[4].name == "mode");
    CHECK(sample.fields[4].kind == psr::FieldKind::Enum);
    REQUIRE(sample.fields[4].enum_values.size() == 2);
    CHECK(sample.fields[4].enum_values[0] == "alpha");
    CHECK(sample.fields[4].enum_values[1] == "beta");

    // Object: kind plus recursively captured child fields.
    CHECK(sample.fields[5].name == "nested");
    CHECK(sample.fields[5].kind == psr::FieldKind::Object);
    REQUIRE(sample.fields[5].children.size() == 2);
    CHECK(sample.fields[5].children[0].name == "a");
    CHECK(sample.fields[5].children[0].kind == psr::FieldKind::Integer);
    CHECK(sample.fields[5].children[1].kind == psr::FieldKind::Number);

    // Array: kind plus exactly one element schema (an Object here).
    CHECK(sample.fields[6].name == "items");
    CHECK(sample.fields[6].kind == psr::FieldKind::Array);
    REQUIRE(sample.fields[6].children.size() == 1);
    CHECK(sample.fields[6].ElementSchema().kind == psr::FieldKind::Object);
    CHECK(sample.fields[6].ElementSchema().children.size() == 2);

    // Array: kind plus exactly one element schema (a bare NameId here).
    CHECK(sample.fields[7].name == "tags");
    CHECK(sample.fields[7].kind == psr::FieldKind::Array);
    REQUIRE(sample.fields[7].children.size() == 1);
    CHECK(sample.fields[7].ElementSchema().kind == psr::FieldKind::NameId);

    const psr::ComponentSchema& tag = model.components[1];
    CHECK(tag.id == "sample_tag");
    CHECK(tag.is_tag);
    CHECK(tag.fields.empty());
}

TEST_CASE("BuildEntityJsonSchema emits a strict, kind-appropriate schema", "[EntitySchema]")
{
    entt::meta_ctx ctx;
    const psr::EntitySchemaModel model = RegisterSampleModel(ctx);
    const rapidjson::Document schema = psr::BuildEntityJsonSchema(model);

    // Top-level pins schema_version and only allows the two known components.
    CHECK(schema["properties"]["schema_version"]["enum"][0].GetInt() == 1);

    const rapidjson::Value& components = ComponentsNode(schema);
    CHECK(components["additionalProperties"].GetBool() == false);
    REQUIRE(components["properties"].HasMember("sample"));
    REQUIRE(components["properties"].HasMember("sample_tag"));

    const rapidjson::Value& body = components["properties"]["sample"];
    CHECK(body["additionalProperties"].GetBool() == false);

    const rapidjson::Value& fields = body["properties"];
    CHECK(fields["count"]["type"] == "integer");
    CHECK(fields["ref"]["oneOf"].Size() == 2); // integer OR string
    CHECK(fields["size"]["properties"].HasMember("x"));
    CHECK(fields["tint"]["oneOf"].Size() == 2); // hex string OR {r,g,b,a}

    // Enum: a string restricted to the registered constant names.
    CHECK(fields["mode"]["type"] == "string");
    REQUIRE(fields["mode"]["enum"].Size() == 2);
    CHECK(fields["mode"]["enum"][0] == "alpha");
    CHECK(fields["mode"]["enum"][1] == "beta");

    // Object: a closed nested object of its own fields.
    CHECK(fields["nested"]["type"] == "object");
    CHECK(fields["nested"]["additionalProperties"].GetBool() == false);
    CHECK(fields["nested"]["properties"]["a"]["type"] == "integer");
    CHECK(fields["nested"]["properties"]["b"]["type"] == "number");

    // Array: an array whose items validate against the element (object) schema.
    CHECK(fields["items"]["type"] == "array");
    CHECK(fields["items"]["items"]["type"] == "object");
    CHECK(fields["items"]["items"]["properties"]["a"]["type"] == "integer");

    // Array of bare NameId: an array whose items are integer-OR-string (same
    // oneOf shape as a scalar NameId field, just wrapped in "items").
    CHECK(fields["tags"]["type"] == "array");
    CHECK(fields["tags"]["items"]["oneOf"].Size() == 2);

    // Tag body admits only {}.
    CHECK(components["properties"]["sample_tag"]["maxProperties"].GetInt() == 0);
}

TEST_CASE("ValidateEntityDocument accepts a conforming document", "[EntitySchema]")
{
    entt::meta_ctx ctx;
    const psr::EntitySchemaModel model = RegisterSampleModel(ctx);

    rapidjson::Document doc = Parse(R"({
        "schema_version": 1,
        "components": {
            "sample": {
                "count": 3,
                "ref": "some_name",
                "size": { "x": 16, "y": 24 },
                "tint": "#a1b2c3",
                "mode": "beta",
                "nested": { "a": 1, "b": 2.5 },
                "items": [ { "a": 3, "b": 4.0 }, { "a": 5 } ],
                "tags": [ "held", "held" ]
            },
            "sample_tag": {}
        }
    })");

    REQUIRE_NOTHROW(psr::ValidateEntityDocument(doc, model));
}

TEST_CASE("ValidateEntityDocument rejects non-conforming documents", "[EntitySchema]")
{
    entt::meta_ctx ctx;
    const psr::EntitySchemaModel model = RegisterSampleModel(ctx);

    SECTION("unknown component id")
    {
        rapidjson::Document doc = Parse(R"({ "schema_version": 1,
            "components": { "nope": {} } })");
        REQUIRE_THROWS_AS(psr::ValidateEntityDocument(doc, model), psr::EntityLoaderError);
    }

    SECTION("unknown field name")
    {
        rapidjson::Document doc = Parse(R"({ "schema_version": 1,
            "components": { "sample": { "bogus": 1 } } })");
        REQUIRE_THROWS_AS(psr::ValidateEntityDocument(doc, model), psr::EntityLoaderError);
    }

    SECTION("string on a strict integer field")
    {
        rapidjson::Document doc = Parse(R"({ "schema_version": 1,
            "components": { "sample": { "count": "oops" } } })");
        REQUIRE_THROWS_AS(psr::ValidateEntityDocument(doc, model), psr::EntityLoaderError);
    }

    SECTION("color channel out of range")
    {
        rapidjson::Document doc = Parse(R"({ "schema_version": 1,
            "components": { "sample": { "tint": { "r": 256 } } } })");
        REQUIRE_THROWS_AS(psr::ValidateEntityDocument(doc, model), psr::EntityLoaderError);
    }

    SECTION("extra axis on a Vec2 field")
    {
        rapidjson::Document doc = Parse(R"({ "schema_version": 1,
            "components": { "sample": { "size": { "x": 1, "y": 2, "z": 3 } } } })");
        REQUIRE_THROWS_AS(psr::ValidateEntityDocument(doc, model), psr::EntityLoaderError);
    }

    SECTION("unknown enum value")
    {
        rapidjson::Document doc = Parse(R"({ "schema_version": 1,
            "components": { "sample": { "mode": "gamma" } } })");
        REQUIRE_THROWS_AS(psr::ValidateEntityDocument(doc, model), psr::EntityLoaderError);
    }

    SECTION("wrong element type in an array")
    {
        rapidjson::Document doc = Parse(R"({ "schema_version": 1,
            "components": { "sample": { "items": [ { "a": "x" } ] } } })");
        REQUIRE_THROWS_AS(psr::ValidateEntityDocument(doc, model), psr::EntityLoaderError);
    }

    SECTION("wrong element type in an array of bare NameId")
    {
        rapidjson::Document doc = Parse(R"({ "schema_version": 1,
            "components": { "sample": { "tags": [ true ] } } })");
        REQUIRE_THROWS_AS(psr::ValidateEntityDocument(doc, model), psr::EntityLoaderError);
    }

    SECTION("unknown key in a nested object")
    {
        rapidjson::Document doc = Parse(R"({ "schema_version": 1,
            "components": { "sample": { "nested": { "c": 1 } } } })");
        REQUIRE_THROWS_AS(psr::ValidateEntityDocument(doc, model), psr::EntityLoaderError);
    }

    SECTION("non-empty tag body")
    {
        rapidjson::Document doc = Parse(R"({ "schema_version": 1,
            "components": { "sample_tag": { "x": 1 } } })");
        REQUIRE_THROWS_AS(psr::ValidateEntityDocument(doc, model), psr::EntityLoaderError);
    }

    SECTION("wrong schema_version")
    {
        rapidjson::Document doc = Parse(R"({ "schema_version": 2,
            "components": { "sample_tag": {} } })");
        REQUIRE_THROWS_AS(psr::ValidateEntityDocument(doc, model), psr::EntityLoaderError);
    }
}
