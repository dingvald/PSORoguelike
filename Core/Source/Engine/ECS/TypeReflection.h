#pragma once

#include <string_view>
#include <type_traits>
#include <vector>

namespace psr {

// Opt-in reflection for a plain value type nested inside a component (e.g. an
// AiAction inside UtilityAiComponent). A describable type declares:
//
//   template <typename V> static void Describe(V& v)
//   {
//       v.template Field<&T::member>("member");
//       ...
//   }
//
// which the schema machinery drives with two different visitors: one that binds
// the members into entt-meta (so JsonToMeta can read them) and records their
// (possibly recursive) FieldSchema. This is the value-type analogue of a
// component's static Register(). See ComponentSchemaRegistrar.h.

namespace detail {

    // A probe visitor used only to detect a well-formed Describe: it never runs,
    // so its Field() body is empty and its signature just has to match the calls
    // Describe makes.
    struct DescribeProbe
    {
        template <auto Member> void Field(std::string_view) {}
    };

} // namespace detail

// A value type that exposes the static Describe() reflection hook above. Leaf
// types (arithmetic, enums, Vec2/Color, std::vector) are not Describable, so
// BuildFieldSchema dispatches them to their own cases first.
template <typename T>
concept Describable = requires(detail::DescribeProbe probe) { T::Describe(probe); };

// True for std::vector<E, A> specialisations -- the only list container the
// authoring pipeline understands.
template <typename T> struct IsStdVector : std::false_type
{
};

template <typename E, typename A> struct IsStdVector<std::vector<E, A>> : std::true_type
{
};

// Maps a C++ enum used as a component field to its authorable string names.
// Specialise per enum with a static constexpr array of {name, value} pairs, in
// declaration order (the order the JSON-schema "enum" list preserves):
//
//   template <> struct EnumNames<CurveType>
//   {
//       static constexpr std::array<std::pair<std::string_view, CurveType>, N> kValues{{ ... }};
//   };
//
// Used to register the enum's constants into entt-meta (so a JSON string maps to
// the value at load) and to emit the schema's allowed-values list.
template <typename E> struct EnumNames;

} // namespace psr
