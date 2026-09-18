#include "Engine/ECS/ComponentJson.h"

#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/Math/Color.h"
#include "Engine/Math/Vec2.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string_view>

namespace psr {

using namespace entt::literals;

namespace {

    // Reads an integer axis (defaulting to 0 when absent), so authors only spell
    // out the components they care about of a Vec2 and the rest fall back to the
    // struct's own defaults.
    int ReadAxis(const rapidjson::Value& object, const char* name)
    {
        auto member = object.FindMember(name);
        if (member == object.MemberEnd())
            return 0;
        if (!member->value.IsInt())
            throw EntityLoaderError(std::string("JsonToMeta: axis '") + name + "' must be an integer");
        return member->value.GetInt();
    }

    std::uint8_t ReadChannel(const rapidjson::Value& object, const char* name, std::uint8_t fallback)
    {
        auto member = object.FindMember(name);
        if (member == object.MemberEnd())
            return fallback;
        if (!member->value.IsUint() || member->value.GetUint() > 255)
            throw EntityLoaderError(std::string("JsonToMeta: color channel '") + name + "' must be 0-255");
        return static_cast<std::uint8_t>(member->value.GetUint());
    }

    entt::meta_any MakeColor(const rapidjson::Value& json, entt::meta_ctx& ctx)
    {
        if (json.IsString())
            return entt::meta_any{ctx, Color{std::string_view{json.GetString(), json.GetStringLength()}}};
        if (json.IsObject())
            return entt::meta_any{ctx, Color{ReadChannel(json, "r", 0), ReadChannel(json, "g", 0),
                                             ReadChannel(json, "b", 0), ReadChannel(json, "a", 255)}};
        throw EntityLoaderError("JsonToMeta: a Color field must be a \"#RRGGBB[AA]\" string or an {r,g,b,a} object");
    }

    // Every leaf kind that isn't a Vec2/Color/String rides entt's
    // arithmetic/enum conversion helper -- meta_any's plain cast<T>() only
    // succeeds on an exact type match, so the underlying arithmetic width must
    // be normalised through allow_cast<double>() first. Mirrors
    // EntityDescriber.cpp's own AsDouble, which this duplicates rather than
    // shares (that TU's copy stays private/display-only; this one feeds JSON
    // numbers, not display text).
    double AsDouble(const entt::meta_any& raw)
    {
        entt::meta_any converted = raw.allow_cast<double>();
        return converted ? converted.cast<double>() : 0.0;
    }

    rapidjson::Value BuildFieldJson(const FieldSchema& field, const entt::meta_any& raw,
                                    rapidjson::Document::AllocatorType& allocator);

    rapidjson::Value BuildFieldsJson(const std::vector<FieldSchema>& fields, const entt::meta_type& type,
                                     const entt::meta_any& instance, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value object(rapidjson::kObjectType);
        for (const FieldSchema& field : fields)
        {
            entt::meta_data data = type.data(entt::hashed_string::value(field.name.c_str()));
            if (!data)
                continue; // schema/meta out of sync -- skip rather than crash, same as EntityDescriber
            entt::meta_any raw = data.get(instance);
            if (!raw)
                continue;
            object.AddMember(rapidjson::Value(field.name.c_str(), allocator),
                             BuildFieldJson(field, raw, allocator), allocator);
        }
        return object;
    }

