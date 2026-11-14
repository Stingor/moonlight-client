// LES LUMIÈRES PONCTUELLES DU DOCUMENT D'ÉDITION : lues, écrites, refusées.

#include <array>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/rsw.h"
#include "ro/mapedit/map_edit.h"

using ro::mapedit::AddedLight;
using ro::mapedit::MapEdit;

TEST_CASE("Lumières : le document les écrit et les relit à l'identique", "[mapedit]") {
  MapEdit edit;
  edit.AddLight(AddedLight{.position = {10.0F, -5.0F, 20.0F},
                           .color = {1.0F, 0.75F, 0.5F},
                           .range = 40.0F,
                           .night_only = true,
                           .flicker = true,
                           .why = "la lanterne du pont"});
  edit.AddLight(AddedLight{
      .position = {}, .color = {2.0F, 2.0F, 2.0F}, .range = 12.5F, .why = "le feu de camp"});
  CHECK_FALSE(edit.empty());
  const std::string text = ro::mapedit::MapEditToYaml(edit, "gonryun");
  auto reread = ro::mapedit::ParseMapEdit(text, "gonryun");
  REQUIRE(reread.has_value());
  CHECK(*reread == edit);

  // Retirer la première fait remonter la seconde.
  edit.RemoveLight(0);
  REQUIRE(edit.lights().size() == 1);
  CHECK(edit.lights()[0].why == "le feu de camp");
  edit.SetLight(0, AddedLight{.position = {}, .color = {}, .range = 1.0F, .why = "éteinte"});
  CHECK(edit.lights()[0].why == "éteinte");
}

TEST_CASE("Lumières : une portée nulle ou une raison absente est refusée", "[mapedit]") {
  constexpr const char* kNoRange =
      "version: 1\nmap: 'gonryun'\nmodels: []\nlights:\n"
      "  - position: [0, 0, 0]\n    color: [1, 1, 1]\n    range: 0\n    why: \"x\"\n";
  CHECK_FALSE(ro::mapedit::ParseMapEdit(kNoRange, "gonryun").has_value());
  constexpr const char* kNoWhy =
      "version: 1\nmap: 'gonryun'\nmodels: []\nlights:\n"
      "  - position: [0, 0, 0]\n    color: [1, 1, 1]\n    range: 5\n";
  CHECK_FALSE(ro::mapedit::ParseMapEdit(kNoWhy, "gonryun").has_value());
}

TEST_CASE("Effets : le document les écrit et les relit à l'identique", "[mapedit]") {
  MapEdit edit;
  edit.AddEffect(ro::mapedit::AddedEffect{.number = 47,
                                          .position = {12.0F, -3.0F, 40.0F},
                                          .emit_speed = 125.0F,
                                          .param = {0.6F, 1.0F, 0.0F, 0.0F},
                                          .night_only = true,
                                          .glow = ro::mapedit::EffectGlow{},
                                          .why = "les flammes de pvp_n_5-5"});
  edit.AddEffect(ro::mapedit::AddedEffect{.number = 44, .position = {}, .why = "une cheminée"});
  CHECK_FALSE(edit.empty());
  const std::string text = ro::mapedit::MapEditToYaml(edit, "prontera");
  auto reread = ro::mapedit::ParseMapEdit(text, "prontera");
  REQUIRE(reread.has_value());
  CHECK(*reread == edit);
  edit.RemoveEffect(0);
  REQUIRE(edit.effects().size() == 1);
  CHECK(edit.effects()[0].number == 44);
}

TEST_CASE("Effets : une raison absente ou une lueur sans portée est refusée", "[mapedit]") {
  constexpr const char* kNoWhy =
      "version: 1\nmap: 'prontera'\nmodels: []\neffects:\n"
      "  - number: 47\n    position: [0, 0, 0]\n";
  CHECK_FALSE(ro::mapedit::ParseMapEdit(kNoWhy, "prontera").has_value());
  constexpr const char* kDarkGlow =
      "version: 1\nmap: 'prontera'\nmodels: []\neffects:\n"
      "  - number: 47\n    position: [0, 0, 0]\n    glow:\n      range: 0\n    why: \"x\"\n";
  CHECK_FALSE(ro::mapedit::ParseMapEdit(kDarkGlow, "prontera").has_value());
}

