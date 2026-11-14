// LES SONS D'AMBIANCE DANS L'ÉDITEUR : ce que la session fait des intentions de
// la section « Sons ».

#include <array>
#include <cstddef>
#include <optional>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/map_scene.h"
#include "ro/ui/map_editor_panel.h"

#include "support/map_builder.h"
#include "world/map_editor_session.h"

using moonclient::MapEditorSession;
using moonclient::MapEditStore;
using ro::ui::MapEditorSoundKey;
namespace intents = ro::ui::map_editor;

namespace {

constexpr const char* kMap = "essai";
constexpr MapEditorSoundKey kFountain{.added = false, .index = 0};
constexpr MapEditorSoundKey kFirstAdded{.added = true, .index = 0};

// Une carte d'une source de son : une fontaine.
ro::render::MapScene OneFountain() {
  auto gnd = ro::formats::GndMap::Parse(ro::test_support::BuildGnd({}));
  ro::test_support::RswBuildOptions options;
  options.sounds = {{.name = "fontaine",
                     .wave_name = "fontaine.wav",
                     .position = {10.0F, 0.0F, 20.0F},
                     .volume = 0.8F,
                     .width = 10,
                     .height = 10,
                     .range = 60.0F,
                     .cycle_seconds = 4.0F}};
  auto rsw = ro::formats::RswScene::Parse(ro::test_support::BuildRsw(options));
  REQUIRE(gnd.has_value());
  REQUIRE(rsw.has_value());
  REQUIRE(rsw->sounds().size() == 1);
  ro::render::MapScene scene{.gnd = std::move(*gnd), .rsw = std::move(*rsw)};
  scene.unfixed_models = scene.rsw.models();
  return scene;
}

MapEditorSession OpenOn(const ro::render::MapScene& scene, MapEditStore& store) {
  MapEditorSession session;
  session.FollowMap(kMap, scene);
  static_cast<void>(session.Apply(intents::Toggle{}, scene, store));
  return session;
}

}  // namespace

TEST_CASE("Éditeur : une source de son se liste, se retouche, se masque et se rétablit",
          "[app][mapedit]") {
  const ro::render::MapScene scene = OneFountain();
  MapEditStore store;
  MapEditorSession session = OpenOn(scene, store);
  ro::ui::MapEditorView view = session.View(scene, store);
  REQUIRE(view.sounds.size() == 1);
  CHECK(view.sounds[0].file == "fontaine.wav");
  CHECK(view.sounds[0].range == 60.0F);
  CHECK_FALSE(view.sounds[0].edited);

  static_cast<void>(session.Apply(intents::SelectSound{kFountain}, scene, store));
  view = session.View(scene, store);
  REQUIRE(view.selected_sound == std::optional<MapEditorSoundKey>(kFountain));

  // Sa portée réglée : la retouche naît, avec sa cible, et seulement ce qui
  // change.
  intents::SetSound set{.sound = view.sounds[0], .released = true};
  set.sound.range = 30.0F;
  static_cast<void>(session.Apply(set, scene, store));
  const ro::mapedit::SoundEdit* retouch = store.Document(kMap).SoundEditAt(0);
  REQUIRE(retouch != nullptr);
  CHECK(retouch->wave_file == "fontaine.wav");
  CHECK(retouch->origin == std::array<float, 3>{10.0F, 0.0F, 20.0F});
  CHECK(retouch->range == std::optional<float>(30.0F));
  CHECK_FALSE(retouch->volume.has_value());
  CHECK_FALSE(retouch->position.has_value());
  // Sans raison saisie, une raison générique dit la retouche.
  CHECK(retouch->why == "Retouche du son fontaine.wav");
  CHECK(session.View(scene, store).sounds[0].edited);

  // Ctrl+flèche la pousse ; Suppr la fait taire, puis reprendre.
  static_cast<void>(session.Apply(intents::Nudge{{1.0F, 0.0F, 0.0F}}, scene, store));
  CHECK((*store.Document(kMap).SoundEditAt(0)->position)[0] == 11.0F);
  static_cast<void>(session.Apply(intents::Delete{}, scene, store));
  CHECK(store.Document(kMap).SoundEditAt(0)->hidden);
  static_cast<void>(session.Apply(intents::Delete{}, scene, store));
  CHECK_FALSE(store.Document(kMap).SoundEditAt(0)->hidden);

  // Rétablie : la retouche s'en va ; annulée : elle revient.
  static_cast<void>(session.Apply(intents::RevertSound{kFountain}, scene, store));
  CHECK(store.Document(kMap).SoundEditAt(0) == nullptr);
  static_cast<void>(session.Apply(intents::Undo{}, scene, store));
  CHECK(store.Document(kMap).SoundEditAt(0) != nullptr);
}

