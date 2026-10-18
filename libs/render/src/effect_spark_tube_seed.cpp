#include "ro/render/effect_spark_tube_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_burst.h"

#include "effect_draw_below.h"
#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES DEUX CASES DE QUEUE que les trois bâtisseurs posent.
constexpr std::size_t kSlotOfHeightScale = 0;
constexpr std::size_t kSlotOfPointCount = 1;

// LES OCTETS QUE LES BÂTISSEURS COMPARENT, un à un et par égalité exacte.
constexpr std::uint8_t kStyleShort = 1;
constexpr std::uint8_t kStyleHalo = 3;

// L'ALPHA DE DÉPART : un plancher commun aux trois, et deux largeurs de tirage.
//
// ⚠ CENT VINGT-CINQ AU MINIMUM, et jamais moins : l'étincelle naît déjà
// visible, contrairement à presque toute la famille procédurale. La semer à
// zéro attendrait un mouvement qui ne la monte jamais — celui-ci ne sait que
// la faire tomber.
constexpr std::int16_t kAlphaFloor = 125;
constexpr std::int32_t kAlphaNarrowSpread = 51;
constexpr std::int32_t kAlphaWideSpread = 121;

// L'ÉCHELLE DE HAUTEUR de la courbe, tirée par dixièmes.
constexpr float kHeightScaleStep = 0.1F;
constexpr std::int32_t kHeightScaleSpread = 7;
constexpr float kHeightScaleRisingFloor = 0.6F;
constexpr float kHeightScaleFloor = 0.8F;

// LES QUATRE VITESSES D'OUVERTURE, une par cas mesuré. Elles ne se déduisent
// d'aucune autre et valent des centièmes : c'est la largeur du tube.
constexpr float kWidthShort = 0.1F;
constexpr float kWidthRisingHalo = 0.07F;
constexpr float kWidthLateHalo = 0.05F;
constexpr float kWidthDirectedHalo = 0.06F;

// DE COMBIEN LE CAP DE L'ÉTINCELLE DIRIGÉE S'ÉCARTE de la ligne vers la cible.
constexpr std::int32_t kDirectedSpread = 121;
constexpr std::int32_t kDirectedBias = 60;

// LES CINQ RETARDS, un par cas mesuré : un plancher et une largeur de tirage.
//
// ⚠ AUCUN NE SE DÉDUIT D'UN AUTRE, et l'écart est large — de zéro à dix tics
// pour le plus court, de trente à soixante-dix pour le plus long. C'est lui
// qui échelonne les quatre étincelles d'un même nœud ; les semer à zéro les
// ferait partir ensemble.
constexpr std::int32_t kDelayShortSpread = 11;
constexpr std::int32_t kDelayRisingHaloFloor = 30;
constexpr std::int32_t kDelayRisingHaloSpread = 41;
constexpr std::int32_t kDelayRisingSpread = 26;
constexpr std::int32_t kDelayLateSpread = 31;
constexpr std::int32_t kDelayDirectedHaloFloor = 25;
constexpr std::int32_t kDelayDirectedHaloSpread = 21;
constexpr std::int32_t kDelayDirectedFloor = 15;
constexpr std::int32_t kDelayDirectedSpread = 26;

// LA PORTÉE DE LA COURBE, en unités du monde : un plancher et une largeur.
//
// ⚠ L'ÉTINCELLE COURTE VA MOINS LOIN ET VARIE MOINS : cinq à neuf, contre six
// à onze ou douze pour les autres.
constexpr float kReachShortFloor = 5.0F;
constexpr std::int32_t kReachShortSpread = 5;
constexpr float kReachFloor = 6.0F;
constexpr std::int32_t kReachSpread = 6;
constexpr std::int32_t kReachDirectedSpread = 7;

