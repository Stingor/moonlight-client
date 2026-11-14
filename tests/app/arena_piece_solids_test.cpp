// LE DÉCOR NE MENT PAS SUR LA CARTE : le solide des modèles que le sol généré
// pose sur une pièce tombe dans son emprise de `sim`, sous chaque quart de
// tour, et l'emprise ne bloque pas de case loin de lui.
//
// Le solide des vrais modèles est le relevé figé (`SurveyedModelSolids`,
// `roc-cli survivors emprises --releve`) : aucun fichier de l'installation
// n'est lu. Des solides synthétiques asymétriques éprouvent la règle de pose
// elle-même — quart de tour, angle du RSW, recentrage et milieu du solide.

#include "survivors/arena_piece_solids.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_terrain.h"

#include "survivors/arena_ground_look.h"
#include "survivors/arena_ground_plan.h"

using moonclient::AuditFootprint;
using moonclient::FootprintAudit;
using moonclient::GroundModelChoice;
using moonclient::ModelSolid;
using moonclient::PieceLook;
using moonclient::PieceModelSlot;
using moonclient::PieceSolidGrid;
using moonclient::PlacedSolidOutlines;
using moonclient::SolidGrid;
using moonclient::SolidOutline;
using moonclient::SolidPoint;
using ro::sim::TerrainPieceRule;

namespace {

constexpr std::uint8_t kQuarterTurns = ro::sim::kTerrainQuarterTurns;
// La grille lue déborde de l'emprise d'autant de cases.
constexpr std::int32_t kMarginCells = 4;
// La première famille de RSM : celle des modèles du catalogue.
constexpr std::int32_t kFirstFamilyRsm = 0x0104;

[[nodiscard]] const TerrainPieceRule* RuleOf(const ro::sim::TerrainRule& terrain,
                                             std::string_view key) {
  for (const TerrainPieceRule& piece : terrain.pieces) {
    if (std::string_view(piece.key) == key) return &piece;
  }
  return nullptr;
}

// ── UN SOLIDE SYNTHÉTIQUE ASYMÉTRIQUE : une boîte de 4 × 1 cases (20 × 5
// unités) dont seule la moitié est est solide. Posé sur la place (2, ½) d'une
// emprise « ..## », son solide couvre les deux cases de l'est.
constexpr std::array<SolidPoint, 4> kEastHalf{
    {{0.0F, -2.5F}, {10.0F, -2.5F}, {10.0F, 2.5F}, {0.0F, 2.5F}}};
constexpr std::array<SolidOutline, 1> kEastHalfOutlines{{{kEastHalf}}};
constexpr std::string_view kEastHalfModel = "essai\\moitie_est.rsm";
constexpr ModelSolid kEastHalfSolid{.model = kEastHalfModel,
                                    .scale = 1.0F,
                                    .rsm_version = kFirstFamilyRsm,
                                    .bounds_min = {-10.0F, -5.0F, -2.5F},
                                    .bounds_max = {10.0F, 0.0F, 2.5F},
                                    .outlines = kEastHalfOutlines};
constexpr std::array<GroundModelChoice, 1> kEastHalfChoice{{{.model = kEastHalfModel}}};
constexpr std::array<std::string_view, 1> kEastHalfFootprint = {"..##"};

// ── UN SOLIDE DÉCENTRÉ : un carré d'une case, à deux cases à l'est du pivot
// et une au sud ; le catalogue déclare ce milieu pour que, sous tout angle
// tiré, il tombe sur la place.
constexpr std::array<SolidPoint, 4> kOffSquare{
    {{7.5F, -7.5F}, {12.5F, -7.5F}, {12.5F, -2.5F}, {7.5F, -2.5F}}};
constexpr std::array<SolidOutline, 1> kOffSquareOutlines{{{kOffSquare}}};
constexpr std::string_view kOffSquareModel = "essai\\decentre.rsm";
constexpr ModelSolid kOffSquareSolid{.model = kOffSquareModel,
                                     .scale = 1.0F,
                                     .rsm_version = kFirstFamilyRsm,
                                     .bounds_min = {-15.0F, -5.0F, -15.0F},
                                     .bounds_max = {15.0F, 0.0F, 15.0F},
                                     .outlines = kOffSquareOutlines};
constexpr float kOffSquareEastCells = 2.0F;
constexpr float kOffSquareNorthCells = -1.0F;
constexpr std::array<GroundModelChoice, 1> kOffSquareChoice{
    {{.model = kOffSquareModel,
      .centre_east_cells = kOffSquareEastCells,
      .centre_north_cells = kOffSquareNorthCells}}};
constexpr std::array<std::string_view, 2> kSquareFootprint = {"##", "##"};

}  // namespace

