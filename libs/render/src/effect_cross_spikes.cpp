#include "ro/render/effect_cross_spikes.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/effect_node.h"
#include "ro/render/effect_scene.h"

namespace ro::render {
namespace {

// LES QUATRE ARÊTES DU CARRÉ, chacune avec SON SENS, telles que le client les
// écrit — et il ne les parcourt PAS en cycle.
//
// ⚠ CE N'EST PAS UN TOUR DU CARRÉ : le dessinateur empile douze triplets de
// flottants et sa boucle en prend neuf à la fois. Les quatre arêtes sortent
// dans l'ordre bas, gauche, haut, droite — et la première part du MÊME coin que
// la deuxième, en sens inverse du tour. Les quatre côtés y sont, chacun une
// fois ; seul l'ORDRE diffère, et l'ordre est ce qui décide quelle bande de
// texture va sur quelle face.
//
// Un tour régulier donnerait à chaque côté la bande de son VOISIN, et la
// première face à l'envers : les pointes ne montreraient pas les mêmes faces
// que celles du client.
constexpr std::array<std::array<std::array<float, 2>, 2>, kCrossSpikeFaces> kFaceEdges = {{
    {{{{-1.0F, -1.0F}}, {{1.0F, -1.0F}}}},
    {{{{-1.0F, -1.0F}}, {{-1.0F, 1.0F}}}},
    {{{{-1.0F, 1.0F}}, {{1.0F, 1.0F}}}},
    {{{{1.0F, 1.0F}}, {{1.0F, -1.0F}}}},
}};

// LA COULEUR À PART : son plancher par canal tiré, le nombre de valeurs de
// chaque tirage, et le canal plein.
constexpr std::uint32_t kOwnColorFloor = 40;
constexpr std::uint32_t kOwnColorDraws = 11;
constexpr std::uint8_t kOwnColorFull = 255;

}  // namespace

std::array<std::uint8_t, 4> DrawCrossSpikeOwnColor(std::uint32_t& generator) {
  const auto red =
      static_cast<std::uint8_t>(kOwnColorFloor + NextEffectNodeDraw(generator) % kOwnColorDraws);
  const auto vert =
      static_cast<std::uint8_t>(kOwnColorFloor + NextEffectNodeDraw(generator) % kOwnColorDraws);
  return {red, vert, kOwnColorFull, kOwnColorFull};
}

EffectCrossSpikeRequest EffectCrossSpikeRequestOf(const EffectNode& node, std::uint8_t track_mark) {
  EffectCrossSpikeRequest spawn_request;
  // ⚠ LE DEMI-CÔTÉ EST L'ÉCHELLE DU SPRITE, et non une demi-taille : c'est
  // l'ADR 0017 sur un champ qui porte pourtant un nom clair. Cette figure-ci s'en
  // sert comme d'une longueur du monde.
  spawn_request.half_side = node.sprite_scale;
  spawn_request.apex_height = node.half_height.half_size;
  spawn_request.tint = {static_cast<std::uint8_t>(std::clamp(node.color[0], 0.0F, 255.0F)),
                        static_cast<std::uint8_t>(std::clamp(node.color[1], 0.0F, 255.0F)),
                        static_cast<std::uint8_t>(std::clamp(node.color[2], 0.0F, 255.0F)),
                        static_cast<std::uint8_t>(std::clamp(node.alpha, 0.0F, 255.0F))};
  spawn_request.draws_its_own_color = track_mark == kCrossSpikeDrawnColor;
  return spawn_request;
}

EffectCrossSpikeRequest EffectCrossSpikeRequestOf(const EffectNode& node, std::uint8_t track_mark,
                                                  std::uint32_t& generator) {
  EffectCrossSpikeRequest spawn_request = EffectCrossSpikeRequestOf(node, track_mark);
  if (spawn_request.draws_its_own_color) {
    spawn_request.tint = DrawCrossSpikeOwnColor(generator);
  }
  return spawn_request;
}

std::vector<EffectWorldQuad> ComposeCrossSpikeQuads(const EffectCrossSpikeRequest& request,
                                                    glm::vec3 node_origin,
                                                    const EffectRingOrientation& orientation) {
  std::vector<EffectWorldQuad> quads;
  if (request.half_side <= 0.0F) {
    return quads;
  }

  // ⚠ LE SOMMET MONTE EN Y, ET LE CLIENT L'ÉCRIT EN Z. Ce n'est pas une
  // correction de sa valeur mais la MÊME conversion que partout ailleurs, prise
  // un cran plus loin.
  //
  // Son dessinateur pose bien ses quatre triangles base en XY et sommet en +Z.
  // Mais il multiplie ensuite par la matrice du nœud, que le bâtisseur a
  // tournée de QUATRE-VINGTS À QUATRE-VINGT-DIX-NEUF DEGRÉS autour de X : c'est
  // elle qui redresse la pointe. Recopier le repère local sans la matrice
  // laisserait la pyramide COUCHÉE — le pic de terre sortirait du sol à
  // l'horizontale.
  //
  // Le quart de tour est donc replié dans la figure, comme le repère descendant
  // du client l'est ailleurs : base en XZ, sommet en +Y. L'identité rend alors
  // une pointe DEBOUT, ce que toutes les autres figures de la famille
  // promettent déjà — voir `kEffectRingUpright`.
  //
  // ⚠ L'ÉCART À CE QUART DE TOUR — le penchant et le lacet tirés au semis —
  // arrive par `orientation` : voir `EffectNodeLeaningOrientation`.
  const glm::vec3 vertex =
      node_origin + glm::vec3(orientation * glm::vec4{0.0F, request.apex_height, 0.0F, 1.0F});

  quads.reserve(kCrossSpikeFaces);
  for (std::size_t face = 0; face < kCrossSpikeFaces; ++face) {
    const std::array<float, 2>& here = kFaceEdges[face][0];
    const std::array<float, 2>& after = kFaceEdges[face][1];
    // LA BASE EST AU SOL, donc en XZ, l'ordonnée nulle. Voir le sommet.
    const glm::vec3 from =
        node_origin + glm::vec3(orientation * glm::vec4{here[0] * request.half_side, 0.0F,
                                                        here[1] * request.half_side, 1.0F});
    const glm::vec3 vers =
        node_origin + glm::vec3(orientation * glm::vec4{after[0] * request.half_side, 0.0F,
                                                        after[1] * request.half_side, 1.0F});

    // ⚠ UN TRIANGLE PORTÉ PAR UN QUAD : le sommet est répété, comme le client le
    // fait. Chercher un quatrième point distinct n'en trouverait pas.
    EffectWorldQuad quad;
    quad.corners = {from, vers, vertex, vertex};
    // ⚠ ET LE SOMMET PREND L'ABSCISSE DU PREMIER COIN, pas du second. Le client
    // écrit ses trois sommets ainsi : la base court d'un bord à l'autre de la
    // bande, et la pointe retombe sur son bord GAUCHE. La bande se pose donc en
    // triangle rectangle, et non en triangle isocèle ni retournée. Lui donner
    // l'autre bord mirait la face.
    const float band = static_cast<float>(face) * kCrossSpikeTextureStep;
    quad.texture_uv = {glm::vec2{band, 1.0F}, glm::vec2{band + kCrossSpikeTextureStep, 1.0F},
                       glm::vec2{band, 0.0F}, glm::vec2{band, 0.0F}};
    quad.tint = request.tint;
    // ⚠ LA FILE RANGE LE TRIANGLE PAR SON PREMIER SOMMET, un coin de la base :
    // voir `EffectQuadSortPoint::kFirstCorner`.
    quad.sort_point = EffectQuadSortPoint::kFirstCorner;
    quad.blend_source = kEffectNodeBlendSource;
    // ⚠ LE MODE À PART ADDITIONNE : c'est la marque de piste qui le dit, et un
    // seul bâtisseur sur neuf la pose. Les huit autres se mélangent.
    quad.blend_destination = request.draws_its_own_color ? 2 : 6;
    quads.push_back(quad);
  }
  return quads;
}

}  // namespace ro::render