// LE RETARD D'UNE PISTE, en tics NÉGATIFS — et il diffère pour chacun des cinq
// cas mesurés. C'est lui qui échelonne les quatre étincelles d'un nœud.
[[nodiscard]] std::int32_t DrawnDelay(const EffectSparkTubePattern& pattern,
                                      std::uint32_t& rng_state) {
  switch (pattern.builder) {
    case EffectSparkTubeBuilder::kRising:
      if (pattern.style == kStyleShort) {
        return -DrawBelow(rng_state, kDelayShortSpread);
      }
      if (pattern.style == kStyleHalo) {
        return -kDelayRisingHaloFloor - DrawBelow(rng_state, kDelayRisingHaloSpread);
      }
      return -DrawBelow(rng_state, kDelayRisingSpread);
    case EffectSparkTubeBuilder::kRisingLate:
      return -DrawBelow(rng_state, kDelayLateSpread);
    case EffectSparkTubeBuilder::kDirected:
      return pattern.style == kStyleHalo
                 ? -kDelayDirectedHaloFloor - DrawBelow(rng_state, kDelayDirectedHaloSpread)
                 : -kDelayDirectedFloor - DrawBelow(rng_state, kDelayDirectedSpread);
  }
  return 0;
}

// LA LARGEUR DU TUBE, par bâtisseur et par octet.
[[nodiscard]] float TubeWidth(const EffectSparkTubePattern& pattern) {
  switch (pattern.builder) {
    case EffectSparkTubeBuilder::kRising:
      return pattern.style == kStyleHalo ? kWidthRisingHalo : kWidthShort;
    case EffectSparkTubeBuilder::kRisingLate:
      return pattern.style == kStyleHalo ? kWidthLateHalo : kWidthShort;
    case EffectSparkTubeBuilder::kDirected:
      return pattern.style == kStyleHalo ? kWidthDirectedHalo : kWidthRisingHalo;
  }
  return kWidthShort;
}

// COMBIEN DE POINTS LA COURBE PORTE : trois, quatre ou cinq.
//
// ⚠ C'EST LA LONGUEUR DE LA TRAÎNÉE, et elle ne se déduit ni de l'octet ni du
// bâtisseur seuls : il faut les deux. Un point de plus, c'est un tronçon de
// tube de plus et dix d'alpha en moins au bout.
[[nodiscard]] float PointCount(const EffectSparkTubePattern& pattern) {
  switch (pattern.builder) {
    case EffectSparkTubeBuilder::kRising:
      if (pattern.style == kStyleShort) {
        return 3.0F;
      }
      return pattern.style == kStyleHalo ? 4.0F : 5.0F;
    case EffectSparkTubeBuilder::kRisingLate:
      return 4.0F;
    case EffectSparkTubeBuilder::kDirected:
      return 3.0F;
  }
  return 3.0F;
}

// UN TIC DU MOUVEMENT, joué d'avance par le premier bâtisseur : l'angle monte
// et les cases glissent. Voir `AdvanceSparkTubeTracks`, qui fait exactement
// cela — la boucle du client y est recopiée mot pour mot.
void PlayTickAhead(EffectTrack& track) {
  track.value[0] += kSparkTubeAngleStepDegrees;
  if (track.value[0] > kSparkTubeAngleCeilingDegrees) {
    track.value[0] = kSparkTubeAngleCeilingDegrees;
  }
  const auto count = static_cast<std::int32_t>(track.tail[kSlotOfPointCount]);
  for (std::int32_t rang = count - 1; rang >= 0; --rang) {
    const auto source = static_cast<std::size_t>(rang);
    if (source + 1 < track.value.size()) {
      track.value[source + 1] = track.value[source];
    }
  }
}

}  // namespace

