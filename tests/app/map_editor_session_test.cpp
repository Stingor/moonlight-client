// LE MODE ÉDITEUR DE CARTE : ce que la session fait des intentions du panneau.

#include "world/map_editor_session.h"

#include <array>
#include <cmath>
#include <utility>
#include <variant>

#include <catch2/catch_test_macros.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/map_scene.h"
#include "ro/ui/map_editor_panel.h"

#include "support/map_builder.h"
#include "world/replace_tally.h"

using moonclient::MapEditorSession;
using moonclient::MapEditStore;
namespace intents = ro::ui::map_editor;

namespace {

constexpr const char* kMap = "essai";

// Une carte de deux modèles, dont le placement d'origine est gardé.
ro::render::MapScene TwoModels() {
  auto gnd = ro::formats::GndMap::Parse(ro::test_support::BuildGnd({}));
  ro::test_support::RswBuildOptions options;
  options.models = {
      {.name = "pont", .model_name = "decor\\pont.rsm", .position = {1.0F, 2.0F, 3.0F}},
      {.name = "arbre", .model_name = "decor\\arbre.rsm", .position = {4.0F, 5.0F, 6.0F}},
  };
  auto rsw = ro::formats::RswScene::Parse(ro::test_support::BuildRsw(options));
  REQUIRE(gnd.has_value());
  REQUIRE(rsw.has_value());
  ro::render::MapScene scene{.gnd = std::move(*gnd), .rsw = std::move(*rsw)};
  scene.unfixed_models = scene.rsw.models();
  return scene;
}

// Une session ouverte sur la carte, le pont désigné.
MapEditorSession OpenOnBridge() {
  MapEditorSession session;
  ro::render::MapScene scene = TwoModels();
  session.FollowMap(kMap, scene);
  MapEditStore store;
  static_cast<void>(session.Apply(intents::Toggle{}, scene, store));
  session.Pick(0);
  return session;
}

intents::Place MovedTo(float x, bool released) {
  return intents::Place{.position = {x, 2.0F, 3.0F},
                        .rotation = {},
                        .scale = {1.0F, 1.0F, 1.0F},
                        .hidden = false,
                        .released = released};
}

// LE PONT VU PAR L'OUTIL SOMMETS : un nœud de deux sommets, posé dix unités
// plus loin sur l'axe x du monde.
constexpr float kPoseShift = 10.0F;

MapEditorSession::VertexContext BridgeVertices(const ro::render::MapScene& scene) {
  MapEditorSession::VertexContext context;
  context.model_index = 0;
  context.model_file = "decor\\pont.rsm";
  context.pose_origin = {1.0F, 2.0F, 3.0F};
  context.to_world = glm::translate(glm::mat4(1.0F), glm::vec3(kPoseShift, 0.0F, 0.0F));
  context.map_frame = ro::render::MapEffectFrame{scene.gnd.width(), scene.gnd.height()};
  MapEditorSession::VertexNode node;
  node.name = "planche";
  node.origins = {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}};
  node.world = {{kPoseShift, 0.0F, 0.0F}, {kPoseShift + 1.0F, 0.0F, 0.0F}};
  context.nodes.push_back(std::move(node));
  return context;
}

// Le geste qui amène le sommet `vertex` au point `world` du monde.
intents::MoveVertex VertexTo(const MapEditorSession::VertexContext& context, std::size_t vertex,
                             glm::vec3 world, bool released) {
  const glm::vec3 native = context.map_frame.ToNative(world);
  return intents::MoveVertex{
      .node = 0, .vertex = vertex, .native = {native.x, native.y, native.z}, .released = released};
}

// Une session sur le pont, l'outil Sommets en main.
MapEditorSession OnBridgeVertices(const ro::render::MapScene& scene, MapEditStore& store) {
  MapEditorSession session = OpenOnBridge();
  static_cast<void>(
      session.Apply(intents::SetTool{ro::ui::MapEditorTool::kVertices}, scene, store));
  session.SetVertexContext(BridgeVertices(scene));
  return session;
}

}  // namespace

TEST_CASE("Éditeur : un geste qui glisse se voit, et ne compte qu'au relâcher", "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  REQUIRE(session.active());
  // Pendant le glissé : le document change et le décor se repose, sans geste
  // dans l'historique.
  CHECK(session.Apply(MovedTo(5.0F, false), scene, store).replace_models);
  CHECK(store.Document(kMap).ModelAt(0) != nullptr);
  CHECK(session.View(scene, store).undo_label.empty());
  // Au relâcher : UNE entrée, du document d'avant le glissé à celui d'après.
  CHECK(session.Apply(MovedTo(6.0F, true), scene, store).replace_models);
  CHECK_FALSE(session.View(scene, store).undo_label.empty());
  CHECK((*store.Document(kMap).ModelAt(0)->position)[0] == 6.0F);
  CHECK(store.Unsaved(kMap));

  // Annuler rend le document d'avant le geste entier.
  CHECK(session.Apply(intents::Undo{}, scene, store).replace_models);
  CHECK(store.Document(kMap).empty());
  CHECK(session.Apply(intents::Redo{}, scene, store).replace_models);
  CHECK((*store.Document(kMap).ModelAt(0)->position)[0] == 6.0F);
}

