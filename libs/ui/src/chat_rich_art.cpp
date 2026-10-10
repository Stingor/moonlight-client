#include "ro/ui/chat_rich_art.h"

namespace ro::ui {
namespace {

// UN DÉLAI TROP COURT COMPTE POUR UN DÉLAI ORDINAIRE, comme dans les
// navigateurs : beaucoup de GIF portent 0 ou 10 ms par image, et joués tels
// quels ils défileraient à la vitesse de l'écran — ou pas du tout, une
// animation de durée nulle n'ayant pas d'image courante.
constexpr std::uint32_t kShortestHonoredDelayMs = 20;
constexpr std::uint32_t kShortDelayShownMs = 100;

[[nodiscard]] std::uint64_t ShownDelayMs(const ChatPictureFrame& frame) {
  return frame.delay_ms < kShortestHonoredDelayMs ? kShortDelayShownMs : frame.delay_ms;
}

}  // namespace

std::size_t ChatPictureFrameAt(std::span<const ChatPictureFrame> frames, std::uint64_t elapsed_ms) {
  if (frames.size() <= 1) {
    return 0;
  }
  std::uint64_t loop_ms = 0;
  for (const ChatPictureFrame& frame : frames) {
    loop_ms += ShownDelayMs(frame);
  }
  // EN BOUCLE : une vignette jouée une fois s'arrêterait avant qu'on la voie.
  std::uint64_t left_ms = elapsed_ms % loop_ms;
  for (std::size_t rank = 0; rank < frames.size(); ++rank) {
    const std::uint64_t shown_ms = ShownDelayMs(frames[rank]);
    if (left_ms < shown_ms) {
      return rank;
    }
    left_ms -= shown_ms;
  }
  return frames.size() - 1;
}

}  // namespace ro::ui
