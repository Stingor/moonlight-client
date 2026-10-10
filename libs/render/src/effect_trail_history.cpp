#include "ro/render/effect_trail_history.h"

#include <algorithm>

#include "ro/render/effect_track.h"

namespace ro::render {

void AdvanceEffectTrailHistory(std::vector<EffectTrailStep>& history, const EffectNode& node,
                               glm::vec3 ground_position) {
  if (history.empty()) {
    return;
  }
  const auto how_many = static_cast<float>(history.size());

  // ⚠ DE LA FIN VERS LE DÉBUT, et l'ordre décide de tout. À l'endroit, l'état
  // courant se recopierait dans toutes les cases d'un seul tic, et la traînée
  // deviendrait autant de copies superposées — le même piège que les cascades de
  // pistes, et il se voit aussi peu.
  for (std::size_t rang = history.size() - 1; rang > 0; --rang) {
    history[rang] = history[rang - 1];

    // CHAQUE CRAN PERD SA PART DU CHEMIN. Le client divise par le NOMBRE
    // d'étapes et multiplie par le rang : la dernière est donc presque éteinte,
    // et la traînée s'estompe d'un bout à l'autre plutôt que de s'arrêter net.
    const auto part = static_cast<float>(rang) / how_many;
    history[rang].alpha = std::max(node.alpha - node.alpha * part, 0.0F);
    history[rang].sprite_scale =
        std::max(node.sprite_scale - node.sprite_scale * part * kEffectTrailScaleFadeShare, 0.0F);
  }

  // LA TÊTE PREND L'ÉTAT COURANT, sans rien perdre : c'est le nœud lui-même.
  EffectTrailStep& head = history.front();
  head.ground_position = ground_position;
  head.sprite_scale = node.sprite_scale;
  head.angle_degrees = node.orbit_degrees;
  head.alpha = node.alpha;
  head.color = node.color;
}

void SeedEffectTrailHistory(std::vector<EffectTrailStep>& history, std::int32_t count,
                            const EffectNode& node, glm::vec3 birth_position) {
  if (count <= 1) {
    history.clear();
    return;
  }
  EffectTrailStep birth;
  birth.ground_position = birth_position;
  history.assign(static_cast<std::size_t>(count), birth);
  // Un premier décalage pose les rampes d'alpha et d'échelle ; les positions,
  // toutes égales, ne bougent pas.
  AdvanceEffectTrailHistory(history, node, birth_position);
}

float EffectTrailStepScaleFactor(const EffectTrailStep& step, const EffectNode& node) {
  return node.sprite_scale > 0.0F ? step.sprite_scale / node.sprite_scale : 1.0F;
}

namespace {

// CE QUE LE MOUVEMENT DE LA TRAÎNÉE FAIT À L'ALPHA DU NŒUD.
//
// Il monte de cinq par tic pendant cinquante tics, PLAFONNE à cent cinquante —
// et non à deux cent cinquante-cinq —, puis redescend de cinq pendant les
// cinquante derniers tics de la vie du nœud. Entre les deux il ne bouge plus :
// c'est un éclat qui s'allume, se tient, et s'éteint.
constexpr std::int32_t kTicksOfGlowRise = 50;
constexpr float kAlphaStepPerTick = 5.0F;
constexpr float kAlphaCeiling = 150.0F;
constexpr std::int32_t kTicksOfFadeBeforeEnd = 50;

}  // namespace

void AdvanceFadingTrailNode(std::array<EffectTrack, kEffectTracksPerNode>& tracks, EffectNode& node,
                            std::vector<EffectTrailStep>& history, glm::vec3 ground_position) {
  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    if (track.ticks < kTicksOfGlowRise) {
      node.alpha = std::min(node.alpha + kAlphaStepPerTick, kAlphaCeiling);
    } else if (track.ticks > node.life_ticks - kTicksOfFadeBeforeEnd) {
      node.alpha = std::max(node.alpha - kAlphaStepPerTick, 0.0F);
    }
  }
  AdvanceEffectTrailHistory(history, node, ground_position);
}

}  // namespace ro::render
