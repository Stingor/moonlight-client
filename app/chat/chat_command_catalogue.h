#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <string>
#include <vector>

#include "ro/ui/chat_command_hints.h"
#include "ro/ui/chat_macros.h"

#include "chat/server_atcommands.h"

namespace moonclient {

// LES COMMANDES QUE LE CHAT PROPOSE, et le menu des macros avec.
//
// 🔴 UNE SEULE LISTE POUR LES DEUX : la liste au-dessus de la barre de saisie et
// le menu du clic droit des macros puisent ici. Deux listes finiraient par
// proposer, l'une ou l'autre, une commande que le client ne lit pas ou que le
// serveur refuse au joueur.
//
//   · « / » : `SlashCommands` puis les émoticônes de `EmotionCommandRows` —
//     ce que moonclient prend vraiment en charge ;
//   · « @ » : `ServerAtCommands`, la table tirée du serveur, réduite à ce que
//     le GROUPE DU JOUEUR atteint. Le client n'en connaît que le niveau, que le
//     serveur lui dit dans ses réglages ; un serveur muet laisse zéro, donc les
//     seules commandes du groupe des joueurs.
//
// ⚠ RECOMPOSÉE AU CHANGEMENT DE LANGUE OU DE GROUPE, et seulement alors : le
// pointeur que rend `Tr` meurt avec la langue, et les textes sont donc COPIÉS.
class ChatCommandCatalogue {
 public:
  void Refresh(std::uint8_t group_level);

  [[nodiscard]] std::span<const ro::ui::ChatCommandHint> hints() const noexcept { return hints_; }
  [[nodiscard]] std::span<const ro::ui::ChatMacroCommandGroup> macro_groups() const noexcept {
    return macro_groups_;
  }

 private:
  // ⚠ UNE FILE ET NON UN VECTEUR : les vues posées sur un texte doivent
  // survivre à l'ajout du suivant.
  std::deque<std::string> texts_;
  std::vector<ro::ui::ChatCommandHint> hints_;
  std::vector<ro::ui::ChatMacroCommand> macro_items_;
  std::vector<ro::ui::ChatMacroCommandGroup> macro_groups_;
  // ⚠ COMMENCENT HORS BORNES pour que la première demande compose vraiment.
  std::size_t language_serial_ = static_cast<std::size_t>(-1);
  int group_level_ = -1;
};

// LE GROUPE DE CE NIVEAU ATTEINT-IL CETTE COMMANDE DU SERVEUR ?
[[nodiscard]] constexpr bool ServerCommandOffered(const ServerAtCommand& command,
                                                  std::uint8_t group_level) noexcept {
  return command.min_group_level != kNoGroupLevel && command.min_group_level <= group_level;
}

// Combien d'entrées un sous-menu « @ » des macros porte au plus avant de se
// couper à la lettre suivante.
inline constexpr std::size_t kMacroServerGroupSize = 20;

// Ce que la colonne d'explication d'un menu de macro montre au plus, en octets.
inline constexpr std::size_t kMacroHelpBytes = 64;

}  // namespace moonclient