TEST_CASE("Emprises : chaque modèle du catalogue livré est au relevé figé", "[app][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  std::size_t checked = 0;
  for (const ro::sim::TerrainRule& terrain : rules.terrains) {
    const moonclient::BiomeLook* biome = moonclient::FindBiomeLook(terrain.key);
    if (biome == nullptr) continue;
    for (const PieceLook& look : biome->pieces) {
      for (const PieceModelSlot& slot : look.slots) {
        for (const GroundModelChoice& choice : slot.models) {
          INFO(std::string(choice.model)
               << " à l'échelle " << choice.scale << ", enfoncé de " << choice.sink_units
               << " : relancer roc-cli survivors emprises --releve");
          CHECK(moonclient::FindModelSolid(moonclient::SurveyedModelSolids(), choice) != nullptr);
          ++checked;
        }
      }
    }
  }
  CHECK(checked > 0);
  // Un relevé lu sous une autre hauteur de solide est périmé.
  CHECK(moonclient::SurveyedSolidHeightUnits() == moonclient::kSolidHeightUnits);
}

TEST_CASE("Emprises : le solide de chaque pièce livrée tombe dans son emprise tournée",
          "[app][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  std::size_t audited = 0;
  for (const ro::sim::TerrainRule& terrain : rules.terrains) {
    const moonclient::BiomeLook* biome = moonclient::FindBiomeLook(terrain.key);
    if (biome == nullptr) continue;
    for (const PieceLook& look : biome->pieces) {
      const TerrainPieceRule* rule = RuleOf(terrain, look.key);
      REQUIRE(rule != nullptr);
      for (std::uint8_t quarter = 0; quarter < kQuarterTurns; ++quarter) {
        std::vector<std::string_view> missing;
        const SolidGrid grid = PieceSolidGrid(
            look, *rule, quarter, moonclient::SurveyedModelSolids(), kMarginCells, missing);
        const FootprintAudit audit = AuditFootprint(grid, *rule, quarter);
        INFO(std::string(look.key) << ", " << static_cast<int>(quarter) << " quart(s) de tour");
        CHECK(missing.empty());
        // Rien de solide hors de l'emprise : le héros n'entre pas dans le décor.
        CHECK(audit.solid_outside == 0);
        // LA TOLÉRANCE DU BORD : le solide ne déborde de l'emprise de plus
        // d'un huitième de case nulle part, pas même sur un coin de case.
        CHECK(audit.touched_outside == 0);
        // LA TOLÉRANCE : une case bloquée sans solide touche une case solide.
        CHECK(audit.empty_far == 0);
        CHECK(audit.matched > 0);
        ++audited;
      }
    }
  }
  CHECK(audited > 0);
}

TEST_CASE("Emprises : un modèle à angle imposé tourne avec l'emprise de sa pièce",
          "[app][survivors]") {
  constexpr std::array<PieceModelSlot, 1> kSlots{
      {{.east_cells = 2.0F, .north_cells = 0.5F, .models = kEastHalfChoice, .any_yaw = false}}};
  const PieceLook look{.key = "essai", .slots = kSlots};
  const TerrainPieceRule rule{.piece_id = 1, .key = "essai", .footprint = kEastHalfFootprint};
  const std::array<ModelSolid, 1> solids = {kEastHalfSolid};
  for (std::uint8_t quarter = 0; quarter < kQuarterTurns; ++quarter) {
    std::vector<std::string_view> missing;
    const SolidGrid grid = PieceSolidGrid(look, rule, quarter, solids, kMarginCells, missing);
    const FootprintAudit audit = AuditFootprint(grid, rule, quarter);
    INFO(static_cast<int>(quarter) << " quart(s) de tour");
    CHECK(missing.empty());
    // Les deux cases de l'est, tournées, et rien d'autre.
    CHECK(audit.matched == 2);
    CHECK(audit.solid_outside == 0);
    CHECK(audit.empty_inside == 0);
  }
}

TEST_CASE("Emprises : le milieu déclaré du solide tombe sur la place sous tout angle",
          "[app][survivors]") {
  constexpr std::array<PieceModelSlot, 1> kSlots{
      {{.east_cells = 1.0F, .north_cells = 1.0F, .models = kOffSquareChoice}}};
  const ro::sim::FootprintSize drawn{.width = 2, .height = 2};
  // La place est au coin commun des quatre cases : le solide, un carré d'une
  // case centré sur elle, ne s'en écarte pas de plus d'une demi-diagonale.
  constexpr float kHalfDiagonalCells = 0.7072F;
  for (std::uint8_t quarter = 0; quarter < kQuarterTurns; ++quarter) {
    for (const moonclient::PlannedPose& pose :
         moonclient::SlotSurveyPoses(kSlots[0], kOffSquareChoice[0], drawn, quarter)) {
      for (const auto& outline : PlacedSolidOutlines(kOffSquareSolid, pose)) {
        for (const glm::vec2& point : outline) {
          INFO("angle " << pose.yaw_degrees << ", échelle " << pose.scale);
          CHECK(glm::length(point - glm::vec2(1.0F, 1.0F)) <=
                kHalfDiagonalCells * (1.0F + moonclient::kPieceScaleJitter));
        }
      }
    }
  }
  // Et l'emprise de 2 × 2 le contient sous chacun.
  const PieceLook look{.key = "essai", .slots = kSlots};
  const TerrainPieceRule rule{.piece_id = 1, .key = "essai", .footprint = kSquareFootprint};
  const std::array<ModelSolid, 1> solids = {kOffSquareSolid};
  std::vector<std::string_view> missing;
  const SolidGrid grid = PieceSolidGrid(look, rule, 0, solids, kMarginCells, missing);
  const FootprintAudit audit = AuditFootprint(grid, rule, 0);
  CHECK(audit.solid_outside == 0);
  CHECK(audit.matched == 4);
}

