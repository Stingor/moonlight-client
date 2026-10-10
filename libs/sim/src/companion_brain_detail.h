#pragma once

// CE QUE LES FICHIERS DU CERVEAU D'UN COMPAGNON PARTAGENT : l'instant d'un tick,
// les délais du cerveau et les calculs de cases. Interne à `sim`.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_sight.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/world.h"

namespace ro::sim {

// L'INSTANT D'UN TICK : ce que toutes les étapes lisent.
struct CompanionBrain::Situation {
  const World& world;
  const Actor& self;
  const Actor& owner;
  const CompanionFacts& facts;
  const CompanionAiSettings& settings;
  std::uint32_t now_ms = 0;
  CellPosition self_cell;
  CellPosition owner_cell;
  bool owner_walking = false;
  OwnSide side;
  // Les amis du profil en vue, reconnus à leur nom.
  std::vector<const Actor*> friends;
  // CE QUE LA CARTE PERMET SUR UN JOUEUR à cet instant, et si la guilde du
  // maître y est épargnée.
  PlayerTargeting players = PlayerTargeting::kNone;
  bool guild_spared = false;
};

namespace companion_detail {

inline constexpr int kPercentWhole = 100;
// « AU CÔTÉ DU MAÎTRE » : le serveur range un compagnon qui le rejoint à deux
// cases derrière lui. Un seuil plus court ne serait jamais atteint, et l'ordre
// de retour repartirait sans fin.
inline constexpr int kOwnerSideCells = 2;
// UN DÉPLACEMENT DEMANDÉ met un aller-retour réseau à se voir : avant ce délai,
// un compagnon encore immobile n'est pas « arrêté ».
inline constexpr std::uint32_t kMoveSettleMs = 1000;

[[nodiscard]] inline int PercentOf(std::int64_t value, std::int64_t most) noexcept {
  if (most <= 0) {
    return kPercentWhole;
  }
  const std::int64_t percent = value * kPercentWhole / most;
  return static_cast<int>(std::clamp<std::int64_t>(percent, 0, kPercentWhole));
}

[[nodiscard]] inline CellPosition CellAt(int x, int y) noexcept {
  CellPosition cell;
  cell.x = static_cast<std::uint16_t>(std::clamp(x, 0, static_cast<int>(kMaxPackedCoordinate)));
  cell.y = static_cast<std::uint16_t>(std::clamp(y, 0, static_cast<int>(kMaxPackedCoordinate)));
  return cell;
}

[[nodiscard]] inline CellPosition Plain(CellPosition cell) noexcept {
  return CellAt(cell.x, cell.y);
}

[[nodiscard]] inline CellPosition Midpoint(CellPosition from, CellPosition to) noexcept {
  return CellAt((static_cast<int>(from.x) + static_cast<int>(to.x)) / 2,
                (static_cast<int>(from.y) + static_cast<int>(to.y)) / 2);
}

// LA CASE `to`, ramenée le long du trajet à `most_cells` de Manhattan depuis
// `from`.
[[nodiscard]] inline CellPosition ShortenTo(CellPosition from, CellPosition to,
                                            int most_cells) noexcept {
  const int manhattan = ManhattanCells(from, to);
  if (manhattan <= most_cells) {
    return Plain(to);
  }
  const int dx = static_cast<int>(to.x) - static_cast<int>(from.x);
  const int dy = static_cast<int>(to.y) - static_cast<int>(from.y);
  return CellAt(static_cast<int>(from.x) + (dx * most_cells / manhattan),
                static_cast<int>(from.y) + (dy * most_cells / manhattan));
}

// LA CASE OÙ SE TENIR pour avoir `target` à portée, sur la ligne qui va de la
// cible vers `from` : la plus éloignée de la cible qui reste à portée.
[[nodiscard]] inline CellPosition ApproachCell(CellPosition from, CellPosition target,
                                               int range_cells) noexcept {
  if (WithinCells(from, target, range_cells)) {
    return Plain(from);
  }
  const double dx = static_cast<double>(from.x) - static_cast<double>(target.x);
  const double dy = static_cast<double>(from.y) - static_cast<double>(target.y);
  const double length = std::sqrt((dx * dx) + (dy * dy));
  for (int reach = range_cells; reach > 0; --reach) {
    const double scale = static_cast<double>(reach) / length;
    const CellPosition cell =
        CellAt(static_cast<int>(target.x) + static_cast<int>(std::lround(dx * scale)),
               static_cast<int>(target.y) + static_cast<int>(std::lround(dy * scale)));
    if (WithinCells(cell, target, range_cells)) {
      return cell;
    }
  }
  return Plain(target);
}

// LA CASE À `step_cells` DE `from`, à l'opposé de `threat`.
[[nodiscard]] inline CellPosition AwayFrom(CellPosition from, CellPosition threat,
                                           int step_cells) noexcept {
  int dx = static_cast<int>(from.x) - static_cast<int>(threat.x);
  int dy = static_cast<int>(from.y) - static_cast<int>(threat.y);
  if (dx == 0 && dy == 0) {
    dx = 1;
  }
  const int length_cells = std::max(WholeCellDistance(from, threat), 1);
  return CellAt(static_cast<int>(from.x) + (dx * step_cells / length_cells),
                static_cast<int>(from.y) + (dy * step_cells / length_cells));
}

inline constexpr std::int64_t kMsPerSecond = 1000;

[[nodiscard]] inline std::int64_t SignedElapsedMs(std::uint32_t now_ms,
                                                  std::uint32_t since_ms) noexcept {
  return static_cast<std::int64_t>(now_ms) - static_cast<std::int64_t>(since_ms);
}

[[nodiscard]] inline std::int64_t MsOf(int seconds) noexcept {
  return static_cast<std::int64_t>(seconds) * kMsPerSecond;
}

// Les PV d'un acteur en pour cent, quand le client les connaît.
[[nodiscard]] inline std::optional<std::int64_t> HealthPercent(const Actor& actor) noexcept {
  if (actor.max_health <= 0 || actor.health < 0) {
    return std::nullopt;
  }
  return PercentOf(actor.health, actor.max_health);
}

// Sous ou au-dessus ; inconnu reste inconnu.
[[nodiscard]] inline std::optional<bool> Measured(std::optional<std::int64_t> value,
                                                  RuleComparison comparison, int number) noexcept {
  if (!value.has_value()) {
    return std::nullopt;
  }
  return comparison == RuleComparison::kBelow ? *value < number : *value > number;
}

// Est ou n'est pas ; inconnu reste inconnu, « n'est pas » compris.
[[nodiscard]] inline std::optional<bool> Stated(std::optional<bool> value,
                                                RuleComparison comparison) noexcept {
  if (!value.has_value()) {
    return std::nullopt;
  }
  return comparison == RuleComparison::kIsNot ? !*value : *value;
}

[[nodiscard]] inline bool Casting(const Actor& actor, std::uint32_t now_ms) noexcept {
  return actor.cast.has_value() && !actor.cast->cancelled &&
         now_ms - actor.cast->started_ms < actor.cast->duration_ms;
}

}  // namespace companion_detail
}  // namespace ro::sim