TEST_CASE("Effets de la carte : la retouche se relit, et reconnaît son objet", "[mapedit]") {
  MapEdit edit;
  edit.SetEffectEdit(ro::mapedit::EffectEdit{.effect_index = 2,
                                             .number = 47,
                                             .origin = {10.0F, -5.0F, 20.0F},
                                             .position = std::array<float, 3>{12.0F, -5.0F, 20.0F},
                                             .hidden = false,
                                             .night_only = true,
                                             .glow = ro::mapedit::EffectGlow{},
                                             .why = "brasero du nord"});
  edit.SetEffectEdit(ro::mapedit::EffectEdit{
      .effect_index = 0, .number = 44, .origin = {}, .hidden = true, .why = "cheminée"});
  // Rangées par rang.
  REQUIRE(edit.effect_edits().size() == 2);
  CHECK(edit.effect_edits()[0].effect_index == 0);
  const std::string text = ro::mapedit::MapEditToYaml(edit, "gonryun");
  auto reread = ro::mapedit::ParseMapEdit(text, "gonryun");
  REQUIRE(reread.has_value());
  CHECK(*reread == edit);

  const ro::mapedit::EffectEdit& north = *edit.EffectEditAt(2);
  CHECK(ro::mapedit::EffectEditTargets(
      north, ro::formats::RswEffect{.position = {10.0F, -5.0F, 20.0F}, .number = 47}));
  // Un autre numéro, ou un objet déplacé par un patch : la retouche ne vaut plus.
  CHECK_FALSE(ro::mapedit::EffectEditTargets(
      north, ro::formats::RswEffect{.position = {10.0F, -5.0F, 20.0F}, .number = 48}));
  CHECK_FALSE(ro::mapedit::EffectEditTargets(
      north, ro::formats::RswEffect{.position = {11.0F, -5.0F, 20.0F}, .number = 47}));
  edit.RemoveEffectEdit(2);
  CHECK(edit.EffectEditAt(2) == nullptr);
}

TEST_CASE("Plage d'heures : elle passe minuit, et fond à ses bornes", "[mapedit]") {
  const ro::mapedit::LitHours night{.from_hour = 20.0F, .to_hour = 6.0F};
  CHECK(ro::mapedit::LitHoursCover(night, 23.0F));
  CHECK(ro::mapedit::LitHoursCover(night, 2.0F));
  CHECK(ro::mapedit::LitHoursCover(night, 20.0F));
  CHECK_FALSE(ro::mapedit::LitHoursCover(night, 12.0F));
  CHECK(ro::mapedit::LitHoursStrength(night, 0.0F) == 1.0F);
  CHECK(ro::mapedit::LitHoursStrength(night, 12.0F) == 0.0F);
  // Au quart d'heure après l'ouverture, la moitié du fondu d'une demi-heure.
  CHECK(ro::mapedit::LitHoursStrength(night, 20.25F) == 0.5F);
  CHECK(ro::mapedit::LitHoursStrength(night, 5.75F) == 0.5F);
  // Deux bornes égales couvrent tout le jour.
  const ro::mapedit::LitHours always{.from_hour = 8.0F, .to_hour = 8.0F};
  CHECK(ro::mapedit::LitHoursCover(always, 15.0F));
  CHECK(ro::mapedit::LitHoursStrength(always, 15.0F) == 1.0F);
}

TEST_CASE("Plage d'heures : le document l'écrit, la relit, et refuse une heure hors du jour",
          "[mapedit]") {
  MapEdit edit;
  edit.AddLight(AddedLight{.position = {},
                           .color = {1.0F, 1.0F, 1.0F},
                           .range = 30.0F,
                           .night_only = true,
                           .hours = ro::mapedit::LitHours{.from_hour = 18.5F, .to_hour = 7.0F},
                           .why = "lanterne"});
  edit.AddEffect(
      ro::mapedit::AddedEffect{.number = 47,
                               .position = {},
                               .night_only = true,
                               .hours = ro::mapedit::LitHours{.from_hour = 21.0F, .to_hour = 4.25F},
                               .why = "flamme"});
  const std::string text = ro::mapedit::MapEditToYaml(edit, "gonryun");
  auto reread = ro::mapedit::ParseMapEdit(text, "gonryun");
  REQUIRE(reread.has_value());
  CHECK(*reread == edit);
  const std::string late =
      "lights:\n  - position: [0, 0, 0]\n    color: [1, 1, 1]\n    range: 5\n"
      "    hours: [20, 25]\n    why: \"x\"\n";
  CHECK_FALSE(ro::mapedit::ParseMapEdit(late, "gonryun").has_value());
}

TEST_CASE("Lumières : « sans ombre » et une lueur sans vacillement se relisent", "[mapedit]") {
  MapEdit edit;
  edit.AddLight(AddedLight{.position = {},
                           .color = {1.0F, 1.0F, 1.0F},
                           .range = 30.0F,
                           .shadow = false,
                           .why = "lampion"});
  edit.AddEffect(
      ro::mapedit::AddedEffect{.number = 47,
                               .position = {},
                               .glow = ro::mapedit::EffectGlow{.flicker = false, .shadow = false},
                               .why = "flamme sage"});
  const std::string text = ro::mapedit::MapEditToYaml(edit, "gonryun");
  auto reread = ro::mapedit::ParseMapEdit(text, "gonryun");
  REQUIRE(reread.has_value());
  CHECK(*reread == edit);
  CHECK_FALSE(reread->lights()[0].shadow);
  CHECK_FALSE(reread->effects()[0].glow->flicker);
}
