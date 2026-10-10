#pragma once

#include <cstdint>

#include "ro/sim/actor.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"

namespace ro::sim {

// Une position en cases FRACTIONNAIRES.
//
// Entre deux cases, un acteur n'est nulle part en particulier : c'est ce que le
// client invente pour que la marche ne soit pas une suite de sauts.
struct FractionalCell {
  float x = 0.0F;
  float y = 0.0F;
};

// Les huit directions, telles que le serveur les numérote.
//
// La table est celle de dirx/diry, dans src/map/unit.cpp : zéro pointe vers le
// NORD (y croissant), un vers le NORD-OUEST, et la suite tourne donc dans le
// sens INVERSE des aiguilles d'une montre, vue de dessus. Se tromper d'origine
// ferait regarder tous les personnages à quarante-cinq degrés de là où ils
// vont ; se tromper de sens les retournerait.
//
// ⚠ Ce n'est PAS la numérotation d'un .act, qui part du sud et tourne dans
// l'autre sens : voir SpriteDirection, dans render/sprite_frame.h.
inline constexpr std::uint8_t kDirectionCount = 8;

// Vers où l'on regarde en allant d'une case à l'autre.
//
// Rend `fallback` si les deux cases sont la même : on ne déduit pas une
// direction d'un déplacement nul, et remettre zéro ferait pivoter vers le nord
// quiconque s'arrête.
[[nodiscard]] std::uint8_t DirectionTowards(CellPosition from, CellPosition to,
                                            std::uint8_t fallback);

// L'ANGLE QUE LE CLIENT TIENT POUR UNE VISÉE : l'écart en cases vers ce qu'on
// vise — x vers l'est, y vers le nord —, en degrés dans [0, 360). Le sud vaut
// zéro, l'est quatre-vingt-dix, le nord cent quatre-vingts : c'est atan2(dx,
// −dz) dans le repère du client, dont la profondeur croît avec la case.
//
// C'est le regard qu'un coup, un soin ou une incantation posent sur leur
// auteur. La boussole du serveur en est l'arrondi ; ce qui dessine le corps et
// la dérive des nombres jaillissants le lisent tel quel.
[[nodiscard]] float ClientAimDegrees(float dx_cells, float dy_cells) noexcept;

// LA DIRECTION DE BOUSSOLE LA PLUS PROCHE D'UNE VISÉE, dans la numérotation du
// serveur ; `fallback` pour un écart nul, qui ne tourne personne.
//
// 🔴 PAR L'ANGLE, ET NON PAR LE SIGNE DE CHAQUE ÉCART. Viser cinq cases à
// l'est et une au nord regarde l'EST, comme chez le client, et non la diagonale
// que rendrait `DirectionTowards` — faite pour des pas d'une case, où le signe
// dit tout.
[[nodiscard]] std::uint8_t DirectionOfAim(float dx_cells, float dy_cells,
                                          std::uint8_t fallback) noexcept;

// 🔴 LA CASE OÙ UN ACTEUR SE TIENT, ET NON CELLE QU'IL ANNONCE : l'arrivée de
// son déplacement s'il en a un, sa case sinon.
//
// `Actor::position` est le DÉPART d'un déplacement, et il le reste tant que le
// serveur n'a pas annoncé autre chose. Tout ce qui se calcule ENTRE DEUX
// ACTEURS — une direction, une portée — part donc d'ici : partir de
// `Actor::position` fait viser depuis une case que l'acteur a quittée, et la
// visée s'inverse dès qu'il l'a dépassée.
[[nodiscard]] CellPosition StandingCell(const Actor& actor) noexcept;

// Les trois regards, relatifs au CORPS et non à la carte.
//
// Un sprite au repos porte exactement trois images par direction, et ce sont
// ces trois-là : de face, et d'un huitième de tour de part et d'autre. C'est
// pourquoi un regard ne peut jamais dépasser un huitième — au-delà, c'est le
// corps qui tourne.
inline constexpr std::uint8_t kGazeAhead = 0;
// Vers la direction de numéro INFÉRIEUR, donc dans le sens des aiguilles d'une
// montre vue de dessus, la numérotation du serveur tournant à l'envers.
inline constexpr std::uint8_t kGazeDownward = 1;
inline constexpr std::uint8_t kGazeUpward = 2;

// Un corps et le regard qu'il porte.
struct Facing {
  std::uint8_t direction = 0;
  std::uint8_t head_direction = kGazeAhead;

