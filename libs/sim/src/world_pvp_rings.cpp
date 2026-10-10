// L'ANNEAU DE RANG PVP porté par les acteurs : la part du monde qui le pose, le
// repose et le retire.
//
// Elle vit à part de `world.cpp` pour tenir le plafond de huit cents lignes, et
// le découpage suit la matière : ces trois méthodes ne parlent qu'au carnet des
// rangs (`ro/sim/pvp_rank.h`) et à la liste d'effets d'un acteur, alors que le
// reste du monde ne les regarde jamais.

#include <algorithm>

#include "ro/core/log.h"
#include "ro/sim/pvp_rank.h"
#include "ro/sim/world.h"

namespace ro::sim {

bool World::TellActorPvpRank(std::uint32_t id, std::int32_t rank, std::uint32_t now_ms) {
  // 🔴 LE CARNET D'ABORD, ET MÊME POUR UN ABSENT. Le serveur ne redit un rang
  // que lorsqu'il CHANGE : celui qu'on jette parce que son porteur n'est pas
  // encore en vue ne reviendra jamais. Voir `PvpStanding::NoteActor`.
  pvp_standing_.NoteActor(id, rank);
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  found->second.pvp_rank = rank;
  RefreshPvpRing(id, now_ms);
  return true;
}

void World::RefreshPvpRings(std::uint32_t now_ms) {
  for (const auto& [id, actor] : actors_) {
    RefreshPvpRing(id, now_ms);
  }
}

void World::RefreshPvpRing(std::uint32_t id, std::uint32_t now_ms) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return;
  }
  const Actor& actor = found->second;
  // CE QU'IL PORTE AUJOURD'HUI, et pour quel rang. L'absence et « pour un autre
  // rang » ne se distinguent pas ici : les deux demandent une pose.
  const auto worn = std::ranges::find(actor.effects, kPvpMarkEffectId, &PlayedEffect::id);
  const bool wears_it = worn != actor.effects.end();

  if (!PvpRankMarked(actor.pvp_rank)) {
    // Hors des dix premiers, l'anneau s'en va. Ne poser que la pose laisserait
    // une carte entière marquée au bout d'une heure : chacun y passe.
    if (wears_it) {
      static_cast<void>(RemoveEffect(id, kPvpMarkEffectId));
      core::LogDebug("anneau PVP retiré de {} (rang {})", id, actor.pvp_rank);
    }
    return;
  }
  // 🔴 LA POSE SE REJOUE QUAND LE RANG BOUGE, et non au seul franchissement du
  // seuil : c'est le RANG qui décide de la couleur de l'anneau — presque blanc
  // au premier, rouge vif au dixième —, et un anneau posé une fois pour toutes
  // garderait la teinte du rang qu'on avait en y entrant. Le client d'origine
  // fait de même, en plus brutal : son récepteur RETIRE puis REPOSE l'effet à
  // chaque annonce, sans regarder si le rang a bougé.
  //
  // ⚠ ET LE RANG VOYAGE AVEC L'EFFET, dans sa valeur. C'est le champ que le
  // client emploie : le rang et le nombre de coups d'un coup multi-coups vont
  // tous deux dans le paramètre de l'instance. Voir `ro/render/pvp_rank_tint.h`.
  //
  // ⚠ C'EST AUSSI CE QUI EMPÊCHE DE REPOSER À CHAQUE IMAGE. Cette passe tourne
  // à chaque battement ; comparer la valeur PORTÉE à celle voulue est ce qui la
  // rend muette tant que rien ne change — sans quoi l'anneau rejouerait son
  // animation soixante fois par seconde.
  //
  // 🔴 ET LA POSE RETIRE D'ABORD, comme le client. L'anneau ne meurt plus de
  // lui-même — son bâtisseur le rebâtit sans fin, voir l'horaire du patron 159 —,
  // si bien qu'une pose sans retrait EMPILERAIT : l'anneau du rang précédent
  // tournerait sous le nouveau, dans son ancienne teinte. Le retrait vaut aussi
  // quand la liste ne le porte plus alors que l'image le montre encore — un
  // acteur chargé d'effets pousse les plus anciens hors de sa liste.
  if (!wears_it || worn->value != actor.pvp_rank) {
    const std::int32_t rank = actor.pvp_rank;
    static_cast<void>(RemoveEffect(id, kPvpMarkEffectId));
    static_cast<void>(RecordEffect(id, kPvpMarkEffectId, now_ms, rank));
    core::LogDebug("anneau PVP posé sur {} (rang {})", id, rank);
  }
}

}  // namespace ro::sim
