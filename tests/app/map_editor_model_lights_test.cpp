// LES LUMIÈRES DE MODÈLE DANS L'ÉDITEUR : assignées depuis une pose, réglées sur
// une autre, éteintes sur une seule, et défaites geste par geste.

#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/mapedit/model_light.h"
#include "ro/render/map_scene.h"
#include "ro/ui/map_editor_panel.h"

#include "support/map_builder.h"
#include "world/map_editor_session.h"

using moonclient::MapEditorSession;
using moonclient::MapEditStore;
using ro::mapedit::kModelLightsDocument;
namespace intents = ro::ui::map_editor;

namespace {

constexpr const char* kMap = "essai";
constexpr const char* kLamp = "decor\\lampe.rsm";

// Deux lampes — la seconde tournée d'un quart de tour — et un arbre.
ro::render::MapScene TwoLamps() {
  auto gnd = ro::formats::GndMap::Parse(ro::test_support::BuildGnd({}));
  ro::test_support::RswBuildOptions options;
  options.models = {
      {.name = "lampe", .model_name = kLamp, .position = {1.0F, 2.0F, 3.0F}},
      {.name = "lampe", .model_name = "DECOR/Lampe.RSM", .position = {40.0F, 0.0F, 50.0F}},
      {.name = "arbre", .model_name = "decor\\arbre.rsm", .position = {4.0F, 5.0F, 6.0F}},
  };
  auto rsw = ro::formats::RswScene::Parse(ro::test_support::BuildRsw(options));
  REQUIRE(gnd.has_value());
  REQUIRE(rsw.has_value());
  ro::render::MapScene scene{.gnd = std::move(*gnd), .rsw = std::move(*rsw)};
  scene.rsw.PlaceModel(1, {40.0F, 0.0F, 50.0F}, {0.0F, 90.0F, 0.0F}, {1.0F, 1.0F, 1.0F});
  scene.unfixed_models = scene.rsw.models();
  for (const ro::formats::RswModel& model : scene.rsw.models()) {
    scene.model_keys.push_back(ro::mapedit::ModelFileKey(model.model_name));
  }
  return scene;
}

void CheckNear(const std::array<float, 3>& got, const std::array<float, 3>& wanted) {
  constexpr double kMargin = 1.0e-3;
  for (std::size_t axis = 0; axis < got.size(); ++axis) {
    CHECK(got[axis] == Catch::Approx(wanted[axis]).margin(kMargin));
  }
}

}  // namespace

TEST_CASE("Lumières de modèle : assignée sur une pose, toutes les poses du fichier la portent",
          "[app][mapedit]") {
  ro::render::MapScene scene = TwoLamps();
  MapEditorSession session;
  MapEditStore store;
  session.FollowMap(kMap, scene);
  static_cast<void>(session.Apply(intents::Toggle{}, scene, store));
  session.Pick(0);
  static_cast<void>(session.Apply(
      intents::AddModelLight{.position = std::array<float, 3>{1.0F, -18.0F, 3.0F}}, scene, store));

  const ro::mapedit::MapEdit& book = store.Document(kModelLightsDocument);
  REQUIRE(book.model_lights().size() == 1);
  CHECK(book.model_lights()[0].model_file == kLamp);
  CheckNear(book.model_lights()[0].light.position, {0.0F, -20.0F, 0.0F});
  CHECK_FALSE(book.model_lights()[0].light.why.empty());
  CHECK(store.Document(kMap).empty());
  CHECK(store.Unsaved(kModelLightsDocument));

  // LA SECONDE LAMPE, au nom écrit autrement, la porte aussi ; l'arbre non.
  const std::vector<ro::mapedit::AddedLight> lights =
      ro::mapedit::ModelLightsOn(book, scene.rsw.models(), scene.model_keys, {}, {});
  REQUIRE(lights.size() == 2);
  CheckNear(lights[1].position, {40.0F, -20.0F, 50.0F});

  const ro::ui::MapEditorView view = session.View(scene, store);
  REQUIRE(view.model_lights.size() == 1);
  CHECK(view.selected_model_light == std::optional<std::size_t>(0));
  CheckNear(view.model_lights[0].light.position, {1.0F, -18.0F, 3.0F});
  CHECK(view.unsaved);

  // ANNULER défait le document commun, pas celui de la carte.
  static_cast<void>(session.Apply(intents::Undo{}, scene, store));
  CHECK(store.Document(kModelLightsDocument).model_lights().empty());
  static_cast<void>(session.Apply(intents::Redo{}, scene, store));
  CHECK(store.Document(kModelLightsDocument).model_lights().size() == 1);
}

TEST_CASE("Lumières de modèle : déplacée sur une pose tournée, elle s'écrit dans son repère",
          "[app][mapedit]") {
  ro::render::MapScene scene = TwoLamps();
  MapEditorSession session;
  MapEditStore store;
  session.FollowMap(kMap, scene);
  static_cast<void>(session.Apply(intents::Toggle{}, scene, store));
  session.Pick(0);
  static_cast<void>(session.Apply(
      intents::AddModelLight{.position = std::array<float, 3>{1.0F, -18.0F, 3.0F}}, scene, store));
  session.Pick(1);
  const std::array<float, 3> wanted{45.0F, -20.0F, 50.0F};
  static_cast<void>(session.Apply(
      intents::MoveModelLight{.slot = 0, .native = wanted, .released = true}, scene, store));
  const ro::mapedit::ModelLight& carried = store.Document(kModelLightsDocument).model_lights()[0];
  CheckNear(ro::mapedit::ModelLightPosition(scene.rsw.models()[1], carried.light.position), wanted);
  // Sur la pose d'origine, l'écart a tourné d'un quart de tour avec elle.
  CHECK(carried.light.position[0] != Catch::Approx(5.0F).margin(1.0e-3));
}

TEST_CASE("Lumières de modèle : éteintes sur une seule pose, par une retouche de la carte",
          "[app][mapedit]") {
  ro::render::MapScene scene = TwoLamps();
  MapEditorSession session;
  MapEditStore store;
  session.FollowMap(kMap, scene);
  static_cast<void>(session.Apply(intents::Toggle{}, scene, store));
  session.Pick(1);
  const MapEditorSession::Outcome off =
      session.Apply(intents::SetModelUnlit{.unlit = true}, scene, store);
  CHECK(off.replace_models);
  const ro::mapedit::ModelEdit* edit = store.Document(kMap).ModelAt(1);
  REQUIRE(edit != nullptr);
  CHECK(edit->unlit);
  CHECK_FALSE(edit->why.empty());

  // RALLUMÉE, la retouche ne change plus rien : elle quitte le document.
  static_cast<void>(session.Apply(intents::SetModelUnlit{.unlit = false}, scene, store));
  CHECK(store.Document(kMap).ModelAt(1) == nullptr);
}
