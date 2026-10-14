#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace ro::ui {

// LE BORD DE L'ÉCRAN AUQUEL UNE PLACE S'ANCRE, sur un axe. Voir `WindowPlace`.
//
// ⚠ À PART, et sans rien d'autre : les réglages de la minimap le portent, et
// l'en-tête des places les inclut déjà.
enum class AnchorSide : std::uint8_t {
  kStart,   // le bord gauche, ou le haut
  kCenter,  // le milieu de l'écran
  kEnd,     // le bord droit, ou le bas
};

// Les mots d'une ancre dans les fichiers de disposition, dans l'ordre de
// `AnchorSide`. Une seule table : les places des fenêtres et celle de la
// minimap s'écrivent de la même façon.
inline constexpr std::array<std::string_view, 3> kAnchorSideWords = {"start", "center", "end"};

[[nodiscard]] constexpr std::string_view AnchorSideWord(AnchorSide side) {
  return kAnchorSideWords[static_cast<std::size_t>(side)];
}

[[nodiscard]] constexpr std::optional<AnchorSide> AnchorSideFromWord(std::string_view word) {
  for (std::size_t rank = 0; rank < kAnchorSideWords.size(); ++rank) {
    if (word == kAnchorSideWords[rank]) {
      return static_cast<AnchorSide>(rank);
    }
  }
  return std::nullopt;
}

}  // namespace ro::ui
