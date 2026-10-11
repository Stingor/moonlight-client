#pragma once

#include <cstdint>
#include <span>

#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::sim {
class SkillTree;
}  // namespace ro::sim

namespace ro::net {

// LES MESSAGES PAR NUMÉRO : le serveur ne dit pas sa phrase, il en donne le
// numéro dans la table de messages du client, et c'est le client qui l'écrit.
//
//   ZC 0x0291 [op][msg:2]                 en blanc, sauf les refus rouges
//   ZC 0x09CD [op][msg:2][couleur:4]      dans la couleur du paquet (BGR)
//   ZC 0x07E2 [op][msg:2][valeur:4]       en jaune, la valeur dans le gabarit
//   ZC 0x07E6 [op][skill:2][msg:4]        en lavande, « [compétence] message »
//
// Chaque message devient une ligne `kServer` de la conversation. Sa case est la
// générale, sauf l'issue d'une compétence (« échecs de compétence ») et, par
// 0x0291 seulement, les plafonds du tribut d'expérience à la guilde (« EXP de
// contribution »).
//
// ⚠ UN NUMÉRO QUE LA TABLE NE PORTE PAS N'ÉCRIT RIEN : la trame est `kIgnored`
// et le journal le dit. Inventer un texte ferait lire au joueur une phrase que
// le serveur n'a jamais voulue.
//
// `skills` sert au nom affiché d'une compétence ; nul, le nom interne que le
// serveur a donné prend la place, puis le numéro.
[[nodiscard]] FrameOutcome ApplyServerMessageFrame(const Frame& frame, sim::World& world,
                                                   const sim::SkillTree* skills,
                                                   std::uint32_t now_ms);

// UNE ENTRÉE DE LA TABLE : le numéro, et le texte français qui sert de clé de
// traduction.
struct ServerMessageEntry {
  std::uint16_t message_id = 0;
  const char* french = nullptr;
};

// La table entière, triée par numéro croissant.
[[nodiscard]] std::span<const ServerMessageEntry> ServerMessageTable();

}  // namespace ro::net
