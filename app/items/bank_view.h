#pragma once

#include <cstdint>

#include "ro/net/connection.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/bank_window.h"

namespace moonclient {

// LA BANQUE, VUE DE LA PARTIE.
//
// Elle fait trois choses que la fenêtre ne peut pas faire seule : elle recopie
// les soldes du monde, elle route les verbes vers le serveur, et elle DIT dans
// la conversation ce que le serveur a répondu.
//
// 🔴 CE TROISIÈME POINT EST À NOUS, PAS AU SERVEUR. Il n'envoie qu'un CODE — la
// banque travaille, pas assez de zeny, le plafond —, et c'est le client qui
// choisit la phrase. C'est donc ici qu'elle se traduit : `net` ne traduit rien
// (il RELAIE ce que le serveur écrit, mot pour mot), et `ui` ne connaît aucun
// code de protocole.
//
// ⚠ ET C'EST BIEN DANS LA CONVERSATION que ces refus-là paraissent, pas dans la
// bande de la fenêtre : c'est là que le client d'origine les met. La bande, elle,
// ne porte que ce que la fenêtre a refusé toute seule, sans rien envoyer.
class BankView {
 public:
  // CE QUE LA FENÊTRE MONTRERA, recopié du monde à chaque image.
  [[nodiscard]] static ro::ui::BankWindowView Of(const ro::sim::World& world);

  // Route ce que la fenêtre a demandé.
  //
  // ⚠ REVALIDE AVANT D'ÉMETTRE, dans l'ordre du client d'origine, et pose le
  // message de la bande sur un refus. Le serveur revalide de toute façon ; ce
  // qu'on gagne ici, c'est un aller-retour évité et le MÊME libellé que lui.
  void Apply(const ro::ui::BankWindowAction& asked, ro::ui::BankWindowState& state,
             ro::net::Connection& link, ro::sim::World& world);

  // OUVRIR OU REFERMER LA SESSION — le raccourci, et le sac de zeny du pied.
  //
  // 🔴 OUVERTE, ON FERME SANS UN PAQUET ; fermée, on DEMANDE. C'est exactement
  // ce que fait le raccourci du client d'origine, et il n'a pas le choix : le
  // serveur ne tient aucun état de banque, et sa réponse BASCULE la fenêtre.
  //
  // ⚠ DEUX DEMANDES COUP SUR COUP L'OUVRIRAIENT PUIS LA REFERMERAIENT, pour
  // cette raison même — d'où la cadence minimale. Elle vaut pour l'OUVERTURE
  // seulement : un double-clic sur « Déposer » ne peut rien envoyer de plus, le
  // champ étant vidé au premier.
  void ToggleSession(ro::net::Connection& link, ro::sim::World& world, std::uint32_t now_ms);

  // CE QUE LE SERVEUR VIENT DE RÉPONDRE, écrit dans la conversation — une fois.
  //
  // ⚠ À APPELER À CHAQUE IMAGE, banque ouverte ou non : un refus arrive aussi
  // sur une demande d'ouverture qui n'ouvre rien, et il doit se lire quand même.
  void NoteVerdict(ro::sim::World& world, std::uint32_t now_ms);

 private:
  // Le rang du dernier verdict déjà écrit. ⚠ ZÉRO EST « AUCUN » : le monde
  // numérote ses verdicts à partir de un.
  std::uint64_t said_verdict_ = 0;
  // Quand la dernière demande d'ouverture est partie. ⚠ ZÉRO N'EST PAS « JAMAIS »
  // — c'est un instant comme un autre —, d'où le drapeau.
  std::uint32_t asked_ms_ = 0;
  bool ever_asked_ = false;
};

}  // namespace moonclient
