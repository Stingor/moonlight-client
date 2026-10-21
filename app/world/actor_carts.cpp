#include "world/actor_carts.h"

#include <algorithm>

#include "ro/core/log.h"
#include "ro/render/effect_frame.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/terrain_mesh.h"
#include "ro/render/tick_blend.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "world/hit_popup_rules.h"

namespace moonclient {
namespace {

// Au-delà, le temps écoulé depuis la dernière image ne se rattrape plus : une
// fenêtre restée figée une minute rejouerait sinon trois mille tics d'un coup.
constexpr std::uint32_t kMostTicksPerFrame = 60;

// Un point de NOTRE monde, ramené au repère du client : l'ordonnée et la
// profondeur s'y retournent.
ro::render::FalconPoint ClientPoint(const std::array<float, 3>& ours) noexcept {
  return ro::render::FalconPoint{ours[0], -ours[1], -ours[2]};
}

glm::vec3 OurPoint(const ro::render::FalconPoint& client) noexcept {
  return glm::vec3(client.x, -client.y, -client.z);
}

ro::render::FalconMaster MasterAt(const CartFrame& frame, const ro::sim::Actor& actor,
                                  std::uint32_t at_ms) {
  const ro::sim::WalkPath& path = frame.planner->PathFor(actor);
  const ro::sim::FractionalCell cell = ro::sim::InterpolatedCell(actor, path, at_ms);
  return ro::render::FalconMaster{ClientPoint(GroundPositionAt(*frame.stage, cell.x, cell.y)),
                                  ClientFacingDegrees(ro::sim::FacingAt(actor, path, at_ms))};
}

// Les deux fichiers de l'ombre, sous `data\sprite\`.
std::string ShadowPath(std::string_view extension) {
  return std::string(ro::render::kShadowSpritePath) + std::string(extension);
}

}  // namespace

std::int32_t CartTypeOf(const ro::sim::Actor& actor) {
  if (!actor.status_effects.contains(kEfstOnPushCart)) {
    return 0;
  }
  const auto value = actor.status_values.find(kEfstOnPushCart);
  return value == actor.status_values.end() ? 0 : value->second;
}

const ActorCarts::Art* ActorCarts::ArtFor(ro::vfs::Vfs& vfs, std::string_view spr_path,
                                          std::string_view act_path) {
  std::string key = std::string(spr_path) + "|" + std::string(act_path);
  if (const auto found = arts_.find(key); found != arts_.end()) {
    return found->second.get();
  }
  auto art = std::make_unique<Art>();
  auto spr_bytes = vfs.Read(std::string(spr_path));
  auto act_bytes = vfs.Read(std::string(act_path));
  if (spr_bytes && act_bytes) {
    auto sprite = ro::formats::SprSprite::Parse(*spr_bytes);
    auto animation = ro::formats::ActAnimation::Parse(*act_bytes);
    if (sprite && animation) {
      art->sprite = std::move(*sprite);
      art->frame_delay = ro::render::CartFrameDelay(*animation);
      art->animation = std::move(*animation);
    }
  }
  if (!art->sprite.has_value()) {
    ro::core::LogWarning("charrette : {} ou {} illisible — elle ne se dessinera pas", spr_path,
                         act_path);
  }
  return arts_.emplace(std::move(key), std::move(art)).first->second.get();
}

void ActorCarts::ComposeShadow(ro::vfs::Vfs& vfs) {
  if (shadow_art_ != nullptr) {
    return;
  }
  const std::string spr_path = ShadowPath(".spr");
  const std::string act_path = ShadowPath(".act");
  shadow_art_ = ArtFor(vfs, spr_path, act_path);
  if (!shadow_art_->sprite.has_value() || shadow_art_->animation->actions().empty()) {
    return;
  }
  for (const ro::render::SpriteQuad& quad :
       ro::render::BuildFrameQuads(*shadow_art_->sprite, *shadow_art_->animation, 0, 0)) {
    shadow_.sources.push_back(
        ro::render::SpriteImageKey{&*shadow_art_->sprite, nullptr, quad.section, quad.image_index});
    shadow_.quads.push_back(quad);
  }
}

void ActorCarts::Census(const CartFrame& frame) {
  std::erase_if(carts_, [&frame](const auto& kept) {
    const ro::sim::Actor* master = frame.world->Find(kept.first);
    return master == nullptr || !ro::render::CarriesCart(CartTypeOf(*master), master->option) ||
           CartTypeOf(*master) != kept.second.type;
  });
  for (const auto& [id, actor] : frame.world->actors()) {
    const std::int32_t type = CartTypeOf(actor);
    if (!ro::render::CarriesCart(type, actor.option) || carts_.contains(id)) {
      continue;
    }
    // LA CHARRETTE SE CHOISIT À LA NAISSANCE, et ne change plus tant qu'elle
    // vit : un autre type la fait renaître, derrière son maître.
    const std::optional<ro::render::CartArt> wanted =
        ro::render::CartArtFor(type, actor.look.job, actor.option);
    Kept born;
    born.cart = ro::render::BornCart(MasterAt(frame, actor, frame.now_ms));
    born.art =
        wanted.has_value() ? ArtFor(*frame.vfs, wanted->spr_path, wanted->act_path) : nullptr;
    born.type = type;
    carts_.emplace(id, std::move(born));
  }
}

void ActorCarts::Tick(const CartFrame& frame, std::uint32_t tick_ms) {
  const MapStage& stage = *frame.stage;
  const ro::render::FalconGround ground = [&stage](float x, float z) {
    const std::array<float, 2> cell = ro::render::FractionalCellAt(stage.scene.gnd, x, -z);
    return -GroundPositionAt(stage, cell[0], cell[1])[1];
  };
  for (auto& [id, kept] : carts_) {
    const ro::sim::Actor* master = frame.world->Find(id);
    if (master == nullptr || kept.art == nullptr || !kept.art->animation.has_value()) {
      continue;
    }
    const ro::formats::ActAnimation& animation = *kept.art->animation;
    const ro::render::CartFrameCount frame_count = [&animation](std::size_t action) {
      return action < animation.actions().size() ? animation.actions()[action].frames.size()
                                                 : std::size_t{0};
    };
    ro::render::StepCart(kept.cart, MasterAt(frame, *master, tick_ms), ground, frame_count,
                         kept.art->frame_delay, frame.camera_yaw_degrees);
  }
}

std::span<const ro::render::DrawnActor> ActorCarts::Update(const CartFrame& frame) {
  drawn_.clear();
  if (frame.world == nullptr || frame.planner == nullptr || frame.stage == nullptr ||
      frame.vfs == nullptr) {
    return drawn_;
  }
  Census(frame);
  if (carts_.empty()) {
    last_tick_ms_.reset();
    return drawn_;
  }
  ComposeShadow(*frame.vfs);

  const auto tick_ms = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  if (!last_tick_ms_.has_value() || frame.now_ms - *last_tick_ms_ > kMostTicksPerFrame * tick_ms) {
    last_tick_ms_ = frame.now_ms;
    // LE TEMPS NE SE RATTRAPE PAS : la position d'avant le saut ne vaut plus
    // rien, et l'image se peindrait encore à mi-chemin d'elle.
    for (auto& [id, kept] : carts_) {
      kept.cart.previous_position = kept.cart.position;
    }
  }
  while (frame.now_ms - *last_tick_ms_ >= tick_ms) {
    *last_tick_ms_ += tick_ms;
    Tick(frame, *last_tick_ms_);
  }
  const float weight =
      ro::render::TickBlendWeight(frame.now_ms - *last_tick_ms_, tick_ms, frame.blended);

  const std::uint8_t alpha =
      frame.translucent ? ro::render::kCartTranslucentAlpha : ro::render::kCartOpaqueAlpha;
  for (auto& [id, kept] : carts_) {
    kept.frame = ro::render::ComposedActor();
    if (kept.art == nullptr || !kept.art->sprite.has_value()) {
      continue;
    }
    const ro::formats::ActAnimation& animation = *kept.art->animation;
    const std::size_t action = ro::render::CartAction(kept.cart, frame.camera_yaw_degrees);
    if (action >= animation.actions().size() ||
        kept.cart.frame >= animation.actions()[action].frames.size()) {
      continue;
    }
    for (const ro::render::SpriteQuad& quad :
         ro::render::BuildFrameQuads(*kept.art->sprite, animation, action, kept.cart.frame)) {
      kept.frame.sources.push_back(
          ro::render::SpriteImageKey{&*kept.art->sprite, nullptr, quad.section, quad.image_index});
      kept.frame.quads.push_back(quad);
    }
    if (kept.frame.quads.empty()) {
      continue;
    }
    const glm::vec3 placed = OurPoint(ro::render::CartDrawnPosition(kept.cart, weight));
    // 🔴 L'OMBRE N'EST PAS TRANSLUCIDE : elle garde la couleur de son propre
    // calque, quand la charrette prend l'opacité du réglage. Elle part donc à
    // part, AVANT la charrette et au même endroit — le tri, stable, la garde
    // dessous.
    if (frame.round_shadow && !shadow_.quads.empty()) {
      drawn_.push_back(ro::render::DrawnActor{&shadow_, nullptr, placed});
    }
    std::array<std::uint8_t, 4> tint = {255, 255, 255, alpha};
    if (frame.light != nullptr) {
      const std::array<float, 2> cell =
          ro::render::FractionalCellAt(frame.stage->scene.gnd, placed.x, placed.z);
      tint = ro::render::LitTint(tint, frame.light->Under(cell[0], cell[1]), frame.cold_light);
    }
    drawn_.push_back(ro::render::DrawnActor{&kept.frame, nullptr, placed, tint});
  }
  return drawn_;
}

}  // namespace moonclient
