#include "ro/render/skill_icons.h"

#include <utility>

#include "ro/formats/bmp.h"
#include "ro/formats/image.h"
#include "ro/render/skin_pieces.h"

namespace ro::render {

const IconTexture& SkillIcons::Get(std::uint16_t skill_id, std::string_view idname) {
  const auto known = loaded_.find(skill_id);
  if (known != loaded_.end()) {
    return known->second;
  }
  // ⚠ LA CASE SE POSE D'ABORD, ET VIDE. C'est elle qui retient l'absence : sans
  // cela, une icône manquante se redemanderait à chaque image.
  IconTexture& icon = loaded_[skill_id];
  if (idname.empty() || vfs_ == nullptr) {
    return icon;
  }

  std::string path(kSkinRoot);
  path += kIconFolder;
  path += idname;
  path += kIconSuffix;
  // ⚠ LE CLIENT ÉCRIT SES NOMS DE FICHIER EN MINUSCULES, et les tables donnent
  // le nom interne en MAJUSCULES. Le VFS ne fait pas la différence — il range ses
  // clés sans casse —, mais qui lit ce chemin dans un journal doit savoir d'où
  // vient le nom.
  auto bytes = vfs_->Read(path);
  if (!bytes) {
    return icon;
  }
  auto image = formats::DecodeBmp(*bytes);
  if (!image) {
    return icon;
  }
  // Même convention que les pièces du skin : le magenta pur est la transparence
  // des textures du jeu, antérieure au canal alpha.
  formats::ApplyMagentaColorKey(*image);
  // De l'art au pixel, que le DESSIN choisit d'interpoler ou non : sous ImGui,
  // le filtre ne se choisit pas à la texture. Voir `Filtering::kPixelArt`, et la
  // grille du grimoire, qui pose le mode net par un callback.
  auto texture = GlTexture::Upload(*image, GlTexture::Filtering::kPixelArt);
  if (!texture) {
    return icon;
  }
  icon.texture = std::move(*texture);
  icon.width_px = image->width;
  icon.height_px = image->height;
  return icon;
}

}  // namespace ro::render
