// L'OUTIL RELIEF DU MODE ÉDITEUR : un glissé de brosse, le mode Sommets,
// l'historique, et le sol qui suit le document.

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/mapedit/ground_edit.h"
#include "ro/render/map_scene.h"
#include "ro/sim/walk_map.h"
#include "ro/ui/map_editor_panel.h"

#include "support/map_builder.h"
#include "world/map_editor_session.h"
#include "world/map_stage.h"

using Catch::Approx;
using moonclient::MapEditorSession;
using moonclient::MapEditStore;
namespace intents = ro::ui::map_editor;

namespace {

constexpr const char* kMap = "relief";
constexpr std::int32_t kSide = 4;
// Le point du milieu de la carte, dans la grille des coins.
constexpr std::array<float, 2> kMiddle = {2.0F, 2.0F};
// Les heures de deux pas de brosse, en millisecondes : le premier pose
// l'horloge, le second dure cinquante millisecondes.
constexpr double kFirstStepMs = 1000.0;
constexpr double kSecondStepMs = 1050.0;

// Une carte plate de 4 × 4 cubes, sans modèle, ses hauteurs de fichier gardées.
ro::render::MapScene FlatMap() {
  ro::test_support::GndBuildOptions ground;
  ground.width = kSide;
  ground.height = kSide;
  auto gnd = ro::formats::GndMap::Parse(ro::test_support::BuildGnd(ground));
  auto rsw = ro::formats::RswScene::Parse(ro::test_support::BuildRsw({}));
  REQUIRE(gnd.has_value());
  REQUIRE(rsw.has_value());
  ro::render::MapScene scene{.gnd = std::move(*gnd), .rsw = std::move(*rsw)};
  for (const ro::formats::GndCube& cube : scene.gnd.cubes()) {
    scene.file_corner_heights.push_back(cube.corner_height);
  }
  return scene;
}

// Une session ouverte, la brosse de relief en main.
MapEditorSession WithBrush(const ro::render::MapScene& scene, MapEditStore& store) {
  MapEditorSession session;
  session.FollowMap(kMap, scene);
  static_cast<void>(session.Apply(intents::Toggle{}, scene, store));
  static_cast<void>(session.Apply(intents::SetTool{ro::ui::MapEditorTool::kRelief}, scene, store));
  return session;
}

// Un glissé de deux pas au milieu de la carte, sans le relâcher.
void Stroke(MapEditorSession& session, const ro::render::MapScene& scene, MapEditStore& store,
            bool lower) {
  session.SetGroundPointer(kMiddle);
  REQUIRE(session.Grab({0.0F, 0.0F}, ro::ui::MapEditorView()));
  session.StrokeGround(kFirstStepMs, lower, scene, store);
  session.StrokeGround(kSecondStepMs, lower, scene, store);
}

void Release(MapEditorSession& session, const ro::render::MapScene& scene, MapEditStore& store) {
  session.Release();
  for (const ro::ui::MapEditorIntent& intent : session.TakeQueued()) {
    static_cast<void>(session.Apply(intent, scene, store));
  }
}

}  // namespace

TEST_CASE("Relief : un glissé de brosse est UNE entrée, et annuler rend le sol", "[app][mapedit]") {
  ro::render::MapScene scene = FlatMap();
  MapEditStore store;
  MapEditorSession session = WithBrush(scene, store);
  Stroke(session, scene, store, false);
  // Pendant le glissé : le document change, l'historique attend.
  const ro::mapedit::MapEdit& during = store.Document(kMap);
  REQUIRE_FALSE(during.ground().empty());
  CHECK(session.View(scene, store).undo_label.empty());
  CHECK(session.stroking_ground());
  // Le centre a monté de la force pendant cinquante millisecondes.
  const ro::ui::MapEditorBrush brush;
  const auto center = std::ranges::find_if(during.ground(), [](const auto& point) {
    return point.corner_x == 2 && point.corner_y == 2;
  });
  REQUIRE(center != during.ground().end());
  constexpr double kMillisecondsPerSecond = 1000.0;
  CHECK(center->lift == Approx(static_cast<double>(brush.units_per_second) *
                               (kSecondStepMs - kFirstStepMs) / kMillisecondsPerSecond));
  // La raison générique est posée.
  CHECK_FALSE(during.ground_why().empty());

  Release(session, scene, store);
  CHECK_FALSE(session.stroking_ground());
  CHECK_FALSE(session.View(scene, store).undo_label.empty());
  CHECK(session.View(scene, store).ground.edited_points == store.Document(kMap).ground().size());

  // Le sol suit le document : levé, puis rendu par l'annulation.
  static_cast<void>(ro::render::ReshapeSceneGround(scene, store.Document(kMap).ground()));
  CHECK(ro::mapedit::CornerPointHeight(scene.gnd, 2, 2).value_or(0.0F) > 0.0F);
  static_cast<void>(session.Apply(intents::Undo{}, scene, store));
  CHECK(store.Document(kMap).ground().empty());
  static_cast<void>(ro::render::ReshapeSceneGround(scene, store.Document(kMap).ground()));
  CHECK(ro::mapedit::CornerPointHeight(scene.gnd, 2, 2).value_or(1.0F) == 0.0F);
}

