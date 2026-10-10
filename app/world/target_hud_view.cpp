#include "world/target_hud_view.h"

#include <algorithm>
#include <span>

#include "ro/render/sprite_catalog.h"
#include "ro/sim/world.h"

#include "world/actor_view.h"

namespace moonclient {
namespace {

// Ce dont le portrait se retire des bords de son cadre.
constexpr float kPortraitPadding = 3.0F;
constexpr int kDirectionCount = 8;

[[nodiscard]] std::size_t Rank(ro::ui::TargetPart part) { return static_cast<std::size_t>(part); }

}  // namespace

TargetHudView::TargetHudView(const ro::vfs::StatusTable* statuses, ro::render::StatusIcons* icons,
                             ro::render::ActorWardrobe* wardrobe, const ro::vfs::ItemWords& words)
    : wardrobe_(wardrobe), words_(&words), statuses_(statuses, icons) {
  if (wardrobe_ != nullptr) {
    portrait_.emplace(*wardrobe_);
  }
}

void TargetHudView::Refresh(const ro::sim::World& world, const Targeting& targeting,
                            const TargetOptions& options, bool ground_skill_armed,
                            ActorStyles& styles, std::uint32_t now_ms) {
  // Masquée pour cette entité, la fenêtre se conduit comme sans cible.
  facts_ = targeting.enabled() && !targeting.Hidden(world) ? FactsOfTarget(world, *words_)
                                                           : TargetFacts();
  view_ = ro::ui::TargetView();
  view_.present = facts_.present;
  view_.name = facts_.name;
  view_.guild = facts_.guild;
  view_.level = facts_.level;
  view_.is_npc = facts_.is_npc;
  view_.is_player = facts_.is_player;
  view_.ally = facts_.ally;
  view_.health = facts_.health;
  view_.max_health = facts_.max_health;
  view_.magic = facts_.magic;
  view_.max_magic = facts_.max_magic;
  view_.species = facts_.species;
  view_.clickable = targeting.FramesAct(world, options.look().locked, ground_skill_armed);

  const ro::ui::TargetHudLook& look = options.look();
  if (look.parts[Rank(ro::ui::TargetPart::kStatuses)].shown) {
    // L'aperçu court-circuite tout le reste : il sert justement sans cible.
    std::span<const ro::sim::ActiveStatus> told;
    const ro::sim::TargetDetail& detail = world.target_detail();
    // Jamais les miens : la barre d'états les montre déjà.
    if (!options.status_preview() && facts_.present && world.selected_id() != world.player_id() &&
        detail.id == world.selected_id() && detail.statuses_told) {
      told = detail.statuses;
    }
    statuses_.RefreshList(told, now_ms, options.status_preview() ? look.status_most : 0);
    view_.statuses = statuses_.cells();
  }
  if (facts_.present && look.parts[Rank(ro::ui::TargetPart::kPortrait)].shown) {
    RefreshPortrait(world, options, styles, now_ms);
  }
}

void TargetHudView::RefreshPortrait(const ro::sim::World& world, const TargetOptions& options,
                                    ActorStyles& styles, std::uint32_t now_ms) {
  const ro::sim::Actor* actor = world.selected();
  if (actor == nullptr || wardrobe_ == nullptr || !portrait_.has_value()) {
    return;
  }
  const ro::ui::HudRect& rect = options.look().parts[Rank(ro::ui::TargetPart::kPortrait)].rect;
  const auto width = static_cast<std::int32_t>(rect.width - 2.0F * kPortraitPadding);
  const auto height = static_cast<std::int32_t>(rect.height - 2.0F * kPortraitPadding);
  if (width <= 0 || height <= 0) {
    return;
  }
  // L'apparence que la carte montre, couleurs composées comprises.
  ro::render::ActorLook dressed = LookOf(*actor);
  styles.Dress(actor->id, dressed, *wardrobe_, world.styles());
  if (ro::render::DrawnAsModel(wardrobe_->catalog(), dressed)) {
    view_.model = true;
    return;
  }

  const TargetingSettings& settings = options.settings();
  ro::render::ActorVignette::Pose pose;
  pose.look = dressed;
  pose.animation = static_cast<std::size_t>(std::max(0, settings.portrait_animation));
  pose.direction = static_cast<std::size_t>(
      ((settings.portrait_direction % kDirectionCount) + kDirectionCount) % kDirectionCount);
  pose.animated = settings.portrait_animated;
  // Un joueur au repos ou assis regarde droit, animé ou non.
  pose.held_frame = VignetteHeldFrame(*actor, pose.animation);
  pose.elapsed_ms = static_cast<float>(now_ms);
  pose.box.width = width;
  pose.box.height = height;
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
