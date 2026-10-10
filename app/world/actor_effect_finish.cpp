#include "world/actor_effect_finish.h"

#include <utility>
#include <vector>

#include "ro/render/actor_effect_table.h"
#include "ro/render/afterimage_blur.h"
#include "ro/render/effect_frame.h"

namespace moonclient {
namespace {

constexpr auto kTickMs = static_cast<std::uint32_t>(ro::render::kEffectTickMs);

// VRAI QUAND L'EFFET LAISSE SUR SON PORTEUR une trace que l'on relit dans la
// liste, à son âge, après la mort de son instance.
//
// La table des changements d'acteur les porte presque tous : une écriture de
// champ garde sa dernière valeur, une teinte remonte vers le blanc huit par
// tic. Trois numéros agissent hors de la table, par leur seul numéro : la
// toupie et les kunai en étoile font pivoter le corps, la traînée colorée le
// double d'échos.
[[nodiscard]] bool MarksBearer(std::uint32_t effect_id) {
  return ro::render::ActorEffectChangeFor(effect_id) != nullptr ||
         effect_id == ro::render::kSpinningTopEffect || effect_id == ro::render::kKunaiStarEffect ||
         effect_id == ro::render::kAfterimageBlurEffect;
}

}  // namespace

bool EffectFinishedAt(const ro::sim::PlayedEffect& effect, std::uint32_t now_ms) {
  // ⚠ UN EFFET DATÉ DE L'AVENIR N'A PAS COMMENCÉ : l'écart non signé, lui,
  // passerait pour un âge immense.
  if (now_ms < effect.since_ms) {
    return false;
  }
  if (effect.lasting || effect.ends_ms.has_value() || effect.posed_by_state.has_value() ||
      effect.born_of_class || effect.value.has_value() || MarksBearer(effect.id)) {
    return false;
  }
  const std::uint32_t age_ticks = (now_ms - effect.since_ms) / kTickMs;
  return !ro::render::InstanceAliveAt(effect.id, age_ticks);
}

std::size_t ForgetFinishedEffects(ro::sim::World& world, std::uint32_t now_ms) {
  // Ce qu'on ôte se collecte d'abord : on n'ôte pas ce qu'on parcourt.
  std::vector<std::pair<std::uint32_t, std::uint32_t>> finished;
  for (const auto& [id, actor] : world.actors()) {
    for (const ro::sim::PlayedEffect& effect : actor.effects) {
      if (EffectFinishedAt(effect, now_ms)) {
        finished.emplace_back(id, effect.id);
      }
    }
  }
  for (const auto& [id, effect_id] : finished) {
    static_cast<void>(world.ForgetEffect(id, effect_id));
  }
  return finished.size();
}

}  // namespace moonclient
