#include "Hub/HubDefinitionFile.h"

#include "Engine/Persistence/JsonFile.h"

namespace psr {

HubDefinition LoadHubDefinition(const std::filesystem::path& path)
{
    const rapidjson::Document document = ReadJsonFile(path, kHubDefinitionVersion);
    if (!document.IsObject())
        throw JsonFileError("hub file: must be an object");

    auto piece_id_member = document.FindMember("piece_id_string");
    if (piece_id_member == document.MemberEnd() || !piece_id_member->value.IsString())
        throw JsonFileError("hub file: 'piece_id_string' must be a string");

    HubDefinition hub;
    hub.piece_id_string = piece_id_member->value.GetString();
    return hub;
}

} // namespace psr