TEST_CASE("Éditeur : revenir au placement d'origine retire la retouche", "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  static_cast<void>(session.Apply(MovedTo(5.0F, true), scene, store));
  // Les glissières ramenées à l'origine : plus rien à retoucher.
  static_cast<void>(session.Apply(MovedTo(1.0F, true), scene, store));
  CHECK(store.Document(kMap).empty());
  // « Rétablir le modèle » fait de même d'un coup.
  static_cast<void>(session.Apply(MovedTo(5.0F, true), scene, store));
  CHECK(session.Apply(intents::Revert{}, scene, store).replace_models);
  CHECK(store.Document(kMap).empty());
}

TEST_CASE("Éditeur : une raison saisie avant la retouche la rejoint", "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  static_cast<void>(session.Apply(intents::SetWhy{"les planches s'enfoncent"}, scene, store));
  CHECK(store.Document(kMap).empty());
  CHECK(session.View(scene, store).why == "les planches s'enfoncent");
  static_cast<void>(session.Apply(MovedTo(5.0F, true), scene, store));
  CHECK(store.Document(kMap).ModelAt(0)->why == "les planches s'enfoncent");
}

TEST_CASE("Éditeur : sans raison saisie, une raison générique dit la retouche", "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  static_cast<void>(session.Apply(MovedTo(5.0F, true), scene, store));
  CHECK(store.Document(kMap).ModelAt(0)->why == "Retouche de decor\\pont.rsm");
  // Saisie, la raison du staff la remplace.
  static_cast<void>(session.Apply(intents::SetWhy{"les planches s'enfoncent"}, scene, store));
  CHECK(store.Document(kMap).ModelAt(0)->why == "les planches s'enfoncent");
}

TEST_CASE("Éditeur : une autre carte oublie la sélection et l'historique", "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  static_cast<void>(session.Apply(MovedTo(5.0F, true), scene, store));
  session.FollowMap("prontera", scene);
  const ro::ui::MapEditorView view = session.View(scene, store);
  CHECK_FALSE(view.selected.has_value());
  CHECK(view.undo_label.empty());
  // Le mode reste ouvert : c'est la carte qui a changé, pas l'outil.
  CHECK(view.active);
}

TEST_CASE("Éditeur : la vue montre le modèle désigné et ce que l'édition en fait",
          "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  ro::ui::MapEditorView view = session.View(scene, store);
  REQUIRE(view.selected == std::optional<std::size_t>(0));
  CHECK(view.selected_file == "decor\\pont.rsm");
  CHECK(view.origin == std::array<float, 3>{1.0F, 2.0F, 3.0F});
  CHECK(view.position == view.origin);
  static_cast<void>(session.Apply(MovedTo(5.0F, true), scene, store));
  view = session.View(scene, store);
  CHECK(view.position[0] == 5.0F);
  CHECK(view.edited == std::vector<std::size_t>{0});
  // L'arbre de la scène montre les deux modèles, le pont marqué retouché.
  REQUIRE(view.models != nullptr);
  REQUIRE(view.models->size() == 2);
  CHECK((*view.models)[0].file == "decor\\pont.rsm");
  CHECK((*view.models)[0].edited);
  CHECK_FALSE((*view.models)[1].edited);
  // Un clic dans le vide garde la sélection et le dit.
  session.Pick(std::nullopt);
  view = session.View(scene, store);
  CHECK(view.selected == std::optional<std::size_t>(0));
  CHECK_FALSE(view.status.empty());
}

TEST_CASE("Éditeur : un sommet déplacé s'écrit dans le repère de son nœud", "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OnBridgeVertices(scene, store);
  const MapEditorSession::VertexContext context = BridgeVertices(scene);
  // Pendant le glissé, le maillage change sans geste dans l'historique.
  CHECK(session.Apply(VertexTo(context, 1, {kPoseShift + 1.0F, 1.0F, 0.0F}, false), scene, store)
            .replace_models);
  CHECK(session.View(scene, store).undo_label.empty());
  CHECK(session.Apply(VertexTo(context, 1, {kPoseShift + 1.0F, 2.0F, 0.0F}, true), scene, store)
            .replace_models);
  const ro::mapedit::MeshEdit* mesh = store.Document(kMap).MeshAt(0);
  REQUIRE(mesh != nullptr);
  CHECK(mesh->model_file == "decor\\pont.rsm");
  CHECK(mesh->origin == std::array<float, 3>{1.0F, 2.0F, 3.0F});
  // La pose retirée : le sommet est écrit là où le nœud le porte.
  const ro::mapedit::VertexMove* moved = mesh->VertexAt("planche", 1);
  REQUIRE(moved != nullptr);
  CHECK(moved->origin == std::array<float, 3>{1.0F, 0.0F, 0.0F});
  CHECK(std::abs(moved->position[0] - 1.0F) < ro::mapedit::kOriginTolerance);
  CHECK(std::abs(moved->position[1] - 2.0F) < ro::mapedit::kOriginTolerance);
  CHECK_FALSE(session.View(scene, store).undo_label.empty());
  CHECK(session.View(scene, store).moved_vertices == 1);
  // L'arbre de la scène le marque retouché, comme une pose déplacée.
  CHECK((*session.View(scene, store).models)[0].edited);
  CHECK(session.View(scene, store).edited == std::vector<std::size_t>{0});

  // Ramené à son point d'origine, le sommet sort du maillage, et le maillage
  // vide du document.
  static_cast<void>(
      session.Apply(VertexTo(context, 1, {kPoseShift + 1.0F, 0.0F, 0.0F}, true), scene, store));
  CHECK(store.Document(kMap).empty());
  // L'annulation rend le sommet déplacé.
  static_cast<void>(session.Apply(intents::Undo{}, scene, store));
  CHECK(store.Document(kMap).MeshAt(0) != nullptr);
}

