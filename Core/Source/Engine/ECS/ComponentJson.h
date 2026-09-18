#pragma once

#include "Engine/ECS/ComponentSchema.h"

#include <entt/entt.hpp>

#include <rapidjson/document.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace psr {

// Thrown for a structurally valid JSON document whose *content* can't be turned
// into components -- an unknown component or field name, a duplicate prefab id, a
// value whose JSON shape doesn't match the target field type, etc. A malformed
// or unreadable file (or a schema_version mismatch) surfaces as JsonFileError
// from ReadJsonFile instead. One error type per subsystem, mirroring
// JsonFileError.
class EntityLoaderError : public std::runtime_error
{
public:
    explicit EntityLoaderError(const std::string& message) : std::runtime_error(message) {}
};

// Builds a meta value of target's type from a JSON value, ready to hand to
// meta_data::set / a component's "emplace"_hs. Recurses for object-shaped
// fields; see JsonEntityLoader.h's own doc comment for the accepted value
// shapes (numbers/bools, name-as-id strings on numeric fields, Color/Vec2,
// enum names, arrays, nested objects). Shared by JsonEntityLoader (a fresh
// prefab entity) and Registry::ApplyEntityComponentsJson (an existing runtime
// entity being restored) -- both need the identical JSON-to-meta contract.
entt::meta_any JsonToMeta(const entt::meta_type& target, const rapidjson::Value& json, entt::meta_ctx& ctx);

// Builds a component of type from its JSON body (via JsonToMeta) and emplaces
// it onto entity in registry via the "emplace"_hs meta func every registered
// component binds (see ComponentMeta.h's EmplaceComponent<T>) -- an
// emplace_or_replace, so this works identically whether entity is a
// freshly-created prefab clone (JsonEntityLoader::Populate) or an existing
// runtime entity being restored (Registry::ApplyEntityComponentsJson).
void EmplaceComponentFromJson(const entt::meta_type& type, entt::registry& registry, entt::entity entity,
                              const rapidjson::Value& body, entt::meta_ctx& ctx);

// The write-direction mirror of JsonToMeta / EntityDescriber's
// DescribeComponentValue: walks fields (a component's or a nested object's
// FieldSchema list, in the same order BuildFieldSchema produced them) off
// instance (a meta_any of type, already read off a live component -- see
// Registry::SerializeEntityComponents), producing the exact JSON shape
// JsonToMeta accepts back for each field. Unlike EntityDescriber's
// display-formatted text, this is meant for a lossless round trip: a NameId
// field writes its resolved NameIdRegistry label when one is known (so the
// saved file stays readable and re-registers the same label on load) or the
// raw numeric hash otherwise (never the "#12345" display fallback, which
// JsonToMeta would mis-hash as a literal string). Throws EntityLoaderError if
// an Enum field's current value matches none of its schema's enum_values
// (schema/meta out of sync).
rapidjson::Value FieldsToJson(const std::vector<FieldSchema>& fields, const entt::meta_type& type,
                              const entt::meta_any& instance, rapidjson::Document::AllocatorType& allocator);

} // namespace psr
