#pragma once

#include "Progression/CharacterClass.h"

#include <filesystem>

class ApplicationFilepaths
{
public:
    static const std::filesystem::path AssetsPath;
    static const std::filesystem::path FontsPath;
    static const std::filesystem::path RmlDocumentsPath;
    static const std::filesystem::path ShadersPath;
    static const std::filesystem::path TexturesPath;
    static const std::filesystem::path DataPath;
    static const std::filesystem::path AreasPath;
    static const std::filesystem::path PiecesPath;
    static const std::filesystem::path DungeonsPath;
    static const std::filesystem::path EntitiesPath;
    static const std::filesystem::path PhotonArtsPath;
    static const std::filesystem::path TechniquesPath;
    static const std::filesystem::path StatusEffectsPath;
    static const std::filesystem::path ClassesPath;
    static const std::filesystem::path HubPath;
    static const std::filesystem::path ShopStockPath;

    // App/Assets/Data/Classes/<class-id>.json -- see ClassDefinitionFile.h.
    static std::filesystem::path ClassDefinitionPath(psr::ClassId class_id);
};
