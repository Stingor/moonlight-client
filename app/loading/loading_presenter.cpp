#include "loading/loading_presenter.h"

#include <array>

#include <SDL3/SDL.h>

#include "ro/core/log.h"

#include "game/client_shell.h"

namespace moonclient {
namespace {

// Le noir sous l'écran : le fondu part de lui.
constexpr std::array<float, 4> kLoadingClear = {0.0F, 0.0F, 0.0F, 1.0F};

[[nodiscard]] std::uint32_t NowMs() { return static_cast<std::uint32_t>(SDL_GetTicks()); }

}  // namespace

LoadingPresenter::LoadingPresenter(ClientShell& shell,
                                   std::span<const ro::core::LoadStepWeight> plan,
                                   std::string_view what, bool already_shown)
    : shell_(shell),
      progress_(plan, this),
      pacing_(NowMs(), already_shown),
      what_(what),
      started_ms_(NowMs()) {
  if (already_shown) {
    Present(started_ms_);
  }
}

LoadingPresenter::~LoadingPresenter() {
  if (!pacing_.shown()) {
    return;
  }
  // LA DERNIÈRE IMAGE DIT OÙ LE CHARGEMENT S'EST ARRÊTÉ — cent pour cent quand
  // il a tout fait — avant que la scène prenne la place.
  const std::uint32_t now_ms = NowMs();
  Present(now_ms);
  ro::core::LogInfo("écran de chargement ({}) : {} image(s), {} ms de présentation sur {} ms",
                    what_, presented_frames_, presenting_ms_, now_ms - started_ms_);
}

void LoadingPresenter::Changed(const ro::core::LoadProgress& /*progress*/) {
  const std::uint32_t now_ms = NowMs();
  if (pacing_.Due(now_ms)) {
    Present(now_ms);
  }
}

void LoadingPresenter::Present(std::uint32_t now_ms) {
  // ⚠ LA FILE SE SERT, ELLE NE SE LIT PAS : voir l'en-tête.
  SDL_PumpEvents();
  if (!quit_noted_ && SDL_HasEvent(SDL_EVENT_QUIT)) {
    quit_noted_ = true;
    ro::core::LogInfo("écran de chargement ({}) : fermeture demandée, elle attend la fin", what_);
  }
  ro::render::GlContext& gl = shell_.gl();
  gl.BindForDisplay();
  gl.ClearBoundTarget(kLoadingClear);
  shell_.DrawLoadingFrame(&progress_, pacing_.Opacity(now_ms), now_ms);
  gl.SwapBuffers();
  pacing_.NotePresented(now_ms);
  ++presented_frames_;
  presenting_ms_ += NowMs() - now_ms;
}

}  // namespace moonclient
