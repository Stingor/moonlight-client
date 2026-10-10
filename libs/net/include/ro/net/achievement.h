#pragma once

#include <cstdint>
#include <optional>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/achievement_book.h"

namespace ro::net {

// LES SUCCÈS : la progression que le serveur pousse, et la réclamation d'une
// récompense.
//
// TROIS PAQUETS REÇUS :
//
//   · `ZC 0x0A23`, la liste complète — à l'entrée en jeu, et après chaque
//     récompense qui donne un titre. ⚠ JAMAIS QUAND ELLE EST VIDE : un
//     personnage sans aucun succès commencé ne la reçoit pas du tout ;
//   · `ZC 0x0A24`, un seul succès — à chaque progrès, et après une récompense
//     sans titre ;
//   · `ZC 0x0A26`, l'accusé d'une réclamation. 🔴 SON OCTET VAUT UN POUR UNE
//     RÉUSSITE ET ZÉRO POUR UN REFUS, quoi que dise le nom du champ : c'est ce
//     qu'écrit le serveur. Une réussite se lit d'ailleurs dans la mise à jour qui
//     la suit ; l'accusé ne fait que clore la réclamation en vol.
//
// ⚠ 0x0A23 ET 0x0A26 SONT AUSSI LUS PAR `ApplyTitleFrame`, qui en tire les
// titres : c'est à l'appelant de les interroger tous les deux.

struct AchievementFrameResult {
  FrameOutcome outcome = FrameOutcome::kUnhandled;
  // Le succès que la trame vient d'ACHEVER — le moment de la bannière.
  std::optional<std::uint32_t> completed;
  // Une réclamation refusée par le serveur.
  std::optional<std::uint32_t> refused_claim;
};

[[nodiscard]] AchievementFrameResult ApplyAchievementFrame(const Frame& frame,
                                                           sim::AchievementBook& book);

// « JE RÉCLAME LA RÉCOMPENSE DE CE SUCCÈS », si les gardes du serveur la
// permettent — `rewards` dit si le catalogue lui en connaît une. Hors d'elles,
// rien ne part : le serveur ne répondrait qu'un refus.
//
// ⚠ ÉMET, ET NOTE LA RÉCLAMATION EN VOL : la récompense ne devient « réclamée »
// qu'à la réponse.
[[nodiscard]] core::Result<void> ClaimAchievementReward(Connection& link,
                                                        sim::AchievementBook& book,
                                                        std::uint32_t achievement_id, bool rewards);

}  // namespace ro::net
