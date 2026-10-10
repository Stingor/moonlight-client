#include "ro/render/effect_track_figures.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "ro/render/effect_scene.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_trail_palette.h"

namespace ro::render {
namespace {

[[nodiscard]] float DegreesToRadians(float degrees) {
  return degrees * std::numbers::pi_v<float> / 180.0F;
}

// LES DEUX CLÉS QUI SOULÈVENT LE RUBAN, et les décalages qu'elles posent.
//
// Le client compare la clé de la piste à ces deux flottants, à l'égalité
// exacte. Ce n'est pas une plage : toute autre valeur — et le sous-type 30 y
// pose vingt-cinq ou trente — ne décale rien du tout.
//
// ⚠ LE REPÈRE DU CLIENT DESCEND ET LE NÔTRE MONTE : ce qu'il pose en ordonnée
// négative se pose ici en positive.
constexpr float kLiftKeyHigh = 6.001F;
constexpr float kLiftKeyLow = 24.000999F;
constexpr float kLiftHigh = 2.0F;
constexpr float kLiftLow = 1.0F;

// COMMENT LA JUPE LIT SA PISTE. Les cases d'une piste n'ont pas de sens en
// elles-mêmes — voir `effect_track.h` —, et c'est ici que l'anneau donne le sien
// aux siennes : chaque case porte la longueur du ruban à un segment.
constexpr std::size_t kSegmentCount = kEffectTrackSlots;

// Combien de segments le ruban compte au plus. Le client borne son index à
// vingt et, la table en portant vingt et un, le dernier sert de fermeture.
constexpr std::int32_t kLastSegment = static_cast<std::int32_t>(kSegmentCount) - 1;

// La coordonnée de texture au travers du ruban : le bord intérieur porte le
// haut de l'image, l'extérieur le bas.
constexpr float kTextureAtInnerEdge = 1.0F;
constexpr float kTextureAtOuterEdge = 0.0F;

// Le décalage vertical que la clé de la piste impose, dans NOTRE repère.
[[nodiscard]] float LiftFromKey(float lift_key) {
  if (lift_key == kLiftKeyHigh) {
    return kLiftHigh;
  }
  if (lift_key == kLiftKeyLow) {
    return kLiftLow;
  }
  return 0.0F;
}

// Un point du monde depuis des coordonnées locales à la piste.
//
// Le repère local est celui du client : le cercle s'étend en x et en z, la
// hauteur monte en y. C'est le même placement que les figures du nœud, et il
// doit le rester — deux façons de poser un point feraient diverger les deux
// familles sur la quatrième colonne de l'orientation.
[[nodiscard]] glm::vec3 PlaceInWorld(const EffectRingOrientation& orientation, glm::vec3 anchor,
                                     glm::vec3 local) {
  return anchor + orientation * glm::vec4{local, 1.0F};
}

}  // namespace

std::vector<EffectWorldQuad> ComposeEffectTrackSkirt(const EffectTrack& track, std::uint8_t variant,
                                                     glm::vec3 anchor,
                                                     const EffectRingOrientation& orientation) {
  std::vector<EffectWorldQuad> quads;
  // UN BALAYAGE NÉGATIF N'EST PAS UN ARC À L'ENVERS : le client teste le bit de
  // signe et abandonne la piste. Le prendre pour un sens de rotation ferait
  // tourner un ruban qui ne doit pas paraître.
  // L'ANNEAU LIT SON ANGLE COMME UN BALAYAGE. Un autre dessinateur y lit un cap.
  if (!track.active || track.degrees < 0) {
    return quads;
  }

  const EffectTrailVariant tint = EffectTrailVariantFor(variant);
  const std::int16_t alpha = tint.forced_alpha != 0 ? tint.forced_alpha : track.alpha;
  if (alpha <= 0) {
    return quads;
  }

  const std::int32_t sweep_degrees = track.degrees;
  const std::int32_t step = sweep_degrees / kLastSegment;
  const float lift = LiftFromKey(track.lift_key);
  const float tilt = DegreesToRadians(static_cast<float>(track.tilt_degrees));
  const float upward = std::sin(tilt);
  const float vers_le_large = std::cos(tilt);

  glm::vec3 previous_inner{0.0F, 0.0F, 0.0F};
  glm::vec3 previous_outer{0.0F, 0.0F, 0.0F};
  bool has_previous = false;
  std::int32_t travelled = 0;
  // 🔴 LE TRACÉ VA JUSQU'AU BOUT DU BALAYAGE, et non jusqu'à la vingtième case :
  // le client avance tant que l'angle cumulé reste dans le balayage, et borne
  // seulement son index à vingt. Un balayage qui n'est pas multiple de vingt
  // gagne ainsi un point de plus, qui relit la dernière case.
  for (std::int32_t point = 0;; ++point) {
    const std::int32_t segment = std::min(point, kLastSegment);
    // LE DERNIER SEGMENT REPREND L'ANGLE DE DÉPART quand le tour est entier :
    // accumuler le pas y laisserait une fente large de l'arrondi.
    const bool referme = sweep_degrees == kEffectTrackFullTurnDegrees && segment == kLastSegment;
    const std::int32_t angle =
        referme ? track.azimuth_degrees
                : (track.azimuth_degrees + travelled) % kEffectTrackFullTurnDegrees;
    const float theta = DegreesToRadians(static_cast<float>(angle));
    const float vers_x = std::cos(theta);
    // LA PROFONDEUR SE RETOURNE AVEC L'ORDONNÉE : le passage au nôtre est un
    // demi-tour autour de X. N'en retourner qu'une ferait de la jupe le miroir
    // de la sienne, qui tournerait à l'envers et ouvrirait ses brèches du
    // mauvais côté.
    const float vers_z = -std::sin(theta);

    const glm::vec3 inner =
        PlaceInWorld(orientation, anchor,
                     {track.radius_at_anchor * vers_x, lift, track.radius_at_anchor * vers_z});
    const float length = track.value[static_cast<std::size_t>(segment)];
    const float offset = length * vers_le_large;
    const glm::vec3 outer =
        PlaceInWorld(orientation, anchor,
                     {(track.radius_at_anchor + offset) * vers_x, length * upward + lift,
                      (track.radius_at_anchor + offset) * vers_z});

    if (has_previous) {
      const float u_before = static_cast<float>(segment - 1) / static_cast<float>(kSegmentCount);
      const float u_after = static_cast<float>(segment) / static_cast<float>(kSegmentCount);
      EffectWorldQuad quad;
      quad.corners = {previous_inner, inner, outer, previous_outer};
      quad.texture_uv = {
          glm::vec2{u_before, kTextureAtInnerEdge}, glm::vec2{u_after, kTextureAtInnerEdge},
          glm::vec2{u_after, kTextureAtOuterEdge}, glm::vec2{u_before, kTextureAtOuterEdge}};
      quad.tint = {tint.color[0], tint.color[1], tint.color[2], static_cast<std::uint8_t>(alpha)};
      quad.blend_source = kEffectNodeBlendSource;
      quad.blend_destination = tint.blend_destination;
      quads.push_back(quad);
    }

    previous_inner = inner;
    previous_outer = outer;
    has_previous = true;
    travelled += step;
    // ⚠ UN PAS NUL NE FINIRAIT JAMAIS : un balayage de moins de vingt degrés
    // s'arrête à la dernière case.
    if (travelled > sweep_degrees || (step <= 0 && segment == kLastSegment)) {
      break;
    }
  }
  return quads;
}

std::vector<EffectWorldQuad> ComposeCloudRingQuads(const EffectTrack& track, std::uint8_t variant,
                                                   glm::vec3 anchor,
                                                   const EffectRingOrientation& orientation) {
  std::vector<EffectWorldQuad> quads;
  // LE CLIENT TESTE LE BALAYAGE STRICTEMENT POSITIF, et abandonne la piste
  // sinon. Ce n'est pas la même garde que la jupe, qui n'écarte que le négatif.
  if (!track.active || track.degrees <= 0) {
    return quads;
  }

  const std::int32_t sweep_degrees = track.degrees;
  const std::int32_t step = sweep_degrees / static_cast<std::int32_t>(kSegmentCount);
  // ⚠ UN PAS NUL FERAIT TOURNER LE CLIENT SANS FIN. Voir l'en-tête.
  if (step <= 0) {
    return quads;
  }

  const EffectTrailVariant tint = EffectTrailVariantFor(variant);
  const std::int16_t alpha = tint.forced_alpha != 0 ? tint.forced_alpha : track.alpha;
  // ⚠ ET AUCUNE GARDE D'ALPHA : ce dessinateur peint une piste à alpha nul, au
  // contraire de la jupe. Lui en prêter une ferait disparaître l'anneau pendant
  // les douze premiers tics, le temps que son mouvement monte l'alpha.

  // ⚠ LA CLÉ EST L'ORDONNÉE DES DEUX BORDS, telle quelle. Le repère du client
  // descend et le nôtre monte : son moins-clé est notre plus-clé.
  const float height = track.lift_key;
  const float tilt = DegreesToRadians(static_cast<float>(track.tilt_degrees));
  const float upward = std::sin(tilt);
  const float vers_le_large = std::cos(tilt);

  glm::vec3 previous_inner{0.0F, 0.0F, 0.0F};
  glm::vec3 previous_outer{0.0F, 0.0F, 0.0F};
  bool has_previous = false;
  std::size_t segment = 0;
  for (std::int32_t travelled = 0; travelled < sweep_degrees; travelled += step) {
    // LE DERNIER SEGMENT REPREND L'AZIMUT quand le tour est entier, comme chez
    // ses deux sœurs.
    const bool referme = sweep_degrees == kEffectTrackFullTurnDegrees &&
                         segment == static_cast<std::size_t>(kLastSegment);
    const std::int32_t angle =
        referme ? track.azimuth_degrees
                : (track.azimuth_degrees + travelled) % kEffectTrackFullTurnDegrees;
    const float theta = DegreesToRadians(static_cast<float>(angle));
    const float vers_x = std::cos(theta);
    // LA PROFONDEUR SE RETOURNE AVEC L'ORDONNÉE, comme chez la jupe : la hauteur
    // est déjà la nôtre, et n'en retourner qu'une ferait de l'anneau un miroir.
    const float vers_z = -std::sin(theta);

    const glm::vec3 inner =
        PlaceInWorld(orientation, anchor,
                     {track.radius_at_anchor * vers_x, height, track.radius_at_anchor * vers_z});
    const float length = track.value[segment];
    const float offset = length * vers_le_large;
    const glm::vec3 outer =
        PlaceInWorld(orientation, anchor,
                     {(track.radius_at_anchor + offset) * vers_x, length * upward + height,
                      (track.radius_at_anchor + offset) * vers_z});

    if (has_previous) {
      const float u_before = static_cast<float>(segment - 1) / static_cast<float>(kSegmentCount);
      const float u_after = static_cast<float>(segment) / static_cast<float>(kSegmentCount);
      EffectWorldQuad quad;
      quad.corners = {previous_inner, inner, outer, previous_outer};
      quad.texture_uv = {
          glm::vec2{u_before, kTextureAtInnerEdge}, glm::vec2{u_after, kTextureAtInnerEdge},
          glm::vec2{u_after, kTextureAtOuterEdge}, glm::vec2{u_before, kTextureAtOuterEdge}};
      quad.tint = {tint.color[0], tint.color[1], tint.color[2],
                   static_cast<std::uint8_t>(std::max<std::int16_t>(alpha, 0))};
      quad.blend_source = kEffectNodeBlendSource;
      quad.blend_destination = tint.blend_destination;
      quads.push_back(quad);
    }

    previous_inner = inner;
    previous_outer = outer;
    has_previous = true;
    // ⚠ L'INDEX SE BORNE À VINGT, et la boucle continue : les tours de trop
    // reprennent la dernière longueur au lieu de sortir du tableau.
    if (segment < static_cast<std::size_t>(kLastSegment)) {
      ++segment;
    }
  }
  return quads;
}

}  // namespace ro::render
