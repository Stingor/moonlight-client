// La teinte de l'anneau du classement PVP. Voir l'en-tête pour les adresses
// où chaque nombre a été lu, et pour ce que la rampe fait d'un rang absent.

#include "ro/render/pvp_rank_tint.h"

#include <algorithm>

namespace ro::render {

std::array<float, 3> PvpTopRankTint(std::int32_t rank) noexcept {
  if (rank == kPvpTopRankGreenRank) {
    return {0.0F, kPvpTopRankRed, 0.0F};
  }
  // ⚠ ÉCRÊTÉ EN HAUT SEULEMENT, comme la rampe du client : elle ramène à 255 ce
  // qui dépasse, et c'est ce qui rend blanc l'anneau d'un rang moins un. Le bas
  // est borné par le dessin, qui ne peint pas de canal négatif.
  const auto pale = static_cast<float>(kPvpTopRankPaleRank - rank) * kPvpTopRankStep;
  const float channel = std::min(pale, 255.0F);
  return {kPvpTopRankRed, channel, channel};
}

}  // namespace ro::render
