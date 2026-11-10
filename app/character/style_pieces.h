#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/render/palette_ramps.h"
#include "ro/render/style_set.h"

namespace moonclient {

// LE PLUS DE PIÈCES QU'ON MARQUE : les huit rampes d'un corps, ou les treize
// classes de teinte d'un chapeau.
inline constexpr std::size_t kPieceMarksMost = ro::render::kLayerAdjustCount;

// CE QUE LE JOUEUR DIT DE SES PIÈCES, en dehors de leurs couleurs : celles
// qu'il a LIÉES — régler l'une règle les autres — et celles qu'il GARDE quand
// l'inspiration tire de nouvelles couleurs.
//
// ⚠ PROPRE À UN CORPS OU À UN CALQUE, comme la recette : un rang ne désigne pas la même pièce
// d'un corps à l'autre. ⚠ ET LOCAL À LA PARTIE : ni la recette ni le code ne le
// portent, c'est une aide à composer, pas une apparence.
class PieceMarks {
 public:
  // Lie deux pièces. Une pièce déjà liée y entraîne tout son groupe.
  void Link(std::size_t from, std::size_t to);
  // Délie une pièce. Un groupe réduit à une pièce disparaît.
  void Unlink(std::size_t rank);
  void ToggleLock(std::size_t rank);

  // Les pièces qu'un réglage de `rank` atteint : elle-même, puis ses liées.
  [[nodiscard]] std::vector<std::size_t> Reached(std::size_t rank) const;

  [[nodiscard]] std::span<const std::uint8_t> links() const noexcept { return links_; }
  // Une pièce gardée par bit.
  [[nodiscard]] std::uint32_t locked() const noexcept { return locked_; }
  [[nodiscard]] bool Locked(std::size_t rank) const noexcept {
    return rank < kPieceMarksMost && (locked_ & (1U << rank)) != 0;
  }

 private:
  // Le groupe de chaque pièce ; zéro : libre.
  std::array<std::uint8_t, kPieceMarksMost> links_{};
  std::uint32_t locked_ = 0;
};

// L'ACCORD QUE L'INSPIRATION TIRE.
enum class Harmony : std::uint8_t {
  // Des teintes voisines, à trente degrés de part et d'autre.
  kAnalogous,
  // Deux teintes opposées.
  kComplementary,
  // Trois teintes à cent vingt degrés.
  kTriadic,
};

// LES COULEURS QUE L'INSPIRATION PROPOSE, une par pièce : rien pour une pièce
// qu'elle laisse.
//
// 🔴 ELLE LAISSE LES PIÈCES GARDÉES, ET LES NEUTRES. Un gris, un noir ou un
// blanc sont le métal, le cuir, la doublure : les teindre au hasard rend un
// costume de carnaval. ⚠ ET ELLE GARDE LA CLARTÉ DE CHAQUE PIÈCE, qui fait le
// modelé du costume : seule la teinte change, et la saturation se relève assez
// pour se voir.
//
// `origin` porte la couleur d'origine de chaque pièce ; `seed` choisit la
// teinte de départ, si bien qu'un même tirage se rejoue.
[[nodiscard]] std::vector<std::optional<std::array<std::uint8_t, 3>>> Inspire(
    std::span<const std::array<std::uint8_t, 3>> origin, const PieceMarks& marks, Harmony harmony,
    std::uint32_t seed);

}  // namespace moonclient
