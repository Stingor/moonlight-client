#include "ro/render/effect_cycling_skirt.h"

#include "ro/render/effect_track.h"

namespace ro::render {

std::vector<EffectWorldQuad> ComposeCyclingSkirtQuads(const EffectTrack& track,
                                                      std::uint8_t variant,
                                                      std::size_t texture_count, glm::vec3 anchor,
                                                      const EffectRingOrientation& orientation) {
  std::vector<EffectWorldQuad> quads = ComposeEffectTrackSkirt(track, variant, anchor, orientation);
  if (texture_count == 0) {
    return {};
  }
  // ⚠ LE COMPTEUR PEUT ÊTRE NÉGATIF — le bâtisseur recule le départ d'une piste
  // pour l'échelonner —, et le reste d'une division entière garde le signe en
  // C++ comme en C. L'index sortirait alors du tableau d'images. Le client ne
  // rencontre pas le cas parce que sa figure ne dessine pas une piste dont le
  // compte n'est pas remonté ; on le ramène quand même, faute de quoi la borne
  // dépendrait d'un détail du bâtisseur.
  const auto ticks = static_cast<std::size_t>(track.ticks < 0 ? 0 : track.ticks);
  const std::size_t image = ticks % texture_count;
  for (EffectWorldQuad& quad : quads) {
    quad.texture = image;
  }
  return quads;
}

}  // namespace ro::render