    rapidjson::Value BuildFieldJson(const FieldSchema& field, const entt::meta_any& raw,
                                    rapidjson::Document::AllocatorType& allocator)
    {
        switch (field.kind)
        {
        case FieldKind::Vec2:
        {
            const Vec2 value = raw.cast<Vec2>();
            rapidjson::Value object(rapidjson::kObjectType);
            object.AddMember("x", value.x, allocator);
            object.AddMember("y", value.y, allocator);
            return object;
        }
        case FieldKind::Color:
        {
            const Color value = raw.cast<Color>();
            char hex[10];
            std::snprintf(hex, sizeof(hex), "#%02X%02X%02X%02X", value.r, value.g, value.b, value.a);
            return rapidjson::Value(hex, allocator);
        }
        case FieldKind::String:
            return rapidjson::Value(raw.cast<std::string>().c_str(), allocator);
        case FieldKind::Boolean:
            return rapidjson::Value(AsDouble(raw) != 0.0);
        case FieldKind::Integer:
            return rapidjson::Value(static_cast<std::int64_t>(std::llround(AsDouble(raw))));
        case FieldKind::Number:
            return rapidjson::Value(AsDouble(raw));
        case FieldKind::NameId:
        {
            const auto hash = static_cast<std::uint32_t>(std::llround(AsDouble(raw)));
            if (std::optional<std::string> label = NameIdRegistry::Find(hash))
                return rapidjson::Value(label->c_str(), allocator);
            return rapidjson::Value(static_cast<std::uint32_t>(hash));
        }
        case FieldKind::Enum:
        {
            const entt::meta_type enum_type = raw.type();
            for (const std::string& name : field.enum_values)
            {
                entt::meta_data constant = enum_type.data(entt::hashed_string::value(name.c_str()));
                if (constant && constant.get({}) == raw)
                    return rapidjson::Value(name.c_str(), allocator);
            }
            throw EntityLoaderError("FieldsToJson: enum field '" + field.name +
                                    "' has a value matching none of its authorable names");
        }
        case FieldKind::Array:
        {
            rapidjson::Value array(rapidjson::kArrayType);
            if (entt::meta_sequence_container view = raw.as_sequence_container())
                for (auto it = view.begin(), end = view.end(); it != end; ++it)
                    array.PushBack(BuildFieldJson(field.ElementSchema(), *it, allocator), allocator);
            return array;
        }
        case FieldKind::Object:
            return BuildFieldsJson(field.children, raw.type(), raw, allocator);
        }
        throw EntityLoaderError("FieldsToJson: unhandled FieldKind for field '" + field.name + "'");
    }