TEST_CASE("Relief : Maj abaisse, et « Rétablir le relief » retire tout", "[app][mapedit]") {
  const ro::render::MapScene scene = FlatMap();
  MapEditStore store;
  MapEditorSession session = WithBrush(scene, store);
  Stroke(session, scene, store, true);
  Release(session, scene, store);
  for (const ro::mapedit::GroundEdit& point : store.Document(kMap).ground()) {
    CHECK(point.lift < 0.0F);
  }
  static_cast<void>(session.Apply(intents::RevertGround{}, scene, store));
  CHECK(store.Document(kMap).ground().empty());
  CHECK(store.Document(kMap).empty());
  // Le retour est lui-même un geste : l'annuler rend les retouches.
  static_cast<void>(session.Apply(intents::Undo{}, scene, store));
  CHECK_FALSE(store.Document(kMap).ground().empty());
}

TEST_CASE("Relief : hors du sol, l'appui ne saisit rien", "[app][mapedit]") {
  const ro::render::MapScene scene = FlatMap();
  MapEditStore store;
  MapEditorSession session = WithBrush(scene, store);
  session.SetGroundPointer(std::nullopt);
  CHECK_FALSE(session.Grab({0.0F, 0.0F}, ro::ui::MapEditorView()));
  CHECK_FALSE(session.dragging());
}

TEST_CASE("Relief : la brosse se règle dans ses bornes", "[app][mapedit]") {
  const ro::render::MapScene scene = FlatMap();
  MapEditStore store;
  MapEditorSession session = WithBrush(scene, store);
  static_cast<void>(session.Apply(
      intents::SetBrush{ro::ui::MapEditorBrush{.radius_cells = 1000.0F,
                                               .units_per_second = 0.0F,
                                               .mode = ro::ui::MapEditorBrushMode::kSmooth}},
      scene, store));
  const ro::ui::MapEditorBrush& shown = session.View(scene, store).ground.brush;
  CHECK(shown.radius_cells == ro::ui::MapEditorBrush::kGreatestRadiusCells);
  CHECK(shown.units_per_second == ro::ui::MapEditorBrush::kLeastUnitsPerSecond);
  CHECK(session.Brush().mode == ro::mapedit::GroundBrushMode::kSmooth);
  // Le rayon se compte en cases, la brosse en cubes.
  CHECK(session.Brush().radius_cubes == Approx(shown.radius_cells / 2.0F));
}

TEST_CASE("Relief : sans ombres en temps réel, le sol d'origine et le document intact",
          "[app][mapedit]") {
  moonclient::MapStage stage{"relief", FlatMap(), ro::sim::WalkMap()};
  constexpr std::int32_t kCells = kSide * 2;
  auto gat = ro::formats::GatMap::Parse(ro::test_support::BuildGat(
      kCells, kCells,
      std::vector<ro::test_support::GatCellSpec>(static_cast<std::size_t>(kCells * kCells))));
  REQUIRE(gat.has_value());
  for (const ro::formats::GatCell& cell : gat->cells()) {
    stage.file_walk_heights.push_back(cell.corner_height);
  }
  stage.gat = std::move(*gat);
  ro::mapedit::MapEdit document;
  document.LiftGround({.corner_x = 2, .corner_y = 2, .lift = 3.0F});
  document.SetGroundWhy("une butte");
  const ro::mapedit::MapEdit kept = document;

  // Réglage allumé : le sol et la grille de marche montent, le maillage attend.
  CHECK(moonclient::ReshapeStageGround(stage, moonclient::ShownGroundEdits(document, true)));
  CHECK(stage.ground_mesh_stale);
  CHECK(ro::mapedit::CornerPointHeight(stage.scene.gnd, 2, 2).value_or(0.0F) == Approx(3.0F));
  CHECK(stage.gat->at(4, 4).corner_height[0] == Approx(-3.0F));
  // Rien n'a changé depuis : rien ne se repose.
  CHECK_FALSE(moonclient::ReshapeStageGround(stage, moonclient::ShownGroundEdits(document, true)));

  // Réglage coupé : le sol du fichier revient, et le document garde tout.
  CHECK(moonclient::ReshapeStageGround(stage, moonclient::ShownGroundEdits(document, false)));
  CHECK(ro::mapedit::CornerPointHeight(stage.scene.gnd, 2, 2).value_or(1.0F) == 0.0F);
  CHECK(stage.gat->at(4, 4).corner_height[0] == 0.0F);
  CHECK(document == kept);

  // Et l'outil Relief se refuse.
  const ro::render::MapScene scene = FlatMap();
  MapEditStore store;
  MapEditorSession session = WithBrush(scene, store);
  session.SetGroundAllowed(false);
  CHECK(session.tool() == ro::ui::MapEditorTool::kMove);
  static_cast<void>(session.Apply(intents::SetTool{ro::ui::MapEditorTool::kRelief}, scene, store));
  CHECK(session.tool() == ro::ui::MapEditorTool::kMove);
  CHECK_FALSE(session.View(scene, store).ground.allowed);
}

