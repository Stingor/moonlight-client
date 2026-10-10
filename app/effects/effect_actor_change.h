#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

#include "ro/render/actor_effect_table.h"
#include "ro/render/effect_class.h"
#include "ro/render/effect_duration.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_table.h"

namespace moonclient {

// UN EFFET QUI NE FAIT QUE CHANGER SON PORTEUR : ni fichier, ni nœuds, ni
// classe, mais une entrée de la table des changements d'acteur. Le client le
// crée comme un autre, dans la liste d'effets de l'acteur, et c'est la mise à
// jour de cette instance qui, à chaque tic, lève son drapeau de rendu, teint
// le corps ou sème ses copies. Le jouer comme un dessin ne montrerait rien.
[[nodiscard]] inline bool ChangesOnlyItsBearer(std::uint32_t effect_id) {
  return ro::render::ActorEffectChangeFor(effect_id) != nullptr &&
         ro::render::EffectFile(effect_id).empty() &&
         !ro::render::KnowsProceduralEffect(effect_id) &&
         !ro::render::IsPortedEffectClass(effect_id);
}

// COMBIEN DE TEMPS L'INSTANCE VIT D'ELLE-MÊME, ou rien quand seule la levée de
// ce qui l'a posée l'éteint — les copies qui rembobinent leur âge un tic avant
// leur borne sont de celles-là. Le mot de rendu de l'acteur se refait à chaque
// image avec les instances vivantes : une instance morte n'y lève plus rien,
// que sa cause tienne encore ou non.
[[nodiscard]] inline std::optional<std::uint64_t> OwnLifeMs(std::uint32_t effect_id) {
  if (!ro::render::EffectDurationIsKnown(effect_id) ||
      std::ranges::find(ro::render::kAgeRewindingActorEffects, effect_id) !=
          ro::render::kAgeRewindingActorEffects.end()) {
    return std::nullopt;
  }
  const std::int32_t life_ticks = ro::render::EffectDurationTicks(effect_id);
  if (life_ticks <= 0 || life_ticks >= ro::render::kEffectLifeForever) {
    return std::nullopt;
  }
  return static_cast<std::uint64_t>(life_ticks) *
         static_cast<std::uint64_t>(ro::render::kEffectTickMs);
}

}  // namespace moonclient
