#pragma once

#include "Engine/ECS/ComponentMeta.h"
#include "Engine/ECS/ComponentSchema.h"
#include "Engine/ECS/TypeReflection.h"
#include "Engine/Math/Color.h"
#include "Engine/Math/Vec2.h"

#include <entt/entt.hpp>
#include <entt/meta/container.hpp> // sequence-container support for std::vector fields

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace psr {

namespace detail {

    // Recovers the value type of a pointer-to-data-member. Used with a NTTP
    // member pointer (&T::member) to name the member's C++ type without spelling
    // it out at the call site.
    template <typename C, typename M> M MemberValueTypeImpl(M C::*);

    template <auto Member> using MemberValueType = decltype(MemberValueTypeImpl(Member));

    // Forward declaration: BuildFieldSchema and the Ensure* helpers below are
    // mutually recursive -- a nested Object/Array field recurses back into
    // BuildFieldSchema for its own fields / element type.
    template <typename M> FieldSchema BuildFieldSchema(std::string name, entt::meta_ctx& ctx);

    // Reflects a describable value type's members into `factory` (so JsonToMeta's
    // generic object path can read them) and collects their FieldSchemas. The
    // mirror of ComponentSchemaBuilder::Data, minus the component-only
    // clone/emplace bindings.
    template <typename T> class ValueTypeReflector
    {
    public:
        ValueTypeReflector(entt::meta_factory<T> factory, entt::meta_ctx& ctx) : m_factory(factory), m_ctx(&ctx) {}

        template <auto Member> void Field(std::string_view name)
        {
            // Hash the name ourselves and use the id overload (see the note in
            // ComponentSchemaBuilder::Data): the string-literal overload would
            // dangle for our temporary std::string.
            const std::string owned{name};
            m_factory = m_factory.template data<Member>(entt::hashed_string::value(owned.c_str()));
            m_fields.push_back(BuildFieldSchema<MemberValueType<Member>>(owned, *m_ctx));
        }

        std::vector<FieldSchema> TakeFields() { return std::move(m_fields); }

    private:
        entt::meta_factory<T> m_factory;
        entt::meta_ctx* m_ctx;
        std::vector<FieldSchema> m_fields;
    };

    // Binds a nested value type's data members and returns its field schemas.
    // entt's data() registration is idempotent (a same-id member is a no-op), so
    // a value type shared by several fields/components registers cleanly.
    template <typename T> std::vector<FieldSchema> EnsureValueTypeRegistered(entt::meta_ctx& ctx)
    {
        ValueTypeReflector<T> reflector{entt::meta_factory<T>{ctx}, ctx};
        T::Describe(reflector);
        return reflector.TakeFields();
    }

    template <typename E, std::size_t... I>
    void RegisterEnumConstants(entt::meta_factory<E> factory, std::index_sequence<I...>)
    {
        // Each enumerator becomes a static meta "data" keyed by the hash of its
        // authorable name -- the same hash JsonToMeta computes from the JSON
        // string, so target.data(hash).get({}) recovers the value at load.
        ((void)(factory = factory.template data<EnumNames<E>::kValues[I].second>(
                    entt::hashed_string::value(std::string{EnumNames<E>::kValues[I].first}.c_str()))),
         ...);
    }

    // Registers an enum's constants (once; idempotent) and returns its authorable
    // names in declaration order for the schema's allowed-values list.
    template <typename E> std::vector<std::string> EnsureEnumRegistered(entt::meta_ctx& ctx)
    {
        RegisterEnumConstants<E>(entt::meta_factory<E>{ctx}, std::make_index_sequence<EnumNames<E>::kValues.size()>{});

        std::vector<std::string> names;
        names.reserve(EnumNames<E>::kValues.size());
        for (const auto& entry : EnumNames<E>::kValues)
            names.emplace_back(entry.first);
        return names;
    }

    // Eagerly reflects a std::vector field type so JsonToMeta can default-construct
    // it and take its sequence-container view. Both come from resolve<std::vector>,
    // which is why <entt/meta/container.hpp> must be included in this TU.
    template <typename Vec> void EnsureVectorRegistered(entt::meta_ctx& ctx) { (void)entt::meta_factory<Vec>{ctx}; }

    // Maps a member's C++ type to its authorable FieldSchema, recursing for enum /
    // array / nested-object members (and registering their meta types as it goes).
    // Kept in lockstep with JsonToMeta's type dispatch (JsonEntityLoader.cpp): the
    // two describe the same contract from opposite directions. std::uint32_t is the
    // name-as-id convention (e.g. a texture_id field) and so also accepts a name
    // string; every other integral is number-only (strict by intent).
    template <typename M> FieldSchema BuildFieldSchema(std::string name, entt::meta_ctx& ctx)
    {
        if constexpr (std::is_same_v<M, Vec2>)
            return FieldSchema{std::move(name), FieldKind::Vec2};
        else if constexpr (std::is_same_v<M, Color>)
            return FieldSchema{std::move(name), FieldKind::Color};
        else if constexpr (std::is_same_v<M, std::uint32_t>)
            return FieldSchema{std::move(name), FieldKind::NameId};
        else if constexpr (std::is_enum_v<M>)
        {
            FieldSchema field{std::move(name), FieldKind::Enum};
            field.enum_values = EnsureEnumRegistered<M>(ctx);
            return field;
        }
        else if constexpr (IsStdVector<M>::value)
        {
            EnsureVectorRegistered<M>(ctx);
            FieldSchema field{std::move(name), FieldKind::Array};
            field.children.push_back(BuildFieldSchema<typename M::value_type>("item", ctx));
            return field;
        }
        else if constexpr (Describable<M>)
        {
            FieldSchema field{std::move(name), FieldKind::Object};
            field.children = EnsureValueTypeRegistered<M>(ctx);
            return field;
        }
        else if constexpr (std::is_same_v<M, bool>)
            return FieldSchema{std::move(name), FieldKind::Boolean};
        else if constexpr (std::is_same_v<M, std::string>)
            return FieldSchema{std::move(name), FieldKind::String};
        else if constexpr (std::is_floating_point_v<M>)
            return FieldSchema{std::move(name), FieldKind::Number};
        else if constexpr (std::is_integral_v<M>)
            return FieldSchema{std::move(name), FieldKind::Integer};
        else
            static_assert(sizeof(M) == 0, "ComponentSchemaRegistrar: unsupported component field type");
    }

} // namespace detail

// Fluent builder for a single component's data members. Mirrors entt's
// meta_factory chaining but takes plain string names: each Data() forwards the
// member to the underlying meta_factory (which hashes the name identically to
// the old .data("name"_hs) form) and records the readable name + inferred kind
// into the component's ComponentSchema.
template <typename TComponent> class ComponentSchemaBuilder
{
public:
    ComponentSchemaBuilder(entt::meta_factory<TComponent> factory, ComponentSchema& schema, entt::meta_ctx& ctx)
        : m_factory(factory), m_schema(&schema), m_ctx(&ctx)
    {
    }

    template <auto Member> ComponentSchemaBuilder& Data(std::string_view name)
    {
        // Hash the name ourselves and use meta_factory's id overload (name left
        // null): the string-literal overload stores the char* in the meta node,
        // which would dangle for our temporary. The hash matches the old
        // "name"_hs form, so the runtime meta id is unchanged.
        const std::string owned{name};
        m_factory = m_factory.template data<Member>(entt::hashed_string::value(owned.c_str()));
        m_schema->fields.push_back(detail::BuildFieldSchema<detail::MemberValueType<Member>>(owned, *m_ctx));
        return *this;
    }

private:
    entt::meta_factory<TComponent> m_factory;
    ComponentSchema* m_schema;
    entt::meta_ctx* m_ctx;
};

// The abstraction layer around component registration. Wraps an entt::meta_ctx
// and, as each component registers, both binds it into meta (via the existing
// RegisterComponent<T>, so clone/emplace/describe_fields bindings are unchanged)
// and accumulates an EntitySchemaModel of the registered component/field names
// and value kinds -- the readable strings entt only stores as one-way hashes.
//
// A component's Register() takes this instead of a raw entt::meta_ctx&:
//
//   static void Register(ComponentSchemaRegistrar& reg)
//   {
//       reg.Component<HealthComponent>("health")
//           .Data<&HealthComponent::current>("current")
//           .Data<&HealthComponent::max>("max");
//   }
class ComponentSchemaRegistrar
{
public:
    explicit ComponentSchemaRegistrar(entt::meta_ctx& ctx) : m_ctx(&ctx) {}

    template <typename TComponent>
    ComponentSchemaBuilder<TComponent> Component(std::string_view type_name, bool authorable = true)
    {
        const std::string owned{type_name};
        entt::meta_factory<TComponent> factory =
            RegisterComponent<TComponent>(*m_ctx).type(entt::hashed_string::value(owned.c_str()));
        m_model.components.push_back(ComponentSchema{owned, std::is_empty_v<TComponent>, authorable, {}});
        return ComponentSchemaBuilder<TComponent>{factory, m_model.components.back(), *m_ctx};
    }

    const EntitySchemaModel& Model() const { return m_model; }
    entt::meta_ctx& Context() { return *m_ctx; }

private:
    entt::meta_ctx* m_ctx;
    EntitySchemaModel m_model;
};

} // namespace psr
