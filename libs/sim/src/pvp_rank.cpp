// LE CARNET DES RANGS D'UNE CARTE. Voir l'en-tête pour la raison qui oblige à
// garder un rang plus longtemps que l'acteur qui le porte.

#include "ro/sim/pvp_rank.h"

namespace ro::sim {

void PvpStanding::NoteActor(std::uint32_t id, std::int32_t rank) {
  if (id == 0) {
    return;
  }
  // ⚠ UN RANG QUI SORT DU CLASSEMENT S'EFFACE AU LIEU DE SE RANGER À ZÉRO. Le
  // carnet ne sert qu'à RENDRE un rang à un acteur qui revient ; y garder les
  // sortants le ferait grossir de tous ceux qui ont joué sur la carte, pour n'y
  // lire jamais rien d'autre que « pas de rang ».
  if (!PvpRankMarked(rank)) {
    ranks_.erase(id);
    return;
  }
  ranks_.insert_or_assign(id, rank);
}

std::int32_t PvpStanding::RankOf(std::uint32_t id) const {
  const auto found = ranks_.find(id);
  return found == ranks_.end() ? 0 : found->second;
}

}  // namespace ro::sim
