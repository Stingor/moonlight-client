#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ro/sim/survivors_state.h"
#include "ro/sim/survivors_week.h"

namespace ro::sim {

// LES RECORDS LOCAUX : les dix meilleures parties d'un tableau (une classe,
// une arène, partie libre ou défi). Le tableau garde de quoi relancer la
// partie (graine, premier skill), pas son journal.
inline constexpr std::size_t kRecordsPerBoard = 10;

struct SurvivorsRecord {
  std::uint64_t score = 0;
  std::uint32_t survived_ms = 0;
  std::uint16_t level = 1;
  SurvivorsOutcome outcome = SurvivorsOutcome::kRunning;
  std::uint32_t kills = 0;
  std::uint8_t cursed_water = 0;
  std::uint16_t ruleset = 0;  // la version des règles qui a joué la partie
  // La CLÉ du premier skill dans le catalogue, jamais son rang : une clé que ce
  // client ne connaît pas se garde telle quelle.
  std::string first_skill_key;
  std::uint64_t seed = 0;
  std::int64_t played_at_utc_s = 0;  // secondes depuis le 1970-01-01 UTC
  IsoWeek week;                      // le défi joué ; {0, 0} en partie libre
  [[nodiscard]] bool operator==(const SurvivorsRecord&) const = default;
};

// L'ORDRE D'UN TABLEAU : score décroissant, puis temps survécu décroissant,
// puis date croissante (la plus ancienne garde sa place). Faux entre deux
// parties que ces trois champs ne départagent pas.
[[nodiscard]] bool RanksAbove(const SurvivorsRecord& first, const SurvivorsRecord& second) noexcept;

// INSCRIT UNE PARTIE dans un tableau rangé, par insertion stable : à égalité
// parfaite, la partie déjà inscrite reste devant. Le tableau ne garde que ses
// kRecordsPerBoard premières lignes. Rend le rang (0 en tête), ou rien quand
// la partie n'y entre pas.
std::optional<std::size_t> PlaceRecord(std::vector<SurvivorsRecord>& board,
                                       const SurvivorsRecord& record);

}  // namespace ro::sim
