#pragma once

#include "Combat/StatusEffect.h"
#include "Combat/StatusEffectLibrary.h"

#include <rapidjson/document.h>

#include <filesystem>

namespace psr {

// StatusEffect file schema_version. Independent of the affix/piece/dungeon
// files' own versions.
inline constexpr int kStatusEffectLibraryVersion = 1;

// Parses one status-effect-shaped JSON body into a StatusEffect with
// id/id_string left default. Caller must validate the source document first
// (ValidateStatusEffectDocument applies the same per-body schema this reads).
StatusEffect ReadStatusEffectBody(const rapidjson::Value& status_effect_def);

// Inverse of ReadStatusEffectBody. Shared by SaveStatusEffect.
rapidjson::Value WriteStatusEffectBody(const StatusEffect& status_effect,
                                       rapidjson::Document::AllocatorType& allocator);

// Recursively loads every *.json under directory (one status effect per
// file) into a StatusEffectLibrary. A status effect's id is its path
// relative to directory, same convention as LoadAffixLibrary/
// LoadPieceLibrary/LoadDungeonLibrary. Throws StatusEffectError on a
// schema-version mismatch or malformed entry, JsonFileError on a missing
// directory or an unparseable file.
StatusEffectLibrary LoadStatusEffectLibrary(const std::filesystem::path& directory);

// Writes status_effect to path as a single status-effect file -- the inverse
// of one LoadStatusEffectLibrary entry. status_effect.id/id_string are not
// written. Throws StatusEffectError if the built document fails
// ValidateStatusEffectDocument.
void SaveStatusEffect(const std::filesystem::path& path, const StatusEffect& status_effect);

} // namespace psr
