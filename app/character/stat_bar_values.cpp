// Ce que les six jauges montrent. Voir l'en-tête pour les deux sources, et pour
// ce que « inconnu » veut dire.

#include "character/stat_bar_values.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <string>
#include <utility>

#include "ro/sim/actor.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"
#include "ro/ui/cast_bars.h"

namespace moonclient {
namespace {

// Une jauge dont les deux nombres sont dits. ⚠ LA SENTINELLE DU SERVEUR VAUT -1 :
// un seul des deux à -1 suffit à rendre la jauge muette, sans quoi elle
// afficherait une fraction dont un bout est faux.
[[nodiscard]] ro::ui::StatBarValue Told(std::int64_t current, std::int64_t most) {
  ro::ui::StatBarValue value;
  if (current < 0 || most < 0) {
    return value;
  }
  value.known = true;
  value.current = current;
  value.most = most;
  return value;
}

[[nodiscard]] std::size_t Rank(ro::ui::StatBar bar) { return static_cast<std::size_t>(bar); }

// CE QU'UNE JAUGE FACTICE COMPTE, et à quel rythme elle ondule. Des maxima
// plausibles, pour que le texte ait la largeur qu'il aura en jeu : une
// expérience à sept chiffres n'occupe pas la place d'une magie à quatre.
// ⚠ DANS L'ORDRE DE `StatBar`. La bourse prend le repère du joueur, et
// l'incantation ne s'en sert pas.
struct Tuned {
  std::int64_t most = 0;
  double period_seconds = 0.0;
  double phase = 0.0;
};
constexpr std::array<Tuned, ro::ui::kStatBarCount> kTuned = {{
    {.most = 12'345, .period_seconds = 4.0, .phase = 0.0},
    {.most = 1'234, .period_seconds = 4.7, .phase = 0.15},
    {.most = 1'500'000, .period_seconds = 6.3, .phase = 0.3},
    {.most = 650'000, .period_seconds = 5.5, .phase = 0.45},
    // Le poids en DIXIÈMES, comme chez le serveur : 2 400,0.
    {.most = 24'000, .period_seconds = 5.1, .phase = 0.6},
    {.most = 0, .period_seconds = 7.2, .phase = 0.75},
    {.most = 0, .period_seconds = 1.0, .phase = 0.0},
}};

constexpr double kMsPerSecond = 1000.0;
constexpr std::uint32_t kTuningCastMs = 3'000;
constexpr const char* kTuningSpell = "Storm Gust";
// Le seuil que rAthena annonce par défaut : la régénération s'arrête à la
// moitié de la charge.
constexpr std::int64_t kUsualOverweightPercent = 50;

}  // namespace

ro::ui::StatBarValues StatBarValuesOf(const ro::sim::World& world, std::int64_t zeny_reference,
                                      ro::ui::StatBarValue own_cast) {
  ro::ui::StatBarValues values{};
  const ro::sim::PlayerStatus& status = world.player_status();

  // LA VIE VIENT DE L'ACTEUR, et de nulle part ailleurs.
  if (const ro::sim::Actor* player = world.player(); player != nullptr) {
    values[Rank(ro::ui::StatBar::kHealth)] = Told(player->health, player->max_health);
  }

  values[Rank(ro::ui::StatBar::kMagic)] = Told(status.sp, status.max_sp);
  values[Rank(ro::ui::StatBar::kBaseExp)] = Told(status.base_exp, status.next_base_exp);
  values[Rank(ro::ui::StatBar::kJobExp)] = Told(status.job_exp, status.next_job_exp);
  values[Rank(ro::ui::StatBar::kWeight)] = Told(status.weight, status.max_weight);
  // 🔴 LA BOURSE SE REMPLIT CONTRE LE REPÈRE DU JOUEUR, jamais contre le plafond
  // du serveur : deux milliards font une jauge qui ne bouge jamais. Le montant
  // écrit, lui, reste le vrai — le repère ne borne que le remplissage.
  const std::int64_t reference = std::max(zeny_reference, ro::ui::kSmallestZenyReference);
  values[Rank(ro::ui::StatBar::kZeny)] = Told(status.zeny, reference);
  values[Rank(ro::ui::StatBar::kCast)] = std::move(own_cast);
  return values;
}

ro::ui::StatBarValues TuningStatBarValues(std::uint32_t now_ms, std::int64_t zeny_reference) {
  ro::ui::StatBarValues values{};
  const double seconds = static_cast<double>(now_ms) / kMsPerSecond;
  for (std::size_t rank = 0; rank < values.size(); ++rank) {
    const Tuned& tuned = kTuned[rank];
    // ⚠ UN COSINUS ET NON UN SINUS : parti de zéro, il touche le vide ET le plein
    // à chaque période, et c'est aux deux bouts qu'un réglage se juge.
    const double share =
        (1.0 - std::cos(2.0 * std::numbers::pi * (seconds / tuned.period_seconds + tuned.phase))) *
        0.5;
    const std::int64_t most = rank == Rank(ro::ui::StatBar::kZeny)
                                  ? std::max(zeny_reference, ro::ui::kSmallestZenyReference)
                                  : tuned.most;
    values[rank] = Told(std::llround(share * static_cast<double>(most)), most);
  }
  // L'INCANTATION REJOUE UN SORT, et ne se vide pas en ondulant : une barre de
  // cast se remplit puis recommence.
  const std::uint32_t elapsed_ms = now_ms % kTuningCastMs;
  ro::ui::StatBarValue& cast = values[Rank(ro::ui::StatBar::kCast)];
  cast = Told(elapsed_ms, kTuningCastMs);
  cast.composed =
      std::string(kTuningSpell) + "  " + ro::ui::CastSecondsText(kTuningCastMs - elapsed_ms);
  return values;
}

ro::ui::StatBarWarnings TuningStatBarWarnings(const ro::sim::World& world) {
  ro::ui::StatBarWarnings warnings = StatBarWarningsOf(world);
  if (warnings.overweight_percent <= 0) {
    warnings.overweight_percent = kUsualOverweightPercent;
  }
  return warnings;
}

ro::ui::StatBarWarnings StatBarWarningsOf(const ro::sim::World& world) {
  ro::ui::StatBarWarnings warnings;
  const std::int64_t told = world.player_status().overweight_percent;
  // Zéro quand le serveur n'a rien dit — et la jauge ne colore alors rien.
  warnings.overweight_percent = told > 0 ? told : 0;
  return warnings;
}

}  // namespace moonclient