// ── LE MODE SOMMETS ─────────────────────────────────────────────────────────

namespace {

using CornerPoint = MapEditorSession::CornerPoint;

// Les points de la grille à l'écran, loin les uns des autres : le point (x, y)
// tombe sur le pixel (x, y) × `kDotSpacing`.
constexpr float kDotSpacing = 100.0F;
// Un appui à mi-chemin de deux points : dans le vide.
constexpr glm::vec2 kVoid{50.0F, 50.0F};
// La poignée verticale : `kHandlePixels` vers le haut de l'écran pour une
// longueur de `kHandleLength` unités du monde.
constexpr float kHandleLength = 10.0F;
constexpr float kHandlePixels = 50.0F;

glm::vec2 DotAt(std::int32_t x, std::int32_t y) {
  return {static_cast<float>(x) * kDotSpacing, static_cast<float>(y) * kDotSpacing};
}

// Une session ouverte, l'outil Relief en main en mode Sommets, chaque point de
// la grille à l'écran.
MapEditorSession WithPoints(const ro::render::MapScene& scene, MapEditStore& store) {
  MapEditorSession session = WithBrush(scene, store);
  static_cast<void>(session.Apply(
      intents::SetBrush{ro::ui::MapEditorBrush{.mode = ro::ui::MapEditorBrushMode::kPoints}}, scene,
      store));
  std::vector<MapEditorSession::GroundDot> dots;
  for (std::int32_t y = 0; y <= kSide; ++y) {
    for (std::int32_t x = 0; x <= kSide; ++x) {
      dots.push_back({.at = DotAt(x, y), .point = {x, y}});
    }
  }
  session.SetGroundDots(std::move(dots));
  return session;
}

// La poignée verticale posée sur le point de l'écran `at`.
void ShowHandle(MapEditorSession& session, glm::vec2 at) {
  const glm::vec2 up{0.0F, -kHandlePixels};
  session.SetGizmo(
      moonclient::GizmoFrame{.length = kHandleLength},
      {moonclient::GizmoHandle{
          .axis = moonclient::GizmoAxis::kY, .points = {at, at + up}, .screen_axis = up}});
}

void Click(MapEditorSession& session, glm::vec2 at, bool extend = false) {
  REQUIRE(session.Grab(at, ro::ui::MapEditorView(), extend));
  session.Release();
}

void Flush(MapEditorSession& session, const ro::render::MapScene& scene, MapEditStore& store) {
  for (const ro::ui::MapEditorIntent& intent : session.TakeQueued()) {
    static_cast<void>(session.Apply(intent, scene, store));
  }
}

float LiftAt(const MapEditStore& store, std::int32_t x, std::int32_t y) {
  for (const ro::mapedit::GroundEdit& edit : store.Document(kMap).ground()) {
    if (edit.corner_x == x && edit.corner_y == y) {
      return edit.lift;
    }
  }
  return 0.0F;
}

}  // namespace

