// LES SPRITES DE DÉCOR DANS L'ÉDITEUR DE CARTE : ce que la session fait des
// intentions du panneau, et le roulis de leurs calques.

#include <array>
#include <cmath>
#include <optional>
#include <utility>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/map_scene.h"
#include "ro/ui/map_editor_panel.h"

#include "support/map_builder.h"
#include "world/map_editor_session.h"
#include "world/map_sprites.h"

using moonclient::MapEditorSession;
using moonclient::MapEditStore;
namespace intents = ro::ui::map_editor;

namespace {

constexpr const char* kMap = "essai";

// Une carte d'un modèle, dont le placement d'origine est gardé.
ro::render::MapScene OneModel() {
  auto gnd = ro::formats::GndMap::Parse(ro::test_support::BuildGnd({}));
  ro::test_support::RswBuildOptions options;
  options.models = {
      {.name = "feu", .model_name = "decor\\feu.rsm", .position = {1.0F, 2.0F, 3.0F}},
  };
  auto rsw = ro::formats::RswScene::Parse(ro::test_support::BuildRsw(options));
  REQUIRE(gnd.has_value());
  REQUIRE(rsw.has_value());
  ro::render::MapScene scene{.gnd = std::move(*gnd), .rsw = std::move(*rsw)};
  scene.unfixed_models = scene.rsw.models();
  return scene;
}

// Une session ouverte sur la carte, le modèle désigné.
MapEditorSession Opened(const ro::render::MapScene& scene, MapEditStore& store) {
  MapEditorSession session;
  session.FollowMap(kMap, scene);
  static_cast<void>(session.Apply(intents::Toggle{}, scene, store));
  session.Pick(0);
  return session;
}

}  // namespace

TEST_CASE("Sprites de décor : un sprite s'ajoute au point de pose, désigné, avec sa raison",
          "[app][mapedit]") {
  const ro::render::MapScene scene = OneModel();
  MapEditStore store;
  MapEditorSession session = Opened(scene, store);
  session.SetDropPoint({7.0F, 0.0F, 9.0F});
  static_cast<void>(session.Apply(intents::AddSprite{"SAVAGE"}, scene, store));
  REQUIRE(store.Document(kMap).sprites().size() == 1);
  const ro::mapedit::MapSprite& added = store.Document(kMap).sprites()[0];
  CHECK(added.sprite == "SAVAGE");
  CHECK(added.position == std::array<float, 3>{7.0F, 0.0F, 9.0F});
  // Sans raison saisie, une raison générique dit l'ajout.
  CHECK(added.why == "Sprite de décor SAVAGE");
  // Le sprite est seul désigné : le modèle ne l'est plus.
  const ro::ui::MapEditorView view = session.View(scene, store);
  CHECK_FALSE(view.selected.has_value());
  REQUIRE(view.selected_sprite == std::optional<std::size_t>(0));
  REQUIRE(view.sprites.size() == 1);
  CHECK(view.sprites[0].sprite == "SAVAGE");
  // Un nom vide n'ajoute rien.
  static_cast<void>(session.Apply(intents::AddSprite{""}, scene, store));
  CHECK(store.Document(kMap).sprites().size() == 1);
}

TEST_CASE("Sprites de décor : un réglage glissé ne compte qu'au relâcher, et s'annule",
          "[app][mapedit]") {
  const ro::render::MapScene scene = OneModel();
  MapEditStore store;
  MapEditorSession session = Opened(scene, store);
  static_cast<void>(session.Apply(intents::AddSprite{"SAVAGE"}, scene, store));
  ro::ui::MapEditorSprite shown = session.View(scene, store).sprites[0];
  shown.frozen = true;
  shown.held_frame = 2;
  shown.roll_degrees = 90.0F;
  shown.roll_degrees_per_second = 30.0F;
  shown.turn_degrees_per_second = 45.0F;
  shown.direction = 11;
  shown.scale = 0.0F;
  shown.opacity = 0.5F;
  static_cast<void>(session.Apply(intents::SetSprite{.slot = 0, .sprite = shown, .released = false},
                                  scene, store));
  const ro::mapedit::MapSprite& set = store.Document(kMap).sprites()[0];
  CHECK(set.held_frame == std::optional<std::uint32_t>(2));
  CHECK(set.roll_degrees == 90.0F);
  CHECK(set.roll_degrees_per_second == 30.0F);
  CHECK(set.turn_degrees_per_second == 45.0F);
  // Ce qui sort des bornes du document y revient.
  CHECK(set.direction == 3);
  CHECK(set.scale > 0.0F);
  CHECK(set.opacity == std::optional<float>(0.5F));
  // Pas encore relâché : rien de plus à annuler que l'ajout.
  CHECK(session.View(scene, store).undo_label == "Ajouter un sprite");
  static_cast<void>(session.Apply(intents::SetSprite{.slot = 0, .sprite = shown, .released = true},
                                  scene, store));
  CHECK(session.View(scene, store).undo_label == "Régler le sprite 0");
  static_cast<void>(session.Apply(intents::Undo{}, scene, store));
  CHECK_FALSE(store.Document(kMap).sprites()[0].held_frame.has_value());
  static_cast<void>(session.Apply(intents::Redo{}, scene, store));
  CHECK(store.Document(kMap).sprites()[0].held_frame == std::optional<std::uint32_t>(2));
}

