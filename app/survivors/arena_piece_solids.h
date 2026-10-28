#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/rsw.h"
#include "ro/render/model_mesh.h"
#include "ro/render/sprite_billboard.h"
#include "ro/sim/survivors_terrain.h"

#include "survivors/arena_ground_look.h"
#include "survivors/arena_ground_plan.h"

namespace moonclient {

// LE SOLIDE D'UN MODÈLE DU SOL GÉNÉRÉ : la part de sa géométrie qui arrête la
// marche, projetée au sol. Le décor ne ment pas (ADR 0131) quand chaque case
// que le solide d'une pièce couvre est bloquée par son emprise de `sim`, et
// que l'emprise ne bloque pas de case loin de tout solide.
//
// LA RÈGLE DE HAUTEUR : est solide, dans un bloc qui touche le sol, ce qui
// passe sous la tête d'un héros debout — margelle, murs, piliers, troncs,
// pierres, le haut d'une pierre penchée compris. Un feuillage, un toit, une
// aile de moulin, blocs en l'air, laissent marcher dessous.
//
// Le héros debout mesure, chapeau compris, `kHeroStandingPixels` pixels de
// sprite, soit 15 unités du monde.
inline constexpr float kHeroStandingPixels = 105.0F;
inline constexpr float kSolidHeightUnits =
    kHeroStandingPixels / ro::render::kSpritePixelsPerWorldUnit;
// Un bloc touche le sol quand son point le plus bas en est à moins de ceci.
inline constexpr float kSolidContactUnits = 1.0F;
// UNE CASE SE LIT EN 4 × 4 POINTS, un bit chacun, et elle est SOLIDE quand le
// solide en couvre au moins le quart : le héros, debout en son centre, ne
// touche pas un solide qui en couvre moins. Un point est couvert dans un
// contour ; un contour sans aire (une face debout, projetée en segment) couvre
// les points à moins d'un demi-pas de point de lui.
inline constexpr std::int32_t kSolidSamplesPerSide = 4;
inline constexpr std::int32_t kSolidLeastSamples = 4;
// Un modèle à angle tiré se lit sous chacun des angles entiers que le plan
// tire, à son échelle et à ses deux échelles extrêmes.
inline constexpr std::int32_t kSolidYawStepDegrees = 1;

// Un point d'un contour, dans le repère du MAILLAGE (x, z) : la matrice du
// décor (`ModelInstanceMatrix`) le pose, recentrage compris.
using SolidPoint = std::array<float, 2>;

// UN CONTOUR CONVEXE du solide ; deux points : une face debout vue du ciel.
struct SolidOutline {
  std::span<const SolidPoint> points;
};

// LE SOLIDE RELEVÉ D'UN MODÈLE à l'échelle et à l'enfoncement du catalogue :
// ce que la matrice du décor lit du maillage (sa famille et sa boîte), et ses
// contours.
struct ModelSolid {
  std::string_view model;
  float scale = 1.0F;
  float sink_units = 0.0F;
  std::int32_t rsm_version = 0;
  std::array<float, 3> bounds_min{};
  std::array<float, 3> bounds_max{};
  std::span<const SolidOutline> outlines;
};

// LE RELEVÉ FIGÉ des modèles du catalogue livré (`roc-cli survivors emprises
// --releve`, table engendrée par `scripts/gen_survivors_piece_solids.py`).
[[nodiscard]] std::span<const ModelSolid> SurveyedModelSolids();
// La hauteur de solide sous laquelle le relevé figé a été lu : un relevé lu
// sous une autre que `kSolidHeightUnits` est périmé.
[[nodiscard]] float SurveyedSolidHeightUnits();

// Le solide relevé du modèle `choice`, à son échelle et à son enfoncement,
// dans `solids` ; nullptr s'il n'y est pas.
[[nodiscard]] const ModelSolid* FindModelSolid(std::span<const ModelSolid> solids,
                                               const GroundModelChoice& choice);

// UNE FACE SOLIDE projetée au sol, et la hauteur de son point le plus bas
// au-dessus du sol, en unités du monde.
struct SolidFace {
  std::vector<SolidPoint> points;
  float lowest_units = 0.0F;
};

// LES FACES SOLIDES d'un maillage posé à l'échelle `scale` et enfoncé de
// `sink_units` (à l'échelle 1) : chaque triangle coupé à `height_units`
// au-dessus du sol, projeté au sol, dans le repère du maillage.
[[nodiscard]] std::vector<SolidFace> SolidFacesOf(const ro::render::ModelMesh& mesh,
                                                  std::int32_t rsm_version, float scale,
                                                  float sink_units,
                                                  float height_units = kSolidHeightUnits);
// LES CONTOURS d'un solide : les faces qui se touchent (boîtes à moins d'une
// demi-unité) font un bloc, et chaque bloc prend son enveloppe convexe. UN BLOC
// EN L'AIR, dont le plus bas reste à plus de `kSolidContactUnits` du sol — le
// bout d'une aile de moulin —, ne compte pas : on passe dessous.
[[nodiscard]] std::vector<std::vector<SolidPoint>> SolidHullsOf(
    const std::vector<SolidFace>& faces);
// LES FACES DES BLOCS QUI TOUCHENT LE SOL, dans leur ordre : le solide exact,
// face par face, sous la même règle que `SolidHullsOf`. Les autres, celles des
// blocs en l'air, vont dans `airborne` s'il est donné.
[[nodiscard]] std::vector<SolidFace> GroundedFacesOf(const std::vector<SolidFace>& faces,
                                                     std::vector<SolidFace>* airborne = nullptr);

// LA POSE DU RSW d'un modèle du plan : `ground_units` est l'altitude du monde
// sous lui, `gnd_width` et `gnd_height` la grille de cubes du sol. C'est la
// pose que le sol dessiné passe au rendu.
[[nodiscard]] ro::formats::RswModel PosePlacement(const PlannedPose& pose, float ground_units,
                                                  std::int32_t gnd_width, std::int32_t gnd_height);

// LES CASES QU'UN SOLIDE COUVRE autour d'une emprise, comptées depuis le coin
// sud-ouest de l'emprise tournée, un masque de 16 points par case.
struct SolidGrid {
  std::int32_t west = 0;
  std::int32_t south = 0;
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::vector<std::uint16_t> samples;

