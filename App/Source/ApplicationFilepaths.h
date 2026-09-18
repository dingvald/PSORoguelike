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

    // Sibling to AssetsPath, not under it -- save games are player data, not
    // shipped content, so they're deliberately outside the tree App-App.lua's
    // postbuild step copies from Assets/ on every build.
    static const std::filesystem::path SaveDataPath;

    // App/Assets/Data/Classes/<class-id>.json -- see ClassDefinitionFile.h.
    static std::filesystem::path ClassDefinitionPath(psr::ClassId class_id);

    // SaveDataPath/slot_<slot>.json -- see App/Source/Persistence/CharacterSaveFile.h.
    static std::filesystem::path SaveSlotPath(int slot);
};
