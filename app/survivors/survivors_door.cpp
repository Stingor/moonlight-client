#include "survivors/survivors_door.h"

#include "ro/core/log.h"

#include "survivors/survivors_file.h"

namespace moonclient {
namespace {

// Une partie a commencé à se jouer sur cet écran : la coupure l'interrompt.
[[nodiscard]] bool RunStarted(SurvivorsScreen screen) noexcept {
  switch (screen) {
    case SurvivorsScreen::kArriving:
    case SurvivorsScreen::kPlaying:
    case SurvivorsScreen::kPaused:
    case SurvivorsScreen::kChoosing:
    case SurvivorsScreen::kChest:
    case SurvivorsScreen::kDying:
    case SurvivorsScreen::kWon:
      return true;
    case SurvivorsScreen::kClosed:
    case SurvivorsScreen::kPreparing:
    case SurvivorsScreen::kLoading:
    case SurvivorsScreen::kSummary:
      return false;
  }
  return false;
}

// Le mode tient l'écran : voir `SurvivorsStage::owns_screen`.
[[nodiscard]] bool OwnsScreen(SurvivorsScreen screen) noexcept {
  return screen != SurvivorsScreen::kClosed && screen != SurvivorsScreen::kPreparing;
}

}  // namespace

LinkLossSteps LinkLossStepsOf(SurvivorsScreen screen, bool link_open) noexcept {
  if (link_open || !OwnsScreen(screen)) return {};
  return LinkLossSteps{.freeze = RunStarted(screen), .exit = true};
}

bool FreezeAndSave(SurvivorsStage& stage, const std::filesystem::path& client_root) {
  if (!stage.Freeze()) return false;
  const ro::sim::SurvivorsPlay* play = stage.play();
  const SurvivorsEnding* ending = stage.ending();
  if (play == nullptr || ending == nullptr) return true;
  if (auto saved = SaveInterruptedRun(client_root, play->journal(), *ending); !saved) {
    ro::core::LogWarning("survivors : partie interrompue non écrite — {}",
                         ro::core::Describe(saved.error()));
  }
  return true;
}

SummaryStep SummaryStepOf(bool back, bool replay, bool entry_refused) noexcept {
  if (back) return SummaryStep::kBack;
  if (!replay) return SummaryStep::kStay;
  return entry_refused ? SummaryStep::kBackToPrepare : SummaryStep::kReplay;
}

std::optional<SurvivorsEnding> HubDoor::EndingToShow(bool hub_open, SurvivorsScreen screen,
                                                     const std::filesystem::path& client_root) {
  const bool opened = hub_open && !hub_was_open_;
  hub_was_open_ = hub_open;
  if (!opened || screen != SurvivorsScreen::kClosed) return std::nullopt;
  return TakeInterruptedEnding(client_root);
}

}  // namespace moonclient
