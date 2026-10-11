#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/quest_log.h"

namespace ro::net {

// LES QUÊTES : ce que le serveur pousse, et l'activation d'une quête.
//
// SEPT PAQUETS REÇUS ET UN ÉMIS :
//
//   · `ZC 0x09F8`, la liste entière — à l'entrée sur la carte, jamais quand le
//     personnage n'a aucune quête ;
//   · `ZC 0x09F9`, une quête neuve, suivie de `ZC 0x08FE` qui pose les totaux
//     de ses chasses ;
//   · `ZC 0x09FA`, la progression de TOUTES les chasses d'une quête ;
//   · `ZC 0x0F3B`, la raison du retrait qui suit — achevée, effacée, remplacée,
//     retirée —, que le serveur Moonlight n'envoie qu'à qui a annoncé
//     `kUiCapQuestEnd` ;
//   · `ZC 0x02B4`, le retrait — le même pour l'achèvement et l'abandon ;
//   · `ZC 0x02B7`, l'activation ou la désactivation d'une quête ;
//   · `CZ 0x02B6`, la demande d'activation, que le serveur confirme par 0x02B7.

// Un compte de chasse qui a augmenté : le moment de le montrer.
struct QuestProgress {
  std::uint32_t quest_id = 0;
  // Le rang de la chasse dans sa quête, de zéro.
  std::size_t hunt_rank = 0;
  std::int16_t killed = 0;
  std::int16_t total = 0;
};

struct QuestFrameResult {
  FrameOutcome outcome = FrameOutcome::kUnhandled;
  // La liste entière a remplacé le journal (0x09F8).
  bool replaced = false;
  // La quête que 0x09F9 vient d'ajouter.
  std::optional<std::uint32_t> added;
  // Sur 0x09F9, la quête était DÉJÀ au journal : le serveur renvoie l'ajout
  // quand il change son état, et ce n'est pas une quête reçue.
  bool added_was_listed = false;
  // La quête que 0x02B4 vient de retirer du journal.
  std::optional<std::uint32_t> removed;
  // LA QUÊTE QUE 0x02B4 NOMME, au journal ou non : le client d'origine annonce
  // le retrait dans les deux cas, sous un titre vide quand il ne la tenait pas.
  std::optional<std::uint32_t> deleted;
  // Sur 0x02B4, la raison que 0x0F3B a dite juste avant ; rien si elle ne l'a
  // pas été. Prise même quand la quête n'était plus au journal.
  std::optional<sim::QuestEnding> ending;
  // La quête dont 0x02B7 vient de changer l'état.
  std::optional<std::uint32_t> toggled;
  // 0x09FA : seulement les comptes qui ont augmenté.
  std::vector<QuestProgress> progressed;
  // 0x09FA : CHAQUE CHASSE QUE LA TRAME REDIT et que le journal connaît, qu'elle
  // ait bougé ou non. Le serveur les redit toutes à chaque coup, et le client
  // d'origine écrit une ligne pour chacune.
  std::vector<QuestProgress> reported;
};

[[nodiscard]] QuestFrameResult ApplyQuestFrame(const Frame& frame, sim::QuestLog& log);

// « ACTIVE (OU DÉSACTIVE) CETTE QUÊTE ». N'attend pas la réponse : c'est
// 0x02B7 qui change l'état.
[[nodiscard]] core::Result<void> RequestQuestActive(Connection& link, std::uint32_t quest_id,
                                                    bool active);

}  // namespace ro::net
