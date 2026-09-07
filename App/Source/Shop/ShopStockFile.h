#pragma once

#include "Shop/ShopStock.h"

#include <filesystem>

namespace psr {

inline constexpr int kShopStockVersion = 1;

// Loads the single hand-authored shop_stock.json document. Throws
// JsonFileError on a missing file, parse failure, schema_version mismatch,
// or malformed entry.
ShopStock LoadShopStock(const std::filesystem::path& path);

// Writes stock to path as the shop_stock.json document -- the inverse of
// LoadShopStock, used by ShopStockEditorLayer's Save button (unlike
// GrowthCurveFile, this content type has an editor, so it needs a save
// path).
void SaveShopStock(const std::filesystem::path& path, const ShopStock& stock);

} // namespace psr