std::span<const EffectSparkTubePattern> EffectSparkTubePatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectSparkTubePatternTable(), effect_id,
                           &EffectSparkTubePattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSparkTubeTracks(
    const EffectSparkTubePattern& pattern, glm::vec3 anchor, glm::vec3 toward_target,
    std::uint32_t& generator) {
  const bool dirige = pattern.builder == EffectSparkTubeBuilder::kDirected;
  const bool brief =
      pattern.builder == EffectSparkTubeBuilder::kRising && pattern.style == kStyleShort;
  // ⚠ L'ÉTINCELLE DIRIGÉE VISE À L'ENVERS : le client mesure le cap du PORTEUR
  // VERS LA CIBLE en soustrayant la cible de la position, c'est-à-dire le cap
  // qui S'ÉLOIGNE d'elle. Prendre l'autre sens la ferait partir vers le coup
  // au lieu d'en jaillir. Le cap se mesure dans le repère du client, comme
  // celui que les deux autres tirent : sa profondeur est l'opposée de la nôtre.
  const std::int32_t cap_de_fuite = HeadingOfVector(-toward_target.x, toward_target.z);

  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    track.ticks = DrawnDelay(pattern, generator);
    track.lift_key = TubeWidth(pattern);

    const std::int32_t alpha_width =
        pattern.builder == EffectSparkTubeBuilder::kRising && pattern.style < kStyleHalo
            ? kAlphaNarrowSpread
            : kAlphaWideSpread;
    track.alpha = static_cast<std::int16_t>(DrawBelow(generator, alpha_width) + kAlphaFloor);

    if (dirige) {
      // L'ÉCART TIRÉ S'AJOUTE au cap du client, que la piste garde tel quel.
      const std::int32_t offset = DrawBelow(generator, kDirectedSpread) - kDirectedBias;
      track.tilt_degrees = static_cast<std::int16_t>(WithinFullTurn(cap_de_fuite + offset));
    } else {
      // Les deux autres partent dans une direction TIRÉE AU SORT : ce sont des
      // étincelles, pas des traits.
      track.tilt_degrees =
          static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
    }

    if (brief) {
      track.radius_at_anchor =
          static_cast<float>(DrawBelow(generator, kReachShortSpread)) + kReachShortFloor;
    } else if (dirige) {
      track.radius_at_anchor =
          static_cast<float>(DrawBelow(generator, kReachDirectedSpread)) + kReachFloor;
    } else {
      track.radius_at_anchor = static_cast<float>(DrawBelow(generator, kReachSpread)) + kReachFloor;
    }

    track.value[0] = 0.0F;
    // ⚠ LE PLANCHER BAS EST CELUI DU BÂTISSEUR MONTANT TOUT ENTIER, quel que
    // soit son octet ; le haut n'appartient qu'au tardif et au dirigé.
    track.tail[kSlotOfHeightScale] =
        static_cast<float>(DrawBelow(generator, kHeightScaleSpread)) * kHeightScaleStep +
        (pattern.builder == EffectSparkTubeBuilder::kRising ? kHeightScaleRisingFloor
                                                            : kHeightScaleFloor);
    track.tail[kSlotOfPointCount] = PointCount(pattern);
    // ⚠ L'ANCRE EST UN POINT DU MONDE, et non un décalage : le dessinateur pose
    // ses coins en ABSOLU. Y mettre zéro planterait l'étincelle à l'origine.
    track.anchor = anchor;
    track.style = static_cast<std::int16_t>(pattern.style);

    // ⚠ ET UN SEUL CAS NAÎT DÉJÀ DÉPLOYÉ : le premier bâtisseur joue dix tics
    // de mouvement d'avance pour son octet UN. Ne pas le faire donnerait à
    // EF_STORMKICK un départ que le client ne montre pas.
    if (brief && pattern.style == kSparkTubePreRolledStyle) {
      for (std::int32_t tick = 0; tick < kSparkTubePreRollTicks; ++tick) {
        PlayTickAhead(track);
      }
    }
  }
  return tracks;
}

PooledEffectNode SowEffectSparkTubeNode(const EffectSparkTubePattern& pattern, glm::vec3 anchor,
                                        glm::vec3 toward_target, std::int32_t effect_life_ticks,
                                        std::uint32_t& generator) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kSparkTube, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  // ⚠ L'IMAGE N'EST PAS DANS LE PATRON : les trois bâtisseurs l'écrivent en
  // dur, et aucun ne la reçoit de son site.
  pooled.resource_paths = {std::string(kSparkTubeTexture)};
  pooled.tracks = SeedEffectSparkTubeTracks(pattern, anchor, toward_target, generator);
  return pooled;
}

}  // namespace ro::render
