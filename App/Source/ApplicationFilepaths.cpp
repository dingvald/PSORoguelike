#include "ApplicationFilepaths.h"

#include "Engine/ECS/TypeReflection.h"

#include <string>

const std::filesystem::path ApplicationFilepaths::AssetsPath = "Assets";
const std::filesystem::path ApplicationFilepaths::FontsPath = ApplicationFilepaths::AssetsPath / "Fonts";
const std::filesystem::path ApplicationFilepaths::RmlDocumentsPath = ApplicationFilepaths::AssetsPath / "RML";
const std::filesystem::path ApplicationFilepaths::ShadersPath = ApplicationFilepaths::AssetsPath / "Shaders";
const std::filesystem::path ApplicationFilepaths::TexturesPath = ApplicationFilepaths::AssetsPath / "Textures";
const std::filesystem::path ApplicationFilepaths::DataPath = ApplicationFilepaths::AssetsPath / "Data";
const std::filesystem::path ApplicationFilepaths::AreasPath = ApplicationFilepaths::DataPath / "Areas";
const std::filesystem::path ApplicationFilepaths::PiecesPath = ApplicationFilepaths::DataPath / "Pieces";
const std::filesystem::path ApplicationFilepaths::DungeonsPath = ApplicationFilepaths::DataPath / "Dungeons";
const std::filesystem::path ApplicationFilepaths::EntitiesPath = ApplicationFilepaths::DataPath / "Entities";
const std::filesystem::path ApplicationFilepaths::PhotonArtsPath = ApplicationFilepaths::DataPath / "PhotonArts";
const std::filesystem::path ApplicationFilepaths::TechniquesPath = ApplicationFilepaths::DataPath / "Techniques";
const std::filesystem::path ApplicationFilepaths::StatusEffectsPath = ApplicationFilepaths::DataPath / "StatusEffects";
const std::filesystem::path ApplicationFilepaths::ClassesPath = ApplicationFilepaths::DataPath / "Classes";
const std::filesystem::path ApplicationFilepaths::HubPath = ApplicationFilepaths::DataPath / "hub.json";
const std::filesystem::path ApplicationFilepaths::ShopStockPath =
    ApplicationFilepaths::DataPath / "shop_stock.json";
const std::filesystem::path ApplicationFilepaths::SaveDataPath = "SaveData";

std::filesystem::path ApplicationFilepaths::ClassDefinitionPath(psr::ClassId class_id)
{
    for (const auto& [text, value] : psr::EnumNames<psr::ClassId>::kValues)
        if (value == class_id)
            return ApplicationFilepaths::ClassesPath / (std::string{text} + ".json");
    return ApplicationFilepaths::ClassesPath / "unknown.json"; // unreachable for a valid ClassId
}

std::filesystem::path ApplicationFilepaths::SaveSlotPath(int slot)
{
    return ApplicationFilepaths::SaveDataPath / ("slot_" + std::to_string(slot) + ".json");
}
