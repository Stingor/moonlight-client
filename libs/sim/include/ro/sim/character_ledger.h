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

// CE QU'UN ACCUSÉ DU JEU D'ÉQUIPEMENT DE RECHANGE ANNONCE, pour la discussion.
//
// ⚠ DES FAITS, PAS DES PHRASES : la ligne nomme la pièce, et seule la couche qui
// connaît les tables sait composer un nom.
enum class EquipSwitchEvent : std::uint8_t {
  // La pièce entre dans le jeu de rechange.
  kPlaced,
  // Le serveur refuse de l'y ranger.
  kPlaceRefused,
  // Elle en sort.
  kRemoved,
  // Il refuse de l'en sortir — et elle en sort quand même chez le client.
  kRemoveRefused,
  // Il refuse l'échange complet.
  kSwapRefused,
};

// POURQUOI LE SERVEUR REFUSE, quand il le dit.
enum class EquipSwitchRefusal : std::uint8_t {
  kNone,
  // Rien à échanger.
  kNothingToReplace,
  // La pièce ne peut pas entrer dans le jeu.
  kCannotRegister,
  // L'échange ne peut pas se faire.
  kCannotSwap,
};

struct EquipSwitchNews {
  EquipSwitchEvent event = EquipSwitchEvent::kPlaced;
  // La case de la pièce dans le sac, vue du client ; sans objet pour l'échange.
  std::uint16_t client_slot = 0;
  // Les emplacements qu'elle occupe dans le jeu de rechange.
  std::uint32_t location = 0;
  EquipSwitchRefusal refusal = EquipSwitchRefusal::kNone;
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

  // Une pièce entre dans le jeu, ou y change d'emplacements. ⚠ CELLE QU'ELLE
  // DÉLOGE a déjà été retirée : le serveur envoie ce retrait avant.
  void PlaceEquipSwitch(EquipSwitchPiece piece) {
    std::erase_if(equip_switch_, [&piece](const EquipSwitchPiece& kept) {
      return kept.client_slot == piece.client_slot;
    });
    equip_switch_.push_back(piece);
  }

  void RemoveEquipSwitch(std::uint16_t client_slot) {
    std::erase_if(equip_switch_, [client_slot](const EquipSwitchPiece& kept) {
      return kept.client_slot == client_slot;
    });
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
