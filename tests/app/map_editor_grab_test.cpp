// SAISIR UN MODÈLE PAR SON CORPS, le glisser, le tourner : et les pas de
// rotation du clavier.

#include "world/map_editor_grab.h"

#include <cmath>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/ground_pick.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/map_scene.h"
#include "ro/ui/map_editor_panel.h"

#include "support/map_builder.h"
#include "world/map_editor_session.h"

using moonclient::BodyDragKeys;
using moonclient::MapEditorSession;
using moonclient::MapEditStore;
using moonclient::SteppedAngle;
using ro::render::ScreenRay;
namespace intents = ro::ui::map_editor;

namespace {

constexpr const char* kMap = "essai";
constexpr float kTolerance = 1e-3F;
// La hauteur de l'œil des rayons d'essai, au-dessus du sol.
constexpr float kEyeHeight = 100.0F;

bool Near(float value, float expected) { return std::abs(value - expected) < kTolerance; }

// Une carte d'un seul modèle, au pivot (1, 2, 3) du RSW.
ro::render::MapScene OneModel() {
  auto gnd = ro::formats::GndMap::Parse(ro::test_support::BuildGnd({}));
  ro::test_support::RswBuildOptions options;
  options.models = {
      {.name = "pont", .model_name = "decor\\pont.rsm", .position = {1.0F, 2.0F, 3.0F}}};
  auto rsw = ro::formats::RswScene::Parse(ro::test_support::BuildRsw(options));
  REQUIRE(gnd.has_value());
  REQUIRE(rsw.has_value());
  ro::render::MapScene scene{.gnd = std::move(*gnd), .rsw = std::move(*rsw)};
  scene.unfixed_models = scene.rsw.models();
  return scene;
}

// Le rayon qui tombe à la verticale sur le point du monde au-dessus du pivot,
// décalé de `world_x` et `world_z`.
ScreenRay DownOnPivot(const ro::render::MapScene& scene, float world_x, float world_z) {
  const ro::render::MapEffectFrame map_frame{scene.gnd.width(), scene.gnd.height()};
  const glm::vec3 pivot = map_frame.ToWorld(glm::vec3(1.0F, 2.0F, 3.0F));
  return ScreenRay{.origin = {pivot.x + world_x, pivot.y + kEyeHeight, pivot.z + world_z},
                   .direction = {0.0F, -1.0F, 0.0F}};
}

}  // namespace

TEST_CASE("Saisie par le corps : le plan horizontal se croise devant l'œil", "[app][mapedit]") {
  const ScreenRay down{.origin = {3.0F, 10.0F, 4.0F}, .direction = {0.0F, -1.0F, 0.0F}};
  const std::optional<glm::vec3> hit = moonclient::HorizontalHit(down, 2.0F);
  REQUIRE(hit.has_value());
  CHECK(Near(hit->x, 3.0F));
  CHECK(Near(hit->y, 2.0F));
  CHECK(Near(hit->z, 4.0F));
  // Un plan derrière l'œil ne se croise pas, ni un rayon qui le frôle.
  CHECK_FALSE(moonclient::HorizontalHit(down, 20.0F).has_value());
  const ScreenRay grazing{.origin = {0.0F, 10.0F, 0.0F}, .direction = {1.0F, -0.01F, 0.0F}};
  CHECK_FALSE(moonclient::HorizontalHit(grazing, 0.0F).has_value());
}

TEST_CASE("Saisie par le corps : un pas de rotation mène au multiple suivant", "[app][mapedit]") {
  CHECK(Near(SteppedAngle(0.0F, 45.0F), 45.0F));
  CHECK(Near(SteppedAngle(30.0F, 45.0F), 45.0F));
  CHECK(Near(SteppedAngle(45.0F, 45.0F), 90.0F));
  CHECK(Near(SteppedAngle(30.0F, -45.0F), 0.0F));
  CHECK(Near(SteppedAngle(0.0F, -45.0F), -45.0F));
  // Un angle saisi presque juste compte pour le multiple.
  CHECK(Near(SteppedAngle(44.99999F, 45.0F), 90.0F));
  // Passé un demi-tour, l'angle revient entre -180 et 180.
  CHECK(Near(SteppedAngle(170.0F, 45.0F), 180.0F));
  CHECK(Near(SteppedAngle(-170.0F, -45.0F), -180.0F));
  CHECK(Near(SteppedAngle(200.0F, 45.0F), -135.0F));
}

TEST_CASE("Saisie par le corps : Ctrl+Q et Ctrl+E tournent, Maj incline", "[app][mapedit]") {
  const auto turn = [](char key, bool shift_held) {
    const std::optional<ro::ui::MapEditorIntent> intent =
        moonclient::MapEditorShortcut(key, shift_held);
    REQUIRE(intent.has_value());
    REQUIRE(std::holds_alternative<intents::Turn>(*intent));
    return std::get<intents::Turn>(*intent);
  };
  CHECK(turn('e', false).axis == moonclient::kTurnAxis);
  CHECK(turn('e', false).step_degrees == moonclient::kTurnStepDegrees);
  CHECK(turn('q', false).step_degrees == -moonclient::kTurnStepDegrees);
  CHECK(turn('q', true).axis == moonclient::kTiltAxis);
}

