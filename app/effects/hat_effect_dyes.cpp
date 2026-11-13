#include "effects/hat_effect_dyes.h"

#include "ro/sim/style.h"
#include "ro/sim/world.h"

#include "effects/effect_view.h"

namespace moonclient {
namespace {

// Le profil d'un effet de chapeau, celui du fil : un seul réglage, au rang 0.
constexpr std::uint8_t kHatEffectProfile = 4;

}  // namespace

std::vector<WornHatEffectDye> WornHatEffectDyes(const ro::sim::World& world) {
  std::vector<WornHatEffectDye> worn;
  for (const auto& [id, actor] : world.actors()) {
    if (actor.hat_effects.empty()) {
      continue;
    }
    const std::vector<ro::sim::StyleLayerRecipe>* layers = world.styles().FindLayers(id);
    for (const ro::sim::HatEffectId number : actor.hat_effects) {
      WornHatEffectDye line{.bearer_id = id, .hat_effect_id = number};
      if (layers != nullptr) {
        for (const ro::sim::StyleLayerRecipe& recipe : *layers) {
          if (recipe.layer != ro::sim::StyleLayerKind::kHatEffect || recipe.key != number ||
              recipe.profile != kHatEffectProfile) {
            continue;
          }
          // « Assombrir » voyage dans l'octet de l'absolu.
          const ro::sim::StyleAdjust& adjust = recipe.adjusts[0];
          line.dye = ro::render::MakeEffectDye(adjust.hue, adjust.saturation, adjust.value,
                                               adjust.absolute);
          break;
        }
      }
      worn.push_back(line);
    }
  }
  return worn;
}

void NoticeHatEffectDyes(EffectsInPlay& effects, const ro::sim::World& world,
                         std::span<const WornHatEffectDye> previews) {
  for (const WornHatEffectDye& line : WornHatEffectDyes(world)) {
    effects.SetHatEffectDye(line.bearer_id, line.hat_effect_id, line.dye);
  }
  for (const WornHatEffectDye& line : previews) {
    effects.SetHatEffectDye(line.bearer_id, line.hat_effect_id, line.dye);
  }
}

}  // namespace moonclient
