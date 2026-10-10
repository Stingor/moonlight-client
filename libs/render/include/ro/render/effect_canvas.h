#pragma once

#include <cstdint>
#include <span>

#include <glm/glm.hpp>

#include "ro/formats/image.h"
#include "ro/render/effect_scene.h"

namespace ro::render {

// Peint des quads d'effet dans une image, SANS GPU.
//
// Même raison d'être que le compositeur de sprites : les conventions d'un
// effet — l'ordre des coins, le sens de l'angle, la portion de texture, et
// surtout le MÉLANGE — sont de celles dont les erreurs ne se voient qu'à l'œil.
// Pouvoir produire l'image hors de tout contexte graphique permet de la
// regarder, et un jour de la comparer automatiquement.
//
// Le mélange est celui de Direct3D, appliqué canal par canal : c'est ce que le
// fichier déclare, et le traduire en OpenGL est l'affaire du renderer.
class EffectCanvas {
 public:
  // ⚠ L'image part NOIRE ET OPAQUE, comme un tampon d'image, et non
  // transparente comme une planche de sprite.
  //
  // Ce n'est pas un choix d'affichage : c'est une condition du mélange. Le
  // couple dominant des fichiers prend la source par son alpha et la
  // destination par l'ALPHA DE LA DESTINATION. Sur un fond opaque, cela vaut
  // une addition — c'est le halo des effets du jeu. Sur un fond transparent,
  // la destination ne compte plus, et chaque halo devient un carré plein.
  EffectCanvas(std::int32_t width, std::int32_t height);

  // `textures` fournit une image par NUMÉRO de la liste à plat de l'effet ;
  // un quad qui en désigne une absente ou vide ne se peint pas.
  //
  // `origin` est le point de l'image où tombe le centre de l'effet.
  void Paint(std::span<const EffectQuad> quads, std::span<const formats::Image> textures,
             glm::vec2 origin);

  [[nodiscard]] const formats::Image& image() const noexcept { return image_; }

 private:
  formats::Image image_;
};

}  // namespace ro::render