  // La grille de l'emprise `size`, élargie de `margin` cases de chaque côté.
  [[nodiscard]] static SolidGrid Around(ro::sim::FootprintSize size, std::int32_t margin);
  [[nodiscard]] bool Inside(std::int32_t east, std::int32_t north) const noexcept;
  // Les points couverts de la case ; 0 hors de la grille.
  [[nodiscard]] std::int32_t SamplesAt(std::int32_t east, std::int32_t north) const noexcept;
  [[nodiscard]] bool Solid(std::int32_t east, std::int32_t north) const noexcept;
};

// Les contours du solide `solid` posé par `pose`, en cases depuis le coin de
// l'emprise.
[[nodiscard]] std::vector<std::vector<glm::vec2>> PlacedSolidOutlines(const ModelSolid& solid,
                                                                      const PlannedPose& pose);

// Couvre `grid` du solide `solid` posé par `pose`.
void CoverModelSolid(const ModelSolid& solid, const PlannedPose& pose, SolidGrid& grid);

// LES POSES D'UNE PLACE pour le modèle `choice`, sa pièce tournée de
// `quarter` : la seule pose d'un modèle qui suit le quart de tour ; tous les
// angles et les deux échelles extrêmes d'un modèle à angle tiré.
[[nodiscard]] std::vector<PlannedPose> SlotSurveyPoses(const PieceModelSlot& slot,
                                                       const GroundModelChoice& choice,
                                                       ro::sim::FootprintSize drawn,
                                                       std::uint8_t quarter);

// LE SOLIDE D'UNE PIÈCE tournée de `quarter` : l'union, sur chaque place, de
// chaque modèle possible sous chacune de ses poses. Un modèle absent de
// `solids` s'ajoute à `missing`.
[[nodiscard]] SolidGrid PieceSolidGrid(const PieceLook& look, const ro::sim::TerrainPieceRule& rule,
                                       std::uint8_t quarter, std::span<const ModelSolid> solids,
                                       std::int32_t margin, std::vector<std::string_view>& missing);

// L'ÉCART ENTRE UNE EMPRISE ET SON SOLIDE.
struct FootprintAudit {
  // Les cases solides hors de l'emprise : le héros entre dans le décor.
  std::int32_t solid_outside = 0;
  // Les cases hors de l'emprise dont un point au moins est couvert : le
  // solide y déborde de plus d'un demi-pas de point (un huitième de case).
  std::int32_t touched_outside = 0;
  // Les cases bloquées où aucun point n'est couvert.
  std::int32_t empty_inside = 0;
  // Parmi elles, celles qu'aucune case solide ne touche par un côté : le
  // héros bute sur du vide.
  std::int32_t empty_far = 0;
  // Les cases bloquées et solides.
  std::int32_t matched = 0;
};

// LES TOLÉRANCES : le solide ne déborde nulle part de l'emprise de plus d'un
// huitième de case (`touched_outside` nul) ; une case bloquée vide se tolère au
// bord du solide (elle en touche une case solide), jamais au-delà.
[[nodiscard]] FootprintAudit AuditFootprint(const SolidGrid& grid,
                                            const ro::sim::TerrainPieceRule& rule,
                                            std::uint8_t quarter);

}  // namespace moonclient