TEST_CASE("Relief, Sommets : un clic désigne, Maj+clic ajoute ou retire, le vide et Échap vident",
          "[app][mapedit]") {
  const ro::render::MapScene scene = FlatMap();
  MapEditStore store;
  MapEditorSession session = WithPoints(scene, store);
  CHECK(session.ground_points_mode());

  Click(session, DotAt(1, 1));
  CHECK(session.designated_ground_point() == CornerPoint{1, 1});
  CHECK(session.selected_ground_points() == std::vector<CornerPoint>{{1, 1}});
  // Maj+clic ajoute, et le dernier ajouté porte la poignée.
  Click(session, DotAt(2, 1), true);
  CHECK(session.designated_ground_point() == CornerPoint{2, 1});
  CHECK(session.selected_ground_points() == std::vector<CornerPoint>{{1, 1}, {2, 1}});
  CHECK(session.View(scene, store).ground.selected_points == 2);
  // Maj+clic sur un sélectionné le retire.
  Click(session, DotAt(1, 1), true);
  CHECK(session.selected_ground_points() == std::vector<CornerPoint>{{2, 1}});
  CHECK(session.designated_ground_point() == CornerPoint{2, 1});
  Click(session, DotAt(2, 1), true);
  CHECK(session.selected_ground_points().empty());
  CHECK_FALSE(session.designated_ground_point().has_value());
  // Un clic sans Maj remplace la sélection.
  Click(session, DotAt(1, 1));
  Click(session, DotAt(3, 3));
  CHECK(session.selected_ground_points() == std::vector<CornerPoint>{{3, 3}});
  // Maj+clic dans le vide garde la sélection ; un clic dans le vide la vide.
  Click(session, kVoid, true);
  CHECK(session.selected_ground_points().size() == 1);
  Click(session, kVoid);
  CHECK(session.selected_ground_points().empty());
  // Échap aussi, et un second Échap n'a plus rien à lâcher dans le relief.
  Click(session, DotAt(3, 3));
  CHECK(session.Deselect());
  CHECK(session.selected_ground_points().empty());
  CHECK_FALSE(session.View(scene, store).ground.designated.has_value());
  // Rien n'a touché le document.
  CHECK(store.Document(kMap).ground().empty());
}

TEST_CASE("Relief, Sommets : la poignée lève toute la sélection, en UNE entrée qu'annuler défait",
          "[app][mapedit]") {
  ro::render::MapScene scene = FlatMap();
  MapEditStore store;
  MapEditorSession session = WithPoints(scene, store);
  Click(session, DotAt(1, 1));
  Click(session, DotAt(2, 2), true);
  ShowHandle(session, DotAt(2, 2));

  // Un glissé d'une demi-poignée vers le haut : une demi-longueur.
  REQUIRE(session.Grab(DotAt(2, 2), ro::ui::MapEditorView()));
  CHECK(session.dragging());
  session.DragTo(DotAt(2, 2) + glm::vec2(0.0F, -kHandlePixels / 2.0F));
  Flush(session, scene, store);
  CHECK(LiftAt(store, 1, 1) == Approx(kHandleLength / 2.0F));
  CHECK(LiftAt(store, 2, 2) == Approx(kHandleLength / 2.0F));
  CHECK(LiftAt(store, 3, 3) == 0.0F);
  // La levée compte depuis l'appui : un pas de plus ne s'additionne pas.
  session.DragTo(DotAt(2, 2) + glm::vec2(0.0F, -kHandlePixels / 2.0F));
  Flush(session, scene, store);
  CHECK(LiftAt(store, 2, 2) == Approx(kHandleLength / 2.0F));
  CHECK(session.View(scene, store).undo_label.empty());
  // Maj aimante le désigné à l'unité entière.
  constexpr float kOffGridPixels = 23.0F;
  session.DragTo(DotAt(2, 2) + glm::vec2(0.0F, -kOffGridPixels), true);
  Flush(session, scene, store);
  CHECK(LiftAt(store, 2, 2) == Approx(std::round(kHandleLength * kOffGridPixels / kHandlePixels)));
  session.Release();
  Flush(session, scene, store);
  CHECK_FALSE(session.dragging());
  CHECK_FALSE(session.View(scene, store).undo_label.empty());
  CHECK_FALSE(store.Document(kMap).ground_why().empty());

  // Le sol suit, puis UNE annulation le rend.
  static_cast<void>(ro::render::ReshapeSceneGround(scene, store.Document(kMap).ground()));
  CHECK(ro::mapedit::CornerPointHeight(scene.gnd, 2, 2).value_or(0.0F) > 0.0F);
  static_cast<void>(session.Apply(intents::Undo{}, scene, store));
  CHECK(store.Document(kMap).ground().empty());
  CHECK(session.View(scene, store).undo_label.empty());

  // Ctrl+flèche haut pousse la sélection d'un pas, en une entrée.
  static_cast<void>(session.Apply(
      intents::Nudge{moonclient::MapEditorNudge(moonclient::NudgeDirection::kUp, false)}, scene,
      store));
  CHECK(LiftAt(store, 1, 1) == Approx(moonclient::kNudgeUnits));
  CHECK(LiftAt(store, 2, 2) == Approx(moonclient::kNudgeUnits));
  static_cast<void>(session.Apply(intents::Undo{}, scene, store));
  CHECK(store.Document(kMap).ground().empty());
}

