#pragma once

#include "UI/AssetReferenceFields.h"

#include <string>

namespace psr {

// Rewrites every reference field named in ReferenceFieldNamesFor(kind), across
// every *.json under EditorFilepaths::DataPath, whose string value equals
// old_id to new_id instead. Call after an asset of `kind` has already been
// renamed on disk (its own file already moved to the new id's path) -- this
// only updates *other* assets that pointed at the old id. Returns the number
// of distinct files modified.
int UpdateReferencesOnRename(AssetKind kind, const std::string& old_id, const std::string& new_id);

} // namespace psr
