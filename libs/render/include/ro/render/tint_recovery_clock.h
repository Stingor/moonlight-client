#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace ro::render {

// LES IMAGES QUI NE DOIVENT AUCUN TIC, et ce qu'elles font à la couleur d'un
// corps.
//
// Une image qui ne doit aucun tic ne rejoue pas les effets qui teignent : leur
// mise à jour attend le tic suivant. L'acteur, lui, se met à jour à CHAQUE
// image, et sa couleur y regagne huit par composante. Au-delà de soixante
// images par seconde, une teinte réécrite à chaque tic remonte donc vers le
// blanc entre deux tics, puis y retombe au tic suivant : le corps papillote,
// d'autant plus que l'image est rapide. Voir `TintAfterEffectsAt`.
//
// L'horloge compte ces images, et dit combien sont passées depuis un tic donné.
// Elle tient le temps dû comme la troupe des nœuds : le reste d'un tic
// incomplet passe à l'image suivante.
class TintRecoveryClock {
 public:
  // Une image composée à cet instant.
  void NoteFrameAt(std::uint32_t now_ms);

  // Les images sans tic passées depuis le tic d'il y a `ticks_since` tics,
  // celles de l'attente en cours comprises. Zéro tic désigne le dernier tic
  // franchi.
  [[nodiscard]] std::uint32_t IdleFramesSince(std::uint32_t ticks_since) const;

 private:
  // Au-delà, huit par image et par tic ont ramené toute teinte au blanc.
  static constexpr std::size_t kRememberedTicks = 64;

  std::optional<std::uint32_t> last_frame_ms_;
  std::uint32_t owed_ms_ = 0;
  std::uint32_t tick_count_ = 0;
  std::uint32_t idle_total_ = 0;
  // Le compte des images sans tic au moment où chaque tic a été franchi.
  std::array<std::uint32_t, kRememberedTicks> idle_total_at_tick_{};
};

}  // namespace ro::render
