#include "loading/loading_screens.h"

#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/gl_texture.h"
#include "ro/render/sprite_action_frames.h"
#include "ro/ui/loading_screen.h"

#include "game/skin_image_of.h"
#include "loading/loading_art.h"
#include "loading/loading_plans.h"

namespace moonclient {
namespace {

// LE SABLIER D'ATTENTE DU JEU, sa première action ; écrit par nous, en ASCII.
constexpr std::string_view kSpinnerSprite = "data\\sprite\\waitdbsave\\nowloading.spr";
constexpr std::string_view kSpinnerAnimation = "data\\sprite\\waitdbsave\\nowloading.act";
constexpr std::size_t kSpinnerAction = 0;
constexpr float kMillisecondsPerSecond = 1000.0F;

}  // namespace

LoadingScreens::LoadingScreens() : draw_(std::random_device{}()) {}

void LoadingScreens::Draw(UiStage& interface, ro::vfs::Vfs* vfs, ro::render::GameImages* images,
                          const ro::core::LoadProgress* progress, float opacity,
                          std::uint32_t now_ms) {
  FollowArt(images, now_ms);
  EnsureSpinner(vfs);
  last_shown_ms_ = now_ms;
  if (progress == nullptr) {
    step_line_ = ro::core::Tr("Chargement…");
    percent_text_.clear();
  } else {
    if (std::string line = LoadStepLine(*progress); !line.empty()) {
      step_line_ = std::move(line);
    }
    percent_text_ = LoadPercentText(progress->fraction());
  }
  const std::uint32_t elapsed_ms = now_ms - shown_since_ms_;
  const bool art_alive =
      art_ != nullptr && images != nullptr && images->generation() == art_generation_;
  ro::ui::LoadingScreenView view;
  view.backdrop = art_alive ? SkinImageOf(*art_) : ro::ui::SkinImage{};
  view.spinner = SpinnerAt(elapsed_ms);
  view.step_text = step_line_.c_str();
  view.percent_text = percent_text_.c_str();
  view.fraction = progress != nullptr ? progress->fraction() : -1.0F;
  view.opacity = opacity;
  view.clock_s = static_cast<float>(elapsed_ms) / kMillisecondsPerSecond;
  interface.DrawLoadingOnly(view);
}

void LoadingScreens::FollowArt(ro::render::GameImages* images, std::uint32_t now_ms) {
  const bool expired = LoadingArtExpired(last_shown_ms_, now_ms);
  if (expired) {
    // UNE NOUVELLE SUITE D'ÉCRANS : une autre illustration, l'animation à sa
    // première image, et l'étape d'avant oubliée.
    shown_since_ms_ = now_ms;
    step_line_.clear();
    // ⚠ L'ANCIENNE QUITTE LE CACHE : trente-six plein cadre s'y accumuleraient.
    if (art_rank_.has_value() && images != nullptr) {
      images->Forget(LoadingArtPath(*art_rank_));
    }
    art_rank_ = LoadingArtRank(static_cast<std::uint32_t>(draw_()));
    art_ = nullptr;
    art_asked_ = false;
  }
  // ⚠ DEMANDÉE DÈS QUE LE CACHE EXISTE, et redemandée s'il a été vidé : avant
  // le montage des archives il n'y en a pas, et le fond reste noir.
  const bool stale =
      art_ != nullptr && images != nullptr && images->generation() != art_generation_;
  if (images == nullptr || !art_rank_.has_value() || (art_asked_ && !stale)) {
    return;
  }
  art_asked_ = true;
  art_ = &images->GetOpaque(LoadingArtPath(*art_rank_));
  art_generation_ = images->generation();
  if (!art_->usable()) {
    ro::core::LogInfo("écran de chargement : {} absente — fond noir", LoadingArtPath(*art_rank_));
  }
}

void LoadingScreens::EnsureSpinner(ro::vfs::Vfs* vfs) {
  if (spinner_tried_ || vfs == nullptr) {
    return;
  }
  spinner_tried_ = true;
  auto sprite_bytes = vfs->Read(std::string(kSpinnerSprite));
  auto animation_bytes = vfs->Read(std::string(kSpinnerAnimation));
  if (!sprite_bytes || !animation_bytes) {
    ro::core::LogInfo("écran de chargement : sablier absent — des points le remplacent");
    return;
  }
  auto sprite = ro::formats::SprSprite::Parse(*sprite_bytes);
  auto animation = ro::formats::ActAnimation::Parse(*animation_bytes);
  if (!sprite || !animation) {
    ro::core::LogWarning("écran de chargement : sablier illisible — des points le remplacent");
    return;
  }
  const ro::render::ActionFrames frames =
      ro::render::ComposeActionFrames(*sprite, *animation, kSpinnerAction);
  for (const ro::formats::Image& frame : frames.frames) {
    auto texture = ro::render::GlTexture::Upload(frame);
    if (!texture) {
      ro::core::LogWarning("écran de chargement : sablier refusé par la carte graphique — {}",
                           ro::core::Describe(texture.error()));
      spinner_frames_.clear();
      return;
    }
    spinner_frames_.push_back(ro::render::IconTexture{
        .texture = std::move(*texture), .width_px = frame.width, .height_px = frame.height});
  }
  spinner_interval_ms_ = frames.frame_interval_ms;
}

ro::ui::SkinImage LoadingScreens::SpinnerAt(std::uint32_t elapsed_ms) const {
  if (spinner_frames_.empty() || spinner_interval_ms_ <= 0.0F) {
    return {};
  }
  const auto rank =
      static_cast<std::size_t>(static_cast<float>(elapsed_ms) / spinner_interval_ms_) %
      spinner_frames_.size();
  return SkinImageOf(spinner_frames_[rank]);
}

}  // namespace moonclient
