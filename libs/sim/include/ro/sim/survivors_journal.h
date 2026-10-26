#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/core/error.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/walk_map.h"

namespace ro::sim {

// LE JOURNAL D'UNE PARTIE : son départ et ses seules entrées, pas par pas. Avec
// les mêmes règles et la même arène, il redonne la partie entière : c'est ce
// que le serveur rejoue pour vérifier un score.
inline constexpr std::uint16_t kSurvivorsJournalVersion = 2;

enum class JournalKind : std::uint8_t { kHeading = 0, kAnswer = 1, kAbandon = 2 };
struct JournalEntry {
  std::uint32_t step = 0;
  JournalKind kind = JournalKind::kHeading;
  std::uint8_t value = 0;  // le cap, ou la réponse par PackAnswer ; 0 pour un abandon
  [[nodiscard]] bool operator==(const JournalEntry&) const = default;
};
struct SurvivorsJournal {
  std::uint16_t journal_version = kSurvivorsJournalVersion;
  std::uint16_t ruleset_version = 0;
  SurvivorsStart start;
  std::vector<JournalEntry> entries;  // par pas croissant ; plusieurs par pas
  std::uint32_t final_step = 0;
  [[nodiscard]] bool operator==(const SurvivorsJournal&) const = default;
};

// Sorte sur les 4 bits hauts, index sur les 4 bas. Ne s'emploie que pour une
// réponse ACCEPTÉE, dont l'index est sous kMostOffers et tient sur 4 bits :
// SurvivorsPlay n'inscrit au journal que ce que la partie a accepté.
[[nodiscard]] std::uint8_t PackAnswer(SurvivorsAnswer answer) noexcept;
// Rien pour une sorte inconnue.
[[nodiscard]] std::optional<SurvivorsAnswer> UnpackAnswer(std::uint8_t value) noexcept;

// Petit-boutiste :
//   "MSVJ" | journal_version u16 | ruleset_version u16 | seed u64 | class u16
//   | arena u16 | first_skill u16 | 12 bonus en LEB128 zigzag, dans l'ordre de
//   SurvivorsStat | rerolls u8 | skips u8 | banishes u8 | lives u8
//   | cursed_water u8 | nombre d'Id verrouillés u8, puis chacun u16, par Id
//   strictement croissant | nombre d'entrées LEB128 | pour chacune : écart de
//   pas LEB128, sorte u8, valeur u8 | écart du pas final LEB128
// Un journal d'une autre version se refuse : la version 1 appartient au
// ruleset 2, qui ne se rejoue plus.
[[nodiscard]] std::vector<std::uint8_t> EncodeSurvivorsJournal(const SurvivorsJournal& journal);
// kUnexpectedEnd trop court, ou plus court que le nombre d'entrées annoncé (la
// réserve suit les octets restants, jamais ce nombre) ; kBadSignature ;
// kUnsupportedVersion ; kCorruptData pour une sorte inconnue, un LEB128 de plus
// de 5 octets, écrit plus long que nécessaire (dernier octet nul après un
// octet de suite) ou dont la valeur dépasse 32 bits, un bonus de loadout
// au-delà de ±kMostStatBonus, une Cursed Water au-delà de kMostCursedWater,
// plus de kMostLockedIds Id verrouillés ou des Id non strictement
// croissants, un pas qui dépasse 32 bits, un cap hors de [0,
// kHeadingSteps) autre que kNoHeading, ou des octets en trop.
[[nodiscard]] core::Result<SurvivorsJournal> DecodeSurvivorsJournal(
    std::span<const std::uint8_t> bytes);
// Rejoue le journal jusqu'à son pas final et rend le bilan. À chaque pas, les
// entrées de ce pas s'appliquent d'abord, Y COMPRIS celles du pas final (un
// abandon s'inscrit au pas où la partie s'arrête) ; la partie avance ensuite
// d'un pas, sauf si elle est finie ou au pas final.
// kUnsupportedVersion pour une autre version de règles ; kCorruptData pour une
// entrée d'un pas déjà passé ou d'une partie déjà finie, un cap invalide, une
// réponse refusée, un choix resté sans réponse, des entrées après le pas
// final, ou une partie finie à un autre pas que le final ; les refus de Start
// tels quels.
[[nodiscard]] core::Result<SurvivorsTally> ReplaySurvivors(const SurvivorsRules& rules,
                                                           WalkMap arena,
                                                           const SurvivorsJournal& journal);
// La partie rejouée jusqu'au pas `stop_step` (au plus le pas final), ou jusqu'à
// sa fin si elle survient avant : l'état d'un instant, pour le montrer. Mêmes
// refus que ReplaySurvivors, sauf ceux qui jugent le journal entier (entrées
// après le pas final, partie finie à un autre pas).
[[nodiscard]] core::Result<SurvivorsRun> ReplaySurvivorsTo(const SurvivorsRules& rules,
                                                           WalkMap arena,
                                                           const SurvivorsJournal& journal,
                                                           std::uint32_t stop_step);
// Les mêmes, pour une ARÈNE GÉNÉRÉE : son sol se redérive des règles et de la
// graine du journal, sans carte ni installation. Les refus de
// SurvivorsRun::Start(rules, start) tels quels, dont une arène qui lit une
// carte.
[[nodiscard]] core::Result<SurvivorsTally> ReplaySurvivors(const SurvivorsRules& rules,
                                                           const SurvivorsJournal& journal);
[[nodiscard]] core::Result<SurvivorsRun> ReplaySurvivorsTo(const SurvivorsRules& rules,
                                                           const SurvivorsJournal& journal,
                                                           std::uint32_t stop_step);

}  // namespace ro::sim
