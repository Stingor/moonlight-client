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

bool PauseOptionsDoor::Follow(bool& window_open, bool open_asked, bool escape,
                              SurvivorsScreen screen) noexcept {
  // Fermée par sa propre croix, la fenêtre n'est plus à la pause.
  if (!window_open) opened_here_ = false;
  if (escape && opened_here_) {
    Release(window_open);
    return true;
  }
  if (open_asked && screen == SurvivorsScreen::kPaused && !opened_here_) {
    open_before_ = window_open;
    window_open = true;
    opened_here_ = true;
  }
  return false;
}

void PauseOptionsDoor::Settle(bool& window_open, SurvivorsScreen screen) noexcept {
  if (!opened_here_ || screen == SurvivorsScreen::kPaused) return;
  Release(window_open);
}

void PauseOptionsDoor::Release(bool& window_open) noexcept {
  // Des options déjà ouvertes dans le jeu le restent : la pause n'a fait que
  // les montrer.
  window_open = open_before_;
  opened_here_ = false;
  open_before_ = false;
}

std::optional<SurvivorsEnding> HubDoor::EndingToShow(bool hub_open, SurvivorsScreen screen,
                                                     const std::filesystem::path& client_root) {
  const bool opened = hub_open && !hub_was_open_;
  hub_was_open_ = hub_open;
  if (!opened || screen != SurvivorsScreen::kClosed) return std::nullopt;
  return TakeInterruptedEnding(client_root);
}

}  // namespace moonclient