TEST_CASE(
    "Relief, Sommets : « Hauteur » règle l'absolu, garde la falaise, et les points se "
    "rétablissent",
    "[app][mapedit]") {
  // UNE FALAISE au point (2, 2) : le cube (2, 2) monte de quatre unités, ses
  // voisins restent au sol. Le point vaut la moyenne de ses quatre coins.
  constexpr float kCliff = 4.0F;
  ro::render::MapScene scene = FlatMap();
  scene.gnd.SetCornerHeights(2, 2, {-kCliff, -kCliff, -kCliff, -kCliff});
  scene.file_corner_heights.clear();
  for (const ro::formats::GndCube& cube : scene.gnd.cubes()) {
    scene.file_corner_heights.push_back(cube.corner_height);
  }
  constexpr float kCubesAtPoint = 4.0F;
  const float before = kCliff / kCubesAtPoint;
  REQUIRE(ro::mapedit::CornerPointHeight(scene.gnd, 2, 2).value_or(0.0F) == Approx(before));

  MapEditStore store;
  MapEditorSession session = WithPoints(scene, store);
  Click(session, DotAt(1, 1));
  Click(session, DotAt(2, 2), true);
  REQUIRE(session.View(scene, store).ground.designated.has_value());
  CHECK(session.View(scene, store).ground.designated->height == Approx(before));
  CHECK(session.View(scene, store).ground.designated->cells == std::array{4.0F, 4.0F});

  // Le désigné va à dix ; l'autre bouge d'autant.
  constexpr float kTarget = 10.0F;
  static_cast<void>(session.Apply(intents::SetGroundPointHeight{.height = kTarget}, scene, store));
  const float lift = kTarget - before;
  CHECK(LiftAt(store, 2, 2) == Approx(lift));
  CHECK(LiftAt(store, 1, 1) == Approx(lift));
  const ro::ui::MapEditorGround shown = session.View(scene, store).ground;
  CHECK(shown.designated->height == Approx(kTarget));
  CHECK(shown.designated->edited);
  CHECK(shown.selected_edited == 2);
  CHECK_FALSE(session.View(scene, store).undo_label.empty());

  // La falaise garde sa marche : le cube haut et son voisin montent ensemble.
  static_cast<void>(ro::render::ReshapeSceneGround(scene, store.Document(kMap).ground()));
  CHECK(ro::mapedit::CornerPointHeight(scene.gnd, 2, 2).value_or(0.0F) == Approx(kTarget));
  CHECK(std::ranges::min(scene.gnd.at(2, 2).corner_height) == Approx(-(kCliff + lift)));
  CHECK(std::ranges::min(scene.gnd.at(1, 1).corner_height) == Approx(-lift));
  // Le sol porte maintenant la levée : la hauteur montrée n'en compte pas deux.
  CHECK(session.View(scene, store).ground.designated->height == Approx(kTarget));

  // « Rétablir les points » retire leurs retouches, en un geste annulable.
  static_cast<void>(session.Apply(intents::RevertGroundPoints{}, scene, store));
  CHECK(store.Document(kMap).ground().empty());
  CHECK(store.Document(kMap).empty());
  static_cast<void>(session.Apply(intents::Undo{}, scene, store));
  CHECK(LiftAt(store, 2, 2) == Approx(lift));
}

TEST_CASE("Relief, Sommets : sans ombres en temps réel, le glissé s'abandonne et l'outil se grise",
          "[app][mapedit]") {
  const ro::render::MapScene scene = FlatMap();
  MapEditStore store;
  MapEditorSession session = WithPoints(scene, store);
  Click(session, DotAt(2, 2));
  ShowHandle(session, DotAt(2, 2));
  REQUIRE(session.Grab(DotAt(2, 2), ro::ui::MapEditorView()));
  session.SetGroundAllowed(false);
  CHECK_FALSE(session.dragging());
  CHECK(session.tool() == ro::ui::MapEditorTool::kMove);
  CHECK_FALSE(session.ground_points_mode());
  CHECK_FALSE(session.View(scene, store).ground.allowed);
  static_cast<void>(session.Apply(intents::SetTool{ro::ui::MapEditorTool::kRelief}, scene, store));
  CHECK_FALSE(session.ground_points_mode());
}
