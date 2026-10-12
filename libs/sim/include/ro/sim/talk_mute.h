#pragma once

namespace ro::sim {

// ── LA PAROLE QUE LE JOUEUR NE VEUT PLUS LIRE ──────────────────────────────
//
// Les deux bascules du jeu, sous leurs commandes :
//
//   · `/notalkmsg` (ou `/nm`) : la parole publique des autres, celle du champ de
//     bataille et les répliques des familiers et des monstres n'entrent plus
//     dans la conversation ;
//   · `/notalkmsg2` (ou `/nm2`) : les mêmes, plus la parole de guilde et celle
//     du clan.
//
// 🔴 LA LIGNE SE TAIT, PAS LA BULLE : ce qu'un acteur dit paraît toujours
// au-dessus de sa tête. Ni sa propre parole, ni celle du groupe, ni les
// chuchotements, ni ce qu'écrit le serveur ne sont touchés.
//
// ⚠ ÉTEINTES TOUTES DEUX PAR DÉFAUT, comme au client d'origine.
struct TalkMute {
  bool talk_hidden = false;
  bool talk_and_guild_hidden = false;

  // La parole publique, celle du champ de bataille, les répliques des familiers
  // et des monstres.
  [[nodiscard]] bool hides_talk() const noexcept { return talk_hidden || talk_and_guild_hidden; }
  // La parole de guilde et celle du clan.
  [[nodiscard]] bool hides_guild() const noexcept { return talk_and_guild_hidden; }
};

}  // namespace ro::sim
