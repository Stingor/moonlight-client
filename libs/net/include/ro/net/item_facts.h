#pragma once

#include <cstddef>
#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/item_facts.h"

namespace ro::net {

// CE QUE LE SERVEUR SEUL SAIT D'UN OBJET, sur le fil.
//
//   CZ 0x0F0B [op][len][id:4][is_skill:1][scope:1]     — les sources
//   ZC 0x0F0C [op][len][id:4][is_skill:1][scope:1]
//             puis, pour un objet : [count:1][truncated:1][treasure_excluded:1]
//             et count × [mob:4][rate:4][boss:1][src:1][namelen:1][name]
//   CZ 0x0F11 [op][len][id:4]                          — le script
//   ZC 0x0F12 [op][len][id:4][status:1] puis, si status vaut zéro, trois
//             chaînes [len:2][octets] (script, équipé, déséquipé),
//             [combos:1] et par combo [membres:1] × ([id:4][namelen:1][nom])
//             suivi de son script [len:2][octets].
//
//   ZC 0x0F05 — les réglages de session, dont la liste de ramassage
//             automatique, lue ici (voir `ro/net/player_settings.h`)
//   CZ 0x0369 [op][character:4]                        — le forgeron
//   ZC 0x0AF7 [op][flag:2][character:4][name:24]
//
// ⚠ 0x0F0C SERT AUSSI AUX COMPÉTENCES, et c'est l'octet `is_skill` qui dit la
// forme de la suite. Celles-ci sont lues par `ro/net/skill_facts.h` ; ce
// décodeur-ci ne lit que la branche des objets.
//
// ⚠ RÉSERVÉS À UN CLIENT RECONNU par la poignée de main : avant elle, le serveur
// ignore la demande, et c'est au demandeur de la rejouer.

// Le bloc fixe de chacune des deux réponses.
inline constexpr std::size_t kItemSourcesFixedSize = 10;
inline constexpr std::size_t kItemScriptsFixedSize = 9;

[[nodiscard]] FrameOutcome ApplyItemFactsFrame(const Frame& frame, sim::ItemFacts& facts);

[[nodiscard]] core::Result<void> RequestItemSources(Connection& link, std::uint32_t item_id,
                                                    sim::ItemSourceScope scope);
[[nodiscard]] core::Result<void> RequestItemScripts(Connection& link, std::uint32_t item_id);
// Ajoute un item à la liste de ramassage automatique, ou l'en retire.
void RequestAutoloot(Connection& link, std::uint32_t item_id, bool listed);

// Le nom de qui a forgé ou signé un objet. ⚠ PAS RÉSERVÉ AUX PAQUETS MAISON :
// c'est un paquet du jeu, et le serveur y répond dès l'entrée sur la carte.
[[nodiscard]] core::Result<void> RequestCreatorName(Connection& link, std::uint32_t character_id);

}  // namespace ro::net
