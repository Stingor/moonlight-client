// LES POIGNÉES DE L'ÉDITEUR DE CARTE : la saisie, et ce qu'un glissé en fait.

#include "world/map_editor_gizmo.h"

#include <cmath>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/camera.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/map_scene.h"

#include "support/map_builder.h"
#include "world/map_editor_session.h"

using moonclient::Dragged;
using moonclient::GizmoAxis;
using moonclient::GizmoFrame;
using moonclient::GizmoHandle;
using moonclient::GrabbedHandle;
using ro::ui::MapEditorTool;
using ro::ui::map_editor::Place;

namespace {

constexpr float kTolerance = 1e-4F;

// Une flèche de l'axe x qui part de (100, 100) et mesure cent pixels.
GizmoHandle ArrowX() {
  return GizmoHandle{.axis = GizmoAxis::kX,
                     .points = {{100.0F, 100.0F}, {200.0F, 100.0F}},
                     .screen_axis = {100.0F, 0.0F}};
}

GizmoFrame TenUnits() {
  GizmoFrame frame;
  frame.length = 10.0F;
  return frame;
}

Place AtOrigin() {
  return Place{.position = {1.0F, 2.0F, 3.0F}, .rotation = {}, .scale = {1.0F, 1.0F, 1.0F}};
}

}  // namespace

TEST_CASE("Poignées : un appui près d'une poignée la saisit, loin non", "[app][mapedit]") {
  const std::vector<GizmoHandle> handles = {ArrowX()};
  CHECK(GrabbedHandle(handles, {150.0F, 104.0F}) == std::optional<std::size_t>(0));
  CHECK_FALSE(GrabbedHandle(handles, {150.0F, 120.0F}).has_value());
  // Au-delà du bout de la flèche, la distance se compte jusqu'au bout.
  CHECK_FALSE(GrabbedHandle(handles, {220.0F, 100.0F}).has_value());
}

TEST_CASE("Poignées : déplacer suit la part de l'axe que le glissé parcourt", "[app][mapedit]") {
  // Un glissé de cinquante pixels le long d'une flèche de cent : une demi-longueur.
  const Place moved =
      Dragged(MapEditorTool::kMove, ArrowX(), TenUnits(), AtOrigin(), {50.0F, 0.0F});
  CHECK(std::abs(moved.position[0] - 6.0F) < kTolerance);
  CHECK(moved.position[1] == 2.0F);
  // Un glissé de travers ne compte que pour sa part le long de l'axe.
  const Place across =
      Dragged(MapEditorTool::kMove, ArrowX(), TenUnits(), AtOrigin(), {0.0F, 80.0F});
  CHECK(across.position[0] == 1.0F);
}

TEST_CASE("Poignées : l'échelle grandit le long de l'axe, et ne s'annule jamais",
          "[app][mapedit]") {
  const Place grown =
      Dragged(MapEditorTool::kScale, ArrowX(), TenUnits(), AtOrigin(), {100.0F, 0.0F});
  CHECK(std::abs(grown.scale[0] - 2.0F) < kTolerance);
  const Place crushed =
      Dragged(MapEditorTool::kScale, ArrowX(), TenUnits(), AtOrigin(), {-500.0F, 0.0F});
  CHECK(crushed.scale[0] == moonclient::kGizmoSmallestScale);
  // Un modèle en miroir garde son signe.
  Place mirrored = AtOrigin();
  mirrored.scale[0] = -1.0F;
  CHECK(Dragged(MapEditorTool::kScale, ArrowX(), TenUnits(), mirrored, {100.0F, 0.0F}).scale[0] <
        0.0F);
}

TEST_CASE("Poignées : tourner suit le glissé horizontal, l'échelle uniforme les trois axes",
          "[app][mapedit]") {
  GizmoHandle ring{.axis = GizmoAxis::kY, .closed = true};
  const Place turned = Dragged(MapEditorTool::kRotate, ring, TenUnits(), AtOrigin(), {20.0F, 0.0F});
  CHECK(std::abs(turned.rotation[1] - 20.0F * moonclient::kGizmoDegreesPerPixel) < kTolerance);
  CHECK(turned.rotation[0] == 0.0F);

  GizmoHandle uniform{.axis = GizmoAxis::kAll, .closed = true};
  const Place scaled =
      Dragged(MapEditorTool::kScale, uniform, TenUnits(), AtOrigin(), {50.0F, 0.0F});
  const float factor = 1.0F + 50.0F * moonclient::kGizmoUniformScalePerPixel;
  for (const float scale : scaled.scale) {
    CHECK(std::abs(scale - factor) < kTolerance);
  }
}

