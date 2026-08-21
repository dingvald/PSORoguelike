#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

#include <rapidjson/document.h>

namespace psr {

// Thrown for any JSON file I/O or parse/schema failure -- missing file,
// malformed JSON, unwritable path, or schema_version mismatch.
class JsonFileError : public std::runtime_error
{
public:
    explicit JsonFileError(const std::string& message) : std::runtime_error(message) {}
};

// Reads and parses path as JSON. Throws JsonFileError if the file can't be
// opened or fails to parse.
rapidjson::Document ReadJsonFile(const std::filesystem::path& path);

// As above, then requires a top-level integer "schema_version" member equal
// to expected_schema_version. Throws JsonFileError if missing or mismatched.
rapidjson::Document ReadJsonFile(const std::filesystem::path& path, int expected_schema_version);

// Pretty-prints document to path, creating parent directories as needed.
// Throws JsonFileError if path can't be opened for writing.
void WriteJsonFile(const std::filesystem::path& path, const rapidjson::Document& document);

} // namespace psr
