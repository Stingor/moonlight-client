#include "ro/render/item_icons.h"

#include <string>
#include <utility>

#include "ro/formats/bmp.h"
#include "ro/formats/image.h"

namespace ro::render {

ItemIcons::ItemIcons(vfs::Vfs& vfs, const vfs::ItemTable& items,
                     const vfs::CardIllustrations* cards)
    : vfs_(&vfs), items_(&items), cards_(cards) {}

namespace {

// Charge un bitmap du jeu dans une texture, ou laisse la case vide.
// `on_paper` : une illustration de collection, peinte sur du papier blanc ; son
// fond se détoure, pour qu'elle se pose sur n'importe quel panneau.
void LoadInto(vfs::Vfs& vfs, const std::string& path, IconTexture& into, bool on_paper = false) {
  if (path.empty()) {
    return;
  }
  auto bytes = vfs.ReadRaw(path);
  if (!bytes) {
    return;
  }
  auto image = formats::DecodeBmp(*bytes);
  if (!image) {
    return;
  }
  // Les bitmaps du client n'ont aucun canal alpha : le magenta pur EST leur
  // transparence, et c'est antérieur à l'alpha.
  formats::ApplyMagentaColorKey(*image);
  if (on_paper) {
    formats::MattePaperBackground(*image);
  }
  // ⚠ AU PLUS PROCHE, comme le client : le bitmap fait vingt-quatre pixels dans
  // une case de trente-deux. Interpolée, l'icône se ramollit visiblement.
  auto texture = GlTexture::Upload(*image, GlTexture::Filtering::kPixelArt);
  if (!texture) {
    return;
  }
  into.texture = std::move(*texture);
  into.width_px = image->width;
  into.height_px = image->height;
}

}  // namespace

const IconTexture& ItemIcons::GetCardIllustration(std::uint32_t item_id) {
  const auto known = illustrations_.find(item_id);
  if (known != illustrations_.end()) {
    return known->second;
  }
  IconTexture& art = illustrations_[item_id];
  if (vfs_ == nullptr || cards_ == nullptr) {
    return art;
  }
  LoadInto(*vfs_, CardIllustrationPath(*cards_, item_id), art);
  return art;
}

const IconTexture& ItemIcons::GetCollection(std::uint32_t item_id) {
  const auto known = collections_.find(item_id);
  if (known != collections_.end()) {
    return known->second;
  }
  IconTexture& art = collections_[item_id];
  if (vfs_ == nullptr || items_ == nullptr) {
    return art;
  }
  LoadInto(*vfs_, ItemCollectionPath(*items_, item_id), art, /*on_paper=*/true);
  return art;
}

void ItemIcons::Clear() {
  loaded_.clear();
  collections_.clear();
  illustrations_.clear();
  ++generation_;
}

const IconTexture& ItemIcons::Get(std::uint32_t item_id) {
  const auto known = loaded_.find(item_id);
  if (known != loaded_.end()) {
    return known->second;
  }
  // ⚠ LA CASE SE POSE D'ABORD, ET VIDE. C'est elle qui retient l'absence : sans
  // cela, une icône manquante se redemanderait à chaque image.
  IconTexture& icon = loaded_[item_id];
  if (vfs_ == nullptr || items_ == nullptr) {
    return icon;
  }
  LoadInto(*vfs_, ItemIconPath(*items_, item_id), icon);
  return icon;
}

}  // namespace ro::render
