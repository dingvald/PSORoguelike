#include "EditorFilepaths.h"

const std::filesystem::path EditorFilepaths::AssetsPath = "Assets";
const std::filesystem::path EditorFilepaths::FontsPath = EditorFilepaths::AssetsPath / "Fonts";
const std::filesystem::path EditorFilepaths::RmlDocumentsPath = EditorFilepaths::AssetsPath / "RML";
const std::filesystem::path EditorFilepaths::ShadersPath = EditorFilepaths::AssetsPath / "Shaders";
const std::filesystem::path EditorFilepaths::TexturesPath = EditorFilepaths::AssetsPath / "Textures";

const std::filesystem::path EditorFilepaths::DataPath = std::filesystem::path(PSR_APP_ASSETS_DIR) / "Data";
const std::filesystem::path EditorFilepaths::PiecesPath = EditorFilepaths::DataPath / "Pieces";
const std::filesystem::path EditorFilepaths::DungeonsPath = EditorFilepaths::DataPath / "Dungeons";
const std::filesystem::path EditorFilepaths::EntitiesPath = EditorFilepaths::DataPath / "Entities";
const std::filesystem::path EditorFilepaths::AffixesPath = EditorFilepaths::DataPath / "Affixes";
const std::filesystem::path EditorFilepaths::PhotonArtsPath = EditorFilepaths::DataPath / "PhotonArts";
const std::filesystem::path EditorFilepaths::TechniquesPath = EditorFilepaths::DataPath / "Techniques";
const std::filesystem::path EditorFilepaths::DropTablesPath = EditorFilepaths::DataPath / "DropTables";