TEST_CASE("Éditeur : un sommet se désigne sur son point, et se rétablit", "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OnBridgeVertices(scene, store);
  const MapEditorSession::VertexContext context = BridgeVertices(scene);
  static_cast<void>(
      session.Apply(VertexTo(context, 0, {kPoseShift, 3.0F, 0.0F}, true), scene, store));
  REQUIRE(store.Document(kMap).MeshAt(0) != nullptr);
  // Un clic loin de tout point ne désigne rien.
  const glm::vec2 dot_at{100.0F, 50.0F};
  session.SetVertexDots({MapEditorSession::VertexDot{.at = dot_at, .node = 0, .vertex = 0}});
  const ro::ui::MapEditorView view = session.View(scene, store);
  CHECK_FALSE(session.Grab(dot_at + glm::vec2(500.0F, 0.0F), view));
  CHECK_FALSE(session.selected_vertex().has_value());
  // Sur le point, le sommet est désigné, et la vue le nomme.
  CHECK(session.Grab(dot_at, view));
  CHECK(session.selected_vertex() == std::optional(std::pair<std::size_t, std::size_t>(0, 0)));
  CHECK(session.View(scene, store).selected_vertex == "planche #0");
  CHECK(session.Apply(intents::RevertVertex{}, scene, store).replace_models);
  CHECK(store.Document(kMap).empty());
  // Changer de modèle oublie le sommet.
  session.Pick(1);
  CHECK_FALSE(session.selected_vertex().has_value());
}

TEST_CASE("Éditeur : un maillage retouché porte sa raison", "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OnBridgeVertices(scene, store);
  const MapEditorSession::VertexContext context = BridgeVertices(scene);
  static_cast<void>(
      session.Apply(VertexTo(context, 0, {kPoseShift, 3.0F, 0.0F}, true), scene, store));
  // Sans raison saisie, une raison générique dit la retouche.
  CHECK(store.Document(kMap).MeshAt(0)->why.starts_with("Retouche du maillage de "));
  // Avec l'outil Sommets, la raison va au maillage, pas à la pose.
  static_cast<void>(session.Apply(intents::SetWhy{"la falaise perce"}, scene, store));
  CHECK(store.Document(kMap).MeshAt(0)->why == "la falaise perce");
  CHECK(store.Document(kMap).ModelAt(0) == nullptr);
  // Étendu à toutes les poses du fichier.
  CHECK(session.Apply(intents::SetAllPoses{true}, scene, store).replace_models);
  CHECK(store.Document(kMap).MeshAt(0)->all_poses);
  CHECK(session.View(scene, store).all_poses);
}

TEST_CASE("Éditeur : une raison saisie avant de toucher au maillage le rejoint", "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OnBridgeVertices(scene, store);
  const MapEditorSession::VertexContext context = BridgeVertices(scene);
  static_cast<void>(session.Apply(intents::SetWhy{"la falaise perce"}, scene, store));
  CHECK(store.Document(kMap).empty());
  CHECK(session.View(scene, store).why == "la falaise perce");
  static_cast<void>(
      session.Apply(VertexTo(context, 0, {kPoseShift, 3.0F, 0.0F}, true), scene, store));
  CHECK(store.Document(kMap).MeshAt(0)->why == "la falaise perce");
}

TEST_CASE("Éditeur : les raccourcis de Ctrl", "[app][mapedit]") {
  using moonclient::MapEditorShortcut;
  CHECK(std::holds_alternative<intents::Undo>(*MapEditorShortcut('z', false)));
  CHECK(std::holds_alternative<intents::Redo>(*MapEditorShortcut('z', true)));
  CHECK(std::holds_alternative<intents::Redo>(*MapEditorShortcut('y', false)));
  CHECK(std::holds_alternative<intents::Save>(*MapEditorShortcut('s', false)));
  const auto vertices = MapEditorShortcut('4', false);
  REQUIRE(std::holds_alternative<intents::SetTool>(*vertices));
  CHECK(std::get<intents::SetTool>(*vertices).tool == ro::ui::MapEditorTool::kVertices);
  CHECK_FALSE(MapEditorShortcut('w', false).has_value());
}

