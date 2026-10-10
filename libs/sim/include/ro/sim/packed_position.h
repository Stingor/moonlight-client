#pragma once

#include <array>
#include <cstdint>

namespace ro::sim {

// Une case de la carte, et vers où l'acteur regarde.
//
// Les coordonnées sont en CASES, pas en unités de monde : c'est ainsi que le
// serveur raisonne, et toute divergence sur ce point produirait un personnage
// posé à côté de là où le serveur le croit.
struct CellPosition {
  std::uint16_t x = 0;
  std::uint16_t y = 0;
  // Huit directions, la neuvième valeur (0..15 sur le fil) n'étant pas utilisée.
  std::uint8_t direction = 0;

  [[nodiscard]] friend bool operator==(const CellPosition&, const CellPosition&) = default;
};

// Un déplacement en cours, tel que le serveur l'annonce.
//
// Il ne donne que les deux BOUTS : le chemin entre eux est calculé par le
// client. Les décalages de sous-case disent où l'acteur se trouvait exactement
// au départ, en seizièmes de case.
struct CellMovement {
  CellPosition from;
  CellPosition to;
  std::uint8_t sub_x = 0;
  std::uint8_t sub_y = 0;

  [[nodiscard]] friend bool operator==(const CellMovement&, const CellMovement&) = default;
};

// Ce que peuvent porter les champs empaquetés.
//
// Dix bits par coordonnée, quatre pour la direction. Une carte de Ragnarok ne
// dépasse pas 512 cases de côté, donc la limite n'est jamais atteinte — mais
// elle borne ce qu'un aller-retour peut promettre.
inline constexpr std::uint16_t kMaxPackedCoordinate = 1023;
inline constexpr std::uint8_t kMaxPackedDirection = 15;

// Une position tient sur TROIS octets, un déplacement sur SIX.
inline constexpr std::size_t kPackedPositionBytes = 3;
inline constexpr std::size_t kPackedMovementBytes = 6;

// Défait l'empaquetage d'une position.
//
// Le serveur y comprime deux coordonnées de dix bits et une direction de quatre
// dans trois octets, à cheval sur les frontières d'octets. Les décalages
// ci-dessous sont ceux de RBUFPOS, dans src/map/clif.cpp — les recalculer de
// tête donnerait un personnage à quelques cases de là où il est.
[[nodiscard]] CellPosition UnpackPosition(
    const std::array<std::uint8_t, kPackedPositionBytes>& packed);

// L'inverse. Sert aux essais, et aux paquets que le client envoie.
[[nodiscard]] std::array<std::uint8_t, kPackedPositionBytes> PackPosition(CellPosition position);

// Défait l'empaquetage d'un déplacement.
//
// ⚠ Le second point n'a PAS de direction : le serveur n'en envoie pas, elle se
// déduit du trajet. Celle du premier point n'existe pas non plus — les quatre
// bits qu'elle occuperait dans une position isolée portent ici le début de la
// coordonnée d'arrivée.
[[nodiscard]] CellMovement UnpackMovement(
    const std::array<std::uint8_t, kPackedMovementBytes>& packed);

[[nodiscard]] std::array<std::uint8_t, kPackedMovementBytes> PackMovement(
    const CellMovement& movement);

}  // namespace ro::sim
