#include "effects/effect_draw_gate.h"

namespace moonclient {

bool EffectDrawGate::Draws(std::uint32_t bearer_id, bool from_hat) const {
  const auto found = concealed.find(bearer_id);
  if (found == concealed.end() || found->second == BearerGate::kShown) {
    return true;
  }
  if (found->second == BearerGate::kConcealed) {
    return false;
  }
  return !from_hat;
}

bool EffectDrawGate::DrawsTarget(std::uint32_t target_id) const {
  if (!present.contains(target_id)) {
    return false;
  }
  const auto found = concealed.find(target_id);
  return found == concealed.end() || found->second != BearerGate::kConcealed;
}

bool EffectDrawGate::DrawsWindSign(std::uint32_t bearer_id) const {
  return present.contains(bearer_id) && !concealed_bosses.contains(bearer_id);
}

bool EffectDrawGate::DrawsEffect(std::uint32_t effect_id, std::uint32_t bearer_id,
                                 bool from_hat) const {
  return effect_id == kWindSignEffect ? DrawsWindSign(bearer_id) : Draws(bearer_id, from_hat);
}

const ro::render::EffectDye* EffectDrawGate::HatDyeOf(std::uint32_t effect_id,
                                                      std::uint32_t bearer_id,
                                                      bool from_hat) const {
  if (!from_hat || bearer_id == 0) {
    return nullptr;
  }
  const auto found = hat_dyes.find({bearer_id, effect_id});
  return found == hat_dyes.end() ? nullptr : &found->second;
}

}  // namespace moonclient
