#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ro/sim/inventory.h"

namespace ro::sim {

// CE QUE LE SERVEUR APPREND SUR DES OBJETS QUI NE CHANGENT PAS LE SAC : le
// ramassage d'un membre du groupe, et une pièce d'équipement qui s'abîme.
//
// Ce ne sont que des nouvelles à DIRE. Le texte se compose plus haut, là où
// l'on sait nommer un objet et retrouver un membre du groupe : `net` dépose les
// faits, l'application les prend une fois et les écrit dans la conversation.
//
// ⚠ LES FAITS SONT DATÉS À LA RÉCEPTION : la ligne prend cet instant, et non
// celui où on la relève.

// Un membre du groupe a ramassé un objet. Le serveur le dit à ceux du groupe qui
// sont sur la même carte, et pas à celui qui ramasse.
struct PartyPickup {
  std::uint32_t account_id = 0;
  // L'objet tel que le fil le décrit : identifiant, raffinement, cartes, grade.
  // ⚠ SANS CASE NI QUANTITÉ : il n'est pas dans notre sac.
  InventoryItem item;
  std::uint32_t heard_ms = 0;
};

// Une pièce d'équipement s'est abîmée. `location` porte les drapeaux
// d'emplacement (`EQP_*`) de la pièce.
struct EquipDamage {
  std::uint32_t account_id = 0;
  std::uint16_t location = 0;
  std::uint32_t heard_ms = 0;
};

// Au-delà, la plus ancienne nouvelle part : l'application les prend à chaque
// image, et un tel retard ne vient que d'une partie figée.
inline constexpr std::size_t kItemNewsKept = 64;

class ItemNews {
 public:
  void NotePartyPickup(PartyPickup pickup);
  void NoteEquipDamage(EquipDamage damage);

  // Les nouvelles arrivées depuis la dernière prise, dans leur ordre ; la file
  // se vide.
  [[nodiscard]] std::vector<PartyPickup> TakePartyPickups();
  [[nodiscard]] std::vector<EquipDamage> TakeEquipDamages();

 private:
  std::vector<PartyPickup> pickups_;
  std::vector<EquipDamage> damages_;
};

}  // namespace ro::sim
