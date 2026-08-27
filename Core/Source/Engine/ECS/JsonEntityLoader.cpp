#include "Engine/ECS/JsonEntityLoader.h"

#include "Engine/ECS/EntitySchemaEmitter.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/Math/Color.h"
#include "Engine/Math/Vec2.h"

#include <entt/entt.hpp>

#include <string>
#include <string_view>

namespace psr {

using namespace entt::literals;

namespace {

    // Top-level "schema_version" the entities document must carry (M0's explicit
    // no-migration policy, enforced by ReadJsonFile).
    constexpr int kEntitySchemaVersion = 1;

    // Reads an integer axis (defaulting to 0 when absent), so authors only spell
    // out the components they care about of a Vec2 and the rest fall back to the
    // struct's own defaults.
    int ReadAxis(const rapidjson::Value& object, const char* name)
    {
        auto member = object.FindMember(name);
        if (member == object.MemberEnd())
            return 0;
        if (!member->value.IsInt())
            throw EntityLoaderError(std::string("JsonEntityLoader: axis '") + name + "' must be an integer");
        return member->value.GetInt();
    }

    std::uint8_t ReadChannel(const rapidjson::Value& object, const char* name, std::uint8_t fallback)
    {
        auto member = object.FindMember(name);
        if (member == object.MemberEnd())
            return fallback;
        if (!member->value.IsUint() || member->value.GetUint() > 255)
            throw EntityLoaderError(std::string("JsonEntityLoader: color channel '") + name + "' must be 0-255");
        return static_cast<std::uint8_t>(member->value.GetUint());
    }

    entt::meta_any MakeColor(const rapidjson::Value& json, entt::meta_ctx& ctx)
    {
        if (json.IsString())
            return entt::meta_any{ctx, Color{std::string_view{json.GetString(), json.GetStringLength()}}};
        if (json.IsObject())
            return entt::meta_any{ctx, Color{ReadChannel(json, "r", 0), ReadChannel(json, "g", 0),
                                             ReadChannel(json, "b", 0), ReadChannel(json, "a", 255)}};
        throw EntityLoaderError(
            "JsonEntityLoader: a Color field must be a \"#RRGGBB[AA]\" string or an {r,g,b,a} object");
    }

    // Builds a meta value of target's type from a JSON value, ready to hand to
    // meta_data::set / the component's "emplace"_hs. Recurses for object-shaped
    // fields. The two Core value types (Color/Vec2) are matched by type_info and
    // built directly; everything arithmetic rides entt's built-in numeric
    // conversion (we hand set() a double and it lands in the field's exact
    // type); any other object-shaped field falls back to generic, meta-driven
    // field-by-field construction.
    entt::meta_any JsonToMeta(const entt::meta_type& target, const rapidjson::Value& json, entt::meta_ctx& ctx)
    {
        const entt::type_info& info = target.info();

        if (info == entt::type_id<Color>())
            return MakeColor(json, ctx);

        if (info == entt::type_id<Vec2>())
        {
            if (!json.IsObject())
                throw EntityLoaderError("JsonEntityLoader: a Vec2 field must be an {x,y} object");
            return entt::meta_any{ctx, Vec2{ReadAxis(json, "x"), ReadAxis(json, "y")}};
        }

        if (info == entt::type_id<std::string>())
        {
            if (!json.IsString())
                throw EntityLoaderError("JsonEntityLoader: a string field must be a JSON string");
            return entt::meta_any{ctx, std::string{json.GetString(), json.GetStringLength()}};
        }

        // An enum is authored as one of its registered constant names; the loader
        // hashes the string to the same id the constant was registered under (see
        // EnsureEnumRegistered) and reads the value straight off the meta type.
        if (target.is_enum())
        {
            if (!json.IsString())
                throw EntityLoaderError("JsonEntityLoader: an enum field must be a name string");
            entt::meta_data constant = target.data(entt::hashed_string::value(json.GetString()));
            if (!constant)
                throw EntityLoaderError(std::string("JsonEntityLoader: unknown enum value '") + json.GetString() + "'");
            // A default meta_handle is the "no instance" argument for reading a
            // static datum (the enum constant); the getter ignores it.
            entt::meta_any value = constant.get(entt::meta_handle{});
            if (!value)
                throw EntityLoaderError(std::string("JsonEntityLoader: could not read enum value '") +
                                        json.GetString() + "'");
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
            throw EntityLoaderError("JsonEntityLoader: a numeric field must be a number, bool, or name string");
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
                throw EntityLoaderError("JsonEntityLoader: array field is not a registered sequence container");

            const entt::meta_type element = view.value_type();
            view.reserve(json.Size());
            for (const rapidjson::Value& item : json.GetArray())
            {
                if (!view.insert(view.end(), JsonToMeta(element, item, ctx)))
                    throw EntityLoaderError("JsonEntityLoader: could not append an array element");
            }
            return value;
        }

        if (json.IsObject())
        {
            entt::meta_any value = target.construct();
            if (!value)
                throw EntityLoaderError("JsonEntityLoader: field type is not default-constructible / not registered");

            for (auto field = json.MemberBegin(); field != json.MemberEnd(); ++field)
            {
                entt::meta_data data = target.data(entt::hashed_string::value(field->name.GetString()));
                if (!data)
                    throw EntityLoaderError(std::string("JsonEntityLoader: unknown field '") + field->name.GetString() +
                                            "'");
                if (!data.set(value, JsonToMeta(data.type(), field->value, ctx)))
                    throw EntityLoaderError(std::string("JsonEntityLoader: could not assign field '") +
                                            field->name.GetString() + "'");
            }
            return value;
        }

        throw EntityLoaderError("JsonEntityLoader: unsupported field type for the given JSON value");
    }