TEST_CASE("Emprises : un solide hors de l'emprise et une case bloquée sur du vide se voient",
          "[app][survivors]") {
  // La moitié est posée une case trop à l'ouest : une case solide sort de
  // l'emprise, et la case de l'est, bloquée, reste vide.
  constexpr std::array<PieceModelSlot, 1> kShifted{
      {{.east_cells = 1.0F, .north_cells = 0.5F, .models = kEastHalfChoice, .any_yaw = false}}};
  const PieceLook look{.key = "essai", .slots = kShifted};
  const TerrainPieceRule rule{.piece_id = 1, .key = "essai", .footprint = kEastHalfFootprint};
  const std::array<ModelSolid, 1> solids = {kEastHalfSolid};
  std::vector<std::string_view> missing;
  const SolidGrid grid = PieceSolidGrid(look, rule, 0, solids, kMarginCells, missing);
  const FootprintAudit audit = AuditFootprint(grid, rule, 0);
  CHECK(audit.solid_outside == 1);
  CHECK(audit.empty_inside == 1);
  // Elle touche la case solide de l'ouest : tolérée au bord du solide.
  CHECK(audit.empty_far == 0);
  // Une emprise plus large que le solide d'une case de trop, elle, ne l'est pas.
  constexpr std::array<std::string_view, 1> kWide = {"..####"};
  const TerrainPieceRule wide{.piece_id = 1, .key = "essai", .footprint = kWide};
  constexpr std::array<PieceModelSlot, 1> kPlaced{
      {{.east_cells = 2.0F, .north_cells = 0.5F, .models = kEastHalfChoice, .any_yaw = false}}};
  const PieceLook placed{.key = "essai", .slots = kPlaced};
  const SolidGrid wide_grid = PieceSolidGrid(placed, wide, 0, solids, kMarginCells, missing);
  CHECK(AuditFootprint(wide_grid, wide, 0).empty_far == 1);
}

TEST_CASE("Emprises : un solide qui effleure une case hors de l'emprise se voit",
          "[app][survivors]") {
  // La moitié est (2 × 1 cases) posée d'un cinquième de case trop à l'est et
  // au nord, dans une emprise de deux rangs : elle couvre la première colonne
  // de points des deux cases à l'est de l'emprise, sans le point du bas de
  // la première — moins du quart de chacune, pas solide, mais effleurée.
  constexpr float kNudgeCells = 0.2F;
  constexpr std::array<std::string_view, 2> kTwoRows = {"..##", "..##"};
  constexpr std::array<PieceModelSlot, 1> kNudged{{{.east_cells = 2.0F + kNudgeCells,
                                                    .north_cells = 0.5F + kNudgeCells,
                                                    .models = kEastHalfChoice,
                                                    .any_yaw = false}}};
  const PieceLook look{.key = "essai", .slots = kNudged};
  const TerrainPieceRule rule{.piece_id = 1, .key = "essai", .footprint = kTwoRows};
  const std::array<ModelSolid, 1> solids = {kEastHalfSolid};
  std::vector<std::string_view> missing;
  const SolidGrid grid = PieceSolidGrid(look, rule, 0, solids, kMarginCells, missing);
  const FootprintAudit audit = AuditFootprint(grid, rule, 0);
  CHECK(audit.solid_outside == 0);
  CHECK(audit.touched_outside == 2);
  // À sa place, rien n'effleure.
  constexpr std::array<PieceModelSlot, 1> kPlaced{
      {{.east_cells = 2.0F, .north_cells = 0.5F, .models = kEastHalfChoice, .any_yaw = false}}};
  const PieceLook placed{.key = "essai", .slots = kPlaced};
  const SolidGrid placed_grid = PieceSolidGrid(placed, rule, 0, solids, kMarginCells, missing);
  CHECK(AuditFootprint(placed_grid, rule, 0).touched_outside == 0);
}