TEST_CASE("Saisie par le corps : le pas de rotation se pose sur le modèle désigné",
          "[app][mapedit]") {
  const ro::render::MapScene scene = OneModel();
  MapEditorSession session;
  session.FollowMap(kMap, scene);
  MapEditStore store;
  static_cast<void>(session.Apply(intents::Toggle{}, scene, store));
  session.Pick(0);
  static_cast<void>(session.Apply(intents::Turn{.axis = 1, .step_degrees = 45.0F}, scene, store));
  const ro::mapedit::ModelEdit* edit = store.Document(kMap).ModelAt(0);
  REQUIRE(edit != nullptr);
  REQUIRE(edit->rotation.has_value());
  CHECK(Near((*edit->rotation)[1], 45.0F));
  CHECK(Near((*edit->rotation)[0], 0.0F));
}

TEST_CASE("Saisie par le corps : glisser, tourner, incliner, sans saut", "[app][mapedit]") {
  const ro::render::MapScene scene = OneModel();
  MapEditorSession session;
  session.FollowMap(kMap, scene);
  MapEditStore store;
  static_cast<void>(session.Apply(intents::Toggle{}, scene, store));
  session.Pick(0);
  static_cast<void>(session.TakeQueued());
  session.GrabModelBody(0, {100.0F, 100.0F}, DownOnPivot(scene, 0.0F, 0.0F), scene);
  REQUIRE(session.dragging_body());
  // Dans la zone morte de l'appui, rien ne bouge.
  session.DragBodyTo({101.0F, 100.0F}, DownOnPivot(scene, 3.0F, 0.0F), BodyDragKeys{});
  CHECK(session.TakeQueued().empty());
  // Le modèle suit le plan sous la souris : l'axe z du RSW va à l'opposé du monde.
  session.DragBodyTo({120.0F, 100.0F}, DownOnPivot(scene, 10.0F, -5.0F), BodyDragKeys{});
  std::vector<ro::ui::MapEditorIntent> queued = session.TakeQueued();
  REQUIRE(queued.size() == 1);
  const auto& slid = std::get<intents::Place>(queued[0]);
  CHECK_FALSE(slid.released);
  CHECK(Near(slid.position[0], 11.0F));
  CHECK(Near(slid.position[1], 2.0F));
  CHECK(Near(slid.position[2], 8.0F));
  // ALT : le glissé horizontal le tourne sur la verticale, sans le déplacer.
  const BodyDragKeys turn{.turn = true};
  session.DragBodyTo({140.0F, 100.0F}, DownOnPivot(scene, 30.0F, 0.0F), turn);
  session.DragBodyTo({180.0F, 100.0F}, DownOnPivot(scene, 30.0F, 0.0F), turn);
  // MAJ AVEC ALT : le glissé vertical l'incline en plus.
  const BodyDragKeys both{.tilt = true, .turn = true};
  session.DragBodyTo({180.0F, 100.0F}, DownOnPivot(scene, 30.0F, 0.0F), both);
  session.DragBodyTo({180.0F, 140.0F}, DownOnPivot(scene, 30.0F, 0.0F), both);
  // Les touches lâchées, il glisse de nouveau, depuis là où il est.
  session.DragBodyTo({200.0F, 140.0F}, DownOnPivot(scene, 30.0F, 0.0F), BodyDragKeys{});
  session.DragBodyTo({220.0F, 140.0F}, DownOnPivot(scene, 32.0F, 0.0F), BodyDragKeys{});
  session.Release();
  CHECK_FALSE(session.dragging());
  queued = session.TakeQueued();
  REQUIRE_FALSE(queued.empty());
  const auto& released = std::get<intents::Place>(queued.back());
  CHECK(released.released);
  CHECK(Near(released.position[0], 13.0F));
  CHECK(Near(released.position[2], 8.0F));
  CHECK(Near(released.rotation[1], 40.0F * moonclient::kGizmoDegreesPerPixel));
  CHECK(Near(released.rotation[0], 40.0F * moonclient::kGizmoDegreesPerPixel));
}

TEST_CASE("Saisie par le corps : un clic sans glissé ne fait que désigner", "[app][mapedit]") {
  const ro::render::MapScene scene = OneModel();
  MapEditorSession session;
  session.FollowMap(kMap, scene);
  MapEditStore store;
  static_cast<void>(session.Apply(intents::Toggle{}, scene, store));
  session.Pick(0);
  static_cast<void>(session.TakeQueued());
  session.GrabModelBody(0, {100.0F, 100.0F}, DownOnPivot(scene, 0.0F, 0.0F), scene);
  session.Release();
  CHECK(session.TakeQueued().empty());
  // L'outil Sommets ne saisit pas le corps : ses appuis visent les sommets.
  static_cast<void>(
      session.Apply(intents::SetTool{ro::ui::MapEditorTool::kVertices}, scene, store));
  session.GrabModelBody(0, {100.0F, 100.0F}, DownOnPivot(scene, 0.0F, 0.0F), scene);
  CHECK_FALSE(session.dragging_body());
}