    // Builds a component of type from its JSON body and emplaces it onto entity
    // via the "emplace"_hs meta func (which Registry binds for every component).
    // instance is a context-only placeholder; the free EmplaceComponent<T> takes
    // all of its arguments positionally (see ComponentMeta.h).
    void EmplaceFromJson(const entt::meta_type& type, entt::registry& registry, entt::entity entity,
                         const rapidjson::Value& body, entt::meta_ctx& ctx)
    {
        if (!body.IsObject())
            throw EntityLoaderError("JsonEntityLoader: a component body must be an object");

        entt::meta_any value = JsonToMeta(type, body, ctx);
        entt::meta_any instance{entt::meta_ctx_arg, ctx};
        type.invoke("emplace"_hs, instance, entt::forward_as_meta(ctx, registry), entity, value);
    }

} // namespace

JsonEntityLoader::JsonEntityLoader(entt::meta_ctx& ctx, const EntitySchemaModel* schema) : m_ctx(ctx), m_schema(schema)
{
}

bool JsonEntityLoader::Load(std::filesystem::path path)
{
    m_entries = LoadJsonDirectory(path, kEntitySchemaVersion);
    if (m_schema)
        for (const JsonDirectoryEntry& entry : m_entries)
        {
            try
            {
                ValidateEntityDocument(entry.document, *m_schema);
            }
            catch (const EntityLoaderError& error)
            {
                throw EntityLoaderError(entry.path.string() + ": " + error.what());
            }
        }
    return true;
}

void JsonEntityLoader::Populate(entt::registry& prefab_registry,
                                std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids)
{
    for (const JsonDirectoryEntry& entry : m_entries)
    {
        std::uint32_t prefab_id = entt::hashed_string::value(entry.id.c_str());
        NameIdRegistry::Register(prefab_id, entry.id);

        if (out_prefab_ids.contains(prefab_id))
            throw EntityLoaderError("JsonEntityLoader: duplicate prefab id for '" + entry.id + "'");
        if (!entry.document.IsObject())
            throw EntityLoaderError("JsonEntityLoader: entity '" + entry.id + "' must be an object");

        entt::entity prefab = prefab_registry.create();

        auto components = entry.document.FindMember("components");
        if (components != entry.document.MemberEnd())
        {
            if (!components->value.IsObject())
                throw EntityLoaderError("JsonEntityLoader: 'components' of '" + entry.id + "' must be an object");

            for (auto component = components->value.MemberBegin(); component != components->value.MemberEnd();
                 ++component)
            {
                entt::meta_type type = entt::resolve(m_ctx, entt::hashed_string::value(component->name.GetString()));
                if (!type)
                    throw EntityLoaderError(std::string("JsonEntityLoader: unknown component '") +
                                            component->name.GetString() + "' in '" + entry.id + "'");
                EmplaceFromJson(type, prefab_registry, prefab, component->value, m_ctx);
            }
        }

        out_prefab_ids.emplace(prefab_id, prefab);
    }
}

} // namespace psr
