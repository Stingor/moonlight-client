#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ro/render/item_icons.h"
#include "ro/render/skin_pieces.h"
#include "ro/vfs/card_illustrations.h"
#include "ro/vfs/encoding.h"

namespace ro::render {
namespace {

// Un dossier SOUS la racine d'interface, transcrit une fois pour toutes.
//
// La racine s'écrit en coréen dans les sources, l'archive l'indexe en CP949 ;
// ce n'est pas un état, c'est une constante que l'on ne sait calculer qu'à
// l'exécution.
[[nodiscard]] std::string UnderSkinRoot(std::string_view folder) {
  std::string utf8(kSkinRoot);
  utf8 += folder;
  auto encoded = vfs::Utf8ToCp949(utf8);
  return encoded.has_value() ? std::move(*encoded) : std::string();
}

[[nodiscard]] const std::string& IconFolderCp949() {
  static const std::string folder = UnderSkinRoot(kIconFolder);
  return folder;
}

[[nodiscard]] const std::string& CollectionFolderCp949() {
  static const std::string folder = UnderSkinRoot(kCollectionFolder);
  return folder;
}

[[nodiscard]] const std::string& CardIllustrationFolderCp949() {
  static const std::string folder = UnderSkinRoot(kCardIllustrationFolder);
  return folder;
}

}  // namespace

std::string CardIllustrationPath(const vfs::CardIllustrations& cards, std::uint32_t item_id) {
  const std::string_view resource = cards.Find(item_id);
  if (resource.empty() || CardIllustrationFolderCp949().empty()) {
    return {};
  }
  std::string path = CardIllustrationFolderCp949();
  path += resource;
  path += kIconSuffix;
  return path;
}

std::string ItemCollectionPath(const vfs::ItemTable& items, std::uint32_t item_id) {
  return ItemCollectionPathOfResource(items.ResourceOf(item_id));
}

std::string ItemCollectionPathOfResource(std::string_view resource) {
  if (resource.empty() || CollectionFolderCp949().empty()) {
    return {};
  }
  std::string path = CollectionFolderCp949();
  path += resource;
  path += kIconSuffix;
  return path;
}

std::string ItemIconPath(const vfs::ItemTable& items, std::uint32_t item_id) {
  return ItemIconPathOfResource(items.ResourceOf(item_id));
}

std::string ItemIconPathOfResource(std::string_view resource) {
  if (resource.empty() || IconFolderCp949().empty()) {
    return {};
  }
  std::string path = IconFolderCp949();
  path += resource;
  path += kIconSuffix;
  return path;
}

std::vector<std::uint32_t> ItemsWithCollection(const vfs::Vfs& vfs, const vfs::ItemTable& items) {
  std::vector<std::uint32_t> ids;
  ids.reserve(items.size());
  for (const auto& [item_id, entry] : items.entries()) {
    ids.push_back(item_id);
  }
  std::ranges::sort(ids);
  std::unordered_set<std::string_view> resources;
  std::vector<std::uint32_t> owned;
  for (const std::uint32_t item_id : ids) {
    const std::string path = ItemCollectionPath(items, item_id);
    if (path.empty() || !vfs.ContainsRaw(path)) {
      continue;
    }
    if (resources.insert(items.ResourceOf(item_id)).second) {
      owned.push_back(item_id);
    }
  }
  return owned;
}

}  // namespace ro::render
