#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "ro/sim/companion_rules.h"

namespace ro::sim {

// ── CE QUI SURVIT D'UNE IA DE COMPAGNON ─────────────────────────────────────
//
// Les délais des règles, les traces « skill K lancé à », les suspensions en
// cours et l'attente survivent à un portail, à un changement de profil et à
// une reconnexion : sans cela le compagnon relancerait ses buffs à chaque
// portail. Le reste de l'IA — combat, cibles, file réservée — repart de zéro.
//
// 🔴 TOUT EST RELATIF À UN INSTANT : un âge (« il y a ») ou un reste
// (« encore »), jamais une date de l'horloge du client, qui ne survit pas à une
// reconnexion. Et rien ne nomme un acteur, dont l'identifiant non plus : un état
// se rattache à sa règle, par son contenu et son rang, ou à son skill.

// UNE MÉMOIRE PLUS VIEILLE QUE CET HORIZON NE SERT PLUS : il couvre le plus long
// délai propre d'une règle et la plus longue durée qu'une condition mesure.
inline constexpr std::uint32_t kCompanionMemoryHorizonMs = 3600000;
static_assert(kCompanionMemoryHorizonMs >= kRuleDelayMostMs);
static_assert(kCompanionMemoryHorizonMs >=
              static_cast<std::uint32_t>(kRuleSecondsMost) * std::uint32_t{1000});

// L'ÉTAT D'UNE RÈGLE : son rang dans la liste, son contenu, depuis quand elle a
// agi, et pour combien de temps un refus la suspend encore.
struct CompanionRuleMemory {
  std::size_t index = 0;
  CompanionRule rule;
  std::optional<std::uint32_t> acted_ago_ms;
  std::optional<std::uint32_t> suspended_for_ms;

  [[nodiscard]] friend bool operator==(const CompanionRuleMemory&,
                                       const CompanionRuleMemory&) = default;
};

// L'ÉTAT D'UN SKILL DU COMPAGNON : sa dernière trace, et sa suspension.
struct CompanionSkillMemory {
  std::uint16_t skill_id = 0;
  std::optional<std::uint32_t> cast_ago_ms;
  std::optional<std::uint32_t> suspended_for_ms;

  [[nodiscard]] friend bool operator==(const CompanionSkillMemory&,
                                       const CompanionSkillMemory&) = default;
};

struct CompanionMemory {
  std::vector<CompanionRuleMemory> rules;
  std::vector<CompanionSkillMemory> skills;
  // L'attente, quand le réglage « garder l'attente » la garde.
  bool standby = false;

  [[nodiscard]] friend bool operator==(const CompanionMemory&, const CompanionMemory&) = default;
};

// LA MÊME MÉMOIRE, `elapsed_ms` PLUS TARD : les âges grandissent, les restes
// fondent. Une suspension échue et un âge au-delà de l'horizon disparaissent,
// et un état qui n'a plus rien à dire aussi.
[[nodiscard]] CompanionMemory AgedMemory(CompanionMemory memory, std::uint64_t elapsed_ms);

}  // namespace ro::sim
