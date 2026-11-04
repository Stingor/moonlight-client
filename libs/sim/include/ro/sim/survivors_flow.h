#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/sim/packed_position.h"
#include "ro/sim/survivors_ground.h"
#include "ro/sim/walk_field.h"
#include "ro/sim/walk_map.h"

namespace ro::sim {

// LA POURSUITE D'UNE HORDE : un seul champ de marche, sur une fenêtre autour du
// héros, refait seulement quand sa case change, au plus une fois par
// `refresh_steps` pas. Le champ est celui de WalkField, mêmes règles de coin.
class HordeFlow {
 public:
  // Le premier appel inonde toujours ; les suivants seulement si la case du
  // héros a changé ET que `refresh_steps` pas ont passé depuis la dernière
  // inondation. La fenêtre couvre [héros − radius_cells, héros + radius_cells]
  // sur chaque axe, bornée au sol.
  void Refresh(const SurvivorsGround& ground, CellPosition hero_cell, std::uint32_t step,
               std::int32_t radius_cells, std::uint32_t refresh_steps);
  // LE MÊME CHAMP VERS PLUSIEURS HÉROS : chaque monstre marche vers le plus
  // proche. La fenêtre part du milieu de leur boîte englobante, et s'élargit
  // de la moitié de sa taille. Il se refait dès qu'un héros change de case.
  void Refresh(const SurvivorsGround& ground, std::span<const CellPosition> hero_cells,
               std::uint32_t step, std::int32_t radius_cells, std::uint32_t refresh_steps);
  // La case de l'arène vers laquelle marcher depuis (x, y) ; rien hors
  // fenêtre, sur la source ou si injoignable. Son `direction` est la direction
  // serveur du pas.
  [[nodiscard]] std::optional<CellPosition> NextCellFrom(std::int32_t x, std::int32_t y) const;
  // Le coût de marche (dix par pas droit, quatorze en diagonale) de (x, y)
  // jusqu'à la source ; rien hors fenêtre ou si injoignable.
  [[nodiscard]] std::optional<std::uint32_t> CostFrom(std::int32_t x, std::int32_t y) const;
  // Vrai si (x, y) est dans la fenêtre d'un champ inondé depuis une case
  // praticable : le champ sait alors dire si la case mène au héros. Faux avant
  // la première inondation.
  [[nodiscard]] bool Covers(std::int32_t x, std::int32_t y) const noexcept;
  // La case d'où le champ est parti : celle du héros à la dernière inondation.
  [[nodiscard]] std::optional<CellPosition> source() const noexcept {
    return flooded_from_.empty() ? std::nullopt : std::optional(flooded_from_.front());
  }
  // Les cases d'où le champ est parti, dans l'ordre reçu.
  [[nodiscard]] std::span<const CellPosition> sources() const noexcept { return flooded_from_; }
  // Le nombre d'inondations faites : ce que le banc et les essais mesurent.
  [[nodiscard]] std::uint32_t floods() const noexcept { return floods_; }

 private:
  WalkField field_;
  std::int32_t origin_x_ = 0;
  std::int32_t origin_y_ = 0;
  std::vector<CellPosition> flooded_from_;
  bool sourced_ = false;  // la source était praticable : le champ mène quelque part
  std::uint32_t flooded_step_ = 0;
  std::uint32_t floods_ = 0;
};

}  // namespace ro::sim
