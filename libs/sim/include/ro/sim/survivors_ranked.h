#pragma once

#include <cstdint>
#include <optional>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

namespace ro::sim {

// ── CE QU'UNE PARTIE CLASSÉE DIT AU SERVEUR, ET CE QU'ELLE EN CONTRÔLE ──────
//
// Des fonctions pures : aucune horloge, aucun octet. Le serveur refait le
// score depuis les composantes par la formule de `SurvivorsScore` : il n'y a
// pas d'autre score ici.

// Les composantes d'une partie finie, telles qu'elles partent au serveur.
struct SurvivorsEndComponents {
  std::uint32_t survived_steps = 0;
  std::uint32_t kill_points = 0;
  std::uint32_t kills = 0;
  std::uint16_t boss_kills = 0;
  std::uint16_t level = 0;
  // kFell, kSurvived ou kVictory : jamais une partie en cours ou abandonnée.
  SurvivorsOutcome outcome = SurvivorsOutcome::kFell;
  std::uint32_t journal_bytes = 0;  // la taille du journal de la partie
  [[nodiscard]] bool operator==(const SurvivorsEndComponents&) const = default;
};

// Les composantes lues dans le bilan ; rien pour une partie en cours ou
// abandonnée, qui ne s'envoie pas.
[[nodiscard]] std::optional<SurvivorsEndComponents> EndComponentsOf(
    const SurvivorsTally& tally, std::uint32_t journal_bytes) noexcept;

// LE DÉPART D'UNE PARTIE CLASSÉE : neutre, la classe et l'arène livrées en
// tête comme les autres départs, la graine tirée par le serveur et le premier
// bolt choisi par le joueur.
[[nodiscard]] SurvivorsStart RankedStartOf(const SurvivorsRules& rules, std::uint64_t seed,
                                           std::uint16_t first_skill_id);

// Vrai quand la graine rendue en défi est `SeedOfWeek` de la semaine ISO de
// `issued_at_utc_s`, l'heure du SERVEUR : la semaine change le lundi à 00:00
// UTC.
[[nodiscard]] bool ChallengeTicketMatches(std::uint64_t ticket_seed,
                                          std::int64_t issued_at_utc_s) noexcept;

// Les versions de règles qu'un serveur classe, bornes comprises.
struct SurvivorsRulesetRange {
  std::uint16_t low = 0;
  std::uint16_t high = 0;
  [[nodiscard]] bool operator==(const SurvivorsRulesetRange&) const = default;
};

// Vrai quand `ruleset` est dans l'intervalle ; jamais quand il est vide
// (`low > high`).
[[nodiscard]] bool RulesetServed(const SurvivorsRulesetRange& service,
                                 std::uint16_t ruleset) noexcept;

}  // namespace ro::sim