TEST_CASE("Éditeur : une lumière s'ajoute où regarde la caméra, se règle et se retire",
          "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  session.SetDropPoint({7.0F, 0.0F, 9.0F});
  static_cast<void>(session.Apply(intents::AddLight{}, scene, store));
  REQUIRE(store.Document(kMap).lights().size() == 1);
  const ro::mapedit::AddedLight& added = store.Document(kMap).lights()[0];
  CHECK(added.position[0] == 7.0F);
  CHECK(added.position[2] == 9.0F);
  // Au-dessus du sol : un y négatif monte.
  CHECK(added.position[1] < 0.0F);
  CHECK(added.range > 0.0F);
  // Neuve, elle ne luit qu'à la nuit, de 17 h à 9 h.
  CHECK(added.night_only);
  REQUIRE(added.hours.has_value());
  CHECK(added.hours->from_hour == 17.0F);
  CHECK(added.hours->to_hour == 9.0F);
  ro::ui::MapEditorView view = session.View(scene, store);
  REQUIRE(view.lights.size() == 1);
  CHECK(view.selected_light == std::optional<std::size_t>(0));

  // Sans raison saisie, une raison générique dit l'ajout.
  CHECK(store.Document(kMap).lights()[0].why == "Ajout d'une lumière");

  // Un glissé ne compte qu'au relâcher.
  ro::ui::MapEditorLight warmer = view.lights[0];
  warmer.range = 80.0F;
  const std::string undo_before = session.View(scene, store).undo_label;
  static_cast<void>(session.Apply(intents::SetLight{.slot = 0, .light = warmer, .released = false},
                                  scene, store));
  CHECK(store.Document(kMap).lights()[0].range == 80.0F);
  CHECK(session.View(scene, store).undo_label == undo_before);
  warmer.why = "la lanterne du pont";
  static_cast<void>(session.Apply(intents::SetLight{.slot = 0, .light = warmer}, scene, store));
  CHECK(session.View(scene, store).undo_label != undo_before);
  CHECK(store.Document(kMap).lights()[0].why == "la lanterne du pont");

  static_cast<void>(session.Apply(intents::RemoveLight{0}, scene, store));
  CHECK(store.Document(kMap).lights().empty());
  CHECK_FALSE(session.View(scene, store).selected_light.has_value());
  // L'annulation la rend.
  static_cast<void>(session.Apply(intents::Undo{}, scene, store));
  CHECK(store.Document(kMap).lights().size() == 1);
}

TEST_CASE("Éditeur : un clic sur le cercle d'une lumière la désigne", "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  static_cast<void>(session.Apply(intents::AddLight{}, scene, store));
  static_cast<void>(session.Apply(intents::AddLight{}, scene, store));
  static_cast<void>(session.Apply(intents::SelectLight{std::nullopt}, scene, store));
  const glm::vec2 second_at{40.0F, 30.0F};
  session.SetLightDots({MapEditorSession::LightDot{.at = {300.0F, 300.0F}, .slot = 0},
                        MapEditorSession::LightDot{.at = second_at, .slot = 1}});
  const ro::ui::MapEditorView view = session.View(scene, store);
  CHECK_FALSE(session.Grab(second_at + glm::vec2(100.0F, 0.0F), view));
  CHECK(session.Grab(second_at + glm::vec2(2.0F, 0.0F), view));
  CHECK(session.View(scene, store).selected_light == std::optional<std::size_t>(1));
  const std::vector<ro::ui::MapEditorLine> marks = session.LightTrace();
  REQUIRE(marks.size() == 2);
  CHECK(marks[1].kind == ro::ui::MapEditorLineKind::kLightSelected);
  CHECK(marks[0].kind == ro::ui::MapEditorLineKind::kLight);
}

TEST_CASE("Éditeur : dupliquer un décor garde un nom que le CP949 n'écrit pas", "[app][mapedit]") {
  // « ⚠x.rsm » déposé par le joueur : le RSW de la scène en porte les octets UTF-8,
  // qui ne se relisent pas en CP949.
  const std::string file = "decor\\\xE2\x9A\xA0x.rsm";
  auto gnd = ro::formats::GndMap::Parse(ro::test_support::BuildGnd({}));
  ro::test_support::RswBuildOptions options;
  options.models = {
      {.name = "pont", .model_name = "decor\\pont.rsm", .position = {1.0F, 2.0F, 3.0F}},
      {.name = "ajout", .model_name = file, .position = {4.0F, 5.0F, 6.0F}},
  };
  auto rsw = ro::formats::RswScene::Parse(ro::test_support::BuildRsw(options));
  REQUIRE(gnd.has_value());
  REQUIRE(rsw.has_value());
  ro::render::MapScene scene{.gnd = std::move(*gnd), .rsw = std::move(*rsw)};
  scene.unfixed_models = {scene.rsw.models()[0]};

  MapEditStore store;
  ro::mapedit::MapEdit document = store.Document(kMap);
  document.AddModel(ro::mapedit::AddedModel{.model_file = file});
  store.Set(kMap, document);

  MapEditorSession session;
  session.FollowMap(kMap, scene);
  static_cast<void>(session.Apply(intents::Toggle{}, scene, store));
  session.Pick(1);
  static_cast<void>(session.Apply(intents::Duplicate{}, scene, store));

  REQUIRE(store.Document(kMap).added().size() == 2);
  CHECK(store.Document(kMap).added()[1].model_file == file);
}

