#include "Engine/Persistence/JsonFile.h"

#include <fstream>
#include <sstream>

#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

namespace psr {

rapidjson::Document ReadJsonFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        throw JsonFileError("JsonFile: cannot open " + path.string());

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    std::string contents = buffer.str();

    rapidjson::Document document;
    if (document.Parse(contents.c_str(), contents.size()).HasParseError())
        throw JsonFileError("JsonFile: malformed JSON in " + path.string());

    return document;
}

rapidjson::Document ReadJsonFile(const std::filesystem::path& path, int expected_schema_version)
{
    rapidjson::Document document = ReadJsonFile(path);

    if (!document.HasMember("schema_version") || !document["schema_version"].IsInt() ||
        document["schema_version"].GetInt() != expected_schema_version)
        throw JsonFileError("JsonFile: schema_version mismatch in " + path.string());

    return document;
}

void WriteJsonFile(const std::filesystem::path& path, const rapidjson::Document& document)
{
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path());

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream)
        throw JsonFileError("JsonFile: cannot open " + path.string() + " for writing");
    stream << buffer.GetString();
}

} // namespace psr