TEST_CASE("Éditeur : revenir aux valeurs du RSW retire la retouche", "[app][mapedit]") {
  const ro::render::MapScene scene = OneFountain();
  MapEditStore store;
  MapEditorSession session = OpenOn(scene, store);
  const ro::ui::MapEditorSound original = session.View(scene, store).sounds[0];
  intents::SetSound set{.sound = original, .released = true};
  set.sound.volume = 0.2F;
  static_cast<void>(session.Apply(set, scene, store));
  REQUIRE(store.Document(kMap).SoundEditAt(0) != nullptr);
  static_cast<void>(session.Apply(intents::SetSound{.sound = original}, scene, store));
  CHECK(store.Document(kMap).SoundEditAt(0) == nullptr);
}

TEST_CASE("Éditeur : un son s'ajoute où regarde la caméra, se règle, se duplique et s'annule",
          "[app][mapedit]") {
  const ro::render::MapScene scene = OneFountain();
  MapEditStore store;
  MapEditorSession session = OpenOn(scene, store);
  session.SetDropPoint({50.0F, 0.0F, 60.0F});
  static_cast<void>(session.Apply(intents::AddSound{"_ambience\\grillon.wav"}, scene, store));
  REQUIRE(store.Document(kMap).sounds().size() == 1);
  const ro::mapedit::AddedSound& added = store.Document(kMap).sounds()[0];
  CHECK(added.wave_file == "_ambience\\grillon.wav");
  CHECK(added.position == std::array<float, 3>{50.0F, 0.0F, 60.0F});
  CHECK(added.why == "Ajout du son _ambience\\grillon.wav");
  ro::ui::MapEditorView view = session.View(scene, store);
  REQUIRE(view.selected_sound == std::optional<MapEditorSoundKey>(kFirstAdded));
  REQUIRE(view.sounds.size() == 2);

  // Réglé d'un glissé : il se voit, et ne compte qu'au relâcher.
  intents::SetSound set{.sound = view.sounds[1], .released = false};
  set.sound.cycle_seconds = 9.0F;
  set.sound.night_only = true;
  static_cast<void>(session.Apply(set, scene, store));
  set.released = true;
  static_cast<void>(session.Apply(set, scene, store));
  CHECK(store.Document(kMap).sounds()[0].cycle_seconds == 9.0F);
  CHECK(store.Document(kMap).sounds()[0].night_only);

  // Ctrl+D : une copie sur place, désignée à son tour.
  static_cast<void>(session.Apply(intents::Duplicate{}, scene, store));
  REQUIRE(store.Document(kMap).sounds().size() == 2);
  CHECK(store.Document(kMap).sounds()[1].cycle_seconds == 9.0F);
  CHECK(store.Document(kMap).sounds()[1].position == store.Document(kMap).sounds()[0].position);
  CHECK(session.View(scene, store).selected_sound ==
        std::optional<MapEditorSoundKey>(MapEditorSoundKey{.added = true, .index = 1}));

  // Annuler défait la copie, puis le réglage, puis l'ajout.
  static_cast<void>(session.Apply(intents::Undo{}, scene, store));
  CHECK(store.Document(kMap).sounds().size() == 1);
  static_cast<void>(session.Apply(intents::Undo{}, scene, store));
  CHECK_FALSE(store.Document(kMap).sounds()[0].night_only);
  static_cast<void>(session.Apply(intents::Undo{}, scene, store));
  CHECK(store.Document(kMap).sounds().empty());
}

