#include "Engine/Dungeon/PieceLibraryFile.h"

#include "Engine/Dungeon/DungeonError.h"
#include "Engine/Dungeon/PieceSchema.h"
#include "Engine/Dungeon/PieceSchemaEmitter.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/Persistence/JsonDirectoryLoader.h"
#include "Engine/Persistence/JsonFile.h"

#include <entt/core/hashed_string.hpp>

#include <string_view>
#include <utility>

namespace psr {

namespace {

    std::string ReadString(const rapidjson::Value& object, const char* key, const std::string& fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsString())
            throw DungeonError(std::string("piece file: '") + key + "' must be a string");
        return it->value.GetString();
    }

    // A name-id field: a prefab string hashed to an entt id, a raw numeric id,
    // or a fallback. Mirrors the schema's NameId oneOf(string, integer). The
    // hash is one-way, so a string-authored value is captured into
    // NameIdRegistry here -- the only place the source string is still alive
    // -- exactly like JsonEntityLoader.cpp's own NameId branch, so
    // WriteCellPrefab can later look the label back up instead of writing an
    // unreadable hash.
    std::uint32_t ReadNameId(const rapidjson::Value& object, const char* key, std::uint32_t fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (it->value.IsString())
        {
            const std::uint32_t hash = entt::hashed_string::value(it->value.GetString());
            NameIdRegistry::Register(hash, it->value.GetString());
            return hash;
        }
        if (it->value.IsUint())
            return it->value.GetUint();
        if (it->value.IsInt())
            return static_cast<std::uint32_t>(it->value.GetInt());
        throw DungeonError(std::string("piece file: '") + key + "' must be a name string or id");
    }

