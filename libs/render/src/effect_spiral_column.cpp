#include "ro/render/effect_spiral_column.h"

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

// LES TROIS DERNIÈRES CASES DE LA QUEUE, et ce qu'elles portent.
//
// ⚠ C'EST LE VECTEUR VERS LA CIBLE quand le bâtisseur « dirigé » l'a posé, et
// zéro sinon. Deux bâtisseurs sur deux ne diffèrent que par ces trois flottants.
constexpr std::size_t kSlotOfAimX = 6;
constexpr std::size_t kSlotOfAimY = 7;
constexpr std::size_t kSlotOfAimZ = 8;

// LES COORDONNÉES D'IMAGE de la passe intérieure, telles que le client les passe
// à la primitive du quad projeté : l'image entière, tournée d'un quart.
constexpr float kInnerTopV = 1.0F;
constexpr float kInnerBottomV = 0.0F;
constexpr float kInnerLeftU = 0.0F;
constexpr float kInnerRightU = 1.0F;

// LA LARGEUR DE BANDE DE LA PRIMITIVE DE TRAÎNÉE : elle divise le rang d'un
// segment par vingt et un, quel que soit le nombre de segments qu'on lui donne.
// La colonne n'en a que quatre, et n'emploie donc qu'un cinquième de l'image.
constexpr float kTrailBandSegments = 21.0F;

// UN POINT DU MONDE, deux fois ancré.
[[nodiscard]] glm::vec3 PointDeBase(const EffectTrack& track, glm::vec3 anchor, float radius,
                                    float vers_x, float vers_z) {
  return glm::vec3{radius * vers_x + track.anchor.x + track.tail[kSlotOfAimX],
                   track.anchor.y + track.tail[kSlotOfAimY],
                   radius * vers_z + track.anchor.z + track.tail[kSlotOfAimZ]} +
         anchor;
}

}  // namespace

std::vector<EffectWorldQuad> ComposeSpiralColumnQuads(const EffectTrack& track,
                                                      std::uint8_t variant, glm::vec3 anchor) {
  std::vector<EffectWorldQuad> quads;
  // Un balayage NÉGATIF n'est pas un arc à l'envers : le client teste le bit de
  // signe et abandonne la piste.
  if (!track.active || track.degrees < 0 || track.alpha <= 0) {
    return quads;
  }
  // ⚠ ET UNE COLONNE SANS HAUTEUR N'EST PAS UNE COLONNE. Le client, lui, soumet
  // ses quads degeneres pendant les cent tics d'attente — base et sommet
  // confondus, aire nulle. Nous ne les soumettons pas : un ruban sans aire ne se
  // distingue pas d'une absence, et en peindre huit par piste paierait des lots
  // de rendu pour rien. C'est la meme regle que la jupe d'anneau applique.
  bool has_height = false;
  for (std::size_t point = 0; point < kSpiralColumnPoints; ++point) {
    if (track.value[point] != 0.0F) {
      has_height = true;
      break;
    }
  }
  if (!has_height) {
    return quads;
  }

  const auto last = static_cast<std::int32_t>(kSpiralColumnQuadsPerPass);
  const std::int32_t step = track.degrees / last;
  const float tilt = DegreesToRadians(static_cast<float>(track.tilt_degrees));
  // ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : là où il SOUSTRAIT la part
  // verticale du segment, nous l'ajoutons. Recopier le signe enfoncerait la
  // colonne dans le sol.
  const float upward = std::sin(tilt);
  const float vers_le_large = std::cos(tilt);
  const EffectTrailVariant tint = EffectTrailVariantFor(variant);

  // LES DEUX PASSES : l'intérieure au rayon de la piste, l'extérieure plus large
  // et tournée.
  for (int passe = 0; passe < 2; ++passe) {
    const bool exterieure = passe == 1;
    const float ring_radius =
        exterieure ? track.radius_at_anchor * kSpiralColumnOuterScale : track.radius_at_anchor;
    const std::int32_t start =
        track.azimuth_degrees + (exterieure ? kSpiralColumnOuterOffsetDegrees : 0);

    glm::vec3 base_precedente{0.0F, 0.0F, 0.0F};
    glm::vec3 previous_vertex{0.0F, 0.0F, 0.0F};
    for (std::int32_t point = 0; point <= last; ++point) {
      // LE DERNIER POINT REPREND L'AZIMUT DE DÉPART quand le tour est entier :
      // accumuler le pas y laisserait une fente large de l'arrondi.
      const bool referme = track.degrees == kEffectTrackFullTurnDegrees && point == last;
      const std::int32_t angle =
          referme ? track.azimuth_degrees : (start + step * point) % kEffectTrackFullTurnDegrees;
      const float theta = DegreesToRadians(static_cast<float>(angle));
      const float vers_x = std::cos(theta);
      // LA PROFONDEUR SE RETOURNE AVEC L'ORDONNÉE : n'en retourner qu'une ferait
      // de la colonne le miroir de la sienne, qui s'enroulerait à l'envers.
      const float vers_z = -std::sin(theta);

      const glm::vec3 base = PointDeBase(track, anchor, ring_radius, vers_x, vers_z);
      const float length = track.value[static_cast<std::size_t>(point)];
      const float offset = length * vers_le_large;
      const glm::vec3 vertex = base + glm::vec3{offset * vers_x, length * upward, offset * vers_z};

      if (point > 0) {
        EffectWorldQuad quad;
        quad.corners = {base_precedente, base, vertex, previous_vertex};
        if (exterieure) {
          // LA BANDE D'IMAGE de la primitive de traînée : le rang du segment
          // divisé par vingt et un, et le suivant.
          const float v_before = static_cast<float>(point - 1) / kTrailBandSegments;
          const float v_after = static_cast<float>(point) / kTrailBandSegments;
          quad.texture_uv = {glm::vec2{0.0F, v_before}, glm::vec2{0.0F, v_after},
                             glm::vec2{1.0F, v_after}, glm::vec2{1.0F, v_before}};
          const std::int16_t alpha = tint.forced_alpha != 0 ? tint.forced_alpha : track.alpha;
          quad.tint = {tint.color[0], tint.color[1], tint.color[2],
                       static_cast<std::uint8_t>(alpha)};
          quad.blend_destination = tint.blend_destination;
        } else {
          quad.texture_uv = {
              glm::vec2{kInnerLeftU, kInnerTopV}, glm::vec2{kInnerRightU, kInnerTopV},
              glm::vec2{kInnerRightU, kInnerBottomV}, glm::vec2{kInnerLeftU, kInnerBottomV}};
          // ⚠ TEINTE EN DUR ET ALPHA AU CINQUIÈME : ce ruban-ci ne peut être que
          // rouge, et cinq fois plus discret que la piste ne le dit.
          quad.tint = {kSpiralColumnInnerRed, kSpiralColumnInnerGreen, kSpiralColumnInnerBlue,
                       static_cast<std::uint8_t>(track.alpha / kSpiralColumnInnerAlphaDivisor)};
          quad.blend_destination = kEffectNodeBlendDestination;
        }
        quad.blend_source = kEffectNodeBlendSource;
        quads.push_back(quad);
      }
      base_precedente = base;
      previous_vertex = vertex;
    }
  }
  return quads;
}

}  // namespace ro::render
