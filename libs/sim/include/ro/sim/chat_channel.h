#pragma once

#include <cstdint>
#include <string>

namespace ro::sim {

// UN CANAL DE CONVERSATION DU SERVEUR.
//
// 🔴 LE CLIENT N'A AUCUN MOYEN DE LES CONNAÎTRE TOUT SEUL. Ils vivent dans
// `conf/channels.conf`, filtrés par le groupe du joueur, et la seule façon de
// les lire en jeu est de taper « @channel list » et d'analyser un texte
// LOCALISÉ — donc instable. Le serveur pousse donc sa liste au login, avec les
// droits déjà appliqués : le client dessine ce qu'il reçoit, il ne devine rien
// et ne filtre rien.
//
// ⚠ ET UN CANAL N'A PAS DE PAQUET À LUI POUR PARLER : il passe par le
// CHUCHOTEMENT, dont le destinataire est le nom du canal, croisillon compris.
// Voir `ro/net/chat_target.h`.
struct ChatChannel {
  // SANS LE CROISILLON — c'est ainsi que le serveur range ses canaux, et
  // `channel_name2channel` compare toujours à partir du caractère suivant. Le
  // croisillon se remet au moment d'écrire sur le fil, et nulle part ailleurs.
  std::string name;
  // Le libellé d'affichage, « [Global] », tel que `channels.conf` l'écrit.
  // Vide pour un canal qui n'en a pas : c'est alors le nom qui s'affiche.
  std::string alias;
  // 0xRRGGBB, DÉJÀ remis à l'endroit.
  //
  // ⚠ LE SERVEUR LA STOCKE EN BGR, et ce n'est pas une erreur de sa part :
  // `channel.cpp` convertit « RGB to BGR » en lisant `channels.conf`, parce que
  // c'est la forme que Direct3D attend. La remettre à l'endroit est l'affaire
  // de qui décode, une fois, et non de chaque endroit qui peint.
  std::uint32_t color = 0;
  // Le joueur a-t-il le droit d'y écrire ? Un canal en lecture seule existe —
  // une annonce de serveur — et l'armer enverrait dans le vide.
  bool can_write = false;
  // Le canal n'existe que pour un joueur EN GUILDE (`#ally`). Le serveur
  // l'envoie quand même, MARQUÉ : c'est au client de le masquer, et une guilde
  // rejointe en cours de séance le fait alors paraître sans un paquet de plus.
  bool guild_only = false;

  [[nodiscard]] friend bool operator==(const ChatChannel&, const ChatChannel&) = default;
};

// Ce que le serveur écrit devant un nom de canal sur le fil.
inline constexpr char kChannelPrefix = '#';

}  // namespace ro::sim
