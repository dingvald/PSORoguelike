#pragma once

#include "Engine/Combat/PhotonArt.h"
#include "Engine/Combat/PhotonArtLibrary.h"

#include <rapidjson/document.h>

#include <filesystem>

namespace psr {

// Photon Art file schema_version. Independent of every other content
// family's own version.
inline constexpr int kPhotonArtLibraryVersion = 1;

// Parses one Photon-Art-shaped JSON body into a PhotonArt with id/id_string
// left default. Caller must validate the source document first
// (ValidatePhotonArtDocument applies the same per-body schema this reads).
PhotonArt ReadPhotonArtBody(const rapidjson::Value& photon_art_def);

// Inverse of ReadPhotonArtBody. Shared by SavePhotonArt.
rapidjson::Value WritePhotonArtBody(const PhotonArt& photon_art, rapidjson::Document::AllocatorType& allocator);

// Recursively loads every *.json under directory (one Photon Art per file)
// into a PhotonArtLibrary. A Photon Art's id is its path relative to
// directory, same convention as LoadAffixLibrary/LoadPieceLibrary. Throws
// PhotonArtError on a schema-version mismatch or malformed entry,
// JsonFileError on a missing directory or an unparseable file.
PhotonArtLibrary LoadPhotonArtLibrary(const std::filesystem::path& directory);

// Writes photon_art to path as a single Photon Art file -- the inverse of one
// LoadPhotonArtLibrary entry. photon_art.id/id_string are not written. Throws
// PhotonArtError if the built document fails ValidatePhotonArtDocument.
void SavePhotonArt(const std::filesystem::path& path, const PhotonArt& photon_art);

} // namespace psr
