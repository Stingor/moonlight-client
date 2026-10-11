#pragma once

#include <cstdint>
#include <optional>

#include "ro/render/sprite_frame.h"

namespace ro::render {

// LA LÉVITATION DU VOL DE L'UNION : de combien flotte le corps d'un acteur dont
// l'option porte `kOptionFlying` — l'état que pose l'Union du Star Gladiator.
//
// Elle tient en deux nombres : un ANGLE, qui avance de deux degrés à chaque
// calque peint, et un PALIER, la hauteur de repos, qui glisse d'un cran vers
// sa cible chaque fois que l'angle tombe sur un multiple de trente degrés. La
// hauteur vaut `4 sin(angle) + palier`, tronquée, jamais négative, en pixels
// vers le HAUT.
//
// ⚠ L'ANGLE AVANCE PAR CALQUE, PAS PAR IMAGE. Le client recalcule la hauteur à
// chaque partie de l'acteur qu'il peint — corps, tête, coiffes, arme,
// bouclier, cape — si bien qu'un personnage bien équipé oscille plus vite
// qu'un personnage nu. Voir `UnionHoverClock`.
//
// Ce qui flotte : tous les calques du corps et les copies qu'un effet pose
// autour de lui, les `.str` qu'un effet attache à l'acteur, et le décor de
// Digital Space. Ce qui reste au sol : l'ombre, l'emprise cliquable, le nom et
// les jauges.

// VERS QUOI LE PALIER GLISSE, d'après ce que l'acteur fait.
enum class UnionHoverPosture : std::uint8_t {
  // Debout, en marche ou en garde — ou sous le voile rouge : vers vingt, d'un
  // cran par trente degrés.
  kAfloat,
  // Assis : vers cent, au même pas.
  kSitting,
  // Tout le reste — un coup, une incantation, une blessure, la mort : vers
  // zéro, d'un cran à CHAQUE pas.
  kLanding,
};

// La posture d'un acteur qui joue `motion`. `red_veiled` dit que le voile rouge
// d'un effet est levé sur lui : il le tient en l'air quoi qu'il fasse.
[[nodiscard]] UnionHoverPosture UnionHoverPostureOf(Motion motion, bool red_veiled) noexcept;

struct UnionHover {
  // De zéro à trois cent cinquante-huit, toujours pair.
  std::uint16_t angle_degrees = 0;
  // La hauteur de repos, en pixels.
  std::uint8_t rest_pixels = 0;

  [[nodiscard]] friend bool operator==(const UnionHover&, const UnionHover&) = default;
};

// UN PAS : l'angle avance, puis le palier glisse selon la posture.
[[nodiscard]] UnionHover StepUnionHover(UnionHover hover, UnionHoverPosture posture) noexcept;

// La hauteur dont le corps flotte, en pixels de sprite vers le haut.
//
// ⚠ LE CLIENT LA MULTIPLIE PAR UNE ÉCHELLE D'ÉCRAN, quatre cents sur la
// hauteur de sa vue, comme la montée d'un saut. Nous travaillons en pixels de
// sprite, où cette échelle vaut un.
[[nodiscard]] std::int32_t UnionHoverPixels(UnionHover hover) noexcept;

// L'HORLOGE D'UN ACTEUR QUI FLOTTE : elle compte les images que le client
// aurait peintes depuis la dernière — soixante par seconde —, fait avancer la
// lévitation d'autant de pas que l'acteur a de calques à chacune, et dit la
// hauteur qui en sort.
class UnionHoverClock {
 public:
  // La hauteur à cette image, en pixels de sprite vers le haut. `parts_drawn`
  // est le nombre de parties de l'acteur que l'image peint ; un acteur qui n'en
  // a pas compte pour une. Une image composée avant que la suivante du client
  // soit due garde la hauteur de la précédente.
  [[nodiscard]] std::int32_t PixelsAt(std::uint32_t now_ms, UnionHoverPosture posture,
                                      std::uint32_t parts_drawn);

  [[nodiscard]] UnionHover hover() const noexcept { return hover_; }

 private:
  std::optional<std::uint32_t> last_frame_ms_;
  float owed_ms_ = 0.0F;
  UnionHover hover_;
};

}  // namespace ro::render
