#pragma once

#include <algorithm>
#include <cstdint>
#include <span>

#include "ro/sim/actor.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/world_fwd.h"

namespace ro::sim {

// ── CE QUE L'IA D'UN COMPAGNON VOIT DU MONDE ────────────────────────────────
//
// Les distances, les cibles valables, et qui attaque qui. Rien n'y décide :
// c'est `CompanionBrain` qui s'en sert pour choisir un ordre.

// LA VUE DU COMPAGNON, en cases, mesurée depuis LUI : au-delà, une cible est
// perdue.
inline constexpr int kCompanionSightCells = 20;

// COMBIEN DE TEMPS UN COUP COMPTE pour dire « X attaque Y » : le dernier coup
// porté reste une attaque en cours tant qu'il a moins que cet âge.
inline constexpr std::uint32_t kRecentStrikeMs = 5000;

// LA DISTANCE ENTRE DEUX CASES, en cases entières : la partie entière de la
// distance euclidienne. Ni Chebyshev ni Manhattan.
[[nodiscard]] int WholeCellDistance(CellPosition from, CellPosition to) noexcept;

// À PORTÉE : `WholeCellDistance(from, to) <= range_cells`, soit dx² + dy² < (r + 1)².
// Une portée négative n'atteint rien.
[[nodiscard]] bool WithinCells(CellPosition from, CellPosition to, int range_cells) noexcept;

// DANS LE CARRÉ : max(|dx|, |dy|) <= range_cells. C'est la zone du serveur —
// celle de sa vue, qui fait connaître un acteur au client. Une portée
// négative n'atteint rien.
[[nodiscard]] bool WithinSquareCells(CellPosition from, CellPosition to, int range_cells) noexcept;

// LA DISTANCE DE MANHATTAN, |dx| + |dy|, celle des brides de déplacement.
[[nodiscard]] int ManhattanCells(CellPosition from, CellPosition to) noexcept;

// VRAI QUAND `attacker` ATTAQUE `target_id` à cet instant : son dernier coup le
// vise et a moins de `kRecentStrikeMs`, ou il l'incante en ce moment.
[[nodiscard]] bool IsAttacking(const Actor& attacker, std::uint32_t target_id,
                               std::uint32_t now_ms) noexcept;

// UNE CIBLE QU'ON PEUT VISER : ni morte, ni cachée, ni invisible.
[[nodiscard]] bool CanBeTargeted(const Actor& actor) noexcept;

// LE CAMP DU MAÎTRE : lui et ses deux compagnons. Zéro pour un compagnon absent.
struct OwnSide {
  std::uint32_t owner_id = 0;
  std::uint32_t companion_id = 0;
  std::uint32_t partner_id = 0;
  // Les membres de son groupe, par identifiant de compte : celui de leur acteur.
  std::span<const std::uint32_t> party_ids;

  [[nodiscard]] bool Includes(std::uint32_t actor_id) const noexcept {
    return actor_id != 0 &&
           (actor_id == owner_id || actor_id == companion_id || actor_id == partner_id);
  }
  // ⚠ LE GROUPE N'EST PAS LE CAMP pour `Includes` : le monstre d'un membre du
  // groupe reste celui d'un autre joueur. Il n'en fait partie que pour les
  // invocations, voir `SummonSideOf`.
  [[nodiscard]] bool IncludesOrParty(std::uint32_t actor_id) const noexcept {
    return Includes(actor_id) ||
           (actor_id != 0 && std::ranges::find(party_ids, actor_id) != party_ids.end());
  }
};

// D'OÙ VIENT UN MONSTRE INVOQUÉ, d'après le maître que le serveur annonce. Un
// maître qui est lui-même un compagnon de maître connu se juge par ce joueur :
// on ne remonte qu'UN niveau.
enum class SummonSide : std::uint8_t {
  // Pas de maître connu, ou un maître hors de vue qu'on ne sait pas classer :
  // un monstre ordinaire.
  kNone,
  // Invoqué par le maître, un membre de son groupe, le compagnon ou l'autre
  // compagnon — ou par le compagnon d'un membre du groupe, quand le serveur a
  // dit à qui il appartient : jamais une cible automatique.
  kOwnSide,
  // Invoqué par un joueur hors du camp, ou par le compagnon d'un tel joueur :
  // le monstre d'un autre joueur.
  kOthers,
  // Invoqué par un monstre : un sbire.
  kMinion,
};

[[nodiscard]] SummonSide SummonSideOf(const World& world, const Actor& monster,
                                      const OwnSide& side);

// LE MONSTRE D'UN AUTRE JOUEUR : son dernier coup récent vise un joueur hors du
// camp du maître, ou il vient d'être frappé par un tel joueur ou par le
// compagnon d'un tel joueur.
[[nodiscard]] bool IsOthersMonster(const World& world, const Actor& monster, const OwnSide& side,
                                   std::uint32_t now_ms);

}  // namespace ro::sim
