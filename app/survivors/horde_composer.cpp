#include "survivors/horde_composer.h"

#include <algorithm>

#include "ro/render/sprite_frame.h"

#include "survivors/survivors_looks.h"

namespace moonclient {
namespace {

using ro::render::SpriteLayout;

// Le héros et l'inconnu se composent comme des joueurs ; tout le reste est un
// sprite unique, disposé comme une créature.
[[nodiscard]] SpriteLayout LayoutOf(HordeBodyKind kind) noexcept {
  return kind == HordeBodyKind::kHero || kind == HordeBodyKind::kStranger ? SpriteLayout::kPlayer
                                                                          : SpriteLayout::kCreature;
}

// Une gemme et un objet au sol sont des objets ; un mort et une plante, des
// classes, comme un monstre.
[[nodiscard]] bool IsItem(HordeBodyKind kind) noexcept {
  return kind == HordeBodyKind::kGem || kind == HordeBodyKind::kPickup;
}

}  // namespace

HordeComposer::HordeComposer(ro::render::ActorWardrobe& wardrobe) noexcept : wardrobe_(&wardrobe) {}

void HordeComposer::SetAllyLook(std::size_t hero_index, const ro::render::ActorLook& look,
                                const ro::sim::ClassRule& hero_class) {
  if (hero_index + 1U < ally_outfits_.size()) {
    ally_outfits_[hero_index + 1U] = wardrobe_->OutfitFor(look);
    ally_attacks_[hero_index + 1U] = HeroAttackAnimation(wardrobe_->catalog(), hero_class, look);
  }
}

void HordeComposer::SetHeroLook(const ro::render::ActorLook& look,
                                const ro::sim::ClassRule& hero_class) {
  hero_outfit_ = wardrobe_->OutfitFor(look);
  hero_attack_ = HeroAttackAnimation(wardrobe_->catalog(), hero_class, look);
}

void HordeComposer::SetStrangerLook(const ro::render::ActorLook& look) {
  stranger_outfit_ = wardrobe_->OutfitFor(look);
}

std::size_t HordeComposer::HeroAttackRank(std::uint16_t look_id) const noexcept {
  return look_id == 0 || look_id >= ally_attacks_.size() ? hero_attack_ : ally_attacks_[look_id];
}

std::size_t HordeComposer::AnimationOf(const HordePose& pose) const noexcept {
  if (pose.kind == HordeBodyKind::kHero && pose.motion == ro::render::Motion::kAttack) {
    return HeroAttackRank(pose.look_id);
  }
  return ro::render::AnimationFor(pose.motion, LayoutOf(pose.kind));
}

namespace {

// Les durées des motions d'une tenue, lues sur son `.act` ; son attaque se joue
// au rang `attack_animation`.
[[nodiscard]] HordeMotionTimes TimesOfOutfit(const ro::render::ActorOutfit& outfit,
                                             SpriteLayout layout, std::size_t attack_animation) {
  const auto duration = [&](ro::render::Motion motion) {
    return outfit.AnimationDurationMs(ro::render::AnimationFor(motion, layout));
  };
  return HordeMotionTimes{.walk_ms = duration(ro::render::Motion::kWalk),
                          .attack_ms = outfit.AnimationDurationMs(attack_animation),
                          .hurt_ms = duration(ro::render::Motion::kHurt),
                          .die_ms = duration(ro::render::Motion::kDie)};
}

}  // namespace

const HordeComposer::KnownOutfit& HordeComposer::KnownOf(HordeBodyKind kind,
                                                         std::uint16_t look_id) {
  // Un mort a la tenue de son espèce vivante.
  const HordeBodyKind looked = kind == HordeBodyKind::kFalling ? HordeBodyKind::kFoe : kind;
  const bool item = IsItem(looked);
  for (const KnownOutfit& known : known_) {
    if (known.look_id == look_id && IsItem(known.kind) == item) return known;
  }
  ro::render::ActorOutfit* outfit =
      wardrobe_->OutfitFor(item ? ItemLook(look_id) : FoeLook(look_id));
  if (outfit == nullptr) missing_.push_back(HordeLookRef{.kind = looked, .look_id = look_id});
  const std::size_t attack =
      ro::render::AnimationFor(ro::render::Motion::kAttack, SpriteLayout::kCreature);
  known_.push_back(KnownOutfit{
      .kind = looked,
      .look_id = look_id,
      .outfit = outfit,
      .times = outfit != nullptr ? TimesOfOutfit(*outfit, SpriteLayout::kCreature, attack)
                                 : HordeMotionTimes{}});
  return known_.back();
}

ro::render::ActorOutfit* HordeComposer::OutfitOf(HordeBodyKind kind, std::uint16_t look_id) {
  if (kind == HordeBodyKind::kHero) {
    return look_id == 0 || look_id >= ally_outfits_.size() ? hero_outfit_ : ally_outfits_[look_id];
  }
  if (kind == HordeBodyKind::kStranger) return stranger_outfit_;
  return KnownOf(kind, look_id).outfit;
}

HordeMotionTimes HordeComposer::TimesOf(HordeBodyKind kind, std::uint16_t look_id) {
  if (kind == HordeBodyKind::kHero) {
    const ro::render::ActorOutfit* outfit = OutfitOf(kind, look_id);
    return outfit != nullptr
               ? TimesOfOutfit(*outfit, SpriteLayout::kPlayer, HeroAttackRank(look_id))
               : HordeMotionTimes{};
  }
  // L'inconnu se tient immobile : il n'a aucune motion à chronométrer.
  if (kind == HordeBodyKind::kStranger) return HordeMotionTimes{};
  return KnownOf(kind, look_id).times;
}

HordeTimesOf HordeComposer::TimesSource() {
  return [this](HordeBodyKind kind, std::uint16_t look_id) { return TimesOf(kind, look_id); };
}

void HordeComposer::Compose(std::span<const HordePose> poses, const GroundOf& ground_of,
                            HordeShadows shadows) {
  drawn_.clear();
  tags_.clear();
  std::size_t heroes = 0;
  for (const HordePose& pose : poses) {
    ro::render::ActorOutfit* outfit = OutfitOf(pose.kind, pose.look_id);
    if (outfit == nullptr) continue;
    const bool hero = pose.kind == HordeBodyKind::kHero;
    if (hero && drawn_.size() == heroes) ++heroes;
    const std::size_t animation = AnimationOf(pose);
    ro::render::DrawnActor drawn;
    drawn.actor = &outfit->Composed(animation, pose.direction, pose.motion_ms,
                                    ro::render::MotionLoops(pose.motion), pose.held_frame);
    // Sous l'ombre du soleil, qui y entre perd son ombre ronde : sa silhouette
    // en porte une.
    const bool sun_casting = shadows.sun_shadows && (hero || shadows.horde_sun_shadows);
    const ro::render::ComposedActor& round = outfit->Shadow();
    if (CastsRoundShadow(pose) && !sun_casting && !round.quads.empty()) {
      drawn.shadow = &round;
    }
    drawn.ground_position = ground_of(pose);
    drawn.tint = pose.tint;
    drawn.scale = pose.scale;
    drawn_.push_back(drawn);
    tags_.push_back(HordeTag{.kind = pose.kind, .sim_id = pose.sim_id});
  }
  if (!shadows.sun_shadows) {
    sun_casting_ = 0;
  } else {
    sun_casting_ = shadows.horde_sun_shadows ? drawn_.size() : heroes;
  }
}

std::span<const ro::render::DrawnActor> HordeComposer::sun_casters() const noexcept {
  return std::span<const ro::render::DrawnActor>(drawn_).first(sun_casting_);
}

}  // namespace moonclient
