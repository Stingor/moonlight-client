#include "ro/sim/actor_traces.h"

namespace ro::sim {

void ForgetLastMapTraces(Actor& actor) {
  // Il n'est plus en marche : il vient d'arriver.
  actor.movement.reset();

  // LES COUPS, portés et reçus.
  actor.attack.reset();
  actor.hit.reset();
  actor.support.reset();

  // 🔴 L'INCANTATION ET LE SORT POSÉ AU SOL, les deux que le joueur a vus
  // revenir à chaque `@load`. L'anneau d'incantation se pose sur la case que
  // `ground_skill` nomme, et son bruit part avec lui.
  actor.cast.reset();
  actor.ground_skill.reset();
  actor.skill_use.reset();
  actor.skill_scale.reset();

  // CE QU'IL VIENT DE DIRE ou d'annoncer : une bulle n'a pas à traverser une
  // carte, et la bannière de compétence EST une bulle chez le client.
  actor.chat_text.clear();
  actor.chat_since_ms = 0;
  actor.announced_skill_id = 0;
  actor.announced_skill_since_ms = 0;
  actor.emotion.reset();
  actor.emotion_since_ms = 0;

  actor.pickup_since_ms.reset();

  // 🔴 SES TRAITS DE DEVOTION restent sur la carte quittée : ses protégés n'y
  // sont plus, et le serveur ne redit la liste que s'il la retend. La garder
  // ferait passer le croisé, à la première image de la carte suivante, pour un
  // acteur qu'on découvre avec sa liste — et les traits se retendaient seuls.
  actor.devotion = DevotionList();

  // 🔴 SON RANG PvP appartient à la carte quittée. Le serveur ne l'éteint pas
  // quand c'est nous qui partons, et `RefreshPvpRings` repose l'anneau de tout
  // acteur dont le rang reste marqué : l'effacer ci-dessous sans effacer le
  // rang le ferait revenir à la première image de la carte suivante.
  actor.pvp_rank = 0;

  // ⚠ ET SES EFFETS S'ÉTEIGNENT : changer de carte détruit l'acteur du client
  // natif, et ses instances d'effet avec lui. Le serveur s'appuie dessus —
  // `@size 0` ne renvoie aucun effet, il téléporte le joueur sur place et
  // compte sur le client pour oublier. La réduction du corps ne meurt jamais
  // d'elle-même : la garder laisserait un corps rapetissé pour la séance.
  actor.effects.clear();

  // ET SES TRAITS DE DEVOTION avec eux : ce sont des instances d'effet. Seule
  // une annonce du serveur les retend, sur la carte d'arrivée.
  actor.devotion = DevotionList();
}

}  // namespace ro::sim
