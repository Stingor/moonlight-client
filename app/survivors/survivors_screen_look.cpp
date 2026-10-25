#include "survivors/survivors_screen_look.h"

namespace moonclient {

float SurvivorsDesaturation(SurvivorsScreen screen, bool fell) noexcept {
  switch (screen) {
    case SurvivorsScreen::kPaused:
    case SurvivorsScreen::kChoosing:
    case SurvivorsScreen::kChest:
      return kSurvivorsGreyedDesaturation;
    case SurvivorsScreen::kDying:
      return kSurvivorsDeathDesaturation;
    case SurvivorsScreen::kSummary:
      return fell ? kSurvivorsDeathDesaturation : kSurvivorsGreyedDesaturation;
    case SurvivorsScreen::kClosed:
    case SurvivorsScreen::kPreparing:
    case SurvivorsScreen::kLoading:
    case SurvivorsScreen::kArriving:
    case SurvivorsScreen::kPlaying:
    case SurvivorsScreen::kWon:
      return 0.0F;
  }
  return 0.0F;
}

bool SurvivorsMusicPaused(SurvivorsScreen screen) noexcept {
  return screen == SurvivorsScreen::kPaused;
}

bool SurvivorsArenaShown(SurvivorsScreen screen) noexcept {
  switch (screen) {
    case SurvivorsScreen::kArriving:
    case SurvivorsScreen::kPlaying:
    case SurvivorsScreen::kPaused:
    case SurvivorsScreen::kChoosing:
    case SurvivorsScreen::kChest:
    case SurvivorsScreen::kDying:
    case SurvivorsScreen::kWon:
    case SurvivorsScreen::kSummary:
      return true;
    case SurvivorsScreen::kClosed:
    case SurvivorsScreen::kPreparing:
    case SurvivorsScreen::kLoading:
      return false;
  }
  return false;
}

}  // namespace moonclient
