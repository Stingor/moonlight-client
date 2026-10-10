#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ro/sim/inventory.h"

namespace ro::sim {

// L'ÉQUIPEMENT D'UN AUTRE JOUEUR, tel que le serveur l'a montré à notre demande.
//
// 🔴 LA RÉPONSE NE PORTE QUE SON NOM, jamais son identifiant : deux homonymes ne
// s'y distinguent pas, et qui veut redemander doit avoir retenu à qui il a
// demandé. Ce livre ne retient donc que ce que le fil dit ; l'identifiant est
// l'affaire de qui a posé la question.
//
// ⚠ L'APPARENCE EST CELLE QUE LE JOUEUR MONTRE À L'ÉCRAN, costumes compris :
// ce sont les numéros de sprite de l'en-tête, pas les objets de la liste.
struct ViewedLook {
  std::uint16_t job = 0;
  std::uint16_t hair = 0;
  std::uint16_t head_low = 0;
  std::uint16_t head_mid = 0;
  std::uint16_t head_top = 0;
  std::uint16_t robe = 0;
  std::uint16_t hair_color = 0;
  std::uint16_t clothes_color = 0;
  // Le corps montré, BRUT : sans style porté, le serveur y remet la classe.
  std::uint16_t body2 = 0;
  // Un pour un homme, zéro pour une femme.
  std::uint8_t sex = 0;
};

struct ViewedEquipment {
  std::string name;
  ViewedLook look;
  // Les pièces PORTÉES, chacune avec ce que le fil en dit : cartes, options,
  // raffinement, grade, état. Rien d'autre du sac n'est montré.
  //
  // 🔴 UNE ENTRÉE PAR EMPLACEMENT OCCUPÉ, PAS PAR OBJET : le serveur parcourt
  // ses emplacements, si bien qu'une arme à deux mains ou une coiffe qui couvre
  // le haut et le milieu arrive DEUX FOIS, sous la même case du sac. La
  // fenêtre range chaque pièce à sa première place et n'en montre qu'une ; son
  // compte, lui, est celui des entrées.
  std::vector<InventoryItem> pieces;
};

// ⚠ HORS DU MONDE, comme les titres et la guilde : rien ne la lit en même temps
// qu'un acteur, et le monde touche son plafond de lignes.
class ViewedEquipmentBook {
 public:
  // La dernière fiche reçue, ou rien.
  [[nodiscard]] const std::optional<ViewedEquipment>& shown() const noexcept { return shown_; }
  // Combien de réponses sont arrivées : ce qui change dit qu'une nouvelle est là,
  // même quand elle décrit le même joueur à l'identique.
  [[nodiscard]] std::uint32_t received() const noexcept { return received_; }

  void Receive(ViewedEquipment sheet) {
    shown_ = std::move(sheet);
    ++received_;
  }
  void Forget() noexcept { shown_.reset(); }

  // Combien de fois le serveur a refusé : le joueur visé ne montre pas son
  // équipement. ⚠ LE SEUL REFUS QUI PARLE — les autres, cible partie ou sur une
  // autre carte, ne reçoivent aucune réponse.
  [[nodiscard]] std::uint32_t refusals() const noexcept { return refusals_; }
  void NoteRefused() noexcept { ++refusals_; }

 private:
  std::optional<ViewedEquipment> shown_;
  std::uint32_t received_ = 0;
  std::uint32_t refusals_ = 0;
};

}  // namespace ro::sim
