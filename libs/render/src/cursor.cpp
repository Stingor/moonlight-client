#include "ro/render/cursor.h"

#include "ro/formats/gat.h"

namespace ro::render {
namespace {

// Les actions du fichier, relues image par image :
//
//   0  la flèche, onze images       9  la main qui se referme, trois images
//   1  la bulle qui grandit        10  l'anneau au sol, vingt images
//   2  la main qui pointe          13  le panneau d'interdiction
//   5  l'épée                     11  le MÊME anneau, vingt images
//   7  la porte qui s'ouvre, dix images
//
// ⚠ DIX ET ONZE SONT LE MÊME DESSIN, image pour image : le fichier les répète,
// et c'est la table de cadence qui les distingue. Voir CursorMotionOf.
//
// Les autres actions servent au redimensionnement d'une fenêtre ; rien ici ne
// les demande encore.
constexpr std::size_t kArrowAction = 0;
constexpr std::size_t kTalkAction = 1;
constexpr std::size_t kPointAction = 2;
constexpr std::size_t kRotateAction = 4;
constexpr std::size_t kAttackAction = 5;
constexpr std::size_t kWarpAction = 7;
constexpr std::size_t kPickUpAction = 9;
constexpr std::size_t kGroundTargetAction = 10;
constexpr std::size_t kTargetLockedAction = 11;
constexpr std::size_t kForbiddenAction = 13;

// Les deux types de cellule que le client refuse au curseur.
constexpr std::int32_t kBlockedCell = 1;
constexpr std::int32_t kBlockedSnipableCell = 5;

}  // namespace

bool CellRefusesWalk(std::int32_t cell_type) {
  const auto base =
      static_cast<std::int32_t>(static_cast<std::uint32_t>(cell_type) & ~formats::kGatWaterFlag);
  return base == kBlockedCell || base == kBlockedSnipableCell;
}

CursorMotion CursorMotionOf(CursorShape shape) {
  // Les facteurs sont ceux du dessin du curseur chez le client, forme par
  // forme. La flèche et la bulle tombent dans le cas par défaut, à quatre tics
  // l'image ; l'épée est la plus lente, à vingt.
  CursorMotion motion;
  motion.action = CursorAction(shape);
  switch (shape) {
    case CursorShape::kTalk:
      motion.ticks_per_frame = 4.0F;
      motion.plays_once = true;
      break;
    case CursorShape::kPoint:
      motion.ticks_per_frame = 4.0F;
      motion.plays_once = true;
      motion.while_pressed = true;
      break;
    case CursorShape::kAttack:
      motion.ticks_per_frame = 20.0F;
      break;
    case CursorShape::kWarp:
      motion.ticks_per_frame = 10.0F;
      break;
    case CursorShape::kPickUp:
      motion.ticks_per_frame = 1.0F;
      motion.plays_once = true;
      motion.while_pressed = true;
      break;
    case CursorShape::kGroundTarget:
      motion.ticks_per_frame = 6.0F;
      break;
    case CursorShape::kTargetLocked:
      motion.ticks_per_frame = 3.0F;
      break;
    case CursorShape::kForbidden:
      motion.ticks_per_frame = 2.0F;
      break;
    case CursorShape::kArrow:
    case CursorShape::kRotate:
      motion.ticks_per_frame = 4.0F;
      break;
  }
  return motion;
}

std::size_t CursorAction(CursorShape shape) {
  switch (shape) {
    case CursorShape::kTalk:
      return kTalkAction;
    case CursorShape::kPoint:
      return kPointAction;
    case CursorShape::kAttack:
      return kAttackAction;
    case CursorShape::kWarp:
      return kWarpAction;
    case CursorShape::kPickUp:
      return kPickUpAction;
    case CursorShape::kGroundTarget:
      return kGroundTargetAction;
    case CursorShape::kTargetLocked:
      return kTargetLockedAction;
    case CursorShape::kForbidden:
      return kForbiddenAction;
    case CursorShape::kRotate:
      return kRotateAction;
    case CursorShape::kArrow:
      break;
  }
  return kArrowAction;
}

}  // namespace ro::render
