#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "ro/net/connection.h"

namespace ro::net {

// POURQUOI LA PARTIE A PERDU SON SERVEUR, dit au joueur.
//
// ⚠ EN TERMES DE JOUEUR, pas de socket : « protocole : opcode 0x1234 inconnu »
// va au journal, et l'écran dit ce que cela VEUT DIRE pour lui. Le détail
// technique reste dans le journal, où un rapport de bogue le retrouve.
enum class LinkLossCause : std::uint8_t {
  // Le serveur a fermé proprement. Le plus souvent : il n'avait rien reçu de
  // ce client depuis une minute.
  kClosedByPeer,
  // Rien reçu depuis `kLinkSilentLossMs` : voir `LinkWatch`.
  kSilent,
  // Le flux n'est plus interprétable.
  kProtocolError,
  // La socket a échoué.
  kIoError,
  // Le serveur a passé la main à un autre serveur de zone, et l'entrée y a
  // échoué : la partie est restée hors ligne.
  kZoneMoveFailed,
};

// La cause qu'un état de liaison fermée désigne. ⚠ `kOpen` n'en est pas une :
// il rend `kIoError`, la plus générale, plutôt qu'un silence.
[[nodiscard]] LinkLossCause LinkLossCauseOf(LinkStatus status) noexcept;

// La phrase qui la dit, traduite, avec sa majuscule et son point.
[[nodiscard]] std::string LinkLossText(LinkLossCause cause);

// La phrase qui dit une fin de session DÉCIDÉE par le serveur — voir
// `BanText`, dont le motif s'écrit en minuscule pour se glisser dans une ligne.
[[nodiscard]] std::string EvictionText(std::uint8_t code);

// LA PERTE DE LIAISON DE CETTE PARTIE : déclarée une fois, puis acquittée.
//
// 🔴 LA PREMIÈRE CAUSE GAGNE. Une éviction est suivie de la fermeture de la
// socket ; un silence déclaré ferme la liaison à son tour. Sans ce verrou la
// boîte changerait de texte sous les yeux du joueur, et la conversation
// recevrait deux lignes pour une seule panne.
//
// ⚠ LE MOTIF NE PART VERS L'ÉCRAN DE CONNEXION QU'UNE FOIS ACQUITTÉ. Le joueur
// qui ferme la fenêtre pendant que la boîte est ouverte veut QUITTER, et non
// revenir au formulaire.
class LinkLoss {
 public:
  // Déclare la perte, avec la phrase qui en dit la cause. Rend vrai la première
  // fois seulement : c'est elle qui ouvre la boîte et écrit la ligne.
  bool Declare(std::string_view cause);

  // Le joueur a pris acte. Sans effet tant que rien n'a été déclaré.
  void Acknowledge() noexcept;

  [[nodiscard]] bool declared() const noexcept { return declared_; }
  [[nodiscard]] bool acknowledged() const noexcept { return acknowledged_; }

  // LE TEXTE ENTIER : « Déconnecté du serveur. » puis la cause, à la ligne.
  [[nodiscard]] const std::string& text() const noexcept { return text_; }

  // Ce que l'écran de connexion doit afficher en rentrant, ou vide.
  [[nodiscard]] std::string LoginNotice() const { return acknowledged_ ? text_ : std::string(); }

 private:
  std::string text_;
  bool declared_ = false;
  bool acknowledged_ = false;
};

}  // namespace ro::net