TEST_CASE("Éditeur : dupliquer pose une copie sur place et la désigne", "[app][mapedit]") {
  ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  CHECK(session.Apply(intents::Duplicate{}, scene, store).replace_models);
  REQUIRE(store.Document(kMap).added().size() == 1);
  const ro::mapedit::AddedModel& copy = store.Document(kMap).added()[0];
  CHECK(copy.model_file == "decor\\pont.rsm");
  // Sur place : la copie se déplace ensuite.
  CHECK(copy.position == std::array<float, 3>{1.0F, 2.0F, 3.0F});
  // La copie est désignée : son rang suit ceux du RSW.
  CHECK(session.View(scene, store).selected == std::optional<std::size_t>(2));
  CHECK(std::holds_alternative<intents::Duplicate>(*moonclient::MapEditorShortcut('d', false)));
}

TEST_CASE("Éditeur : une lumière se déplace à la flèche, et prend seule la désignation",
          "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  static_cast<void>(session.Apply(intents::AddLight{}, scene, store));
  // La lumière désignée, le pont ne l'est plus.
  ro::ui::MapEditorView view = session.View(scene, store);
  CHECK_FALSE(view.selected.has_value());
  REQUIRE(view.selected_light == std::optional<std::size_t>(0));

  // Le glissé repose la lumière, mais ne compte qu'au relâcher.
  const std::string undo_before = view.undo_label;
  static_cast<void>(session.Apply(
      intents::MoveLight{.slot = 0, .native = {4.0F, -20.0F, 6.0F}, .released = false}, scene,
      store));
  CHECK(store.Document(kMap).lights()[0].position == std::array<float, 3>{4.0F, -20.0F, 6.0F});
  CHECK(session.View(scene, store).undo_label == undo_before);
  static_cast<void>(
      session.Apply(intents::MoveLight{.slot = 0, .native = {5.0F, -20.0F, 6.0F}, .released = true},
                    scene, store));
  CHECK(session.View(scene, store).undo_label != undo_before);
  // Le reste de la lumière n'a pas bougé.
  CHECK(store.Document(kMap).lights()[0].range > 0.0F);

  // Désigner un modèle oublie la lumière.
  session.Pick(1);
  view = session.View(scene, store);
  CHECK(view.selected == std::optional<std::size_t>(1));
  CHECK_FALSE(view.selected_light.has_value());
}

TEST_CASE("Éditeur : le bilan du décor reposé tombe une fois par période", "[app][mapedit]") {
  moonclient::ReplaceTally tally;
  CHECK_FALSE(tally.Add(4.0, 1, 100.0));
  CHECK_FALSE(tally.Add(8.0, 2, 600.0));
  CHECK(tally.Add(6.0, 0, 100.0 + moonclient::ReplaceTally::kPeriodMs));
  CHECK(tally.count == 3);
  CHECK(tally.worst_ms == 8.0);
  CHECK(tally.average_ms() == 6.0);
  CHECK(tally.baked_batches == 3);
}

TEST_CASE("Éditeur : centrer pose le point que la caméra regarde", "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  CHECK_FALSE(session.view_point().has_value());
  // Le pont désigné : la caméra le regardera.
  static_cast<void>(session.Apply(intents::Focus{}, scene, store));
  REQUIRE(session.view_point().has_value());
  CHECK(*session.view_point() == std::array<float, 3>{1.0F, 2.0F, 3.0F});
  // Une lumière désignée prend la place du modèle.
  session.SetDropPoint({7.0F, 0.0F, 9.0F});
  static_cast<void>(session.Apply(intents::AddLight{}, scene, store));
  static_cast<void>(session.Apply(intents::Focus{}, scene, store));
  CHECK((*session.view_point())[0] == 7.0F);
  // Marcher, ou quitter l'éditeur, rend la caméra au personnage.
  session.ReleaseViewPoint();
  CHECK_FALSE(session.view_point().has_value());
  static_cast<void>(session.Apply(intents::Focus{}, scene, store));
  session.Leave();
  CHECK_FALSE(session.view_point().has_value());
  CHECK(std::holds_alternative<intents::Focus>(*moonclient::MapEditorShortcut('f', false)));
}

TEST_CASE("Éditeur : Suppr masque un modèle de la carte, retire un ajout ou une lumière",
          "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  // Le pont de la carte se masque, puis se démasque.
  CHECK(session.Apply(intents::Delete{}, scene, store).replace_models);
  REQUIRE(store.Document(kMap).ModelAt(0) != nullptr);
  CHECK(store.Document(kMap).ModelAt(0)->hidden);
  static_cast<void>(session.Apply(intents::Delete{}, scene, store));
  CHECK((store.Document(kMap).ModelAt(0) == nullptr || !store.Document(kMap).ModelAt(0)->hidden));
  // Une copie ajoutée se retire.
  static_cast<void>(session.Apply(intents::Duplicate{}, scene, store));
  REQUIRE(store.Document(kMap).added().size() == 1);
  static_cast<void>(session.Apply(intents::Delete{}, scene, store));
  CHECK(store.Document(kMap).added().empty());
  // Une lumière désignée se retire.
  static_cast<void>(session.Apply(intents::AddLight{}, scene, store));
  static_cast<void>(session.Apply(intents::Delete{}, scene, store));
  CHECK(store.Document(kMap).lights().empty());
}

