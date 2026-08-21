#pragma once

#include <filesystem>

class EditorFilepaths
{
public:
    static const std::filesystem::path AssetsPath;
    static const std::filesystem::path FontsPath;
    static const std::filesystem::path RmlDocumentsPath;
    static const std::filesystem::path ShadersPath;
    static const std::filesystem::path TexturesPath;

    // Points at App's SOURCE Assets/Data (via PSR_APP_ASSETS_DIR, an absolute
    // path injected by Build-Editor.lua), not the postbuild-copied runtime
    // Assets/ above -- content editors save here so authored JSON survives as
    // real, committed content instead of living only under gitignored Binaries/.
    static const std::filesystem::path DataPath;
    static const std::filesystem::path PiecesPath;
    static const std::filesystem::path DungeonsPath;
    static const std::filesystem::path EntitiesPath;
};
