#pragma once

#include <cstdint>
#include <optional>
#include <vector>

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

// LE DÉPART D'UNE PARTIE CLASSÉE : neutre, la classe `class_id` — un Id que les
// règles ne connaissent pas reste tel quel, et la partie refuse de commencer —,
// l'arène classée `arena_id` — à défaut la première arène classée — et la
// graine tirée par le serveur. Le héros part de son auto-attaque seule : son
// premier niveau lui offre ses skills de départ.
[[nodiscard]] SurvivorsStart RankedStartOf(const SurvivorsRules& rules, std::uint64_t seed,
                                           std::uint16_t class_id, std::uint16_t arena_id);

// L'arène classée `arena_id`, à défaut la première arène classée ; nulle
// quand les règles n'en classent aucune.
[[nodiscard]] const ArenaRule* RankedArenaOr(const SurvivorsRules& rules,
                                             std::uint16_t arena_id) noexcept;

// ── LES BORNES QUE LE SERVEUR IMPOSE À UNE PARTIE D'UNE ARÈNE ───────────────
//
// Le serveur juge une partie vraisemblable par ces bornes, qu'il recopie de
// cette fonction, arène par arène : `RankingBoundsOf` est leur seule source.

// Un boss programmé : l'instant où il paraît, les points et l'XP de sa chute.
struct SurvivorsBossBound {
  std::uint32_t at_ms = 0;
  std::uint32_t kill_points = 0;
  std::uint32_t exp = 0;
  bool final = false;  // sa chute est la victoire
  [[nodiscard]] bool operator==(const SurvivorsBossBound&) const = default;
};

struct SurvivorsArenaBounds {
  // Triés par instant. Une arène qui tire ses boss en garde, rang par rang,
  // le plus tôt paru et le plus riche de tous les tirages : aucune partie ne
  // rencontre plus tôt un boss plus riche.
  std::vector<SurvivorsBossBound> bosses;
  // Les points et l'XP d'un tué ordinaire au plus, élite comprise : toute
  // espèce que les vagues, les événements, les escortes et les invocations
  // font naître.
  std::uint32_t most_common_kill_points = 0;
  std::uint32_t most_common_kill_exp = 0;
  [[nodiscard]] bool operator==(const SurvivorsArenaBounds&) const = default;
};

[[nodiscard]] SurvivorsArenaBounds RankingBoundsOf(const SurvivorsRules& rules,
                                                   const ArenaRule& arena);

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