TEST_CASE("Éditeur : la couleur d'une lumière reste dans ses bornes", "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  static_cast<void>(session.Apply(intents::AddLight{}, scene, store));
  ro::ui::MapEditorLight wild = session.View(scene, store).lights[0];
  wild.color = {-1.0F, 9.0F, 0.5F};
  static_cast<void>(session.Apply(intents::SetLight{.slot = 0, .light = wild}, scene, store));
  const std::array<float, 3>& kept = store.Document(kMap).lights()[0].color;
  CHECK(kept[0] == 0.0F);
  CHECK(kept[1] <= 4.0F);
  CHECK(kept[2] == 0.5F);
}

TEST_CASE("Éditeur : Ctrl+flèches poussent, Ctrl+D duplique aussi une lumière", "[app][mapedit]") {
  using moonclient::MapEditorNudge;
  using moonclient::NudgeDirection;
  CHECK(MapEditorNudge(NudgeDirection::kLeft, false) ==
        std::array<float, 3>{-moonclient::kNudgeUnits, 0.0F, 0.0F});
  CHECK(MapEditorNudge(NudgeDirection::kUp, true) ==
        std::array<float, 3>{0.0F, 0.0F, moonclient::kNudgeWideUnits});

  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  // Le pont poussé d'une unité sur x, d'un seul geste.
  static_cast<void>(session.Apply(intents::Nudge{{1.0F, 0.0F, 0.0F}}, scene, store));
  REQUIRE(store.Document(kMap).ModelAt(0) != nullptr);
  CHECK((*store.Document(kMap).ModelAt(0)->position)[0] == 2.0F);
  CHECK_FALSE(session.View(scene, store).undo_label.empty());

  // Une lumière poussée, puis dupliquée sur place.
  session.SetDropPoint({7.0F, 0.0F, 9.0F});
  static_cast<void>(session.Apply(intents::AddLight{}, scene, store));
  static_cast<void>(session.Apply(intents::Nudge{{0.0F, 0.0F, -5.0F}}, scene, store));
  CHECK(store.Document(kMap).lights()[0].position[2] == 4.0F);
  static_cast<void>(session.Apply(intents::Duplicate{}, scene, store));
  REQUIRE(store.Document(kMap).lights().size() == 2);
  CHECK(store.Document(kMap).lights()[1].position == store.Document(kMap).lights()[0].position);
  CHECK(session.View(scene, store).selected_light == std::optional<std::size_t>(1));
}

TEST_CASE("Éditeur : un effet s'ajoute, se règle, se déplace et prend seul la désignation",
          "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  session.SetDropPoint({7.0F, 0.0F, 9.0F});
  static_cast<void>(session.Apply(intents::AddEffect{47}, scene, store));
  REQUIRE(store.Document(kMap).effects().size() == 1);
  const ro::mapedit::AddedEffect& added = store.Document(kMap).effects()[0];
  CHECK(added.number == 47);
  CHECK(added.position == std::array<float, 3>{7.0F, 0.0F, 9.0F});
  CHECK(added.emit_speed > 0.0F);
  ro::ui::MapEditorView view = session.View(scene, store);
  CHECK_FALSE(view.selected.has_value());
  REQUIRE(view.selected_effect == std::optional<std::size_t>(0));

  // Sans raison saisie, une raison générique dit l'ajout.
  CHECK(store.Document(kMap).effects()[0].why == "Ajout de l'effet #47");

  // La nuit, la lueur et la raison.
  ro::ui::MapEditorEffect lit = view.effects[0];
  lit.night_only = true;
  lit.glows = true;
  lit.glow_range = 0.0F;
  lit.why = "les flammes du pont";
  static_cast<void>(session.Apply(intents::SetEffect{.slot = 0, .effect = lit}, scene, store));
  const ro::mapedit::AddedEffect& set = store.Document(kMap).effects()[0];
  CHECK(set.night_only);
  REQUIRE(set.glow.has_value());
  // Une lueur garde une portée positive.
  CHECK(set.glow->range > 0.0F);

  // À la flèche, puis aux Ctrl+flèches, puis dupliqué et supprimé.
  static_cast<void>(
      session.Apply(intents::MoveEffect{.slot = 0, .native = {1.0F, -2.0F, 3.0F}, .released = true},
                    scene, store));
  CHECK(store.Document(kMap).effects()[0].position == std::array<float, 3>{1.0F, -2.0F, 3.0F});
  static_cast<void>(session.Apply(intents::Nudge{{1.0F, 0.0F, 0.0F}}, scene, store));
  CHECK(store.Document(kMap).effects()[0].position[0] == 2.0F);
  static_cast<void>(session.Apply(intents::Duplicate{}, scene, store));
  REQUIRE(store.Document(kMap).effects().size() == 2);
  CHECK(session.View(scene, store).selected_effect == std::optional<std::size_t>(1));
  static_cast<void>(session.Apply(intents::Delete{}, scene, store));
  CHECK(store.Document(kMap).effects().size() == 1);

  // Désigner une lumière oublie l'effet.
  static_cast<void>(session.Apply(intents::SelectEffect{0}, scene, store));
  static_cast<void>(session.Apply(intents::AddLight{}, scene, store));
  view = session.View(scene, store);
  CHECK_FALSE(view.selected_effect.has_value());
  CHECK(view.selected_light.has_value());
}

