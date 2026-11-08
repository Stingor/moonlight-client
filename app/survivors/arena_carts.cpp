#include "survivors/arena_carts.h"

#include <algorithm>
#include <cmath>

#include "ro/core/log.h"
#include "ro/render/companion_cart.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/terrain_mesh.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_state.h"

#include "survivors/arena_camera.h"

namespace moonclient {
namespace {

// LE TYPE DE CHARIOT que le corps montré traîne : le chariot ordinaire, un plus
// beau évolué, le plus beau rené.
constexpr std::int32_t kPlainCartType = 1;
constexpr std::int32_t kEvolvedCartType = 3;
constexpr std::int32_t kRebornCartType = 5;
// Les dernières images que la traînée peut reprendre.
constexpr std::size_t kHistoryFrames = kArenaCartTrailFrames.back() + 1;

[[nodiscard]] std::int32_t CartTypeOf(const ro::sim::ClassRule& hero_class,
                                      const ro::sim::SurvivorsHero& hero) {
  if (ro::sim::Reborn(hero_class, hero.level) && hero_class.reborn_job_id != 0) {
    return kRebornCartType;
  }
  return hero.evolutions > 0 ? kEvolvedCartType : kPlainCartType;
}

// Le cap de la simulation de `from` vers `to`, en cases continues ; `fallback`
// s'ils se confondent.
[[nodiscard]] std::uint8_t HeadingBetween(glm::vec2 from, glm::vec2 to, std::uint8_t fallback) {
  const glm::vec2 delta = (to - from) * static_cast<float>(ro::sim::kSubunitsPerCell);
  return ro::sim::HeadingOf(
      ro::sim::FieldPoint{static_cast<std::int32_t>(delta.x), static_cast<std::int32_t>(delta.y)},
      fallback);
}

// La direction d'écran d'un cap de la simulation, tronquée comme un monstre.
[[nodiscard]] std::size_t ScreenDirectionOf(std::uint8_t heading) {
  return ro::render::SpriteDirectionOfDegrees(ClientDegreesOfHeading(heading), kArenaYawDegrees,
                                              /*rounded=*/false);
}

// L'index du héros d'une pose de héros : 0 pour le héros en tête, sinon celui
// que l'allié nomme.
[[nodiscard]] std::size_t HeroIndexOf(const HordePose& pose) {
  return pose.look_id == 0 ? 0 : pose.look_id - 1U;
}

}  // namespace

void ArenaCarts::Reset() {
  carts_ = {};
  drawn_.clear();
}

const ArenaCarts::Art* ArenaCarts::ArtFor(const std::string& spr_path,
                                          const std::string& act_path) {
  const std::string key = spr_path + "|" + act_path;
  if (const auto found = arts_.find(key); found != arts_.end()) return found->second.get();
  auto art = std::make_unique<Art>();
  auto spr_bytes = vfs_->Read(spr_path);
  auto act_bytes = vfs_->Read(act_path);
  if (spr_bytes && act_bytes) {
    auto sprite = ro::formats::SprSprite::Parse(*spr_bytes);
    auto animation = ro::formats::ActAnimation::Parse(*act_bytes);
    if (sprite && animation) {
      art->frame_delay = ro::render::CartFrameDelay(*animation);
      art->sprite = std::move(*sprite);
      art->animation = std::move(*animation);
    }
  }
  if (!art->sprite.has_value()) {
    ro::core::LogWarning("survivors : chariot {} ou {} illisible — il ne se dessinera pas",
                         spr_path, act_path);
  }
  return arts_.emplace(key, std::move(art)).first->second.get();
}

std::size_t ArenaCarts::FrameOf(const Cart& cart, std::size_t action) const {
  // L'IMAGE SUIT LA DISTANCE PARCOURUE, comme le chariot du jeu : un chariot
  // arrêté ne tourne pas ses roues.
  const auto& actions = cart.art->animation->actions();
  if (action >= actions.size() || actions[action].frames.empty()) return 0;
  const float units = cart.travelled_cells * ro::render::kWorldUnitsPerCell;
  const float step =
      units * ro::render::kCartFramesPerUnit * ro::render::kCartFrameStride / cart.art->frame_delay;
  return static_cast<std::size_t>(static_cast<std::int64_t>(step)) % actions[action].frames.size();
}

void ArenaCarts::Draw(const Art& art, const Shown& shown, std::uint8_t alpha,
                      const GroundOfCell& ground_of) {
  const ro::formats::ActAnimation& animation = *art.animation;
  if (shown.action >= animation.actions().size() ||
      shown.frame >= animation.actions()[shown.action].frames.size()) {
    return;
  }
  auto composed = std::make_unique<ro::render::ComposedActor>();
  for (const ro::render::SpriteQuad& quad :
       ro::render::BuildFrameQuads(*art.sprite, animation, shown.action, shown.frame)) {
    composed->sources.push_back(
        ro::render::SpriteImageKey{&*art.sprite, nullptr, quad.section, quad.image_index});
    composed->quads.push_back(quad);
  }
  if (composed->quads.empty()) return;
  drawn_.push_back(ro::render::DrawnActor{.actor = composed.get(),
                                          .ground_position = ground_of(shown.cell),
                                          .tint = {255, 255, 255, alpha}});
  composed_.push_back(std::move(composed));
}

void ArenaCarts::Compose(const ro::sim::SurvivorsField& field,
                         std::span<const ro::sim::SurvivorsOrb> orbs,
                         std::span<const std::uint16_t> spinning, std::vector<HordePose>& poses,
                         const GroundOfCell& ground_of, bool trails) {
  drawn_.clear();
  composed_.clear();
  std::array<bool, ro::sim::kMostHeroes> posed{};
  for (HordePose& pose : poses) {
    if (pose.kind != HordeBodyKind::kHero) continue;
    const std::size_t index = HeroIndexOf(pose);
    if (index >= carts_.size() || index >= ro::sim::HeroCount(field)) continue;
    const ro::sim::ClassRule* hero_class = ro::sim::ClassAt(field, index);
    const ro::sim::SurvivorsHero& hero = ro::sim::HeroAt(field, index);
    if (hero_class == nullptr || !hero_class->pushes_cart || hero.down) continue;
    posed[index] = true;
    const std::optional<ro::render::CartArt> wanted = ro::render::CartArtFor(
        CartTypeOf(*hero_class, hero), ro::sim::HeroJobOf(*hero_class, hero), 0);
    if (!wanted.has_value()) continue;
    const Art* art = ArtFor(std::string(wanted->spr_path), std::string(wanted->act_path));
    std::optional<Cart>& slot = carts_[index];
    // UN CHARIOT NEUF NAÎT DERRIÈRE SON HÉROS, ou change d'image avec son corps.
    if (!slot.has_value()) {
      slot = Cart{.art = art,
                  .cell = pose.cell - glm::vec2(0.0F, kArenaCartLeashCells),
                  .heading = hero.facing};
    }
    Cart& cart = *slot;
    cart.art = art;
    const auto orb = std::ranges::find_if(orbs, [&](const ro::sim::SurvivorsOrb& candidate) {
      return candidate.hero_index == index &&
             std::ranges::find(spinning, candidate.skill_id) != spinning.end();
    });
    const glm::vec2 before = cart.cell;
    cart.spinning = orb != orbs.end();
    if (cart.spinning) {
      // IL TOURNE : à la place de l'orbe, tangent à son cercle.
      cart.cell = CellOfField(
          glm::vec2(static_cast<float>(orb->position.x), static_cast<float>(orb->position.y)));
      cart.heading = HeadingBetween(before, cart.cell, cart.heading);
      // Le héros le regarde : c'est lui qui le fait tournoyer.
      pose.direction = ScreenDirectionOf(HeadingBetween(pose.cell, cart.cell, hero.facing));
    } else {
      // IL SUIT, au bout de sa longe, tourné vers son héros.
      const glm::vec2 apart = cart.cell - pose.cell;
      const float distance = glm::length(apart);
      if (distance > kArenaCartLeashCells) {
        cart.cell = pose.cell + apart * (kArenaCartLeashCells / distance);
      }
      if (cart.cell != before) cart.heading = HeadingBetween(cart.cell, pose.cell, cart.heading);
    }
    cart.travelled_cells += glm::length(cart.cell - before);
    if (art == nullptr || !art->sprite.has_value()) continue;
    const std::size_t action = ScreenDirectionOf(cart.heading);
    cart.history.insert(cart.history.begin(),
                        Shown{.cell = cart.cell, .action = action, .frame = FrameOf(cart, action)});
    if (cart.history.size() > kHistoryFrames) cart.history.resize(kHistoryFrames);
    // La traînée d'abord, dessous ; le chariot ensuite.
    if (cart.spinning && trails) {
      for (std::size_t ghost = 0; ghost < kArenaCartTrailFrames.size(); ++ghost) {
        if (kArenaCartTrailFrames[ghost] < cart.history.size()) {
          Draw(*art, cart.history[kArenaCartTrailFrames[ghost]], kArenaCartTrailAlpha[ghost],
               ground_of);
        }
      }
    }
    Draw(*art, cart.history.front(), ro::render::kCartOpaqueAlpha, ground_of);
  }
  // Un héros qui n'est plus posé, à terre ou parti, perd son chariot.
  for (std::size_t index = 0; index < carts_.size(); ++index) {
    if (!posed[index]) carts_[index].reset();
  }
}

}  // namespace moonclient
