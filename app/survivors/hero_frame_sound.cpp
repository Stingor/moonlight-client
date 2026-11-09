#include "survivors/hero_frame_sound.h"

#include "ro/vfs/encoding.h"

namespace moonclient {
namespace {

// L'action dans les bits hauts, l'image dans les bas.
constexpr unsigned kActionShift = 32;

}  // namespace

std::optional<std::string> HeroFrameSound::Hear(const ro::render::ComposedActor* hero) {
  if (hero == nullptr) {
    seen_.reset();
    return std::nullopt;
  }
  const std::uint64_t witness = (static_cast<std::uint64_t>(hero->action_index) << kActionShift) |
                                static_cast<std::uint64_t>(hero->frame_index);
  const bool changed = seen_ != witness;
  seen_ = witness;
  if (!changed || hero->sound.empty()) return std::nullopt;
  // LE .act DONNE SES OCTETS BRUTS, en CP949.
  return ro::vfs::Cp949ToUtf8Lossy(hero->sound);
}

}  // namespace moonclient
