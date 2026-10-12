#include "world/status_portrait_view.h"

#include <cstddef>

#include <glm/glm.hpp>

#include "ro/render/sprite_catalog.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"

#include "world/actor_view.h"

namespace moonclient {
namespace {

// Un cadre a deux bords par axe, et l'écart se prend des deux côtés.
constexpr float kEdgesPerAxis = 2.0F;

// Ce que le serveur dit d'un niveau, ou rien tant qu'il ne l'a pas dit.
[[nodiscard]] std::optional<std::int32_t> KnownLevel(std::int64_t level) {
  if (level == ro::sim::kUnknownStat || level < 0) {
    return std::nullopt;
  }
  return static_cast<std::int32_t>(level);
}

[[nodiscard]] std::size_t Rank(ro::ui::PortraitPart part) { return static_cast<std::size_t>(part); }

}  // namespace

PortraitHudView::PortraitHudView(ro::render::ActorWardrobe* wardrobe, const ro::vfs::JobNames& jobs)
    : wardrobe_(wardrobe), jobs_(&jobs) {
  if (wardrobe_ != nullptr) {
    portrait_.emplace(*wardrobe_);
  }
}

void PortraitHudView::Refresh(const ro::sim::World& world, const StatusPortraitOptions& options,
                              const ro::ui::StatBarsLook& gauges, ActorStyles& styles,
                              std::uint32_t now_ms, ImVec2 screen) {
  view_ = ro::ui::StatusPortraitView();
  const ro::sim::Actor* self = world.player();
  if (self == nullptr || !options.look().shown) {
    return;
  }
  view_.present = true;
  // LES JAUGES À L'ÉCRAN, sauf l'incantation, qui ne paraît que pendant un
  // sort : s'aligner sur elle, c'est s'aligner sur du vide.
  neighbours_.clear();
  if (gauges.shown) {
    for (std::size_t bar = 0; bar < ro::ui::kStatBarCount; ++bar) {
      if (gauges.bars[bar].shown && bar != static_cast<std::size_t>(ro::ui::StatBar::kCast)) {
        neighbours_.push_back(gauges.bars[bar].rect);
      }
    }
  }
  view_.neighbours = neighbours_;
  view_.name = self->name;
  // LE MÉTIER EXERCÉ, celui que la fiche de personnage nomme : un costume ou
  // une monture changent le corps montré, pas la classe.
  view_.job = jobs_->Find(self->look.job);
  const ro::sim::PlayerStatus& status = world.player_status();
  view_.base_level = KnownLevel(status.base_level);
  view_.job_level = KnownLevel(status.job_level);
  if (options.look().parts[Rank(ro::ui::PortraitPart::kHead)].shown) {
    RefreshPortrait(world, options, styles, now_ms, screen);
  }
}

void PortraitHudView::RefreshPortrait(const ro::sim::World& world,
                                      const StatusPortraitOptions& options, ActorStyles& styles,
                                      std::uint32_t now_ms, ImVec2 screen) {
  const ro::sim::Actor* self = world.player();
  if (self == nullptr || wardrobe_ == nullptr || !portrait_.has_value()) {
    return;
  }
  // LA TAILLE MONTRÉE, bornée par l'écran : celle du fichier peut avoir été
  // éditée à la main.
  const ro::ui::HudRect rect =
      ro::ui::HudOnScreen(options.look().parts[Rank(ro::ui::PortraitPart::kHead)].rect, screen);
  // LA VIGNETTE COUVRE L'INTÉRIEUR DU CADRE : un pixel de moins de chaque côté,
  // ce qui la coupe là où le cadre s'arrête.
  const auto width =
      static_cast<std::int32_t>(rect.width - kEdgesPerAxis * ro::ui::kPortraitImageInset);
  const auto height =
      static_cast<std::int32_t>(rect.height - kEdgesPerAxis * ro::ui::kPortraitImageInset);
  if (width <= 0 || height <= 0) {
    return;
  }
  // L'apparence que la carte montre, couleurs composées comprises.
  ro::render::ActorLook dressed = LookOf(*self);
  styles.Dress(self->id, dressed, *wardrobe_, world.styles());
  if (ro::render::DrawnAsModel(wardrobe_->catalog(), dressed)) {
    view_.model = true;
    return;
  }
  const PortraitFramingSettings framing = BoundedFraming(options.framing());
  // LA CAPE ENCOMBRE UN PORTRAIT SERRÉ : la tête seule la retire toujours.
  if (framing.head_only || !framing.garment) {
    dressed.robe = 0;
  }

  ro::render::ActorVignette::Pose pose;
  pose.look = dressed;
  pose.animation = static_cast<std::size_t>(framing.animation);
  pose.direction = static_cast<std::size_t>(framing.direction);
  pose.animated = framing.animated;
  // Au repos ou assis, le portrait regarde droit, animé ou non.
  pose.held_frame = VignetteHeldFrame(*self, pose.animation);
  pose.elapsed_ms = static_cast<float>(now_ms);
  pose.box.width = width;
  pose.box.height = height;
  pose.aim = ro::render::PortraitAim{.head_only = framing.head_only,
                                     .zoom = framing.zoom,
                                     .offset = glm::vec2(framing.offset_x, framing.offset_y),
                                     .inset_px = ro::ui::kPortraitImageInset};
  const ro::render::IconTexture& drawn = portrait_->Get(pose);
  if (!drawn.usable()) {
    return;
  }
  // La taille posée est celle de la boîte : la texture est en pixels de sprite,
  // et c'est la carte qui l'agrandit.
  view_.portrait = ro::ui::SkinImage{.id = static_cast<ImTextureID>(drawn.texture.name()),
                                     .width = static_cast<float>(width),
                                     .height = static_cast<float>(height)};
}

}  // namespace moonclient
