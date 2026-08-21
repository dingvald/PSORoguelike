#pragma once

#include <filesystem>

class ApplicationFilepaths
{
public:
    static const std::filesystem::path AssetsPath;
    static const std::filesystem::path FontsPath;
    static const std::filesystem::path RmlDocumentsPath;
    static const std::filesystem::path ShadersPath;
    static const std::filesystem::path TexturesPath;
};
