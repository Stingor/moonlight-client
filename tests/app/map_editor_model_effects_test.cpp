// LES EFFETS DE MODÈLE DANS L'ÉDITEUR : assignés depuis une pose, réglés sur
// une autre, retirés, et défaits geste par geste.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/mapedit/model_light.h"
#include "ro/render/edited_map_effects.h"
#include "ro/render/map_point_lights.h"
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
constexpr const char* kBrazier = "decor\\brasero.rsm";
constexpr std::int32_t kFlames = 47;

// Deux braseros — le second tourné d'un quart de tour — et un arbre.
ro::render::MapScene TwoBraziers() {
  auto gnd = ro::formats::GndMap::Parse(ro::test_support::BuildGnd({}));
  ro::test_support::RswBuildOptions options;
  options.models = {
      {.name = "brasero", .model_name = kBrazier, .position = {1.0F, 2.0F, 3.0F}},
      {.name = "brasero", .model_name = "DECOR/Brasero.RSM", .position = {40.0F, 0.0F, 50.0F}},
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

// La session ouverte sur la carte, le premier brasero désigné et ses flammes
// assignées au-dessus de lui.
void AssignFlames(MapEditorSession& session, MapEditStore& store, ro::render::MapScene& scene) {
  session.FollowMap(kMap, scene);
  static_cast<void>(session.Apply(intents::Toggle{}, scene, store));
  session.Pick(0);
  static_cast<void>(
      session.Apply(intents::AddModelEffect{.number = kFlames,
                                            .position = std::array<float, 3>{1.0F, -13.0F, 3.0F}},
                    scene, store));
}

}  // namespace

TEST_CASE("Effets de modèle : assigné sur une pose, toutes les poses du fichier le lèvent",
          "[app][mapedit]") {
  ro::render::MapScene scene = TwoBraziers();
  MapEditorSession session;
  MapEditStore store;
  AssignFlames(session, store, scene);

  const ro::mapedit::MapEdit& book = store.Document(kModelLightsDocument);
  REQUIRE(book.model_effects().size() == 1);
  CHECK(book.model_effects()[0].model_file == kBrazier);
  CHECK(book.model_effects()[0].effect.number == kFlames);
  CheckNear(book.model_effects()[0].effect.position, {0.0F, -15.0F, 0.0F});
  CHECK_FALSE(book.model_effects()[0].effect.why.empty());
  CHECK(store.Document(kMap).empty());
  CHECK(store.Unsaved(kModelLightsDocument));

  // LES DEUX BRASEROS, au nom écrit autrement, se lèvent ; l'arbre non.
  const std::vector<ro::formats::RswEffect> raised =
      ro::render::SceneMapEffects(scene, store.Document(kMap), book, 0.0F);
  REQUIRE(raised.size() == 2);
  CHECK(raised[1].number == kFlames);
  CheckNear(raised[0].position, {1.0F, -13.0F, 3.0F});
  CheckNear(raised[1].position, {40.0F, -15.0F, 50.0F});

  const ro::ui::MapEditorView view = session.View(scene, store);
  REQUIRE(view.model_effects.size() == 1);
  CHECK(view.selected_model_effect == std::optional<std::size_t>(0));
  CHECK_FALSE(view.selected_model_light.has_value());
  CheckNear(view.model_effects[0].effect.position, {1.0F, -13.0F, 3.0F});

  // ANNULER défait le document commun, pas celui de la carte.
  static_cast<void>(session.Apply(intents::Undo{}, scene, store));
  CHECK(store.Document(kModelLightsDocument).model_effects().empty());
  static_cast<void>(session.Apply(intents::Redo{}, scene, store));
  CHECK(store.Document(kModelLightsDocument).model_effects().size() == 1);
}

TEST_CASE("Effets de modèle : déplacé sur une pose tournée, il s'écrit dans son repère",
          "[app][mapedit]") {
  ro::render::MapScene scene = TwoBraziers();
  MapEditorSession session;
  MapEditStore store;
  AssignFlames(session, store, scene);
  session.Pick(1);
  const std::array<float, 3> wanted{45.0F, -15.0F, 50.0F};
  static_cast<void>(session.Apply(
      intents::MoveModelEffect{.slot = 0, .native = wanted, .released = true}, scene, store));
  const ro::mapedit::ModelEffect& carried = store.Document(kModelLightsDocument).model_effects()[0];
  CheckNear(ro::mapedit::ModelLightPosition(scene.rsw.models()[1], carried.effect.position),
            wanted);
  CHECK(carried.effect.position[0] != Catch::Approx(5.0F).margin(1.0e-3));
  CHECK(carried.effect.number == kFlames);
}

TEST_CASE("Effets de modèle : éteint sur une pose, retiré partout", "[app][mapedit]") {
  ro::render::MapScene scene = TwoBraziers();
  MapEditorSession session;
  MapEditStore store;
  AssignFlames(session, store, scene);
  // LA LUEUR suit l'effet sur chaque pose.
  ro::ui::map_editor::SetModelEffect set{
      .slot = 0, .effect = session.View(scene, store).model_effects[0].effect};
  set.effect.glows = true;
  static_cast<void>(session.Apply(set, scene, store));
  const ro::mapedit::MapEdit& book = store.Document(kModelLightsDocument);
  REQUIRE(book.model_effects()[0].effect.glow.has_value());
  CHECK(ro::render::SceneLights(store.Document(kMap), book, scene).size() == 2);

  // ÉTEINT sur la seconde pose, il ne s'y lève plus.
  session.Pick(1);
  static_cast<void>(session.Apply(intents::SetModelUnlit{.unlit = true}, scene, store));
  scene.unlit_models = {1};
  CHECK(ro::render::SceneMapEffects(scene, store.Document(kMap), book, 0.0F).size() == 1);
  CHECK(ro::render::SceneLights(store.Document(kMap), book, scene).size() == 1);

  static_cast<void>(session.Apply(intents::RemoveModelEffect{.slot = 0}, scene, store));
  CHECK(store.Document(kModelLightsDocument).model_effects().empty());
  CHECK_FALSE(session.View(scene, store).selected_model_effect.has_value());
}
