#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "ro/formats/bmp.h"
#include "ro/formats/gnd.h"

namespace ro::render {

// Atlas des lightmaps du GND, et les coordonnées de chacune dedans.
//
// Le GND porte une petite lightmap par carreau — quelques dizaines de milliers
// pour une ville. Les envoyer une par une à la carte graphique coûterait autant
// de changements d'état ; on les assemble donc en une seule texture carrée.
//
// Chaque lightmap est faite d'un plan ALPHA (l'ombre, multiplicative) suivi
// d'une couleur RGB par texel (la lumière, additive). Les deux ne se composent
// pas de la même façon, d'où leur séparation dans les canaux de l'atlas.
struct LightmapAtlas {
  formats::Image image;
  // Coordonnées de texture, une paire min/max par lightmap.
  std::vector<std::array<float, 2>> minimum;
  std::vector<std::array<float, 2>> maximum;

  [[nodiscard]] bool empty() const noexcept { return image.empty(); }

  // Coordonnées d'une lightmap, ou l'origine si l'index est hors table — un
  // GND peut nommer une lightmap absente, et cela ne doit pas sortir du tableau.
  [[nodiscard]] std::array<float, 2> MinimumOf(std::size_t index) const;
  [[nodiscard]] std::array<float, 2> MaximumOf(std::size_t index) const;
};

// Assemble l'atlas. Rend un atlas vide si le GND ne porte pas de lightmap.
[[nodiscard]] LightmapAtlas BuildLightmapAtlas(const formats::GndMap& gnd);

}  // namespace ro::render