  [[nodiscard]] friend bool operator==(const Facing&, const Facing&) = default;
};

// CE QU'UN DEMI-TOUR DEMANDÉ DONNE VRAIMENT, et ce n'est pas « regarde là ».
//
// 🔴 LE REGARD PART EN PREMIER, ET LE CORPS SUIT D'UN HUITIÈME À LA FOIS. Le
// client natif ne pose jamais le corps sur la case visée d'un coup : il
// rapproche le corps jusqu'à un huitième du but et confie le dernier huitième
// au REGARD. Viser à quatre-vingt-dix degrés demande donc DEUX demandes — la
// première tourne le corps de quarante-cinq degrés et penche la tête, la
// seconde redresse la tête et achève le corps. Poser la direction visée d'un
// coup ferait pivoter le personnage plus vite que chez tout le monde, et le
// serveur diffuserait ce pivot à tous.
//
// ⚠ UN DEMI-TOUR COMPLET, LUI, EST INSTANTANÉ : à quatre huitièmes, aucun
// regard ne peut couvrir l'écart, et le corps y va d'un coup.
//
// Le regard courant entre dans le calcul : c'est lui qui distingue la première
// demande de la seconde. Une tête déjà penchée du bon côté veut dire « le
// huitième qui manque a déjà été demandé », et c'est alors le corps qui bouge.
[[nodiscard]] Facing TurnToward(Facing from, std::uint8_t wanted_direction) noexcept;

// La sous-case d'un déplacement annoncé : seize par case, et le centre vaut
// huit. C'est ce que le serveur écrit pour une marche neuve ; un acteur
// annoncé EN COURS de marche porte l'endroit exact où il en était.
inline constexpr float kSubCellsPerCell = 16.0F;
inline constexpr float kSubCellCenter = 8.0F;

// ── Le long d'un chemin ─────────────────────────────────────────────────────
//
// Le serveur fait marcher case par case le long du chemin qu'il a calculé, un
// pas à la fois : la vitesse est la durée d'un pas droit, et une diagonale en
// prend quatorze dixièmes. Le chemin vient de WalkPlanner ; ces fonctions ne
// font que le parcourir.
//
// `now_ms` est fourni par l'appelant, jamais lu d'une horloge : c'est ce qui
// permet de rejouer une capture et d'obtenir exactement les mêmes positions.

// Combien de temps la marche dure, en millisecondes. Zéro si la vitesse est
// inconnue — le déplacement est alors instantané, ce qui vaut mieux qu'une
// division par zéro.
[[nodiscard]] std::uint32_t MovementDurationMs(const Actor& actor, const WalkPath& path);

// Où l'acteur se trouve à cet instant.
//
// La sous-case du départ compte : engagé sur son premier pas, l'acteur le
// finit depuis là ; en retrait de la case annoncée, il la rejoint d'abord.
// Sans cela, un acteur aperçu en pleine marche sauterait au centre de sa case.
// La distance PARCOURUE depuis le début du déplacement, en cases.
//
// Un pas de biais en vaut la racine de deux, comme la distance qu'il couvre.
// Zéro pour qui ne marche pas.
//
// À quoi ça sert : le jeu n'anime pas la marche sur une horloge, il l'anime sur
// la distance — c'est ce qui fait qu'un personnage rapide lève les pieds plus
// souvent, sans que rien ne le règle. Voir WalkFrameAt.
[[nodiscard]] float TravelledCells(const Actor& actor, const WalkPath& path, std::uint32_t now_ms);

// La case sur laquelle une marche INTERROMPUE s'arrête.
//
// Le serveur pose l'unité sur la case la plus PROCHE de son chemin : un pas
// franchi à moitié se termine, sinon il se défait (unit_stop_walking avec
// USW_MOVE_FULL_CELL, dans src/map/unit.cpp). L'interpolation étant linéaire
// dans le temps, arrondir la case fractionnaire donne exactement la même case.
[[nodiscard]] CellPosition StoppedCell(const Actor& actor, const WalkPath& path,
                                       std::uint32_t now_ms);

[[nodiscard]] FractionalCell InterpolatedCell(const Actor& actor, const WalkPath& path,
                                              std::uint32_t now_ms);

// Vers où dessiner l'acteur à cet instant : la direction du pas en cours s'il
// marche, celle du dernier pas s'il est arrivé, celle qu'il a annoncée sinon.
[[nodiscard]] std::uint8_t FacingAt(const Actor& actor, const WalkPath& path, std::uint32_t now_ms);

// Vrai tant que l'acteur est EN CHEMIN.
//
// Le déplacement annoncé subsiste après l'arrivée : c'est lui qui garde la
// direction du dernier pas, et l'effacer ferait pivoter vers le nord quiconque
// s'arrête. Marcher se juge donc au temps écoulé et non à sa présence, sans quoi
// un personnage arrivé continuerait d'agiter les jambes sur place.
[[nodiscard]] bool IsWalking(const Actor& actor, const WalkPath& path, std::uint32_t now_ms);

// ── Sans carte ──────────────────────────────────────────────────────────────
//
// Les mêmes, le long de la LIGNE DROITE entre les deux bouts : ce qu'on peut
// faire de mieux sans connaître la carte. Autour d'un obstacle, l'acteur
// passe alors là où le serveur ne le tient pas.
[[nodiscard]] std::uint32_t MovementDurationMs(const Actor& actor);
[[nodiscard]] FractionalCell InterpolatedCell(const Actor& actor, std::uint32_t now_ms);
[[nodiscard]] std::uint8_t FacingAt(const Actor& actor, std::uint32_t now_ms);
[[nodiscard]] bool IsWalking(const Actor& actor, std::uint32_t now_ms);

// ── Avec ou sans carte ──────────────────────────────────────────────────────

// OÙ UN ACTEUR SE TROUVE À CET INSTANT, en cases fractionnaires : le long du
// chemin que la carte donne à sa marche — la ligne droite sans carte, ou quand
// la carte refuse le chemin annoncé —, dans sa case sinon.
//
// C'est d'ici qu'un acteur VISE, et ici qu'on le vise : le client tourne un
// frappeur vers l'endroit où sa cible se trouve au coup, au milieu d'un pas
// s'il le faut, et non vers la case où elle s'arrêtera.
[[nodiscard]] FractionalCell PositionAt(const Actor& actor, const WalkMap* map,
                                        std::uint32_t at_ms);

}  // namespace ro::sim
