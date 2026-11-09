#include "survivors/arena_falcons.h"

#include <algorithm>
#include <ranges>

#include "ro/core/log.h"
#include "ro/render/effect_frame.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/tick_blend.h"
#include "ro/sim/survivors_state.h"

#include "survivors/arena_camera.h"
#include "survivors/survivors_looks.h"

namespace moonclient {
namespace {

// Au-delà, le temps écoulé ne se rattrape plus : l'oiseau reprend où il en était.
constexpr std::uint32_t kMostTicksPerFrame = 60;

// Un point de NOTRE monde, dans le repère du client : l'ordonnée et la
// profondeur s'y retournent.
[[nodiscard]] ro::render::FalconPoint ClientPoint(glm::vec3 ours) noexcept {
  return ro::render::FalconPoint{ours.x, -ours.y, -ours.z};
}

[[nodiscard]] glm::vec3 OurPoint(const ro::render::FalconPoint& client) noexcept {
  return glm::vec3(client.x, -client.y, -client.z);
}

// Vrai pour un skill dont le coup envoie le faucon.
[[nodiscard]] bool SendsFalcon(std::uint16_t skill_id) noexcept {
  return skill_id == kBlitzBeatSkill || skill_id == kFalconAssaultSkill;
}

// L'index du héros d'une pose de héros : 0 pour le héros en tête, sinon celui
// que l'allié nomme.
[[nodiscard]] std::size_t HeroIndexOf(const HordePose& pose) {
  return pose.look_id == 0 ? 0 : pose.look_id - 1U;
}

}  // namespace

void ArenaFalcons::Reset() {
  falcons_ = {};
  drawn_.clear();
  last_tick_ms_.reset();
}

const ArenaFalcons::Art* ArenaFalcons::ArtFor(const ro::render::FalconArt& wanted) {
  const std::string key = std::string(wanted.spr_path) + "|" + std::string(wanted.act_path);
  if (const auto found = arts_.find(key); found != arts_.end()) return found->second.get();
  auto art = std::make_unique<Art>();
  auto spr_bytes = vfs_->Read(std::string(wanted.spr_path));
  auto act_bytes = vfs_->Read(std::string(wanted.act_path));
  if (spr_bytes && act_bytes) {
    auto sprite = ro::formats::SprSprite::Parse(*spr_bytes);
    auto animation = ro::formats::ActAnimation::Parse(*act_bytes);
    if (sprite && animation) {
      art->sprite = std::move(*sprite);
      art->animation = std::move(*animation);
    }
  }
  if (!art->sprite.has_value()) {
    ro::core::LogWarning("survivors : faucon {} ou {} illisible — il ne se dessinera pas",
                         wanted.spr_path, wanted.act_path);
  }
  return arts_.emplace(key, std::move(art)).first->second.get();
}

void ArenaFalcons::Compose(const ro::sim::SurvivorsField& field,
                           std::span<const ro::sim::SurvivorsCast> casts,
                           std::span<const HordePose> poses, const GroundOfCell& ground_of,
                           const HeightAt& height_at, std::uint32_t now_ms) {
  drawn_.clear();
  // LES MAÎTRES : chaque héros posé, debout, qui tient un skill de faucon.
  std::array<bool, ro::sim::kMostHeroes> posed{};
  for (const HordePose& pose : poses) {
    if (pose.kind != HordeBodyKind::kHero) continue;
    const std::size_t index = HeroIndexOf(pose);
    if (index >= falcons_.size() || index >= ro::sim::HeroCount(field)) continue;
    const ro::sim::SurvivorsHero& hero = ro::sim::HeroAt(field, index);
    const ro::sim::ClassRule* hero_class = ro::sim::ClassAt(field, index);
    const bool holds = std::ranges::any_of(
        hero.skills, [](const ro::sim::OwnedSkill& owned) { return SendsFalcon(owned.skill_id); });
    if (hero_class == nullptr || hero.down || !holds) continue;
    posed[index] = true;
    const ro::render::FalconMaster master{.feet = ClientPoint(ground_of(pose.cell)),
                                          .heading_degrees = ClientDegreesOfHeading(hero.facing)};
    std::optional<Kept>& slot = falcons_[index];
    if (!slot.has_value()) {
      slot =
          Kept{.falcon = ro::render::BornFalcon(master),
               .art = ArtFor(ro::render::PlayerFalconArtFor(ro::sim::HeroJobOf(*hero_class, hero))),
               .frame = std::make_unique<ro::render::ComposedActor>()};
    }
    slot->master = master;
  }
  for (std::size_t index = 0; index < falcons_.size(); ++index) {
    if (!posed[index]) falcons_[index].reset();
  }
  // LE PIQUÉ : chaque coup de faucon envoie l'oiseau de son héros vers le point
  // visé ; plusieurs dans l'image, vers le dernier.
  for (const ro::sim::SurvivorsCast& cast : casts) {
    if (!SendsFalcon(cast.skill_id) || cast.hero_index >= falcons_.size()) continue;
    std::optional<Kept>& slot = falcons_[cast.hero_index];
    if (!slot.has_value()) continue;
    ro::render::LaunchFalcon(slot->falcon,
                             ClientPoint(ground_of(CellOfField(glm::vec2(
                                 static_cast<float>(cast.to.x), static_cast<float>(cast.to.y))))));
  }
  // LES TICS DU FAUCON, à l'horloge du client.
  const auto tick_ms = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  if (!last_tick_ms_.has_value() || now_ms - *last_tick_ms_ > kMostTicksPerFrame * tick_ms) {
    last_tick_ms_ = now_ms;
    for (std::optional<Kept>& slot : falcons_) {
      if (slot.has_value()) slot->falcon.previous_position = slot->falcon.position;
    }
  }
  const ro::render::FalconGround ground = [&height_at](float x, float z) {
    return -height_at(x, -z);
  };
  while (now_ms - *last_tick_ms_ >= tick_ms) {
    *last_tick_ms_ += tick_ms;
    for (std::optional<Kept>& slot : falcons_) {
      if (!slot.has_value()) continue;
      const ro::formats::ActAnimation* animation =
          slot->art != nullptr && slot->art->animation.has_value() ? &*slot->art->animation
                                                                   : nullptr;
      const ro::render::FalconFrameCount frame_count = [animation](std::size_t action) {
        if (animation == nullptr || action >= animation->actions().size()) return std::size_t{0};
        return animation->actions()[action].frames.size();
      };
      ro::render::StepFalcon(slot->falcon, slot->master, ground, frame_count, camera_yaw_degrees_);
    }
  }
  const float weight = ro::render::TickBlendWeight(now_ms - *last_tick_ms_, tick_ms, true);
  for (std::optional<Kept>& slot : falcons_) {
    if (!slot.has_value() || slot->art == nullptr || !slot->art->sprite.has_value()) continue;
    const ro::formats::ActAnimation& animation = *slot->art->animation;
    const std::size_t action = ro::render::FalconAction(slot->falcon, camera_yaw_degrees_);
    if (action >= animation.actions().size() ||
        slot->falcon.frame >= animation.actions()[action].frames.size()) {
      continue;
    }
    *slot->frame = ro::render::ComposedActor();
    for (const ro::render::SpriteQuad& quad :
         ro::render::BuildFrameQuads(*slot->art->sprite, animation, action, slot->falcon.frame)) {
      slot->frame->sources.push_back(
          ro::render::SpriteImageKey{&*slot->art->sprite, nullptr, quad.section, quad.image_index});
      slot->frame->quads.push_back(quad);
    }
    if (slot->frame->quads.empty()) continue;
    drawn_.push_back(ro::render::DrawnActor{
        .actor = slot->frame.get(),
        .ground_position = OurPoint(ro::render::FalconDrawnPosition(slot->falcon, weight))});
  }
}

}  // namespace moonclient
