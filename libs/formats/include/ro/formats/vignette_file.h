#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"
#include "ro/core/file_stamp.h"
#include "ro/formats/image.h"

namespace ro::formats {

// ── LE FICHIER D'UNE VIGNETTE GARDÉE SUR LE DISQUE ──────────────────────────
//
// Une vignette de modèle coûte des dizaines de millisecondes à rendre ; la
// garder sur le disque rend la grille de la bibliothèque instantanée aux
// ouvertures suivantes. Le fichier porte sa CLÉ — ce dont l'image a été faite —
// et l'image, en RGBA8 compressé par zlib.
//
// Disposition, en petit-boutiste :
//
//   signature   « MCVG », puis u32 révision du format
//   clé         u16 longueur + nom du modèle (UTF-8) ; i32 côté ; u32 révision
//               du rendu ; u8 origine ; u16 longueur + archive ; u64 place ;
//               u64 taille stockée ; u64 taille ; i64 date
//   image       i32 largeur, i32 hauteur, u32 longueur + flux zlib — une
//               image vide dit que le modèle n'a rien à montrer
//
// 🔴 LA CLÉ SE COMPARE CHAMP À CHAMP avec celle qu'on attend : un seul champ
// qui diffère, et la vignette se refait. Rien n'est haché.

// Ce dont une vignette a été faite.
struct VignetteKey {
  // Le modèle, relatif à data\model\, en UTF-8.
  std::string model_file;
  // Le côté demandé, en pixels.
  std::int32_t side_px = 0;
  // La révision du rendu : une vignette rendue autrement est périmée.
  std::uint32_t render_revision = 0;
  // L'état du fichier du modèle dans l'installation.
  core::FileStamp stamp;

  friend bool operator==(const VignetteKey&, const VignetteKey&) = default;
};

struct VignetteFile {
  VignetteKey key;
  // Vide : le modèle ne se lit pas ou n'a aucune face, et cet échec se garde
  // comme une image.
  Image image;
};

[[nodiscard]] std::vector<std::uint8_t> EncodeVignetteFile(const VignetteFile& file);

// Refuse une signature, une révision ou une image incohérentes : un fichier
// abîmé se refait, il ne s'affiche pas.
[[nodiscard]] core::Result<VignetteFile> DecodeVignetteFile(std::span<const std::uint8_t> data);

}  // namespace ro::formats