    bool ReadBool(const rapidjson::Value& object, const char* key, bool fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsBool())
            throw DungeonError(std::string("piece file: '") + key + "' must be a boolean");
        return it->value.GetBool();
    }

    int ReadInt(const rapidjson::Value& object, const char* key, int fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsInt())
            throw DungeonError(std::string("piece file: '") + key + "' must be an integer");
        return it->value.GetInt();
    }

    Vec2 ReadVec2(const rapidjson::Value& object, const char* key)
    {
        Vec2 out;
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return out;
        if (!it->value.IsObject())
            throw DungeonError(std::string("piece file: '") + key + "' must be an {x,y} object");
        if (auto x = it->value.FindMember("x"); x != it->value.MemberEnd() && x->value.IsNumber())
            out.x = static_cast<int>(x->value.GetDouble());
        if (auto y = it->value.FindMember("y"); y != it->value.MemberEnd() && y->value.IsNumber())
            out.y = static_cast<int>(y->value.GetDouble());
        return out;
    }

    template <typename E>
    E ReadEnum(const rapidjson::Value& object, const char* key, E fallback, const char* error_label)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsString())
            throw DungeonError(std::string("piece file: '") + key + "' must be a " + error_label + " name");
        const std::string_view name = it->value.GetString();
        for (const auto& [text, value] : EnumNames<E>::kValues)
            if (text == name)
                return value;
        throw DungeonError(std::string("piece file: unknown ") + error_label + " '" + std::string(name) + "'");
    }

    PieceCellPrefab ReadCellPrefab(const rapidjson::Value& entry)
    {
        if (!entry.IsObject())
            throw DungeonError("piece file: each cell prefab must be an object");
        PieceCellPrefab prefab;
        prefab.prefab_id = ReadNameId(entry, "prefab_id", 0);
        return prefab;
    }

    std::vector<std::string> ReadStringArray(const rapidjson::Value& object, const char* key)
    {
        std::vector<std::string> values;
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return values;
        if (!it->value.IsArray())
            throw DungeonError(std::string("piece file: '") + key + "' must be an array of strings");
        for (const auto& entry : it->value.GetArray())
        {
            if (!entry.IsString())
                throw DungeonError(std::string("piece file: '") + key + "' entries must be strings");
            values.emplace_back(entry.GetString());
        }
        return values;
    }

    PieceSocket ReadSocket(const rapidjson::Value& entry)
    {
        if (!entry.IsObject())
            throw DungeonError("piece file: each socket must be an object");
        PieceSocket socket;
        socket.cell_offset = ReadVec2(entry, "cell_offset");
        socket.edge = ReadEnum<EdgeDirection>(entry, "edge", EdgeDirection::North, "edge");
        socket.tags = ReadStringArray(entry, "tags");
        socket.connects_to_tags = ReadStringArray(entry, "connects_to_tags");
        socket.fallback_prefab_id = ReadNameId(entry, "fallback_prefab_id", 0);
        return socket;
    }

    std::vector<PieceSocket> ReadSockets(const rapidjson::Value& piece_def)
    {
        std::vector<PieceSocket> sockets;
        auto it = piece_def.FindMember("sockets");
        if (it == piece_def.MemberEnd())
            return sockets;
        if (!it->value.IsArray())
            throw DungeonError("piece file: 'sockets' must be an array");
        for (const auto& entry : it->value.GetArray())
            sockets.push_back(ReadSocket(entry));
        return sockets;
    }

    PieceSpawn ReadSpawn(const rapidjson::Value& entry)
    {
        if (!entry.IsObject())
            throw DungeonError("piece file: each spawn must be an object");
        PieceSpawn spawn;
        spawn.cell_offset = ReadVec2(entry, "cell_offset");
        spawn.prefab_id = ReadNameId(entry, "prefab_id", 0);
        spawn.wave = ReadInt(entry, "wave", 0);
        return spawn;
    }

    std::vector<PieceSpawn> ReadSpawns(const rapidjson::Value& piece_def)
    {
        std::vector<PieceSpawn> spawns;
        auto it = piece_def.FindMember("spawns");
        if (it == piece_def.MemberEnd())
            return spawns;
        if (!it->value.IsArray())
            throw DungeonError("piece file: 'spawns' must be an array");
        for (const auto& entry : it->value.GetArray())
            spawns.push_back(ReadSpawn(entry));
        return spawns;
    }

    PieceCell ReadCell(const rapidjson::Value& entry)
    {
        if (!entry.IsObject())
            throw DungeonError("piece file: each cell must be an object");
        PieceCell cell;
        cell.offset = ReadVec2(entry, "offset");
        auto prefabs_it = entry.FindMember("prefabs");
        if (prefabs_it != entry.MemberEnd())
        {
            if (!prefabs_it->value.IsArray())
                throw DungeonError("piece file: cell 'prefabs' must be an array");
            for (const auto& prefab_entry : prefabs_it->value.GetArray())
                cell.prefabs.push_back(ReadCellPrefab(prefab_entry));
        }
        return cell;
    }

    std::vector<PieceCell> ReadCells(const rapidjson::Value& piece_def)
    {
        std::vector<PieceCell> cells;
        auto it = piece_def.FindMember("cells");
        if (it == piece_def.MemberEnd())
            return cells;
        if (!it->value.IsArray())
            throw DungeonError("piece file: 'cells' must be an array");
        for (const auto& entry : it->value.GetArray())
            cells.push_back(ReadCell(entry));
        return cells;
    }

    rapidjson::Value StringValue(const std::string& text, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value value;
        value.SetString(text.c_str(), static_cast<rapidjson::SizeType>(text.size()), allocator);
        return value;
    }

    rapidjson::Value WriteVec2(Vec2 v, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value object(rapidjson::kObjectType);
        object.AddMember("x", v.x, allocator);
        object.AddMember("y", v.y, allocator);
        return object;
    }

    template <typename E> std::string_view EnumName(E value)
    {
        for (const auto& [text, candidate] : EnumNames<E>::kValues)
            if (candidate == value)
                return text;
        return EnumNames<E>::kValues.front().first; // unreachable for a valid enum value
    }

    // A NameId field's write-side: label from NameIdRegistry when known, not
    // recovered any other way -- hashing is one-way (see DungeonPiece.h).
    // Falls back to the raw id if it was never registered in this process
    // (authored as a bare number, or hashed before this process ever saw the
    // source string).
    void AddNameIdMember(rapidjson::Value& object, const char* key, std::uint32_t id,
                         rapidjson::Document::AllocatorType& allocator)
    {
        if (std::optional<std::string> label = NameIdRegistry::Find(id))
            object.AddMember(rapidjson::StringRef(key), StringValue(*label, allocator), allocator);
        else
            object.AddMember(rapidjson::StringRef(key), id, allocator);
    }

    rapidjson::Value WriteCellPrefab(const PieceCellPrefab& prefab, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value object(rapidjson::kObjectType);
        AddNameIdMember(object, "prefab_id", prefab.prefab_id, allocator);
        return object;
    }

    rapidjson::Value WriteStringArray(const std::vector<std::string>& values,
                                      rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value array(rapidjson::kArrayType);
        for (const std::string& value : values)
            array.PushBack(StringValue(value, allocator), allocator);
        return array;
    }

    rapidjson::Value WriteSocket(const PieceSocket& socket, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value object(rapidjson::kObjectType);
        object.AddMember("cell_offset", WriteVec2(socket.cell_offset, allocator), allocator);
        object.AddMember("edge", StringValue(std::string{EnumName(socket.edge)}, allocator), allocator);
        object.AddMember("tags", WriteStringArray(socket.tags, allocator), allocator);
        object.AddMember("connects_to_tags", WriteStringArray(socket.connects_to_tags, allocator), allocator);
        AddNameIdMember(object, "fallback_prefab_id", socket.fallback_prefab_id, allocator);
        return object;
    }

    rapidjson::Value WriteSockets(const std::vector<PieceSocket>& sockets,
                                  rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value array(rapidjson::kArrayType);
        for (const PieceSocket& socket : sockets)
            array.PushBack(WriteSocket(socket, allocator), allocator);
        return array;
    }

    rapidjson::Value WriteSpawn(const PieceSpawn& spawn, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value object(rapidjson::kObjectType);
        object.AddMember("cell_offset", WriteVec2(spawn.cell_offset, allocator), allocator);
        AddNameIdMember(object, "prefab_id", spawn.prefab_id, allocator);
        object.AddMember("wave", spawn.wave, allocator);
        return object;
    }

    rapidjson::Value WriteSpawns(const std::vector<PieceSpawn>& spawns, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value array(rapidjson::kArrayType);
        for (const PieceSpawn& spawn : spawns)
            array.PushBack(WriteSpawn(spawn, allocator), allocator);
        return array;
    }

    rapidjson::Value WriteCell(const PieceCell& cell, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value object(rapidjson::kObjectType);
        object.AddMember("offset", WriteVec2(cell.offset, allocator), allocator);
        rapidjson::Value prefabs(rapidjson::kArrayType);
        for (const PieceCellPrefab& prefab : cell.prefabs)
            prefabs.PushBack(WriteCellPrefab(prefab, allocator), allocator);
        object.AddMember("prefabs", prefabs, allocator);
        return object;
    }

    rapidjson::Value WriteCells(const std::vector<PieceCell>& cells, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value array(rapidjson::kArrayType);
        for (const PieceCell& cell : cells)
            array.PushBack(WriteCell(cell, allocator), allocator);
        return array;
    }

} // namespace

