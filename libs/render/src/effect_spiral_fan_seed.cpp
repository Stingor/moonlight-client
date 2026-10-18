#include "ro/render/effect_spiral_fan_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_skill_icon.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LES CASES QUE LE BÂTISSEUR REMPLIT.
constexpr std::size_t kSlotOfSideways = 0;
constexpr std::size_t kSlotOfFirstBlade = 1;
constexpr std::size_t kBladeCount = 10;
constexpr std::size_t kMarkOfTexture = 0;
constexpr std::size_t kMarkOfStyle = 4;

}  // namespace

const EffectSpiralFanStyleSeed* EffectSpiralFanStyleSeedAt(std::size_t seed) {
  const std::span<const EffectSpiralFanStyleSeed> table = EffectSpiralFanStyleSeedTable();
  return seed < table.size() ? &table[seed] : nullptr;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSpiralFanTracks(
    const EffectSpiralFanStyleSeed& seed) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  // ⚠ UNE SEULE EST VIVE, et les trois autres le restent : aucun mouvement ne
  // les rallume. Les allumer donnerait quatre éventails superposés.
  EffectTrack& single = tracks.front();
  single.active = true;
  single.ticks = 0;
  // ⚠ L'ALPHA ET LES DIX LAMES NAISSENT NULS. L'éventail s'ouvre lame par lame
  // sous le mouvement ; le semer brillant lui retirerait son ouverture.
  single.alpha = 0;
  single.value[kSlotOfSideways] = seed.sideways;
  for (std::size_t lame = 0; lame < kBladeCount; ++lame) {
    single.value[kSlotOfFirstBlade + lame] = 0.0F;
  }
  single.radius_at_anchor = seed.radius_at_anchor;
  single.mark[kMarkOfTexture] = 0;
  // ⚠ LA MARQUE N'EST PAS LE STYLE : le bâtisseur traduit zéro, un et deux en
  // cinq, six et sept. C'est la marque que la figure lit.
  single.mark[kMarkOfStyle] = seed.draw_mark;
  return tracks;
}

std::string_view EffectSpiralFanDrawnIcon(std::uint32_t& generator) {
  // ⚠ DEUX TIRAGES, ET DANS CET ORDRE : le client décide D'ABORD de la plage par
  // un pile ou face, et seulement ensuite du rang dans la plage. Les inverser
  // donnerait une autre suite à graine égale, et la troupe cesserait de se
  // rejouer à l'identique.
  const bool chez_le_mage = NextEffectNodeDraw(generator) % 2 == 0;
  const std::uint32_t first = chez_le_mage ? kSpiralFanFirstMageSkill : kSpiralFanFirstWizardSkill;
  const std::uint32_t count = chez_le_mage ? kSpiralFanMageSkillCount : kSpiralFanWizardSkillCount;
  return EffectSkillIconFor(first + NextEffectNodeDraw(generator) % count);
}

PooledEffectNode SowEffectSpiralFanNode(const EffectSpiralFanNodePattern& pattern, glm::vec3 anchor,
                                        std::int32_t effect_life_ticks, std::int32_t delay_ticks,
                                        std::string_view texture) {
  // ⚠ LE PORTEUR EST D'ABORD L'ANCRE. Un semis dont la marque fait suivre le
  // porteur lira ce point tant que personne ne lui en donne un autre : il reste
  // alors où il est né, ce qui est faux si le porteur marche, mais jamais
  // inventé. Voir `FanTracksFollowTheirOwner`.
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kSpiralFan, anchor);
  pooled.resource_paths = {std::string(texture.empty() ? pattern.texture : texture)};
  // ⚠ LE RETARD S'AJOUTE À LA VIE, et il le faut : le nœud du client NAÎT au tic
  // de sa phase, et vit sa vie pleine à partir de là. Le nôtre naît tout de
  // suite et attend ; sans ce rattrapage, le dernier des dix serait mort avant
  // d'avoir commencé.
  pooled.node.life_ticks = effect_life_ticks - delay_ticks;
  const EffectSpiralFanStyleSeed* style_seed = EffectSpiralFanStyleSeedAt(pattern.seed);
  if (style_seed != nullptr) {
    pooled.tracks = SeedEffectSpiralFanTracks(*style_seed);
    pooled.tracks.front().ticks = delay_ticks;
  }
  return pooled;
}

}  // namespace ro::render
