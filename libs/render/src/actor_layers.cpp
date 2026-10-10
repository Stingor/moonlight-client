#include "ro/render/actor_layers.h"

#include <algorithm>
#include <array>

namespace ro::render {
namespace {

// Ordre d'émission de face, puis de dos. Seule la place du bouclier diffère.
constexpr std::array<ActorPart, kActorPartCount> kFrontEmission = {
    ActorPart::kBody,           ActorPart::kHead,        ActorPart::kHeadgearLow,
    ActorPart::kHeadgearMiddle, ActorPart::kHeadgearTop, ActorPart::kGarment,
    ActorPart::kWeapon,         ActorPart::kWeaponTrail, ActorPart::kShield};

constexpr std::array<ActorPart, kActorPartCount> kBackEmission = {
    ActorPart::kShield,      ActorPart::kBody,           ActorPart::kHead,
    ActorPart::kHeadgearLow, ActorPart::kHeadgearMiddle, ActorPart::kHeadgearTop,
    ActorPart::kGarment,     ActorPart::kWeapon,         ActorPart::kWeaponTrail};

// Les directions 2 à 5 montrent le dos.
constexpr std::size_t kFirstBackDirection = 2;
constexpr std::size_t kLastBackDirection = 5;

// Pas des clés de priorité. Il place les coiffes et la cape au-dessus de toute
// clé de séquence, dont la plus grande vaut le nombre de parties.
constexpr int kPriorityStep = 100;
// La clé de la cape mise derrière : avant toute clé de séquence, dont la
// première vaut un.
constexpr int kGarmentBehindKey = 0;

// DE FACE, CE QUE LES MAINS TIENNENT PASSE DEVANT LES COIFFES ET LA CAPE :
// l'arme, puis la seconde arme d'un ambidextre, puis le bouclier. Au-dessus de
// toute clé de priorité, dont la plus grande est celle de la cape.
constexpr int kFrontWeaponKey = 5 * kPriorityStep;
constexpr int kFrontOffHandKey = kFrontWeaponKey + 1;
constexpr int kFrontShieldKey = kFrontWeaponKey + 2;

// La clé de retri d'une pièce prioritaire, ou zéro si elle n'en est pas une.
int PriorityKey(ActorPart part) {
  switch (part) {
    case ActorPart::kHeadgearLow:
      return 1 * kPriorityStep;
    case ActorPart::kHeadgearMiddle:
      return 2 * kPriorityStep;
    case ActorPart::kHeadgearTop:
      return 3 * kPriorityStep;
    case ActorPart::kGarment:
      return 4 * kPriorityStep;
    default:
      return 0;
  }
}

// La clé de ce que les mains tiennent, vu de face, ou zéro. La traînée ne
// monte que lorsqu'elle porte la seconde arme d'un ambidextre.
int FrontHeldKey(ActorPart part, bool off_hand_weapon) {
  switch (part) {
    case ActorPart::kWeapon:
      return kFrontWeaponKey;
    case ActorPart::kWeaponTrail:
      return off_hand_weapon ? kFrontOffHandKey : 0;
    case ActorPart::kShield:
      return kFrontShieldKey;
    default:
      return 0;
  }
}

}  // namespace

bool IsBackView(std::size_t direction) noexcept {
  const std::size_t group = direction % kDirectionCount;
  return group >= kFirstBackDirection && group <= kLastBackDirection;
}

bool GarmentPlacement::DrawsBehind(std::size_t action_index, std::size_t frame_index) const {
  const auto found = behind_frames.find(action_index);
  return found != behind_frames.end() &&
         std::ranges::find(found->second, frame_index) != found->second.end();
}

std::vector<ActorPart> PartDrawOrder(std::span<const ActorPart> present, std::size_t direction,
                                     bool garment_behind, bool off_hand_weapon) {
  const bool back = IsBackView(direction);
  const auto& emission = back ? kBackEmission : kFrontEmission;

  // Chaque partie présente reçoit sa clé : celle de sa priorité pour les
  // coiffes et la cape, celle de son rang d'émission pour les autres.
  struct Keyed {
    ActorPart part;
    int key;
  };
  std::vector<Keyed> keyed;
  keyed.reserve(present.size());

  // Mise derrière, la cape précède tout : le jeu la dessine dans une passe à
  // part, avant l'acteur entier, bouclier compris.
  if (garment_behind && std::ranges::find(present, ActorPart::kGarment) != present.end()) {
    keyed.push_back(Keyed{ActorPart::kGarment, kGarmentBehindKey});
  }
  int sequence = 0;
  for (const ActorPart part : emission) {
    if (std::ranges::find(present, part) == present.end() ||
        (garment_behind && part == ActorPart::kGarment)) {
      continue;
    }
    const int held = back ? 0 : FrontHeldKey(part, off_hand_weapon);
    const int priority = held != 0 ? held : PriorityKey(part);
    keyed.push_back(Keyed{part, priority != 0 ? priority : ++sequence});
  }

  std::ranges::stable_sort(
      keyed, [](const Keyed& left, const Keyed& right) { return left.key < right.key; });

  std::vector<ActorPart> order;
  order.reserve(keyed.size());
  for (const Keyed& entry : keyed) {
    order.push_back(entry.part);
  }
  return order;
}

}  // namespace ro::render
