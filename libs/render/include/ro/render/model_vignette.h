#pragma once

#include <cstdint>
#include <span>

#include "ro/formats/gr2_model.h"
#include "ro/formats/image.h"
#include "ro/render/actor_model_pose.h"

namespace ro::render {

// ── LA VIGNETTE D'UN MODÈLE 3D, RENDUE EN IMAGE ─────────────────────────────
//
// Les acteurs sans sprite — Emperium, gardiens de forteresse, drapeau, coffres
// au trésor — se montrent dans une fenêtre comme un sprite : une IMAGE, rendue
// hors écran, que l'interface pose. Le rendu est logiciel et
// déterministe, et s'essaie sans GPU.
//
// ⚠ UNE PROJECTION ORTHOGRAPHIQUE, cadrée sur la POSE COURANTE : une animation
// sort du volume de repos, et cadrer sur le repos ferait respirer le modèle
// hors du cadre. Le modèle tourne autour de sa verticale — son Z —, puis la
// caméra plonge.
struct ModelVignetteView {
  std::int32_t width = 0;
  std::int32_t height = 0;
  // Rotation autour de la verticale du modèle, en radians : ce que la molette
  // manipule.
  float yaw_radians = 0.0F;
  // 🔴 LA PLONGÉE DE LA CAMÉRA DU JEU, et ce n'est pas un goût : un modèle rendu
  // à vingt degrés paraît « pas comme en jeu » à côté de la carte.
  float pitch_radians = 0.785398F;
  // La marge autour du modèle, en fraction du cadre.
  float margin = 0.06F;
  // Rendu au double puis réduit : les arêtes d'un cristal perdent leur escalier.
  bool supersample = true;
  // L'éclairage : ambiant, plus diffus sous une lumière fixe par rapport à la
  // CAMÉRA, pour que la rotation éclaire tour à tour toutes les faces.
  float ambient = 0.50F;
  float diffuse = 0.78F;
};

// Un maillage à peindre, déjà posé.
struct ModelVignetteMesh {
  std::span<const PosedVertex> vertices;
  std::span<const std::uint16_t> indices;
  // Sa texture, ou nulle : le maillage se peint alors en gris.
  const formats::GrannyTextureImage* texture = nullptr;
};

// L'image RGBA, fond transparent, ligne du haut d'abord ; vide quand il n'y a
// rien à peindre.
//
// ⚠ AUCUNE FACE N'EST ÉCARTÉE : le drapeau est un plan et les cristaux de
// l'Emperium des coques. Une face se voit et s'éclaire des deux côtés.
[[nodiscard]] formats::Image RasterModelVignette(std::span<const ModelVignetteMesh> meshes,
                                                 const ModelVignetteView& view);

}  // namespace ro::render
