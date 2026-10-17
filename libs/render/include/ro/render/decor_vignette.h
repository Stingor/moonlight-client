#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

#include "ro/core/error.h"
#include "ro/formats/image.h"
#include "ro/render/model_mesh.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// ── LA VIGNETTE D'UN MODÈLE DU DÉCOR ────────────────────────────────────────
//
// L'aperçu qu'un .rsm ou un .rsm2 montre dans l'éditeur de carte, avant d'être
// posé : une IMAGE carrée, fond transparent, rendue hors écran par
// `RasterModelVignette` — sans GPU, donc vérifiable dans un essai et par
// `roc-cli model thumb`.
//
// Le modèle se pose comme sur la carte, à l'origine et sans rotation —
// `ModelInstanceMatrix` —, à l'image zéro : celle de `BuildModelMesh`. La
// caméra le regarde de trois quarts, sous la plongée du jeu, et sa boîte
// entière tient dans le cadre.

// Le côté de la vignette, en pixels.
inline constexpr std::int32_t kDecorVignetteSidePx = 192;

// La vignette d'un modèle déjà lu ; vide quand il n'a aucune face.
//
// `textures` suit `mesh.textures` ; une entrée nulle peint ses faces en gris.
// `shade_type` est celui du fichier : zéro peint le modèle à pleine lumière,
// comme sur la carte.
[[nodiscard]] formats::Image RasterDecorVignette(
    const ModelMesh& mesh, std::span<const std::shared_ptr<const formats::Image>> textures,
    std::int32_t rsm_version, std::int32_t shade_type, std::int32_t side_px);

// Lit `data\model\<rsm_file>` — un nom en UTF-8, tel que la bibliothèque et
// l'arbre de la scène le montrent — et ses textures, puis en rend la vignette.
//
// Échoue quand le fichier manque, ne se lit pas, ou n'a rien à montrer. Une
// texture absente n'est pas un échec : ses faces se peignent en gris.
[[nodiscard]] core::Result<formats::Image> LoadDecorVignette(vfs::Vfs& vfs,
                                                             std::string_view rsm_file,
                                                             std::int32_t side_px);

}  // namespace ro::render
