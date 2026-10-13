#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ro/sim/inventory.h"

namespace ro::sim {

// CE QUE LE SERVEUR APPREND SUR DES OBJETS QUI NE CHANGENT PAS LE SAC : le
// ramassage d'un membre du groupe, une pièce d'équipement qui s'abîme, et ce
// qu'il annonce à tous — un objet rare obtenu, un raffinage, un grade.
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

// D'OÙ VIENT UN OBJET RARE ANNONCÉ À TOUS. Les valeurs sont celles du fil
// (`BROADCASTING_SPECIAL_ITEM_OBTAIN` du serveur).
enum class RareObtainSource : std::uint8_t {
  kBox = 0,
  kMonster = 1,
  kNpc = 2,
};

// UN JOUEUR A OBTENU UN OBJET RARE, et le serveur l'annonce à tous : ouvert
// d'une boîte, pris sur un monstre, ou reçu d'un PNJ. Le nom du joueur et
// celui du monstre sont en UTF-8.
//
// ⚠ UNE SOURCE INCONNUE SE GARDE TELLE QUELLE : le client la dit comme une
// boîte dont il ne connaît pas le nom.
struct RareItemObtain {
  std::uint8_t source = 0;
  std::string player_name;
  std::uint32_t item_id = 0;
  // La boîte ouverte, pour `kBox` ; le monstre, pour `kMonster`.
  std::uint32_t box_item_id = 0;
  std::string monster_name;
  std::uint32_t heard_ms = 0;
};

// UN RAFFINAGE QUE LE SERVEUR ANNONCE À TOUS, réussi ou manqué.
struct RefineAnnounce {
  std::string player_name;
  std::uint32_t item_id = 0;
  int refine_level = 0;
  bool success = false;
  std::uint32_t heard_ms = 0;
};

// UNE AMÉLIORATION DE GRADE QUE LE SERVEUR ANNONCE À TOUS, réussie ou manquée.
// `grade` est le rang du fil : 1 pour D, jusqu'à 7 pour SS.
struct GradeAnnounce {
  std::string player_name;
  std::uint32_t item_id = 0;
  int grade = 0;
  bool success = false;
  std::uint32_t heard_ms = 0;
};

// Au-delà, la plus ancienne nouvelle part : l'application les prend à chaque
// image, et un tel retard ne vient que d'une partie figée.
inline constexpr std::size_t kItemNewsKept = 64;

class ItemNews {
 public:
  void NotePartyPickup(PartyPickup pickup);
  void NoteEquipDamage(EquipDamage damage);
  void NoteRareObtain(RareItemObtain obtained);
  void NoteRefine(RefineAnnounce refined);
  void NoteGrade(GradeAnnounce graded);

  // Les nouvelles arrivées depuis la dernière prise, dans leur ordre ; la file
  // se vide.
  [[nodiscard]] std::vector<PartyPickup> TakePartyPickups();
  [[nodiscard]] std::vector<EquipDamage> TakeEquipDamages();
  [[nodiscard]] std::vector<RareItemObtain> TakeRareObtains();
  [[nodiscard]] std::vector<RefineAnnounce> TakeRefines();
  [[nodiscard]] std::vector<GradeAnnounce> TakeGrades();

 private:
  std::vector<PartyPickup> pickups_;
  std::vector<EquipDamage> damages_;
  std::vector<RareItemObtain> rare_obtains_;
  std::vector<RefineAnnounce> refines_;
  std::vector<GradeAnnounce> grades_;
};

}  // namespace ro::sim