    // Builds a meta value of target's type from a JSON value, ready to hand to
    // meta_data::set / the component's "emplace"_hs. Recurses for object-shaped
    // fields. The two Core value types (Color/Vec2) are matched by type_info and
    // built directly; everything arithmetic rides entt's built-in numeric
    // conversion (we hand set() a double and it lands in the field's exact
    // type); any other object-shaped field falls back to generic, meta-driven
    // field-by-field construction.
    entt::meta_any JsonToMetaImpl(const entt::meta_type& target, const rapidjson::Value& json, entt::meta_ctx& ctx)
    {
        const entt::type_info& info = target.info();

        if (info == entt::type_id<Color>())
            return MakeColor(json, ctx);

        if (info == entt::type_id<Vec2>())
        {
            if (!json.IsObject())
                throw EntityLoaderError("JsonToMeta: a Vec2 field must be an {x,y} object");
            return entt::meta_any{ctx, Vec2{ReadAxis(json, "x"), ReadAxis(json, "y")}};
        }

        if (info == entt::type_id<std::string>())
        {
            if (!json.IsString())
                throw EntityLoaderError("JsonToMeta: a string field must be a JSON string");
            return entt::meta_any{ctx, std::string{json.GetString(), json.GetStringLength()}};
        }

        // An enum is authored as one of its registered constant names; the loader
        // hashes the string to the same id the constant was registered under (see
        // EnsureEnumRegistered) and reads the value straight off the meta type.
        if (target.is_enum())
        {
            if (!json.IsString())
                throw EntityLoaderError("JsonToMeta: an enum field must be a name string");
            entt::meta_data constant = target.data(entt::hashed_string::value(json.GetString()));
            if (!constant)
                throw EntityLoaderError(std::string("JsonToMeta: unknown enum value '") + json.GetString() + "'");
            // A default meta_handle is the "no instance" argument for reading a
            // static datum (the enum constant); the getter ignores it.
            entt::meta_any value = constant.get(entt::meta_handle{});
            if (!value)
                throw EntityLoaderError(std::string("JsonToMeta: could not read enum value '") + json.GetString() +
                                        "'");
            return value;
        }

        if (target.is_arithmetic())
        {
            // A string on a numeric field is hashed (entt::hashed_string) -- the
            // convention for name-as-id fields like a texture_id member. entt
            // converts the double we build into the field's exact arithmetic
            // type on set(). The hash is one-way, so capture the source string
            // here -- the only place it's still alive -- into NameIdRegistry,
            // letting later UI/debug code turn the id back into a label.
            if (json.IsString())
            {
                const std::uint32_t hash = entt::hashed_string::value(json.GetString());
                NameIdRegistry::Register(hash, json.GetString());
                return entt::meta_any{ctx, static_cast<double>(hash)};
            }
            if (json.IsBool())
                return entt::meta_any{ctx, json.GetBool() ? 1.0 : 0.0};
            if (json.IsNumber())
                return entt::meta_any{ctx, json.GetDouble()};
            throw EntityLoaderError("JsonToMeta: a numeric field must be a number, bool, or name string");
        }

        // A std::vector field: default-construct the container and push each JSON
        // element (recursively deserialised to the element type) through entt's
        // sequence-container view. The vector's meta type was reflected with
        // <entt/meta/container.hpp> in scope (see ComponentSchemaRegistrar), which
        // is what makes the view available here.
        if (json.IsArray())
        {
            entt::meta_any value = target.construct();
            entt::meta_sequence_container view = value.as_sequence_container();
            if (!view)
                throw EntityLoaderError("JsonToMeta: array field is not a registered sequence container");

            const entt::meta_type element = view.value_type();
            view.reserve(json.Size());
            for (const rapidjson::Value& item : json.GetArray())
            {
                if (!view.insert(view.end(), JsonToMetaImpl(element, item, ctx)))
                    throw EntityLoaderError("JsonToMeta: could not append an array element");
            }
            return value;
        }

        if (json.IsObject())
        {
            entt::meta_any value = target.construct();
            if (!value)
                throw EntityLoaderError("JsonToMeta: field type is not default-constructible / not registered");

            for (auto field = json.MemberBegin(); field != json.MemberEnd(); ++field)
            {
                entt::meta_data data = target.data(entt::hashed_string::value(field->name.GetString()));
                if (!data)
                    throw EntityLoaderError(std::string("JsonToMeta: unknown field '") + field->name.GetString() +
                                            "'");
                if (!data.set(value, JsonToMetaImpl(data.type(), field->value, ctx)))
                    throw EntityLoaderError(std::string("JsonToMeta: could not assign field '") +
                                            field->name.GetString() + "'");
            }
            return value;
        }

        throw EntityLoaderError("JsonToMeta: unsupported field type for the given JSON value");
    }

} // namespace

entt::meta_any JsonToMeta(const entt::meta_type& target, const rapidjson::Value& json, entt::meta_ctx& ctx)
{
    return JsonToMetaImpl(target, json, ctx);
}

void EmplaceComponentFromJson(const entt::meta_type& type, entt::registry& registry, entt::entity entity,
                              const rapidjson::Value& body, entt::meta_ctx& ctx)
{
    if (!body.IsObject())
        throw EntityLoaderError("EmplaceComponentFromJson: a component body must be an object");

    entt::meta_any value = JsonToMeta(type, body, ctx);
    entt::meta_any instance{entt::meta_ctx_arg, ctx};
    type.invoke("emplace"_hs, instance, entt::forward_as_meta(ctx, registry), entity, value);
}

rapidjson::Value FieldsToJson(const std::vector<FieldSchema>& fields, const entt::meta_type& type,
                              const entt::meta_any& instance, rapidjson::Document::AllocatorType& allocator)
{
    return BuildFieldsJson(fields, type, instance, allocator);
}

} // namespace psr
