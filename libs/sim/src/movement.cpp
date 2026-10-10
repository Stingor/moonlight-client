#include "ro/sim/movement.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <numbers>
#include <optional>

namespace ro::sim {
namespace {

// Les vecteurs de dirx et diry, dans src/map/unit.cpp. Zéro pointe au nord.
constexpr std::array<int, kDirectionCount> kDeltaX = {0, -1, -1, -1, 0, 1, 1, 1};
constexpr std::array<int, kDirectionCount> kDeltaY = {1, 1, 0, -1, -1, -1, 0, 1};

// Le signe d'un écart, ramené aux trois valeurs que porte la table.
int StepOf(int delta) { return delta > 0 ? 1 : (delta < 0 ? -1 : 0); }
int StepOf(float delta) { return delta > 0.0F ? 1 : (delta < 0.0F ? -1 : 0); }

std::uint8_t DirectionOf(int step_x, int step_y, std::uint8_t fallback) {
  if (step_x == 0 && step_y == 0) {
    return fallback;
  }
  for (std::uint8_t direction = 0; direction < kDirectionCount; ++direction) {
    if (kDeltaX[direction] == step_x && kDeltaY[direction] == step_y) {
      return direction;
    }
  }
  return fallback;
}

FractionalCell Lerp(FractionalCell from, FractionalCell to, float progress) {
  return FractionalCell{std::lerp(from.x, to.x, progress), std::lerp(from.y, to.y, progress)};
}

FractionalCell CellOf(CellPosition cell) {
  return FractionalCell{static_cast<float>(cell.x), static_cast<float>(cell.y)};
}

// Où l'acteur en est de sa marche : sa position, son regard, et s'il marche
// encore. Les trois se lisent au même endroit du même chemin, et les calculer
// séparément inviterait à les désaccorder.
struct Located {
  FractionalCell cell;
  std::uint8_t facing = 0;
  bool walking = false;
};

// Une étape de la marche : d'où à où, en combien de temps, dans quelle
// direction. Les pas du serveur en sont ; la sous-case du départ en ajoute
// une devant, ou raccourcit la première.
struct Stage {
  FractionalCell from;
  FractionalCell to;
  float duration_ms = 0.0F;
  std::uint8_t facing = 0;
};

// Rend chaque étape à `visit`, qui rend vrai pour s'arrêter.
template <typename Visit>
void ForEachStage(const Actor& actor, const WalkPath& path, Visit visit) {
  const CellMovement& movement = *actor.movement;
  const std::uint16_t speed = MovingSpeedOf(actor);
  FractionalCell here{
      static_cast<float>(movement.from.x) +
          (static_cast<float>(movement.sub_x) - kSubCellCenter) / kSubCellsPerCell,
      static_cast<float>(movement.from.y) +
          (static_cast<float>(movement.sub_y) - kSubCellCenter) / kSubCellsPerCell,
  };
  const FractionalCell announced = CellOf(movement.from);
  float offset_x = here.x - announced.x;
  float offset_y = here.y - announced.y;
  std::size_t next = 0;

  // Déjà engagé sur le premier pas : on le finit depuis là.
  if (!path.directions.empty()) {
    const std::uint8_t first = path.directions.front();
    const int dx = kDeltaX[first];
    const int dy = kDeltaY[first];
    const float along = (offset_x * static_cast<float>(dx) + offset_y * static_cast<float>(dy)) /
                        static_cast<float>(dx * dx + dy * dy);
    if (along > 0.0F) {
      const FractionalCell target{announced.x + static_cast<float>(dx),
                                  announced.y + static_cast<float>(dy)};
      const float remaining =
          static_cast<float>(StepDurationMs(speed, first)) * std::max(0.0F, 1.0F - along);
      if (visit(Stage{here, target, remaining, first})) {
        return;
      }
      here = target;
      offset_x = 0.0F;
      offset_y = 0.0F;
      next = 1;
    }
  }

  // En retrait de la case annoncée : on la rejoint d'abord, au pas du serveur
  // pour cette direction-là.
  if (offset_x != 0.0F || offset_y != 0.0F) {
    const std::uint8_t back =
        DirectionOf(StepOf(-offset_x), StepOf(-offset_y), actor.position.direction);
    const float fraction = std::max(std::abs(offset_x), std::abs(offset_y));
    const float duration = static_cast<float>(StepDurationMs(speed, back)) * fraction;
    if (visit(Stage{here, announced, duration, back})) {
      return;
    }
    here = announced;
  }

  for (std::size_t step = next; step < path.directions.size(); ++step) {
    const std::uint8_t direction = path.directions[step];
    const FractionalCell target{here.x + static_cast<float>(kDeltaX[direction]),
                                here.y + static_cast<float>(kDeltaY[direction])};
    if (visit(
            Stage{here, target, static_cast<float>(StepDurationMs(speed, direction)), direction})) {
      return;
    }
    here = target;
  }
}

// L'HORLOGE DE LA MARCHE, qui s'arrête à la mort.
//
// Un mort ne bouge plus, et ce n'est pas une règle de rendu : le client refuse
// TOUT changement d'état à qui est déjà mort — le changement de pose rend la
// main dès son entrée quand l'état vaut trois. Sa machine
// à états ne repasse donc plus par le cas de la marche, et son corps reste
// exactement là où la mort l'a pris, au milieu d'une case s'il le faut.
//
// Le déplacement, lui, ne s'efface pas : c'est lui qui porte le dernier regard.
// C'est donc le TEMPS qu'on arrête, et l'échantillonnage du chemin se fige de
// lui-même — position et direction comprises.
std::uint32_t WalkClockMs(const Actor& actor, std::uint32_t now_ms) {
  return actor.posture == Posture::kDead ? std::min(now_ms, actor.posture_since_ms) : now_ms;
}

Located Locate(const Actor& actor, const WalkPath& path, std::uint32_t now_ms) {
  now_ms = WalkClockMs(actor, now_ms);
  if (!actor.movement.has_value()) {
    // Immobile, on se tient où l'on a dit dans sa case : au centre, sauf un
    // objet tombé au sol.
    FractionalCell cell = CellOf(actor.position);
    cell.x += (static_cast<float>(actor.sub_x) - kSubCellCenter) / kSubCellsPerCell;
    cell.y += (static_cast<float>(actor.sub_y) - kSubCellCenter) / kSubCellsPerCell;
    return Located{cell, actor.position.direction, false};
  }
  const CellMovement& movement = *actor.movement;
  Located arrived{CellOf(movement.to),
                  path.directions.empty()
                      ? DirectionTowards(movement.from, movement.to, actor.position.direction)
                      : path.directions.back(),
                  false};
  if (MovingSpeedOf(actor) == 0) {
    // ⚠ MÊME SANS VITESSE, UN DEMI-TOUR REPREND LA MAIN. Ce retour-ci saute le
    // bloc d'en bas, si bien qu'un acteur dont on ne connaît pas encore le pas
    // gardait le regard de son chemin quoi qu'on lui demande — le même « écrit
    // et jamais dessiné » que plus bas, à un autre endroit.
    if (actor.turned_since_walk_began) {
      arrived.facing = actor.position.direction;
    }
    return arrived;
  }
  // Un instant ANTÉRIEUR au départ ne se produit pas en régime normal, mais un
  // rejeu peut en fabriquer un. Le traiter comme le départ vaut mieux qu'un
  // débordement de soustraction non signée, qui projetterait l'acteur très loin.
  float elapsed_ms = now_ms <= actor.movement_started_ms
                         ? 0.0F
                         : static_cast<float>(now_ms - actor.movement_started_ms);
  Located located = arrived;
  bool found = false;
  ForEachStage(actor, path, [&](const Stage& stage) {
    if (elapsed_ms < stage.duration_ms) {
      located =
          Located{Lerp(stage.from, stage.to, elapsed_ms / stage.duration_ms), stage.facing, true};
      found = true;
      return true;
    }
    elapsed_ms -= stage.duration_ms;
    // La dernière étape franchie donne le dernier regard, et la position où
    // le chemin s'arrête — qui est l'arrivée annoncée, sauf chemin tronqué.
    located = Located{stage.to, stage.facing, false};
    return false;
  });
  // ⚠ UN DEMI-TOUR POSTÉRIEUR AU DÉPART REPREND LE REGARD À LA QUEUE DU CHEMIN.
  // Le déplacement ne s'efface pas à l'arrivée, si bien que sans cette reprise
  // le dernier pas dicterait la direction pour toujours. Voir
  // Actor::turned_since_walk_began. Pendant la marche, en revanche, c'est le
  // PAS EN COURS qui regarde : le serveur repose la direction à chaque case.
  if (!located.walking && actor.turned_since_walk_began) {
    located.facing = actor.position.direction;
  }
  static_cast<void>(found);
  return located;
}

float TravelledAlong(const Actor& actor, const WalkPath& path, std::uint32_t now_ms) {
  if (!actor.movement.has_value() || MovingSpeedOf(actor) == 0) {
    return 0.0F;
  }
  now_ms = WalkClockMs(actor, now_ms);
  float elapsed_ms = now_ms <= actor.movement_started_ms
                         ? 0.0F
                         : static_cast<float>(now_ms - actor.movement_started_ms);
  float travelled = 0.0F;
  ForEachStage(actor, path, [&](const Stage& stage) {
    const float dx = stage.to.x - stage.from.x;
    const float dy = stage.to.y - stage.from.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (elapsed_ms < stage.duration_ms) {
      travelled += length * (stage.duration_ms > 0.0F ? elapsed_ms / stage.duration_ms : 1.0F);
      return true;
    }
    elapsed_ms -= stage.duration_ms;
    travelled += length;
    return false;
  });
  return travelled;
}

}  // namespace

float TravelledCells(const Actor& actor, const WalkPath& path, std::uint32_t now_ms) {
  // Ce qui précède le déplacement en cours COMPTE : le compteur du jeu court
  // sur toute la vie de l'acteur, et le remettre à zéro à chaque ordre de
  // marche tronquerait le cycle de l'animation.
  return actor.travelled_before_cells + TravelledAlong(actor, path, now_ms);
}

std::uint8_t DirectionTowards(CellPosition from, CellPosition to, std::uint8_t fallback) {
  return DirectionOf(StepOf(static_cast<int>(to.x) - static_cast<int>(from.x)),
                     StepOf(static_cast<int>(to.y) - static_cast<int>(from.y)), fallback);
}

float ClientAimDegrees(float dx_cells, float dy_cells) noexcept {
  constexpr float kDegreesPerRadian = 180.0F / std::numbers::pi_v<float>;
  constexpr float kFullTurnDegrees = 360.0F;
  // La profondeur du client croît avec la case : l'écart vers le nord y entre
  // en négatif, et le sud vaut zéro.
  float degrees = std::atan2(dx_cells, -dy_cells) * kDegreesPerRadian;
  if (degrees < 0.0F) {
    degrees += kFullTurnDegrees;
  }
  return degrees;
}

std::uint8_t DirectionOfAim(float dx_cells, float dy_cells, std::uint8_t fallback) noexcept {
  if (StepOf(dx_cells) == 0 && StepOf(dy_cells) == 0) {
    return fallback;
  }
  constexpr float kDegreesPerEighth = 45.0F;
  // Le sud vaut zéro chez le client et QUATRE chez le serveur : les deux
  // numérotations diffèrent d'un demi-tour, et tournent en sens inverse l'une
  // de l'autre — ce que le module huit absorbe pour la première et que
  // `ClientAimDegrees` porte déjà pour la seconde.
  constexpr long kSouthOnServerCompass = 4;
  const long eighths = std::lround(ClientAimDegrees(dx_cells, dy_cells) / kDegreesPerEighth);
  return static_cast<std::uint8_t>((eighths + kSouthOnServerCompass) % kDirectionCount);
}

CellPosition StandingCell(const Actor& actor) noexcept {
  return actor.movement.has_value() ? actor.movement->to : actor.position;
}

Facing TurnToward(Facing from, std::uint8_t wanted_direction) noexcept {
  // Un demi-tour : l'écart que RIEN ne couvre, ni le regard ni un pas de corps.
  constexpr std::uint8_t kHalfTurn = kDirectionCount / 2;
  constexpr std::uint8_t kOneEighth = 1;

  const auto wrapped = [](int direction) {
    return static_cast<std::uint8_t>(((direction % kDirectionCount) + kDirectionCount) %
                                     kDirectionCount);
  };
  const std::uint8_t wanted = wrapped(wanted_direction);
  const std::uint8_t body = wrapped(from.direction);
  // De combien de huitièmes le corps est EN AVANCE sur le but, dans le sens où
  // le serveur numérote.
  const std::uint8_t ahead = wrapped(body - wanted);

  if (ahead == 0 || ahead == kHalfTurn) {
    return Facing{wanted, kGazeAhead};
  }
  if (ahead < kHalfTurn) {
    // Le but est du côté des numéros DESCENDANTS.
    if (ahead == kOneEighth) {
      // Le dernier huitième : la tête l'a-t-elle déjà pris ? Si oui, c'est au
      // corps de finir ; sinon elle le prend maintenant, et le corps attend.
      return from.head_direction == kGazeDownward ? Facing{wanted, kGazeAhead}
                                                  : Facing{body, kGazeDownward};
    }
    return Facing{wrapped(wanted + kOneEighth), kGazeDownward};
  }
  // Le but est du côté des numéros MONTANTS.
  if (ahead == kDirectionCount - kOneEighth) {
    return from.head_direction == kGazeUpward ? Facing{wanted, kGazeAhead}
                                              : Facing{body, kGazeUpward};
  }
  return Facing{wrapped(wanted - kOneEighth), kGazeUpward};
}

std::uint32_t MovementDurationMs(const Actor& actor, const WalkPath& path) {
  if (!actor.movement.has_value() || MovingSpeedOf(actor) == 0) {
    return 0;
  }
  float total_ms = 0.0F;
  ForEachStage(actor, path, [&total_ms](const Stage& stage) {
    total_ms += stage.duration_ms;
    return false;
  });
  return static_cast<std::uint32_t>(std::lround(total_ms));
}

CellPosition StoppedCell(const Actor& actor, const WalkPath& path, std::uint32_t now_ms) {
  const Located located = Locate(actor, path, now_ms);
  CellPosition stopped = actor.position;
  stopped.x = static_cast<std::uint16_t>(std::max(0L, std::lround(located.cell.x)));
  stopped.y = static_cast<std::uint16_t>(std::max(0L, std::lround(located.cell.y)));
  // Le regard reste celui du pas en cours : le serveur ne le change pas non
  // plus en arrêtant quelqu'un.
  stopped.direction = located.facing;
  return stopped;
}

FractionalCell InterpolatedCell(const Actor& actor, const WalkPath& path, std::uint32_t now_ms) {
  return Locate(actor, path, now_ms).cell;
}

std::uint8_t FacingAt(const Actor& actor, const WalkPath& path, std::uint32_t now_ms) {
  return Locate(actor, path, now_ms).facing;
}

bool IsWalking(const Actor& actor, const WalkPath& path, std::uint32_t now_ms) {
  return Locate(actor, path, now_ms).walking;
}

namespace {

WalkPath StraightPathOf(const Actor& actor) {
  if (!actor.movement.has_value()) {
    return WalkPath{};
  }
  return StraightWalkPath(actor.movement->from, actor.movement->to);
}

}  // namespace

std::uint32_t MovementDurationMs(const Actor& actor) {
  return MovementDurationMs(actor, StraightPathOf(actor));
}

FractionalCell InterpolatedCell(const Actor& actor, std::uint32_t now_ms) {
  return InterpolatedCell(actor, StraightPathOf(actor), now_ms);
}

FractionalCell PositionAt(const Actor& actor, const WalkMap* map, std::uint32_t at_ms) {
  if (!actor.movement.has_value()) {
    return InterpolatedCell(actor, at_ms);
  }
  const std::optional<WalkPath> found =
      map != nullptr ? FindWalkPath(*map, actor.movement->from, actor.movement->to) : std::nullopt;
  return InterpolatedCell(actor, found.has_value() ? *found : StraightPathOf(actor), at_ms);
}

std::uint8_t FacingAt(const Actor& actor, std::uint32_t now_ms) {
  return FacingAt(actor, StraightPathOf(actor), now_ms);
}

bool IsWalking(const Actor& actor, std::uint32_t now_ms) {
  return IsWalking(actor, StraightPathOf(actor), now_ms);
}

}  // namespace ro::sim
