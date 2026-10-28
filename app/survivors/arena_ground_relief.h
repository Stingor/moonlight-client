#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/sim/survivors_terrain.h"

namespace moonclient {

// LE RELIEF DU SOL GÉNÉRÉ (Q3, B) : des collines douces, VISUELLES SEULEMENT —
// la simulation ignore la hauteur ; le sol dessiné, le GAT synthétique, les
// pieds, les modèles, le semis et les marques au sol la suivent. Une fonction
// du point du monde et de la graine, et des seules pièces qui l'approchent :
// deux fenêtres qui se recouvrent ont les mêmes hauteurs.
//
// Les hauteurs se comptent dans l'unité et le sens du fichier GND : une hauteur
// positive DESCEND. Une case vaut cinq unités.

// ── LE BRUIT VISUEL, partagé par tout ce que le plan tire de la graine : rien de
// ce qu'il donne ne décide une case.

// Un pas du finaliseur SplitMix64.
[[nodiscard]] std::uint64_t VisualFinalize(std::uint64_t value) noexcept;
// Le mélange entier de la graine, d'un sel et d'un point entier du monde.
[[nodiscard]] std::uint64_t VisualMix(std::uint64_t seed, std::uint64_t salt, std::int64_t x,
                                      std::int64_t y) noexcept;
// LE BRUIT DE VALEUR, dans [0, 1) : un nœud tous les `period_cells` coins de
// case, interpolé par le lissage cubique 3t² − 2t³.
[[nodiscard]] double VisualValueNoise(std::uint64_t seed, std::uint64_t salt, double east,
                                      double north, double period_cells) noexcept;

// LE BRUIT À GRADIENTS : chaque nœud, tous les `period_cells` coins de case,
// porte une pente unité dans l'une de `kGradientDirections` directions tirées
// de la graine ; la surface passe à zéro par chaque nœud avec sa pente, et se
// mêle d'un nœud à l'autre par le même lissage. Sa pente, plus régulière que
// celle du bruit de valeur, ne s'écrase pas entre deux nœuds de même hauteur :
// à plafond égal, les collines se voient.
inline constexpr std::uint32_t kGradientDirections = 8;
// LA CELLULE DU BRUIT : les directions (rang dans [0, kGradientDirections)) des
// nœuds sud-ouest, sud-est, nord-ouest et nord-est, et le point (tx, ty) dans
// [0, 1]² ; la valeur, puis ses dérivées selon tx et ty.
struct GradientCell {
  double value = 0.0;
  double along_east = 0.0;
  double along_north = 0.0;
};
[[nodiscard]] GradientCell GradientNoiseIn(const std::array<std::uint32_t, 4>& directions,
                                           double tx, double ty) noexcept;
// Ses bornes, que l'essai retrouve en parcourant toutes les directions des
// quatre nœuds : la valeur reste sous √½ ; la pente sur un axe, par période,
// sous 1,68 (1,6794 au plus).
inline constexpr double kGradientNoiseMost = 0.7072;
inline constexpr double kGradientNoiseAxisSteepest = 1.68;
// Le bruit au point du monde, dans [−kGradientNoiseMost, kGradientNoiseMost].
[[nodiscard]] double VisualGradientNoise(std::uint64_t seed, std::uint64_t salt, double east,
                                         double north, double period_cells) noexcept;

// ── LES COLLINES : deux ondulations de la graine, l'une ample et large, l'autre
// plus courte, qui casse la régularité de la première.
inline constexpr double kReliefHillPeriodCells = 64.0;
inline constexpr double kReliefHillUnits = 22.0;
inline constexpr double kReliefSwellPeriodCells = 24.0;
inline constexpr double kReliefSwellUnits = 6.0;
// L'écart le plus grand du sol à son niveau moyen, de part et d'autre.
inline constexpr double kGroundReliefMostUnits = kReliefHillUnits + kReliefSwellUnits;
// Le plus grand écart de hauteur entre deux points du sol : un sommet et un creux.
inline constexpr double kGroundReliefSpanUnits = 2.0 * kGroundReliefMostUnits;
inline constexpr double kGroundReliefCellUnits = 5.0;

// ── LE PLAFOND DE PENTE, en unités de hauteur par unité au sol.
//
// UNE CRÊTE NE CACHE JAMAIS UN MONSTRE : le point du sol P se voit tant que le
// sol, entre P et la caméra, reste sous la visée, qui monte de tan(φ) par unité
// au sol, φ étant sa plongée. Un sol dont la pente reste sous tan(φ) n'y monte
// jamais ; et un sprite se peint à la profondeur de son plan debout, plus haut
// que ses pieds : ses pieds vus, tout le sprite l'est. La visée la plus rase de
// la caméra de l'arène (élévation 50°, champ 15°, largeur bornée au 21:9) est
// celle d'un coin du haut de l'image, à 40,3° sous l'horizon (tan 0,848), au
// même angle à tout recul — descente d'entrée comprise. Le plafond en garde la
// MOITIÉ : la visée garde au-dessus du sol au moins la moitié de sa montée, de
// quoi tenir la secousse, l'arrondi du sol et la part d'un sprite couchée
// vers la caméra. `ArenaShallowestSightTangent` le vérifie pour chaque format.
inline constexpr double kGroundSlopeCap = 0.42;
inline constexpr double kGroundSightMarginShare = 0.5;

// LE PLAFOND SUR CHAQUE AXE : une pente qui ne dépasse jamais ceci vers l'est
// ni vers le nord, en aucun point, n'en prend jamais plus de √2 fois dans
// aucune direction ; et un triangle du sol, dont deux côtés suivent les axes,
// non plus, quelle que soit la courbure entre ses coins. C'est ce plafond que
// tient tout le relief : collines et terrasses.
inline constexpr double kGroundAxisSlopeCap = 0.29;
static_assert(2.0 * kGroundAxisSlopeCap * kGroundAxisSlopeCap <= kGroundSlopeCap * kGroundSlopeCap,
              "le plafond par axe dépasserait le plafond de pente");

// LA PENTE LA PLUS RAIDE QUE LES COLLINES PEUVENT PRENDRE SUR UN AXE, bornée par
// le calcul : chaque ondulation, son amplitude rapportée à la valeur la plus
// haute du bruit, y monte au plus de kGradientNoiseAxisSteepest fois cette
// amplitude par période.
inline constexpr double kGroundAxisSlopeBound =
    kGradientNoiseAxisSteepest / kGradientNoiseMost *
    (kReliefHillUnits / (kReliefHillPeriodCells * kGroundReliefCellUnits) +
     kReliefSwellUnits / (kReliefSwellPeriodCells * kGroundReliefCellUnits));
static_assert(kGroundAxisSlopeBound <= kGroundAxisSlopeCap,
              "les collines dépasseraient le plafond de pente");

// ── LES TERRASSES : un repère se dresse sur un sol plat. Le sol de sa place et
// de son site (ou de son emprise, sans place), élargi de `kReliefPadMarginCells`,
// se tient à la hauteur que les collines ont en son centre. Autour, l'écart
// des collines à cette hauteur se BRIDE : il reste sous la pente du plafond par
// axe fois la distance à la terrasse (arrondie sur `kReliefPadRoundCells`, pour
// que le bord de la terrasse ne fasse pas d'arête), et la bride s'adoucit en
// un genou dès que l'écart passe `kReliefPadKneeStart` fois la borne, pour que
// le sol rejoigne les collines sans pli. Une bride ne monte pas plus vite que
// ce qu'elle bride, ni que sa borne : le plafond par axe tient.
//
// Les collines restant à `kGroundReliefMostUnits` de leur niveau moyen, la
// bride ne touche rien au-delà de `kReliefPadReachCells` de la terrasse.
inline constexpr double kReliefPadMarginCells = 2.0;
inline constexpr double kReliefPadRoundCells = 4.0;
inline constexpr double kReliefPadKneeStart = 0.7;
inline constexpr double kReliefPadReachCells =
    kGroundReliefSpanUnits / (kReliefPadKneeStart * kGroundAxisSlopeCap * kGroundReliefCellUnits) +
    kReliefPadRoundCells / 2.0;

struct GroundReliefPad {
  // Le rectangle plat, en cases du monde (coins de case).
  double west = 0.0;
  double south = 0.0;
  double east = 0.0;
  double north = 0.0;
  // Sa hauteur, dans l'unité et le sens du fichier.
  double height = 0.0;
  [[nodiscard]] bool operator==(const GroundReliefPad&) const = default;
};

// LA TERRASSE D'UNE PIÈCE posée, ou rien si ce n'est pas un repère.
[[nodiscard]] std::optional<GroundReliefPad> GroundReliefPadOf(
    const ro::sim::TerrainRule& rule, std::uint64_t run_seed,
    const ro::sim::TerrainPieceRule& piece, const ro::sim::PlacedPiece& placed);

// Les terrasses dans l'ordre où elles se brident, d'ouest en est puis du sud au
// nord : le même pour toute fenêtre qui les lit.
void SortReliefPads(std::vector<GroundReliefPad>& pads);

// Les collines seules, au coin de case (east, north) du monde.
[[nodiscard]] double GroundHillsAt(std::uint64_t run_seed, double world_east,
                                   double world_north) noexcept;

// LA HAUTEUR DU SOL au point (east, north) du monde : les collines, aplanies sur
// les terrasses `pads`, bridées l'une après l'autre dans l'ordre de
// `SortReliefPads`. Les terrasses absentes de `pads` n'y comptent pas : il
// faut toutes celles à moins de `kReliefPadReachCells` du point.
[[nodiscard]] float GroundReliefAt(std::uint64_t run_seed, double world_east, double world_north,
                                   std::span<const GroundReliefPad> pads = {});

// LA PENTE DU SOL au point (east, north) du monde, en unités de hauteur du
// fichier par unité au sol, vers l'est et vers le nord : une différence centrée
// sur `kReliefSlopeProbeCells` de part et d'autre.
[[nodiscard]] glm::dvec2 GroundReliefSlopeAt(std::uint64_t run_seed, double world_east,
                                             double world_north,
                                             std::span<const GroundReliefPad> pads = {});
inline constexpr double kReliefSlopeProbeCells = 0.5;

// LA NORMALE D'UNE PENTE (vers l'est, vers le nord, en unités de hauteur du
// fichier par unité au sol), dans le repère du monde (x vers l'est, y vers le
// haut, z vers le sud), normalisée.
[[nodiscard]] glm::vec3 GroundNormalOfSlope(glm::dvec2 slope);
// Celle des quatre coins d'une case (sud-ouest, sud-est, nord-ouest, nord-est,
// comme le GAT) : la pente moyenne de ses côtés.
[[nodiscard]] glm::vec3 GroundCellNormal(const std::array<float, 4>& corner_heights);

// LA NORMALE DU SOL en ce point, dans le repère du monde, normalisée.
[[nodiscard]] glm::vec3 GroundReliefNormalAt(std::uint64_t run_seed, double world_east,
                                             double world_north,
                                             std::span<const GroundReliefPad> pads = {});

// D'où vient la lumière du soleil de référence, dans le repère du monde, et ce
// qu'elle porte : voir `ro::render::SceneLight`. Sans diffuse, aucune pente ne
// s'assombrit.
struct GroundPlanLight {
  glm::vec3 toward_sun{0.0F, 1.0F, 0.0F};
  glm::vec3 ambient{1.0F, 1.0F, 1.0F};
  glm::vec3 diffuse{0.0F, 0.0F, 0.0F};
};

// LA LUMIÈRE D'UNE PENTE, rapportée à celle du plat : l'ambiante et la diffuse
// du soleil de référence sous la normale `normal`, sur les mêmes sous la
// verticale, en moyenne sur les trois canaux. Un au plat, plus vers le soleil,
// moins à l'opposé.
[[nodiscard]] float GroundSlopeLight(glm::vec3 normal, const GroundPlanLight& light);

// LA PENTE CUITE DANS LA COULEUR DU SOL. Le sol de RO s'éclaire à la verticale,
// et son nuanceur sature au plein jour : une pente n'y change rien. Elle se cuit
// donc dans la couleur des cubes, comme une carte cuit son relief dans ses
// lightmaps, et comme elles, MARQUÉE : l'écart de lumière de la pente au plat,
// multiplié par `kReliefLightGain`, autour du plat tenu à `kGroundFlatShade`
// pour que les pentes au soleil s'éclaircissent ; au plus sombre,
// `kDarkestSlopeShade`. Une pente douce au soleil couchant se lit ; une pente
// au plafond, à l'opposé du soleil, perd moins de la moitié de sa lumière.
inline constexpr float kGroundFlatShade = 0.93F;
inline constexpr float kReliefLightGain = 2.5F;
inline constexpr float kDarkestSlopeShade = 0.55F;
[[nodiscard]] float GroundSlopeShade(glm::vec3 normal, const GroundPlanLight& light);

// LA LUMIÈRE D'UN ACTEUR SUR CETTE PENTE, dans [0, 1] : celle du sol rapportée
// au plat. Un acteur s'assombrit sur une pente à l'ombre, comme le sol sous
// lui, et garde sa lumière ailleurs.
[[nodiscard]] float GroundSlopeActorShade(glm::vec3 normal, const GroundPlanLight& light);

}  // namespace moonclient
