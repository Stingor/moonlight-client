#pragma once

#include <cstddef>
#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// CE QUE LE SERVEUR SEUL SAIT D'UNE COMPÉTENCE — deux demandes, deux réponses,
// réservées aux clients ayant donné leur poignée de main (ADR 0040).
//
// 🔴 CES QUATRE PAQUETS N'ONT PAS D'ÉQUIVALENT DANS LE JEU D'ORIGINE. Le temps
// d'incantation, le temps d'attente et les dégâts estimés se calculent dans le
// serveur, sur l'équipement et les états du personnage : aucune table du client
// ne peut les porter, et les deviner reviendrait à recopier des formules de
// combat qu'on ne saurait pas tenir à jour.
//
// ⚠ ELLES SE DEMANDENT. Rien n'arrive tant qu'on n'a pas ouvert l'onglet, et la
// fenêtre doit donc savoir dire qu'elle attend.

// « Donne-moi la table de cast de cette compétence. » — `CZ 0x0F0B`.
//
// ⚠ `scope` EST RENVOYÉ EN ÉCHO par le serveur, qui ne s'en sert pas pour une
// compétence : il range les sources d'un OBJET par portée, et l'écho permet au
// client d'en garder plusieurs. Une compétence n'en a qu'une.
[[nodiscard]] core::Result<void> RequestSkillCastFacts(Connection& link, std::uint32_t skill_id);

// « Estime les dégâts de cette compétence contre cette cible. » — `CZ 0x0F0D`.
//
// ⚠ UN RANG NUL VEUT DIRE « CELUI QUE J'AI APPRIS » : c'est le serveur qui le
// relit, et lui écrire un rang qu'on n'a pas donnerait une estimation de ce
// qu'on ne peut pas lancer.
//
// ⚠ ET LA CIBLE A TROIS SENS : zéro est le mannequin neutre sans défense,
// `kDamageAgainstSelf` est soi-même, tout le reste est un vrai monstre.
[[nodiscard]] core::Result<void> RequestSkillDamage(Connection& link, std::uint32_t skill_id,
                                                    std::uint16_t level,
                                                    std::uint32_t target_mob_id);

// La cible « soi-même » : le miroir du duel, où l'on encaisse son propre sort.
inline constexpr std::uint32_t kDamageAgainstSelf = 0xFFFFFFFFU;

// Applique une réponse : la table de cast (`ZC 0x0F0C`) ou l'estimation de
// dégâts (`ZC 0x0F0E`).
[[nodiscard]] FrameOutcome ApplySkillFactsFrame(const Frame& frame, sim::World& world);

// ── CE QUE LES STRUCTURES ENGENDRÉES NE PORTENT PAS ─────────────────────────
//
// Le générateur s'arrête au bloc fixe : les deux réponses ajoutent une queue
// variable que seul le serveur décrit. Les décalages se nomment donc ici.

// `[opcode:2][taille:2][id:4][compétence ?:1][portée:1]`, puis la queue.
inline constexpr std::size_t kSkillFactsFixedSize = 10;
// Pour une COMPÉTENCE, la queue commence par le nombre de rangs, sur un octet.
inline constexpr std::size_t kSkillCastCountSize = 1;
// Puis un rang par entrée : `[variable:4][fixe:4][attente:4][délai:4]`.
inline constexpr std::size_t kSkillCastEntrySize = 16;
// L'estimation de dégâts ajoute `[longueur du nom:1][nom]` après son bloc fixe.
inline constexpr std::size_t kSkillDamageFixedSize = 42;

}  // namespace ro::net
