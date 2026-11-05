#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/error.h"
#include "ro/sim/survivors_journal.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"

namespace ro::sim {

// LE JOURNAL D'UNE PARTIE COOP : son départ et les entrées de chaque joueur, tic
// par tic. Un tic avance toujours, même pendant un vote, qui ne compte aucun pas
// de partie : c'est lui qui date les entrées, et que le réseau échange.
inline constexpr std::uint16_t kSurvivorsTeamJournalVersion = 3;

enum class TeamJournalKind : std::uint8_t { kHeading = 0, kBallot = 1, kLeft = 2 };
struct TeamJournalEntry {
  std::uint32_t tick = 0;
  std::uint8_t player = 0;
  TeamJournalKind kind = TeamJournalKind::kHeading;
  std::uint8_t value = 0;  // le cap, ou le bulletin par PackAnswer
  [[nodiscard]] bool operator==(const TeamJournalEntry&) const = default;
};
struct SurvivorsTeamJournal {
  std::uint16_t journal_version = kSurvivorsTeamJournalVersion;
  std::uint16_t ruleset_version = 0;
  SurvivorsStart start;
  std::vector<TeamJournalEntry> entries;  // par tic croissant, puis par joueur
  std::uint32_t final_tick = 0;
  [[nodiscard]] bool operator==(const SurvivorsTeamJournal&) const = default;
};

// Petit-boutiste :
//   "MSVT" | journal_version u16 | ruleset_version u16 | seed u64 | arena u16
//   | cursed_water u8 | nombre de joueurs u8 | pour chacun : class u16,
//   first_skill u16, 12 bonus en LEB128 zigzag, rerolls u8, skips u8, banishes u8,
//   lives u8, leaves u8, nombre d'Id verrouillés u8, puis chacun u16
//   | nombre d'entrées LEB128 | pour chacune : écart de tic LEB128, joueur u8,
//   sorte u8, valeur u8 | écart du tic final LEB128
[[nodiscard]] std::vector<std::uint8_t> EncodeSurvivorsTeamJournal(
    const SurvivorsTeamJournal& journal);
// kUnexpectedEnd, kBadSignature, kUnsupportedVersion ; kCorruptData pour un
// LEB128 mal écrit, un nombre de joueurs hors de [2, kMostHeroes], un joueur
// hors de la partie, une sorte inconnue, un cap invalide, un bulletin illisible,
// des tics décroissants, des bonus ou des Id que le départ refuserait, ou des
// octets en trop.
[[nodiscard]] core::Result<SurvivorsTeamJournal> DecodeSurvivorsTeamJournal(
    std::span<const std::uint8_t> bytes);

// L'EMPREINTE D'UN ÉTAT : deux clients qui jouent la même partie la rendent
// égale à chaque tic. Elle sert à constater qu'une partie a divergé sur une AUTRE
// machine, où l'état lui-même n'est pas lisible : elle couvre le tic, le pas, les
// flux d'aléa, les héros, les monstres, les gemmes, les objets et la réserve.
[[nodiscard]] std::uint64_t SurvivorsDigest(const SurvivorsField& field);

// UNE PARTIE COOP JOUÉE TIC PAR TIC : le journal tenu au fil des entrées, et
// rien de l'horloge, que le transport possède.
class SurvivorsTeamPlay {
 public:
  explicit SurvivorsTeamPlay(SurvivorsRun run);
  // Un tic, un jeu d'entrées par héros, dans l'ordre de leur index. Un cap ne
  // s'inscrit que s'il change ; un bulletin, que si la partie l'accepte.
  void Tick(std::span<const SurvivorsTeamInput> inputs);
  // Ce joueur a quitté la partie, au tic courant : le journal le garde.
  void MarkLeft(std::size_t player);
  [[nodiscard]] SurvivorsNews TakeNews() { return run_.TakeNews(); }
  [[nodiscard]] const SurvivorsRun& run() const noexcept { return run_; }
  [[nodiscard]] SurvivorsRun& mutable_run() noexcept { return run_; }
  [[nodiscard]] const SurvivorsTeamJournal& journal() const noexcept { return journal_; }

 private:
  SurvivorsRun run_;
  SurvivorsTeamJournal journal_;
  std::array<std::uint8_t, kMostHeroes> last_headings_;
};

// Rejoue un journal entier, sur l'arène générée de son départ ; rend l'empreinte
// du dernier état. kUnsupportedVersion pour une autre version de règles ;
// kCorruptData pour une entrée après le tic final ; les refus de Start tels quels.
[[nodiscard]] core::Result<std::uint64_t> ReplaySurvivorsTeam(const SurvivorsRules& rules,
                                                              const SurvivorsTeamJournal& journal);

}  // namespace ro::sim
