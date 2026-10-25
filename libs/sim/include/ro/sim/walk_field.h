#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/sim/packed_position.h"
#include "ro/sim/walk_map.h"

namespace ro::sim {

// UN CHAMP DE DISTANCES À LA MARCHE : pour chaque case d'une carte, ce qu'il en
// coûte pour rejoindre la plus proche des SOURCES, et par où.
//
// C'est l'outil du guidage, et il répond à deux besoins que le chemin du serveur
// ne couvre pas. `FindWalkPath` copie celui du serveur jusque dans sa borne de
// trente-deux pas : il dit où le personnage IRA au prochain clic, pas comment
// traverser une carte de quatre cents cases. Ici la recherche n'est pas bornée,
// et elle part de la DESTINATION :
//
//   · un seul calcul par cible — la sortie de la carte, ou le PNJ visé — et le
//     chemin se relit ensuite depuis n'importe quelle case en descendant la
//     pente. Le joueur peut marcher, se tromper, faire un détour : rien n'est à
//     recalculer tant que la cible ne change pas ;
//   · parti du JOUEUR, le même calcul donne d'un coup la distance à toutes les
//     sorties de sa carte, ce que le choix de l'itinéraire demande.
//
// Les règles de pas sont celles du serveur : huit voisins, dix par pas droit,
// quatorze en diagonale, et une diagonale refusée quand elle couperait un coin
// — elle exige ses deux voisins droits praticables.
class WalkField {
 public:
  WalkField() = default;

  // Le champ vers les `sources`. Une source hors carte ou bloquée est ignorée ;
  // sans source valable, le champ est vide et tout y est injoignable.
  [[nodiscard]] static WalkField Flood(const WalkMap& map, std::span<const CellPosition> sources);

  // Le coût, en unités de pas (dix par case droite), pour rejoindre une source
  // depuis (x, y). Rien si la case est injoignable ou hors carte.
  [[nodiscard]] std::optional<std::uint32_t> CostFrom(std::int32_t x, std::int32_t y) const;

  // Le chemin depuis (x, y) jusqu'à la source la plus proche, les deux bouts
  // compris. Vide si la case est injoignable.
  [[nodiscard]] std::vector<CellPosition> PathFrom(std::int32_t x, std::int32_t y) const;

  // La case voisine où mène le pas qui RAPPROCHE de la source, depuis (x, y) ;
  // son `direction` porte la direction serveur du pas (0 nord, 1 nord-ouest…
  // 7 nord-est). Rien pour une source, une case injoignable ou hors champ. Sans
  // allocation : c'est ce qu'une horde lit à chaque pas.
  [[nodiscard]] std::optional<CellPosition> NextCellFrom(std::int32_t x, std::int32_t y) const;

  [[nodiscard]] bool empty() const noexcept { return cost_.empty(); }
  [[nodiscard]] std::int32_t width() const noexcept { return width_; }
  [[nodiscard]] std::int32_t height() const noexcept { return height_; }

 private:
  std::int32_t width_ = 0;
  std::int32_t height_ = 0;
  std::vector<std::uint32_t> cost_;
  // Par case, la direction du pas qui RAPPROCHE de la source ; une source porte
  // `kNoStep`.
  std::vector<std::uint8_t> toward_source_;
};

// La case praticable la plus proche de (x, y), cherchée en anneaux carrés
// jusqu'à `max_ring`. Un PNJ se tient souvent sur une case que la marche ne
// franchit pas — un comptoir, un décor — et le guidage vise alors la case d'à
// côté. Le balayage est DÉTERMINISTE : deux appels rendent la même case.
[[nodiscard]] std::optional<CellPosition> NearestPassable(const WalkMap& map, std::int32_t x,
                                                          std::int32_t y, std::int32_t max_ring);

// Les COINS d'un chemin : le premier point, le dernier, et chaque case où la
// direction change. Entre deux coins la trajectoire est droite ; une ligne brisée
// par ces seuls points repasse par les mêmes cases.
[[nodiscard]] std::vector<CellPosition> CornersOf(std::span<const CellPosition> path);

// Une estimation de la marche entre deux cases sans rien savoir des obstacles :
// dix par pas droit, quatorze par diagonale. C'est un MINIMUM — la vraie marche
// n'est jamais plus courte.
[[nodiscard]] std::uint32_t OctileCost(std::int32_t from_x, std::int32_t from_y, std::int32_t to_x,
                                       std::int32_t to_y) noexcept;

}  // namespace ro::sim