DungeonPiece ReadPieceBody(const rapidjson::Value& piece_def)
{
    DungeonPiece piece;
    piece.name = ReadString(piece_def, "name", piece.name);
    piece.area_tag = ReadString(piece_def, "area_tag", piece.area_tag);
    piece.category = ReadEnum<PieceCategory>(piece_def, "category", PieceCategory::Room, "category");
    piece.can_rotate = ReadBool(piece_def, "can_rotate", piece.can_rotate);
    piece.can_mirror = ReadBool(piece_def, "can_mirror", piece.can_mirror);
    piece.tags = ReadStringArray(piece_def, "tags");
    piece.cells = ReadCells(piece_def);
    piece.sockets = ReadSockets(piece_def);
    piece.spawns = ReadSpawns(piece_def);
    return piece;
}

rapidjson::Value WritePieceBody(const DungeonPiece& piece, rapidjson::Document::AllocatorType& allocator)
{
    rapidjson::Value object(rapidjson::kObjectType);
    object.AddMember("name", StringValue(piece.name, allocator), allocator);
    object.AddMember("area_tag", StringValue(piece.area_tag, allocator), allocator);
    object.AddMember("category", StringValue(std::string{EnumName(piece.category)}, allocator), allocator);
    object.AddMember("can_rotate", piece.can_rotate, allocator);
    object.AddMember("can_mirror", piece.can_mirror, allocator);
    object.AddMember("tags", WriteStringArray(piece.tags, allocator), allocator);
    object.AddMember("cells", WriteCells(piece.cells, allocator), allocator);
    object.AddMember("sockets", WriteSockets(piece.sockets, allocator), allocator);
    object.AddMember("spawns", WriteSpawns(piece.spawns, allocator), allocator);
    return object;
}

PieceLibrary LoadPieceLibrary(const std::filesystem::path& directory)
{
    const std::vector<JsonDirectoryEntry> entries = LoadJsonDirectory(directory, kPieceLibraryVersion);
    const PieceSchemaModel schema = BuildPieceSchemaModel();

    std::vector<DungeonPiece> pieces;
    for (const JsonDirectoryEntry& entry : entries)
    {
        try
        {
            // Validate each fragment against the generated schema before parsing, so
            // a malformed piece is reported with a JSON-pointer rather than silently
            // mis-parsed. Mirrors JsonEntityLoader::Load's validate-then-read.
            ValidatePieceDocument(entry.document, schema);

            if (!entry.document.IsObject())
                throw DungeonError("piece file: must be an object");

            DungeonPiece piece = ReadPieceBody(entry.document);
            piece.id_string = entry.id;
            piece.id = entt::hashed_string::value(piece.id_string.c_str());
            if (piece.name.empty())
                piece.name = piece.id_string;

            pieces.push_back(std::move(piece));
        }
        catch (const DungeonError& error)
        {
            throw DungeonError(entry.path.string() + ": " + error.what());
        }
    }

    return PieceLibrary{std::move(pieces)};
}

void SavePiece(const std::filesystem::path& path, const DungeonPiece& piece)
{
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    document.AddMember("schema_version", kPieceLibraryVersion, allocator);
    rapidjson::Value body = WritePieceBody(piece, allocator);

    // Round-trip through ReadPieceBody so a save can never produce a file
    // LoadPieceLibrary would reject on content grounds (schema validation
    // below only checks JSON shape).
    ReadPieceBody(body);

    for (auto member = body.MemberBegin(); member != body.MemberEnd(); ++member)
        document.AddMember(member->name, member->value, allocator);

    // A save can never produce a file LoadPieceLibrary would reject.
    ValidatePieceDocument(document, BuildPieceSchemaModel());

    WriteJsonFile(path, document);
}

} // namespace psr
