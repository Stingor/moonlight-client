#include "ro/ui/seat_layout.h"

#include <algorithm>

namespace ro::ui {
namespace {

// L'ASCII imprimable : de l'espace au tilde.
constexpr unsigned char kPrintableLowest = 0x20;
constexpr unsigned char kPrintableHighest = 0x7e;

}  // namespace

bool ValidSeatFraction(float fraction) noexcept {
  return fraction > kSeatFractionLowest && fraction < kSeatFractionHighest;
}

bool ValidSeatHeight(float height) noexcept {
  return height >= kSeatHeightLowest && height <= kSeatHeightHighest;
}

bool ValidBackgroundPath(std::string_view path) noexcept {
  if (path.empty() || path.size() > kBackgroundPathMax) {
    return false;
  }
  if (path.find("..") != std::string_view::npos || path.find(':') != std::string_view::npos) {
    return false;
  }
  if (path.front() == '\\' || path.front() == '/') {
    return false;
  }
  return std::ranges::all_of(path, [](char letter) {
    const auto byte = static_cast<unsigned char>(letter);
    return byte >= kPrintableLowest && byte <= kPrintableHighest;
  });
}

SeatAnchor* AnchorOf(SeatLayout& layout, std::string_view name, float default_x, float default_y) {
  for (SeatAnchor& anchor : layout.anchors) {
    if (anchor.name == name) {
      return &anchor;
    }
  }
  if (layout.anchors.size() >= kSeatAnchorsMax) {
    return nullptr;
  }
  layout.anchors.push_back(SeatAnchor{.name = std::string(name), .x = default_x, .y = default_y});
  return &layout.anchors.back();
}

void ResetSeatPlacement(SeatLayout& layout) {
  layout.seats = kBanquetSeats;
  // ⚠ LES ANCRES SE VIDENT, elles ne se remettent pas à des valeurs figées
  // ici : chaque bloc porte SA place d'usine là où il se dessine, et se
  // réenregistre à l'image suivante.
  layout.anchors.clear();
}

void ResetSeatLayout(SeatLayout& layout) {
  layout.background = std::string(kBanquetBackground);
  ResetSeatPlacement(layout);
}

void ApplyPoseEverywhere(SeatLayout& layout, const CharacterSeat& source) {
  // ⚠ UNE COPIE, ET NON LA RÉFÉRENCE : `source` est souvent l'un des sièges
  // de la table, et la boucle l'écraserait dès la première place traitée.
  const CharacterSeat pose = source;
  for (CharacterSeat& seat : layout.seats) {
    seat.animation = pose.animation;
    seat.animated = pose.animated;
    seat.head_turn = pose.head_turn;
    seat.direction = pose.direction;
    seat.frame = pose.frame;
  }
}

int SeatHeldFrame(const CharacterSeat& seat, int frame_count) noexcept {
  const bool plays =
      seat.animated && seat.animation != kSeatAnimStand && seat.animation != kSeatAnimSit;
  int held = plays ? kAutomaticFrame : 0;
  if (seat.frame >= 0) {
    held = seat.frame;
  }
  if (seat.head_turn > 0 && seat.frame < 0 && held == 0) {
    held = seat.head_turn;
  }
  if (held >= 0 && frame_count > 0 && held >= frame_count) {
    held = 0;
  }
  return held;
}

const char* PoseLabel(const SeatPoseChoice& choice) { return core::Tr(choice.label); }

const char* HeadTurnLabel(const HeadTurnChoice& choice) { return core::Tr(choice.label); }

const char* BackgroundLabel(const LayoutBackground& background) {
  return background.origin == BackgroundOrigin::kFactory ? core::Tr(kFactoryBackgroundLabel)
                                                         : background.label.c_str();
}

std::string BackgroundPath(const SeatLayout& layout) {
  const std::string_view chosen =
      layout.background.empty() ? kBanquetBackground : std::string_view(layout.background);
  return std::string(kBackgroundRoot) + std::string(chosen);
}

}  // namespace ro::ui