TEST_CASE("Éditeur : un effet de la carte se liste, se déplace, se masque et se rétablit",
          "[app][mapedit]") {
  ro::render::MapScene scene = TwoModels();
  ro::test_support::RswBuildOptions options;
  options.models = {
      {.name = "pont", .model_name = "decor\\pont.rsm", .position = {1.0F, 2.0F, 3.0F}},
      {.name = "arbre", .model_name = "decor\\arbre.rsm", .position = {4.0F, 5.0F, 6.0F}},
  };
  // Un objet d'effet, tel que l'outil d'essai l'écrit : numéro 0, à l'origine.
  options.effect_object_count = 1;
  auto rsw = ro::formats::RswScene::Parse(ro::test_support::BuildRsw(options));
  REQUIRE(rsw.has_value());
  scene.rsw = std::move(*rsw);
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  ro::ui::MapEditorView view = session.View(scene, store);
  REQUIRE(view.map_effects.size() == 1);
  CHECK(view.map_effects[0].number == 0);
  CHECK_FALSE(view.map_effects[0].edited);

  static_cast<void>(session.Apply(intents::SelectMapEffect{0}, scene, store));
  view = session.View(scene, store);
  CHECK_FALSE(view.selected.has_value());
  REQUIRE(view.selected_map_effect == std::optional<std::size_t>(0));

  // Poussé d'une unité : la retouche naît, avec sa cible.
  static_cast<void>(session.Apply(intents::Nudge{{1.0F, 0.0F, 0.0F}}, scene, store));
  const ro::mapedit::EffectEdit* retouch = store.Document(kMap).EffectEditAt(0);
  REQUIRE(retouch != nullptr);
  CHECK(retouch->number == 0);
  CHECK(retouch->origin == std::array<float, 3>{0.0F, 0.0F, 0.0F});
  CHECK((*retouch->position)[0] == 1.0F);
  // Sans raison saisie, une raison générique dit la retouche.
  CHECK(retouch->why == "Retouche de l'effet #0");

  // Suppr le masque, puis le démasque.
  static_cast<void>(session.Apply(intents::Delete{}, scene, store));
  CHECK(store.Document(kMap).EffectEditAt(0)->hidden);
  static_cast<void>(session.Apply(intents::Delete{}, scene, store));
  CHECK_FALSE(store.Document(kMap).EffectEditAt(0)->hidden);

  // Ctrl+D en fait une copie ajoutée, à côté de lui.
  static_cast<void>(session.Apply(intents::Duplicate{}, scene, store));
  REQUIRE(store.Document(kMap).effects().size() == 1);
  CHECK(store.Document(kMap).effects()[0].number == 0);

  // Rétabli : la retouche s'en va.
  static_cast<void>(session.Apply(intents::RevertMapEffect{0}, scene, store));
  CHECK(store.Document(kMap).EffectEditAt(0) == nullptr);
}

TEST_CASE("Éditeur : désigner moins lâche le modèle, puis plus rien", "[app][mapedit]") {
  MapEditorSession session = OpenOnBridge();
  REQUIRE(session.selected() == 0U);
  CHECK(session.Deselect());
  CHECK_FALSE(session.selected().has_value());
  // Rien de désigné : la touche revient au jeu.
  CHECK_FALSE(session.Deselect());
}

TEST_CASE("Éditeur : les actions rapides agissent sur un modèle, éditeur fermé", "[app][mapedit]") {
  ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session;
  session.FollowMap(kMap, scene);
  REQUIRE_FALSE(session.active());
  const std::array<float, 3> here{10.0F, 2.0F, 3.0F};
  // Masquer, puis démasquer : le modèle est désigné au passage.
  static_cast<void>(
      session.QuickModel(moonclient::QuickModelAction::kToggleHidden, 1, here, scene, store));
  CHECK(session.selected() == 1U);
  REQUIRE(store.Document(kMap).ModelAt(1) != nullptr);
  CHECK(store.Document(kMap).ModelAt(1)->hidden);
  // Sur le personnage.
  static_cast<void>(
      session.QuickModel(moonclient::QuickModelAction::kMoveHere, 0, here, scene, store));
  REQUIRE(store.Document(kMap).ModelAt(0) != nullptr);
  CHECK(store.Document(kMap).ModelAt(0)->position == here);
  // Restaurer rend le placement d'origine.
  static_cast<void>(
      session.QuickModel(moonclient::QuickModelAction::kRevert, 0, here, scene, store));
  CHECK(store.Document(kMap).ModelAt(0) == nullptr);
  CHECK_FALSE(session.active());
  // Ouvrir avec l'éditeur.
  static_cast<void>(
      session.QuickModel(moonclient::QuickModelAction::kOpenEditor, 1, here, scene, store));
  CHECK(session.active());
  CHECK(session.selected() == 1U);
  // Un rang hors de la carte ne fait rien.
  static_cast<void>(
      session.QuickModel(moonclient::QuickModelAction::kToggleHidden, 9, here, scene, store));
  CHECK(session.selected() == 1U);
}

