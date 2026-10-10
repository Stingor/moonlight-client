#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "ro/formats/gat.h"

namespace ro::sim {

// La grille de franchissement d'une carte, telle que le SERVEUR la tient.
//
// Le serveur ne raisonne pas sur le GAT mais sur les cellules qu'il en dérive
// (read_map de rAthena, reproduit par DeriveServerCells), et parmi elles seuls
// deux types bloquent la marche : le mur (1) et la crevasse (5), d'après
// map_gat2cell dans src/map/map.cpp. L'eau ne change rien : elle ne touche
// que la case marchable, qui le reste.
//
// Hors de la carte, tout est bloqué — c'est ce que map_getcellp répond quand
// on l'interroge sur le franchissement d'une case qui n'existe pas.
class WalkMap {
 public:
  WalkMap() = default;
  // Un OCTET par case, et non un bit — voir `passable_`.
  WalkMap(std::int32_t width, std::int32_t height, std::vector<std::uint8_t> passable);

  // Depuis le GAT lui-même. Le type y est tronqué à un octet et délesté de son
  // drapeau d'eau, comme le serveur le lit ; le niveau d'eau, qui ne change
  // pas la marche, n'a pas à être connu.
  [[nodiscard]] static WalkMap FromGat(const formats::GatMap& gat);

  // Depuis les cellules serveur, une par case, dans l'ordre du GAT.
  [[nodiscard]] static WalkMap FromServerCells(std::int32_t width, std::int32_t height,
                                               std::span<const std::uint8_t> cells);

  // Depuis un dessin, pour les essais : « . » se franchit, « # » bloque. La
  // PREMIÈRE ligne est le NORD de la carte — le y le plus grand — pour que le
  // dessin se lise comme la carte se regarde.
  [[nodiscard]] static WalkMap FromPicture(std::span<const std::string_view> rows);

  [[nodiscard]] std::int32_t width() const noexcept { return width_; }
  [[nodiscard]] std::int32_t height() const noexcept { return height_; }

  // Faux hors de la carte.
  [[nodiscard]] bool Passable(std::int32_t x, std::int32_t y) const noexcept;

 private:
  std::int32_t width_ = 0;
  std::int32_t height_ = 0;
  // ⚠ UN OCTET PAR CASE, ET SURTOUT PAS UN `vector<bool>`. Celui-ci est une
  // spécialisation à BITS : chaque lecture passe par un proxy, un décalage et un
  // masque. Or cette grille se lit par dizaines de milliers d'accès — une fois
  // par case à la construction, puis à chaque voisin qu'un chemin examine. Une
  // carte de 400 sur 400 coûte 160 Kio sous cette forme, ce qu'on rend
  // volontiers. Même raison qu'à `BleedTransparentEdges`.
  std::vector<std::uint8_t> passable_;
};

// Les types de cellule serveur qui bloquent la marche.
inline constexpr std::uint8_t kServerCellWall = 1;
inline constexpr std::uint8_t kServerCellGap = 5;

}  // namespace ro::sim