TEST_CASE("Éditeur : un son ajouté se retire, et une source se duplique en ajout",
          "[app][mapedit]") {
  const ro::render::MapScene scene = OneFountain();
  MapEditStore store;
  MapEditorSession session = OpenOn(scene, store);
  static_cast<void>(session.Apply(intents::SelectSound{kFountain}, scene, store));
  static_cast<void>(session.Apply(intents::Duplicate{}, scene, store));
  REQUIRE(store.Document(kMap).sounds().size() == 1);
  CHECK(store.Document(kMap).sounds()[0].wave_file == "fontaine.wav");
  CHECK(store.Document(kMap).sounds()[0].range == 60.0F);
  CHECK(store.Document(kMap).sounds()[0].width == 10);
  // La copie est désignée : Suppr la retire.
  static_cast<void>(session.Apply(intents::Delete{}, scene, store));
  CHECK(store.Document(kMap).sounds().empty());
  CHECK_FALSE(session.View(scene, store).selected_sound.has_value());
}

TEST_CASE("Éditeur : un clic sur le repère d'un son le désigne, et sa flèche le déplace",
          "[app][mapedit]") {
  const ro::render::MapScene scene = OneFountain();
  MapEditStore store;
  MapEditorSession session = OpenOn(scene, store);
  constexpr glm::vec2 kDotAt{200.0F, 150.0F};
  session.SetSoundDots({MapEditorSession::SoundDot{.at = kDotAt, .key = kFountain}});
  CHECK(session.Grab(kDotAt, session.View(scene, store)));
  CHECK(session.View(scene, store).selected_sound == std::optional<MapEditorSoundKey>(kFountain));
  CHECK_FALSE(session.dragging());
}

TEST_CASE("Éditeur : « Écouter » ne touche pas au document", "[app][mapedit]") {
  const ro::render::MapScene scene = OneFountain();
  MapEditStore store;
  MapEditorSession session = OpenOn(scene, store);
  const MapEditorSession::Outcome outcome =
      session.Apply(intents::ListenSound{.file = "fontaine.wav", .volume = 0.5F}, scene, store);
  REQUIRE(outcome.listen.has_value());
  CHECK(outcome.listen->file == "fontaine.wav");
  CHECK(store.Document(kMap).empty());
  CHECK_FALSE(store.Unsaved(kMap));
}

TEST_CASE("Éditeur : désigner un son lâche le sprite désigné, et inversement", "[app][mapedit]") {
  const ro::render::MapScene scene = OneFountain();
  MapEditStore store;
  MapEditorSession session = OpenOn(scene, store);
  static_cast<void>(session.Apply(intents::AddSprite{"SAVAGE"}, scene, store));
  REQUIRE(session.View(scene, store).selected_sprite == std::optional<std::size_t>(0));

  static_cast<void>(session.Apply(intents::SelectSound{kFountain}, scene, store));
  ro::ui::MapEditorView view = session.View(scene, store);
  CHECK(view.selected_sound == std::optional<MapEditorSoundKey>(kFountain));
  CHECK_FALSE(view.selected_sprite.has_value());
  // Suppr vise le son seul : le sprite reste.
  static_cast<void>(session.Apply(intents::Delete{}, scene, store));
  CHECK(store.Document(kMap).SoundEditAt(0)->hidden);
  CHECK(store.Document(kMap).sprites().size() == 1);

  static_cast<void>(session.Apply(intents::SelectSprite{0}, scene, store));
  view = session.View(scene, store);
  CHECK(view.selected_sprite == std::optional<std::size_t>(0));
  CHECK_FALSE(view.selected_sound.has_value());
}
