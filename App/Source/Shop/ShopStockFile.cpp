#include "Shop/ShopStockFile.h"

#include "Engine/Persistence/JsonFile.h"

namespace psr {

namespace {

    std::string ReadString(const rapidjson::Value& object, const char* key, const std::string& fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsString())
            throw JsonFileError(std::string("shop stock file: '") + key + "' must be a string");
        return it->value.GetString();
    }

    int ReadInt(const rapidjson::Value& object, const char* key, int fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsInt())
            throw JsonFileError(std::string("shop stock file: '") + key + "' must be an integer");
        return it->value.GetInt();
    }

    ShopStockEntry ReadShopStockEntry(const rapidjson::Value& entry)
    {
        if (!entry.IsObject())
            throw JsonFileError("shop stock file: each entry must be an object");

        ShopStockEntry result;
        result.prefab_id_string = ReadString(entry, "prefab_id_string", "");
        result.buy_price = ReadInt(entry, "buy_price", result.buy_price);
        return result;
    }

    rapidjson::Value StringValue(const std::string& text, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value value;
        value.SetString(text.c_str(), static_cast<rapidjson::SizeType>(text.size()), allocator);
        return value;
    }

    rapidjson::Value WriteShopStockEntry(const ShopStockEntry& entry, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value object(rapidjson::kObjectType);
        object.AddMember("prefab_id_string", StringValue(entry.prefab_id_string, allocator), allocator);
        object.AddMember("buy_price", entry.buy_price, allocator);
        return object;
    }

} // namespace

ShopStock LoadShopStock(const std::filesystem::path& path)
{
    const rapidjson::Document document = ReadJsonFile(path, kShopStockVersion);
    if (!document.IsObject())
        throw JsonFileError("shop stock file: must be an object");

    auto entries_member = document.FindMember("entries");
    if (entries_member == document.MemberEnd() || !entries_member->value.IsArray())
        throw JsonFileError("shop stock file: 'entries' must be an array");

    ShopStock stock;
    for (const rapidjson::Value& entry : entries_member->value.GetArray())
        stock.entries.push_back(ReadShopStockEntry(entry));

    return stock;
}

void SaveShopStock(const std::filesystem::path& path, const ShopStock& stock)
{
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    document.AddMember("schema_version", kShopStockVersion, allocator);

    rapidjson::Value entries(rapidjson::kArrayType);
    for (const ShopStockEntry& entry : stock.entries)
        entries.PushBack(WriteShopStockEntry(entry, allocator), allocator);
    document.AddMember("entries", entries, allocator);

    WriteJsonFile(path, document);
}

} // namespace psr