TEST_CASE("Poignées : les axes suivent le repère du RSW, signes compris", "[app][mapedit]") {
  const ro::render::MapEffectFrame map_frame{10, 10};
  ro::render::Camera camera;
  camera.distance = 100.0F;
  const GizmoFrame frame = moonclient::GizmoFrameOf(map_frame, {0.0F, 0.0F, 0.0F}, camera);
  // L'axe y du RSW descend : son image dans le monde regarde vers le bas.
  CHECK(frame.axes[1].y < 0.0F);
  CHECK(std::abs(glm::length(frame.axes[0]) - 1.0F) < kTolerance);
  CHECK(std::abs(frame.length - 100.0F * moonclient::kGizmoLengthShare) < kTolerance);
}

TEST_CASE("Poignées : la session met le glissé en file, et le relâcher le clôt", "[app][mapedit]") {
  moonclient::MapEditorSession session;
  ro::ui::MapEditorView view;
  view.selected = 0;
  view.position = {1.0F, 2.0F, 3.0F};
  // Fermée, la session ne saisit rien.
  session.SetGizmo(TenUnits(), {ArrowX()});
  CHECK_FALSE(session.Grab({150.0F, 100.0F}, view));
  // Ouverte, elle saisit, glisse et relâche.
  moonclient::MapEditStore store;
  auto gnd = ro::formats::GndMap::Parse(ro::test_support::BuildGnd({}));
  auto rsw = ro::formats::RswScene::Parse(ro::test_support::BuildRsw({}));
  REQUIRE(gnd.has_value());
  REQUIRE(rsw.has_value());
  const ro::render::MapScene scene{.gnd = std::move(*gnd), .rsw = std::move(*rsw)};
  static_cast<void>(session.Apply(ro::ui::map_editor::Toggle{}, scene, store));
  session.SetGizmo(TenUnits(), {ArrowX()});
  REQUIRE(session.Grab({150.0F, 100.0F}, view));
  CHECK(session.dragging());
  session.DragTo({200.0F, 100.0F});
  session.Release();
  CHECK_FALSE(session.dragging());
  const std::vector<ro::ui::MapEditorIntent> queued = session.TakeQueued();
  REQUIRE(queued.size() == 2);
  const auto& during = std::get<Place>(queued[0]);
  const auto& released = std::get<Place>(queued[1]);
  CHECK_FALSE(during.released);
  CHECK(released.released);
  CHECK(std::abs(released.position[0] - 6.0F) < kTolerance);
  CHECK(session.TakeQueued().empty());
}

TEST_CASE("Poignées : Maj aimante les seules composantes tirées", "[app][mapedit]") {
  const ro::ui::map_editor::Place start{
      .position = {1.3F, 2.7F, 3.1F}, .rotation = {0.0F, 7.0F, 0.0F}, .scale = {1.0F, 1.0F, 1.0F}};
  ro::ui::map_editor::Place dragged = start;
  dragged.position[0] = 13.2F;
  const ro::ui::map_editor::Place moved =
      moonclient::Snapped(ro::ui::MapEditorTool::kMove, start, dragged);
  CHECK(moved.position[0] == 15.0F);
  // Les axes qu'on ne tire pas restent où ils sont.
  CHECK(moved.position[1] == 2.7F);
  CHECK(moved.position[2] == 3.1F);
  dragged = start;
  dragged.rotation[1] = 37.0F;
  CHECK(moonclient::Snapped(ro::ui::MapEditorTool::kRotate, start, dragged).rotation[1] == 30.0F);
  dragged = start;
  dragged.scale = {1.26F, 1.26F, 1.26F};
  const ro::ui::map_editor::Place scaled =
      moonclient::Snapped(ro::ui::MapEditorTool::kScale, start, dragged);
  CHECK(std::abs(scaled.scale[0] - 1.3F) < 1e-5F);
}
