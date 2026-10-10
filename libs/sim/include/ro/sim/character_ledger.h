#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ro::sim {

// UNE PIÈCE DU JEU D'ÉQUIPEMENT DE RECHANGE que le serveur tient.
struct EquipSwitchPiece {
  // La case de l'objet dans le sac, vue du client (`InventoryItem::client_slot`).
  std::uint16_t client_slot = 0;
  // Les emplacements qu'elle occupe dans ce jeu.
  std::uint32_t location = 0;
};

// CE QUE LE SERVEUR DIT DU PERSONNAGE ET QU'AUCUNE FENÊTRE NE MONTRE ENCORE.
//
// Le serveur l'envoie à l'entrée sur la carte, et ne le redit qu'au
// changement. Le garder dès maintenant, c'est ne pas avoir à attendre une
// reconnexion le jour où une fenêtre le lira.
//
// ⚠ LE JEU D'ÉQUIPEMENT DE RECHANGE N'EST PAS LES PRESETS du client : c'est le
// mécanisme du serveur, que le client n'emploie pas pour changer d'équipement.
// Il est gardé pour ce qu'il dit, rien de plus.
//
// ⚠ HORS DU MONDE, comme les réglages du personnage : c'est un état de
// SESSION, que le changement de carte ne remet pas à zéro.
class CharacterLedger {
 public:
  // Les packs d'émotions payants que le compte possède, et le nombre
  // d'utilisations qui leur restent — rien pour un pack sans limite.
  [[nodiscard]] const std::map<std::uint16_t, std::optional<std::int32_t>>& emotion_packs()
      const noexcept {
    return emotion_packs_;
  }
  [[nodiscard]] const std::vector<EquipSwitchPiece>& equip_switch() const noexcept {
    return equip_switch_;
  }
  // Les points de réputation, par faction.
  [[nodiscard]] const std::map<std::uint64_t, std::int64_t>& reputation() const noexcept {
    return reputation_;
  }

  // Un pack acheté — ou redit à la connexion, un par pack.
  void OwnEmotionPack(std::uint16_t pack_id, std::optional<std::int32_t> uses_left) {
    emotion_packs_[pack_id] = uses_left;
  }

  // ⚠ REMPLACE : le serveur envoie toujours la liste entière.
  void ReplaceEquipSwitch(std::vector<EquipSwitchPiece> pieces) {
    equip_switch_ = std::move(pieces);
  }

  // ⚠ FUSIONNE : le serveur envoie la liste entière à la connexion, puis la
  // seule faction qui change.
  void SetReputation(std::uint64_t faction, std::int64_t points) { reputation_[faction] = points; }

 private:
  std::map<std::uint16_t, std::optional<std::int32_t>> emotion_packs_;
  std::vector<EquipSwitchPiece> equip_switch_;
  std::map<std::uint64_t, std::int64_t> reputation_;
};

}  // namespace ro::sim
