#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/chat_line.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/party_roster.h"
#include "ro/vfs/item_words.h"

#include "world/hit_popup_rules.h"

namespace moonclient {

// ── LES LIGNES DE COMBAT DE LA CONVERSATION ──────────────────────────────────
//
// Le client les écrit en REJOUANT ses messages de dégâts différés : chaque
// message qu'un acteur reçoit — une frappe, à l'instant où elle touche — porte
// la cible, l'attaquant et la part, et s'il porte des dégâts il peut écrire une
// ligne. Des fonctions pures, sans monde : qui écrit quoi se décide ici, et
// `BattleLog` ne fait que le programmer et le poser.

// Un message de dégâts, tel que le client le rejoue.
struct BattleMessage {
  std::uint32_t target_id = 0;
  std::uint32_t attacker_id = 0;
  // La part que ce message porte.
  std::int32_t damage = 0;
  // Vrai quand l'acteur qui REÇOIT le message a le métier d'un joueur : c'est
  // lui qui choisit entre les deux questions — « est-ce moi ou mon groupe qui
  // encaisse » ou « qui frappe ».
  bool receiver_is_player = false;
  // Qui le reçoit : la cible, sauf pour la reprise d'attaque d'un enchaînement.
  std::uint32_t receiver_id = 0;
  // L'instant où il se rejoue.
  std::uint32_t due_ms = 0;
};

// Les messages de dégâts qu'un coup fait rejouer au client : un par frappe
// montrée, à la cible, à l'instant de la frappe ; et, pour un enchaînement qui
// rejoue son coup, un de plus par frappe au LANCEUR — voir
// `ro::render::SwingReplaysOf`. Aucun pour un coup sans dégâts.
//
// `first_hit_ms` est l'instant où la première frappe touche.
[[nodiscard]] std::vector<BattleMessage> BattleMessagesOf(const ro::sim::Strike& strike,
                                                          const StrikeSides& sides,
                                                          std::uint32_t first_hit_ms,
                                                          bool target_is_player,
                                                          bool attacker_is_player);

// Un nom tel que la ligne l'écrit : coupé à son premier `#`, et « Someone »
// quand il n'en reste rien.
[[nodiscard]] std::string BattleName(const ro::vfs::ItemWords& words, std::string_view name);

// Une ligne de combat, prête à poser.
struct BattleLogLine {
  std::string text;
  ro::sim::ChatKind kind = ro::sim::ChatKind::kBattle;
  // En 0xRRGGBB, comme toute couleur de ligne.
  std::uint32_t color = 0;
};

// La ligne qu'un message écrit, ou rien. Les noms sont déjà passés par
// `BattleName`.
[[nodiscard]] std::optional<BattleLogLine> BattleLogLineFor(const ro::vfs::ItemWords& words,
                                                            const BattleMessage& message,
                                                            std::uint32_t self_id,
                                                            const ro::sim::PartyRoster& party,
                                                            std::string_view target_name,
                                                            std::string_view attacker_name);

}  // namespace moonclient
