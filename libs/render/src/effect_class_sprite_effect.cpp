// UN SPRITE, UNE PARTICULE : la classe que décrit `ro/render/sprite_effect_table.h`.
//
// La particule naît à la place de l'instance, dans les couleurs de ses calques,
// et ne se montre qu'à partir de son premier tic. À chaque tic, son âge avance,
// elle reprend les pieds de son porteur relevés de sa hauteur, puis son image
// avance selon sa période. Elle rapetisse avec le corps de son porteur.

#include <memory>

#include "ro/render/sprite_effect_table.h"

#include "effect_class_internal.h"

namespace ro::render::effect_class {
namespace {

class SpriteEffect final : public EffectClassInstance {
 public:
  SpriteEffect(const EffectClassBirth& birth, const SpriteEffectRecipe& recipe)
      : EffectClassInstance(birth), recipe_(recipe) {
    sprite_.sprite = recipe.sprite;
    sprite_.action = recipe.action;
    sprite_.period = recipe.frame_period_ticks;
    sprite_.play = recipe.play == SpriteEffectPlay::kOnce ? SpritePlay::kOnce : SpritePlay::kLoop;
    sprite_.render_word = recipe.render_word;
    sprite_.depth_bias = recipe.depth_bias;
    sprite_.scale = recipe.scale;
    sprite_.keeps_layer_color = true;
    sprite_.position = position_;
    if (recipe.life == SpriteEffectLife::kFixedTicks) {
      duration_ticks_ = kSpriteEffectFixedLifeTicks;
    }
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    switch (recipe_.life) {
      case SpriteEffectLife::kWhileBearer:
        return tick.owner_present;
      case SpriteEffectLife::kUntilPlayed:
        return !sprite_.spent;
      case SpriteEffectLife::kFixedTicks:
        return EffectClassInstance::Alive(tick);
    }
    return false;
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    if (sprite_.age > 0) {
      DrawSprite(sprite_, output);
    }
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    ++sprite_.age;
    const float lift = SpriteEffectLiftUnits(recipe_, tick.owner_job);
    // Le repère du client : ce qui monte a l'ordonnée qui descend.
    sprite_.position = position_ - glm::vec3{0.0F, lift, 0.0F};
    sprite_.owner_scale = tick.owner_shrink_scale;
    if (recipe_.floats_with_bearer) {
      sprite_.screen_lift_pixels = tick.owner_hover_pixels;
    }
    AdvanceSpriteFrame(sprite_, tick);
  }

 private:
  const SpriteEffectRecipe& recipe_;
  SpriteParticle sprite_;
};

}  // namespace

std::unique_ptr<EffectClassInstance> MakeSpriteEffect(const EffectClassBirth& birth,
                                                      const EffectClassView& /*view*/) {
  const SpriteEffectRecipe* recipe = SpriteEffectRecipeFor(birth.effect_id);
  if (recipe == nullptr) {
    return nullptr;
  }
  return std::make_unique<SpriteEffect>(birth, *recipe);
}

}  // namespace ro::render::effect_class
