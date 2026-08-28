#include "ApplicationFilepaths.h"

const std::filesystem::path ApplicationFilepaths::AssetsPath = "Assets";
const std::filesystem::path ApplicationFilepaths::FontsPath = ApplicationFilepaths::AssetsPath / "Fonts";
const std::filesystem::path ApplicationFilepaths::RmlDocumentsPath = ApplicationFilepaths::AssetsPath / "RML";
const std::filesystem::path ApplicationFilepaths::ShadersPath = ApplicationFilepaths::AssetsPath / "Shaders";
const std::filesystem::path ApplicationFilepaths::TexturesPath = ApplicationFilepaths::AssetsPath / "Textures";
const std::filesystem::path ApplicationFilepaths::DataPath = ApplicationFilepaths::AssetsPath / "Data";
const std::filesystem::path ApplicationFilepaths::PiecesPath = ApplicationFilepaths::DataPath / "Pieces";
const std::filesystem::path ApplicationFilepaths::DungeonsPath = ApplicationFilepaths::DataPath / "Dungeons";
const std::filesystem::path ApplicationFilepaths::EntitiesPath = ApplicationFilepaths::DataPath / "Entities";
const std::filesystem::path ApplicationFilepaths::PhotonArtsPath = ApplicationFilepaths::DataPath / "PhotonArts";
const std::filesystem::path ApplicationFilepaths::TechniquesPath = ApplicationFilepaths::DataPath / "Techniques";
const std::filesystem::path ApplicationFilepaths::StatusEffectsPath = ApplicationFilepaths::DataPath / "StatusEffects";