TEST_CASE("Sprites de décor : se déplacer, se dupliquer, se retirer", "[app][mapedit]") {
  const ro::render::MapScene scene = OneModel();
  MapEditStore store;
  MapEditorSession session = Opened(scene, store);
  session.SetDropPoint({7.0F, 0.0F, 9.0F});
  static_cast<void>(session.Apply(intents::AddSprite{"SAVAGE"}, scene, store));
  // Ctrl+flèche pousse le sprite désigné.
  static_cast<void>(session.Apply(intents::Nudge{{1.0F, 0.0F, 0.0F}}, scene, store));
  CHECK(store.Document(kMap).sprites()[0].position[0] == 8.0F);
  static_cast<void>(
      session.Apply(intents::MoveSprite{.slot = 0, .native = {2.0F, -1.0F, 4.0F}, .released = true},
                    scene, store));
  CHECK(store.Document(kMap).sprites()[0].position == std::array<float, 3>{2.0F, -1.0F, 4.0F});
  // Dupliquer pose une copie sur place, et la désigne.
  static_cast<void>(session.Apply(intents::Duplicate{}, scene, store));
  REQUIRE(store.Document(kMap).sprites().size() == 2);
  CHECK(store.Document(kMap).sprites()[1].position == std::array<float, 3>{2.0F, -1.0F, 4.0F});
  CHECK(store.Document(kMap).sprites()[1].sprite == "SAVAGE");
  CHECK(session.View(scene, store).selected_sprite == std::optional<std::size_t>(1));
  // Centrer pose le point que la caméra regarde.
  static_cast<void>(session.Apply(intents::Focus{}, scene, store));
  REQUIRE(session.view_point().has_value());
  CHECK(*session.view_point() == store.Document(kMap).sprites()[1].position);
  // Suppr retire le désigné.
  static_cast<void>(session.Apply(intents::Delete{}, scene, store));
  CHECK(store.Document(kMap).sprites().size() == 1);
  CHECK_FALSE(session.View(scene, store).selected_sprite.has_value());
  static_cast<void>(session.Apply(intents::RemoveSprite{0}, scene, store));
  CHECK(store.Document(kMap).sprites().empty());
  // Annuler les rend.
  static_cast<void>(session.Apply(intents::Undo{}, scene, store));
  CHECK(store.Document(kMap).sprites().size() == 1);
}

TEST_CASE("Sprites de décor : un clic sur le carré désigne, la flèche déplace", "[app][mapedit]") {
  const ro::render::MapScene scene = OneModel();
  MapEditStore store;
  MapEditorSession session = Opened(scene, store);
  static_cast<void>(session.Apply(intents::AddSprite{"SAVAGE"}, scene, store));
  static_cast<void>(session.Apply(intents::SelectSprite{std::nullopt}, scene, store));
  session.Pick(0);
  session.SetSpriteDots({MapEditorSession::LightDot{.at = {100.0F, 100.0F}, .slot = 0}});
  const ro::ui::MapEditorView before = session.View(scene, store);
  CHECK(session.Grab({101.0F, 99.0F}, before));
  const ro::ui::MapEditorView view = session.View(scene, store);
  CHECK(view.selected_sprite == std::optional<std::size_t>(0));
  CHECK_FALSE(view.selected.has_value());
  // Le carré du désigné se distingue.
  const auto marks = session.SpriteTrace();
  REQUIRE(marks.size() == 1);
  CHECK(marks[0].kind == ro::ui::MapEditorLineKind::kSpriteSelected);
}

TEST_CASE("Sprites de décor : la raison se saisit, et n'est jamais vide", "[app][mapedit]") {
  const ro::render::MapScene scene = OneModel();
  MapEditStore store;
  MapEditorSession session = Opened(scene, store);
  static_cast<void>(session.Apply(intents::AddSprite{"SAVAGE"}, scene, store));
  ro::ui::MapEditorSprite shown = session.View(scene, store).sprites[0];
  shown.why = "le rôti du campement";
  static_cast<void>(session.Apply(intents::SetSprite{.slot = 0, .sprite = shown}, scene, store));
  CHECK(store.Document(kMap).sprites()[0].why == "le rôti du campement");
  // Une raison vidée en reprend une générique : la raison n'est jamais vide.
  shown.why.clear();
  static_cast<void>(session.Apply(intents::SetSprite{.slot = 0, .sprite = shown}, scene, store));
  CHECK_FALSE(store.Document(kMap).sprites()[0].why.empty());
}

TEST_CASE("Sprites de décor : le roulis tourne les calques autour du centre de leur boîte",
          "[app][mapedit]") {
  ro::render::ComposedActor composed;
  ro::render::SpriteQuad quad;
  // Un calque de 20 sur 10, centré en (0, -5) : les ordonnées descendent.
  quad.corners = {glm::vec2(-10.0F, -10.0F), glm::vec2(10.0F, -10.0F), glm::vec2(10.0F, 0.0F),
                  glm::vec2(-10.0F, 0.0F)};
  composed.quads.push_back(quad);
  ro::render::ComposedActor rolled = composed;
  moonclient::RollQuads(rolled, 0.0F);
  CHECK(rolled.quads[0].corners == composed.quads[0].corners);
  // Un quart de tour, dans le sens des aiguilles d'une montre à l'écran : le
  // coin haut-gauche passe en haut à droite.
  moonclient::RollQuads(rolled, 90.0F);
  const glm::vec2 top_left = rolled.quads[0].corners[0];
  CHECK(top_left.x == Catch::Approx(5.0F));
  CHECK(top_left.y == Catch::Approx(-15.0F));
  // Le centre ne bouge pas.
  glm::vec2 sum(0.0F);
  for (const glm::vec2& corner : rolled.quads[0].corners) {
    sum += corner;
  }
  CHECK(sum.x / 4.0F == Catch::Approx(0.0F).margin(1.0e-4));
  CHECK(sum.y / 4.0F == Catch::Approx(-5.0F));
}
