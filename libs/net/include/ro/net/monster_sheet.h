#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/monster_sheet.h"

namespace ro::net {

// LA FICHE D'UN MONSTRE : une demande, une réponse, réservées aux clients
// ayant donné leur poignée de main (ADR 0040) — et le relevé du skill Sense.
//
// Voir `ro/sim/monster_sheet.h` pour ce que chacune porte, et pourquoi le
// relevé de Sense ne suffit pas.

// « Donne-moi la fiche de ce monstre. » — `CZ 0x0F1F`.
//
// 🔴 `by_view` DIT CE QUE L'IDENTIFIANT EST. Faux : un identifiant de la base
// des monstres. Vrai : une classe de SPRITE — celle qu'un acteur porte à
// l'écran et que Sense renvoie. Le client n'a pas la base et ne peut pas
// remonter de l'une à l'autre ; le serveur essaie l'identifiant tel quel, puis
// cherche le monstre dont c'est l'apparence.
[[nodiscard]] core::Result<void> RequestMonsterSheet(Connection& link, std::uint32_t monster_id,
                                                     bool by_view);

// Applique une fiche (`ZC 0x0F20`) ou un relevé de Sense (`ZC 0x018C`).
[[nodiscard]] FrameOutcome ApplyMonsterSheetFrame(const Frame& frame, sim::MonsterSheetBook& book);

// CE QU'UNE RÉPONSE DIT, déchiffrée sans rien toucher : ce que les essais lisent.
struct MonsterSheetReply {
  // L'identifiant DEMANDÉ, renvoyé en écho par le serveur.
  std::uint32_t asked_id = 0;
  // Vide quand le serveur ne connaît pas ce monstre.
  std::optional<sim::MonsterSheet> sheet;
};

// Vide quand l'en-tête lui-même est indéchiffrable.
//
// 🔴 LES LISTES SONT CRUES, MAIS PAS SUR PAROLE : une trame tronquée garde ce
// qui y tient entier. Le bloc fixe, lui, est exigé en entier — une fiche sans
// ses statistiques n'en est pas une, et se rend comme un monstre inconnu.
[[nodiscard]] std::optional<MonsterSheetReply> DecodeMonsterSheet(
    std::span<const std::uint8_t> frame);

// ── CE QUE LA STRUCTURE ENGENDRÉE NE PORTE PAS ──────────────────────────────
//
// Le générateur s'arrête à `[opcode:2][taille:2][id:4][statut:1]` ; tout le
// reste est une queue que seul le serveur décrit.
inline constexpr std::size_t kMonsterSheetHeaderSize = 9;
// Le statut d'une fiche trouvée ; tout autre dit « monstre inconnu ».
inline constexpr std::uint8_t kMonsterSheetFound = 0;
// Le bloc fixe : sprite 4, niveau 2, PV/SP 8, EXP 12, ATK/MATK 8, DEF/MDEF 8,
// statistiques 12, portée 2, six octets, mode 4, vitesse 2, résistances 20.
inline constexpr std::size_t kMonsterSheetFixedSize = 88;
// Un butin : `[objet:4][taux:4][genre:1][longueur du nom:1]`, puis le nom.
inline constexpr std::size_t kMonsterDropHeadSize = 10;
// Une apparition : `[nombre:2][longueur de la carte:1]`, puis la carte.
inline constexpr std::size_t kMonsterSpawnHeadSize = 3;
// Une compétence : `[identifiant:2][rang:2][longueur du nom:1]`, puis le nom.
inline constexpr std::size_t kMonsterSkillHeadSize = 5;
// Après le nom interne : `[invoqué:1][homonymes:1][le plus ancien:4]`.
inline constexpr std::size_t kMonsterIdentityTailSize = 6;
// L'EXP estimée : quatre compteurs de quatre octets et un octet d'indicateurs.
inline constexpr std::size_t kMonsterExpGainSize = 17;

}  // namespace ro::net