TEST_CASE("Éditeur : une lumière s'ajoute là où on la demande", "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  const std::array<float, 3> lantern{12.0F, -30.0F, 7.0F};
  static_cast<void>(session.Apply(intents::AddLight{.position = lantern}, scene, store));
  REQUIRE(store.Document(kMap).lights().size() == 1);
  CHECK(store.Document(kMap).lights()[0].position == lantern);
}

TEST_CASE("Éditeur : désigner puis dupliquer, comme le menu d'une ligne", "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  static_cast<void>(session.Apply(intents::AddLight{}, scene, store));
  static_cast<void>(session.Apply(intents::AddLight{}, scene, store));
  // Le menu de la première lumière : la désigner, puis la dupliquer.
  static_cast<void>(session.Apply(intents::SelectLight{0}, scene, store));
  static_cast<void>(session.Apply(intents::Duplicate{}, scene, store));
  REQUIRE(store.Document(kMap).lights().size() == 3);
  CHECK(store.Document(kMap).lights()[2].color == store.Document(kMap).lights()[0].color);
  // Puis la supprimer.
  static_cast<void>(session.Apply(intents::SelectLight{0}, scene, store));
  static_cast<void>(session.Apply(intents::Delete{}, scene, store));
  CHECK(store.Document(kMap).lights().size() == 2);
}

TEST_CASE("Éditeur : l'opacité seule est une retouche, et un geste comme les autres",
          "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  intents::Place faded = MovedTo(1.0F, false);
  faded.opacity = 0.6F;
  // Pendant le glissé, le document change sans geste dans l'historique.
  CHECK(session.Apply(faded, scene, store).replace_models);
  CHECK(session.View(scene, store).undo_label.empty());
  faded.opacity = 0.4F;
  faded.released = true;
  CHECK(session.Apply(faded, scene, store).replace_models);
  const ro::mapedit::ModelEdit* edit = store.Document(kMap).ModelAt(0);
  REQUIRE(edit != nullptr);
  CHECK(edit->opacity == std::optional<float>(0.4F));
  CHECK_FALSE(edit->position.has_value());
  CHECK(session.View(scene, store).opacity == 0.4F);
  CHECK((*session.View(scene, store).models)[0].translucent);

  // Un placement qui ne dit rien de l'opacité la garde : une poussée, un
  // masquage.
  static_cast<void>(session.Apply(intents::Nudge{{1.0F, 0.0F, 0.0F}}, scene, store));
  CHECK(store.Document(kMap).ModelAt(0)->opacity == std::optional<float>(0.4F));
  // La copie la garde aussi.
  static_cast<void>(session.Apply(intents::Duplicate{}, scene, store));
  REQUIRE(store.Document(kMap).added().size() == 1);
  CHECK(store.Document(kMap).added()[0].opacity == std::optional<float>(0.4F));

  // Annuler défait les gestes un à un, jusqu'au document vide.
  for (int gesture = 0; gesture < 3; ++gesture) {
    static_cast<void>(session.Apply(intents::Undo{}, scene, store));
  }
  CHECK(store.Document(kMap).empty());

  // Revenue au plein, l'opacité seule ne retient plus la retouche.
  session.Pick(0);
  static_cast<void>(session.Apply(faded, scene, store));
  REQUIRE(store.Document(kMap).ModelAt(0) != nullptr);
  faded.opacity = 1.0F;
  static_cast<void>(session.Apply(faded, scene, store));
  CHECK(store.Document(kMap).empty());
}

TEST_CASE("Éditeur : un modèle collé sur une cellule se pose à la position donnée",
          "[app][mapedit]") {
  const ro::render::MapScene scene = TwoModels();
  MapEditStore store;
  MapEditorSession session = OpenOnBridge();
  session.SetDropPoint({7.0F, 0.0F, 9.0F});

  // Sans position, il se pose où regarde la caméra.
  static_cast<void>(session.Apply(intents::AddModel{.file = "a.rsm"}, scene, store));
  // Avec elle, sur la cellule visée.
  static_cast<void>(session.Apply(
      intents::AddModel{.file = "b.rsm", .position = std::array<float, 3>{1.0F, 2.0F, 3.0F}}, scene,
      store));
  REQUIRE(store.Document(kMap).added().size() == 2);
  CHECK(store.Document(kMap).added()[0].position == std::array<float, 3>{7.0F, 0.0F, 9.0F});
  CHECK(store.Document(kMap).added()[1].position == std::array<float, 3>{1.0F, 2.0F, 3.0F});
}
