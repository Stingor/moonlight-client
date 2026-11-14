// LES OBJETS D'EFFET À LEVER : ceux du RSW, puis ceux du document, les « nuit
// seulement » à la nuit.

#include "ro/render/edited_map_effects.h"

#include <array>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/map_point_lights.h"
#include "ro/render/scene_light.h"

using ro::mapedit::AddedEffect;
using ro::render::ActiveMapEffects;

TEST_CASE("Effets du document : la nuit lève les siens, et la liste se compare",
          "[render][mapedit]") {
  const std::vector<ro::formats::RswEffect> map_objects = {
      ro::formats::RswEffect{.name = "fontaine", .position = {1.0F, 2.0F, 3.0F}, .number = 44}};
  ro::mapedit::MapEdit added;
  added.AddEffect(AddedEffect{.number = 47,
                              .position = {10.0F, 0.0F, 10.0F},
                              .emit_speed = 125.0F,
                              .param = {0.6F, 1.0F, 0.0F, 0.0F},
                              .night_only = true,
                              .why = "flamme"});
  added.AddEffect(AddedEffect{.number = 300, .position = {}, .why = "toujours"});
  const std::vector<ro::formats::RswEffect> day = ActiveMapEffects(map_objects, added, 0.0F);
  REQUIRE(day.size() == 2);
  CHECK(day[0].name == "fontaine");
  CHECK(day[1].number == 300);
  const std::vector<ro::formats::RswEffect> night = ActiveMapEffects(map_objects, added, 1.0F);
  REQUIRE(night.size() == 3);
  CHECK(night[1].number == 47);
  CHECK(night[1].emit_speed == 125.0F);
  CHECK(night[1].param[0] == 0.6F);
  CHECK_FALSE(ro::render::SameMapEffects(day, night));
  CHECK(ro::render::SameMapEffects(night, ActiveMapEffects(map_objects, added, 0.9F)));
}

TEST_CASE("Effets du document : la nuit se lit dans le ciel et la teinte du serveur",
          "[render][mapedit]") {
  ro::render::SceneLight noon;
  CHECK(ro::render::NightStrength(noon, false) == 0.0F);
  CHECK(ro::render::NightStrength(noon, true) == 1.0F);
  ro::render::SceneLight server_night;
  server_night.tint = glm::vec3(0.2F);
  CHECK(ro::render::NightStrength(server_night, false) >= ro::render::kEffectNightThreshold);
}

TEST_CASE("Effets de la carte : une retouche déplace, masque ou réserve à la nuit",
          "[render][mapedit]") {
  const std::vector<ro::formats::RswEffect> map_objects = {
      ro::formats::RswEffect{.name = "a", .position = {1.0F, 0.0F, 1.0F}, .number = 47},
      ro::formats::RswEffect{.name = "b", .position = {2.0F, 0.0F, 2.0F}, .number = 47},
      ro::formats::RswEffect{.name = "c", .position = {3.0F, 0.0F, 3.0F}, .number = 47}};
  ro::mapedit::MapEdit document;
  document.SetEffectEdit(ro::mapedit::EffectEdit{.effect_index = 0,
                                                 .number = 47,
                                                 .origin = {1.0F, 0.0F, 1.0F},
                                                 .position = std::array<float, 3>{5.0F, 0.0F, 5.0F},
                                                 .glow = ro::mapedit::EffectGlow{},
                                                 .why = "déplacé"});
  document.SetEffectEdit(ro::mapedit::EffectEdit{
      .effect_index = 1, .number = 47, .origin = {2.0F, 0.0F, 2.0F}, .hidden = true, .why = "x"});
  // Une retouche qui ne vise plus son objet ne s'applique pas.
  document.SetEffectEdit(ro::mapedit::EffectEdit{
      .effect_index = 2, .number = 48, .origin = {3.0F, 0.0F, 3.0F}, .hidden = true, .why = "y"});
  const std::vector<ro::formats::RswEffect> objects = ActiveMapEffects(map_objects, document, 0.0F);
  REQUIRE(objects.size() == 2);
  CHECK(objects[0].position == std::array<float, 3>{5.0F, 0.0F, 5.0F});
  CHECK(objects[1].name == "c");
  // La lueur suit la retouche, là où elle pose l'effet.
  const std::vector<ro::mapedit::AddedLight> lights =
      ro::render::DocumentLights(document, map_objects);
  REQUIRE(lights.size() == 1);
  CHECK(lights[0].position == std::array<float, 3>{5.0F, -ro::render::kEffectGlowLiftWorld, 5.0F});
}

TEST_CASE("Effets du document : une plage d'heures prime sur la nuit quand le ciel a une heure",
          "[render][mapedit]") {
  ro::mapedit::MapEdit document;
  document.AddEffect(
      AddedEffect{.number = 47,
                  .position = {},
                  .night_only = true,
                  .hours = ro::mapedit::LitHours{.from_hour = 22.0F, .to_hour = 2.0F},
                  .why = "flamme tardive"});
  // Pleine nuit, mais 21 h : hors de la plage, l'effet attend.
  CHECK(ActiveMapEffects({}, document, 1.0F, 21.0F).empty());
  CHECK(ActiveMapEffects({}, document, 1.0F, 23.0F).size() == 1);
  // Sans heure du ciel, la nuit du serveur décide.
  CHECK(ActiveMapEffects({}, document, 1.0F).size() == 1);
  CHECK(ActiveMapEffects({}, document, 0.0F).empty());
  // Une lumière luit selon sa plage, en fondu à ses bornes.
  CHECK(ro::render::NightGlow(document.effects()[0].hours, 0.0F, 0.0F) == 1.0F);
  CHECK(ro::render::NightGlow(document.effects()[0].hours, 1.0F, 12.0F) == 0.0F);
  CHECK(ro::render::NightGlow(std::nullopt, 0.25F, 12.0F) == 0.25F);
}
