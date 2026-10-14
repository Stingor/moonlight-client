#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/character_ledger.h"
#include "ro/sim/chat_line.h"

namespace moonclient {

// LES LIGNES « ÉQUIPEMENT MIS/RETIRÉ » : une pièce posée ou retirée, et le jeu
// d'équipement de rechange.
//
// Comme le client d'origine : à l'accusé ACCORDÉ de pose ou de retrait
// (`ro::net::GrantedEquipChangeOf`), une ligne « <nom> a été équipé. », verte,
// ou « <nom> a été retiré. », rouge, dans la case 7 de la discussion. Elle ne
// demande que les accusés du jeu ; la case « Attributs d'équipement », que le
// serveur nourrit à part, suit sous elle.
//
// Le nom est le nom affiché complet — affinage, cartes, emplacements, forgeron —,
// sans la marque d'une pièce cassée, que le client d'origine n'y met pas. Une
// pièce absente du sac ne dit rien : sans elle, il n'y a pas de nom.

struct EquipChangeLine {
  std::string text;
  std::uint32_t color = 0;
  ro::sim::ChatType type = ro::sim::ChatType::kEquipmentOnOff;
};

// La phrase traduite autour d'un nom déjà composé, et sa couleur.
[[nodiscard]] EquipChangeLine EquipChangeLineOf(std::string_view item_name, bool equipped);

// LES LIGNES D'UN ACCUSÉ DU JEU DE RECHANGE (`ro::net::EquipSwitchNewsOf`), dans
// l'ordre du client d'origine :
//
// - pièce rangée : « <nom> a été placé dans la fenêtre d'échange
//   d'équipement. », verte, case 7 ;
// - pièce sortie : « <nom> a été retiré de la fenêtre d'échange
//   d'équipement. », verte, case 7 — précédée, pour des munitions, de la ligne
//   rouge « <nom> a été retiré. » ;
// - refus : sa phrase, rouge, dans la case générale (0) ; un refus de sortie
//   sort quand même la pièce, et des munitions disent encore leur retrait.
//
// `item_name` vide tait les lignes qui nomment la pièce, pas les refus.
[[nodiscard]] std::vector<EquipChangeLine> EquipSwitchLines(const ro::sim::EquipSwitchNews& news,
                                                            std::string_view item_name);

}  // namespace moonclient
