#pragma once

#include <cstdint>

#include "ro/sim/actor.h"

namespace moonclient {

// « QUI EST-CE ? » : QUAND RÉCLAMER LA CARTE DE NOM DE CE QUE LA SOURIS DÉSIGNE.
//
// 🔴 LA CARTE D'UN JOUEUR NE VIENT QUE SUR DEMANDE, ET ELLE PART AVEC L'ACTEUR.
// Le serveur ne la pousse pas à l'entrée en vue, et une sortie de vue ou un
// changement de carte l'emporte : le même joueur revient sans elle, sous le
// même identifiant. Un antirebond qui retiendrait la dernière DEMANDE le
// laisserait sans groupe, sans guilde et sans titre jusqu'à ce qu'on survole
// quelqu'un d'autre.
//
// ⚠ L'ANTIREBOND NE VAUT DONC QUE POUR UN SURVOL ININTERROMPU : dès que la
// souris quitte l'acteur, la question peut se reposer. Ce qui empêche de
// redemander sans fin est la carte POSÉE, qui répond tant que l'acteur reste.
class NameCardAsker {
 public:
  // L'identifiant dont il faut demander la carte à cette image, ou zéro.
  [[nodiscard]] std::uint32_t AskedFor(const ro::sim::Actor* hovered);

  // L'arrivée sur une carte : aucun acteur d'avant n'y garde sa carte.
  void Forget() noexcept { asked_id_ = 0; }

 private:
  // L'acteur dont la carte est déjà demandée pendant CE survol, ou zéro.
  std::uint32_t asked_id_ = 0;
};

}  // namespace moonclient
