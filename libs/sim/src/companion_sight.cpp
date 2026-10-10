// Voir l'en-tête.

#include "ro/sim/companion_sight.h"

#include <cstdlib>

#include "ro/sim/summon_master.h"
#include "ro/sim/world.h"

namespace ro::sim {
namespace {

[[nodiscard]] int SquaredCells(CellPosition from, CellPosition to) noexcept {
  const int dx = static_cast<int>(from.x) - static_cast<int>(to.x);
  const int dy = static_cast<int>(from.y) - static_cast<int>(to.y);
  return (dx * dx) + (dy * dy);
}

// Un fait daté de `started_ms` est encore récent s'il a moins de `window_ms`.
[[nodiscard]] bool Recent(std::uint32_t started_ms, std::uint32_t window_ms,
                          std::uint32_t now_ms) noexcept {
  return now_ms - started_ms < window_ms;
}

// LE JOUEUR À QUI APPARTIENT CE COMPAGNON, quand le serveur l'a dit ; zéro pour
// un acteur qui n'est pas un compagnon, ou dont le maître n'est pas connu.
[[nodiscard]] std::uint32_t CompanionOwnerOf(const World& world, const Actor& actor) {
  if (actor.kind != ActorKind::kNonPlayer || actor.familiar == Familiar::kNone) {
    return 0;
  }
  return SummonMasterOf(world, actor.id);
}

// UN JOUEUR HORS DU CAMP, ou le compagnon d'un tel joueur. Un compagnon dont le
// maître est connu se juge par ce maître, même hors de vue.
[[nodiscard]] bool IsOutsider(const World& world, std::uint32_t actor_id, const OwnSide& side) {
  if (side.Includes(actor_id)) {
    return false;
  }
  const Actor* actor = world.Find(actor_id);
  if (actor == nullptr) {
    return false;
  }
  if (actor->kind == ActorKind::kPlayer) {
    return true;
  }
  if (const std::uint32_t owner_id = CompanionOwnerOf(world, *actor); owner_id != 0) {
    return !side.Includes(owner_id);
  }
  return actor->kind == ActorKind::kNonPlayer &&
         (actor->familiar == Familiar::kHomunculus || actor->familiar == Familiar::kMercenary);
}

}  // namespace

int WholeCellDistance(CellPosition from, CellPosition to) noexcept {
  const int squared = SquaredCells(from, to);
  // La racine entière par défaut, en montant : exacte, et courte à l'échelle
  // d'une carte.
  int root = 0;
  while ((root + 1) * (root + 1) <= squared) {
    ++root;
  }
  return root;
}

bool WithinCells(CellPosition from, CellPosition to, int range_cells) noexcept {
  if (range_cells < 0) {
    return false;
  }
  return SquaredCells(from, to) < (range_cells + 1) * (range_cells + 1);
}

bool WithinSquareCells(CellPosition from, CellPosition to, int range_cells) noexcept {
  return std::abs(static_cast<int>(from.x) - static_cast<int>(to.x)) <= range_cells &&
         std::abs(static_cast<int>(from.y) - static_cast<int>(to.y)) <= range_cells;
}

int ManhattanCells(CellPosition from, CellPosition to) noexcept {
  return std::abs(static_cast<int>(from.x) - static_cast<int>(to.x)) +
         std::abs(static_cast<int>(from.y) - static_cast<int>(to.y));
}

bool IsAttacking(const Actor& attacker, std::uint32_t target_id, std::uint32_t now_ms) noexcept {
  if (target_id == 0) {
    return false;
  }
  if (attacker.attack.has_value() && attacker.attack->target == target_id &&
      Recent(attacker.attack->started_ms, kRecentStrikeMs, now_ms)) {
    return true;
  }
  return attacker.cast.has_value() && !attacker.cast->cancelled &&
         attacker.cast->target_id == target_id &&
         Recent(attacker.cast->started_ms, attacker.cast->duration_ms, now_ms);
}

bool CanBeTargeted(const Actor& actor) noexcept {
  constexpr std::uint32_t kConcealed = kOptionHide | kOptionCloak | kOptionInvisible;
  return actor.posture != Posture::kDead && (actor.option & kConcealed) == 0;
}

bool IsOthersMonster(const World& world, const Actor& monster, const OwnSide& side,
                     std::uint32_t now_ms) {
  if (monster.attack.has_value() && Recent(monster.attack->started_ms, kRecentStrikeMs, now_ms) &&
      IsOutsider(world, monster.attack->target, side)) {
    return true;
  }
  return monster.hit.has_value() && Recent(monster.hit->started_ms, kRecentStrikeMs, now_ms) &&
         IsOutsider(world, monster.hit->attacker, side);
}

SummonSide SummonSideOf(const World& world, const Actor& monster, const OwnSide& side) {
  const std::uint32_t master_id = SummonMasterOf(world, monster.id);
  if (master_id == 0) {
    return SummonSide::kNone;
  }
  // Le camp se reconnaît à l'identifiant, même maître hors de vue.
  if (side.IncludesOrParty(master_id)) {
    return SummonSide::kOwnSide;
  }
  const Actor* master = world.Find(master_id);
  if (master == nullptr) {
    return SummonSide::kNone;
  }
  if (master->kind == ActorKind::kMonster) {
    return SummonSide::kMinion;
  }
  // UN SEUL NIVEAU DE PLUS : le maître est un compagnon dont le joueur est
  // connu — la Légion de l'homoncule d'un membre du groupe est de son camp.
  if (const std::uint32_t owner_id = CompanionOwnerOf(world, *master); owner_id != 0) {
    if (side.IncludesOrParty(owner_id)) {
      return SummonSide::kOwnSide;
    }
    return SummonSide::kOthers;
  }
  return IsOutsider(world, master_id, side) ? SummonSide::kOthers : SummonSide::kNone;
}

}  // namespace ro::sim
