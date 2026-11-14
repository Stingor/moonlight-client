// LES EFFETS DE MODÈLE : leur place sur chaque pose, leur extinction avec les
// lumières, et leur section du document commun.

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/rsw.h"
#include "ro/mapedit/map_edit.h"
#include "ro/mapedit/model_light.h"

using ro::mapedit::AddedEffect;
using ro::mapedit::kModelLightsDocument;
using ro::mapedit::MapEdit;
using ro::mapedit::ModelEffect;

namespace {

ro::formats::RswModel Pose(std::array<float, 3> position, std::array<float, 3> rotation) {
  ro::formats::RswModel pose;
  pose.position = position;
  pose.rotation = rotation;
  pose.scale = {1.0F, 1.0F, 1.0F};
  return pose;
}

void CheckNear(const std::array<float, 3>& got, const std::array<float, 3>& wanted) {
  constexpr double kMargin = 1.0e-3;
  for (std::size_t axis = 0; axis < got.size(); ++axis) {
    CHECK(got[axis] == Catch::Approx(wanted[axis]).margin(kMargin));
  }
}

// Les flammes d'un brasero, au-dessus de sa vasque et décalées sur x.
MapEdit BrazierBook() {
  MapEdit book;
  book.AddModelEffect(ModelEffect{
      .model_file = "Prontera/Brasero.rsm",
      .effect = AddedEffect{.number = 47,
                            .position = {5.0F, -15.0F, 0.0F},
                            .emit_speed = 125.0F,
                            .param = {0.6F, 1.0F, 0.0F, 0.0F},
                            .night_only = true,
                            .hours = ro::mapedit::LitHours{.from_hour = 19.0F, .to_hour = 7.0F},
                            .glow = ro::mapedit::EffectGlow{.range = 40.0F, .flicker = false},
                            .why = "les flammes du brasero"}});
  return book;
}

}  // namespace

TEST_CASE("Effet de modèle : chaque pose du fichier le lève, sauf masquée ou éteinte",
          "[mapedit]") {
  const std::vector<ro::formats::RswModel> models = {
      Pose({0.0F, 0.0F, 0.0F}, {}), Pose({100.0F, 0.0F, 0.0F}, {}), Pose({200.0F, 0.0F, 0.0F}, {}),
      Pose({300.0F, 0.0F, 0.0F}, {}), Pose({400.0F, 0.0F, 0.0F}, {0.0F, 90.0F, 0.0F})};
  const std::string brazier = ro::mapedit::ModelFileKey("prontera\\brasero.rsm");
  const std::vector<std::string> keys = {brazier, "prontera\\arbre.rsm", brazier, brazier, brazier};
  const std::vector<std::size_t> hidden = {2};
  const std::vector<std::size_t> unlit = {3};
  const std::vector<AddedEffect> effects =
      ro::mapedit::ModelEffectsOn(BrazierBook(), models, keys, hidden, unlit);
  REQUIRE(effects.size() == 2);
  CheckNear(effects[0].position, {5.0F, -15.0F, 0.0F});
  CHECK(effects[0].number == 47);
  CHECK(effects[0].emit_speed == 125.0F);
  CHECK(effects[0].glow.has_value());
  // UN QUART DE TOUR : l'écart sur x tourne avec la pose, la hauteur reste.
  CHECK(effects[1].position[1] == Catch::Approx(-15.0F).margin(1.0e-3));
  CHECK_FALSE(effects[1].position[0] == Catch::Approx(405.0F).margin(1.0e-3));
  CheckNear(ro::mapedit::ModelLightOffset(models[4], effects[1].position), {5.0F, -15.0F, 0.0F});
}

TEST_CASE("Effet de modèle : le document commun les écrit et les relit à l'identique",
          "[mapedit]") {
  MapEdit book = BrazierBook();
  book.AddModelEffect(
      ModelEffect{.model_file = "decor\\torche.rsm",
                  .effect = AddedEffect{.number = 300, .position = {}, .why = "une torche"}});
  book.AddModelLight(
      ro::mapedit::ModelLight{.model_file = "decor\\torche.rsm",
                              .light = ro::mapedit::AddedLight{.range = 30.0F, .why = "sa lueur"}});
  const std::string text = ro::mapedit::MapEditToYaml(book, kModelLightsDocument);
  auto reread = ro::mapedit::ParseMapEdit(text, kModelLightsDocument);
  REQUIRE(reread.has_value());
  CHECK(*reread == book);
}

TEST_CASE("Effet de modèle : le document d'une carte n'en porte pas", "[mapedit]") {
  const std::string text = ro::mapedit::MapEditToYaml(BrazierBook(), "prontera");
  CHECK_FALSE(ro::mapedit::ParseMapEdit(text, "prontera").has_value());
}

TEST_CASE("Effet de modèle : sans raison, le document commun le refuse", "[mapedit]") {
  MapEdit book;
  book.AddModelEffect(ModelEffect{.model_file = "decor\\torche.rsm",
                                  .effect = AddedEffect{.number = 47, .position = {}, .why = {}}});
  const std::string text = ro::mapedit::MapEditToYaml(book, kModelLightsDocument);
  CHECK_FALSE(ro::mapedit::ParseMapEdit(text, kModelLightsDocument).has_value());
}
