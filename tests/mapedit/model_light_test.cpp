// LES LUMIÈRES DE MODÈLE : leur place sur chaque pose, leur document, leur
// extinction sur une pose, et l'historique qui sait quel document il change.

#include "ro/mapedit/model_light.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/rsw.h"
#include "ro/mapedit/edit_history.h"
#include "ro/mapedit/map_edit.h"

using ro::mapedit::AddedLight;
using ro::mapedit::kModelLightsDocument;
using ro::mapedit::MapEdit;
using ro::mapedit::ModelLight;

namespace {

ro::formats::RswModel Pose(std::array<float, 3> position, std::array<float, 3> rotation,
                           std::array<float, 3> scale) {
  ro::formats::RswModel pose;
  pose.position = position;
  pose.rotation = rotation;
  pose.scale = scale;
  return pose;
}

void CheckNear(const std::array<float, 3>& got, const std::array<float, 3>& wanted) {
  constexpr double kMargin = 1.0e-3;
  for (std::size_t axis = 0; axis < got.size(); ++axis) {
    CHECK(got[axis] == Catch::Approx(wanted[axis]).margin(kMargin));
  }
}

MapEdit LampBook() {
  MapEdit book;
  book.AddModelLight(ModelLight{.model_file = "Prontera/Lampadaire.rsm",
                                .light = AddedLight{.position = {0.0F, -20.0F, 0.0F},
                                                    .color = {2.0F, 1.7F, 1.2F},
                                                    .range = 50.0F,
                                                    .why = "la lanterne du lampadaire"}});
  return book;
}

}  // namespace

TEST_CASE("Lumière de modèle : sans rotation, l'écart s'ajoute à la position de la pose",
          "[mapedit]") {
  const ro::formats::RswModel pose = Pose({10.0F, -2.0F, 30.0F}, {}, {1.0F, 1.0F, 1.0F});
  CheckNear(ro::mapedit::ModelLightPosition(pose, {1.0F, -20.0F, 3.0F}), {11.0F, -22.0F, 33.0F});
}

TEST_CASE("Lumière de modèle : elle grandit et tourne avec la pose", "[mapedit]") {
  // DEUX FOIS PLUS GRANDE : la lanterne monte deux fois plus haut.
  const ro::formats::RswModel tall = Pose({}, {}, {2.0F, 2.0F, 2.0F});
  CheckNear(ro::mapedit::ModelLightPosition(tall, {0.0F, -20.0F, 0.0F}), {0.0F, -40.0F, 0.0F});
  // UN DEMI-TOUR AUTOUR DE LA VERTICALE retourne l'écart horizontal, pas sa
  // hauteur.
  const ro::formats::RswModel turned = Pose({}, {0.0F, 180.0F, 0.0F}, {1.0F, 1.0F, 1.0F});
  CheckNear(ro::mapedit::ModelLightPosition(turned, {5.0F, -20.0F, 3.0F}), {-5.0F, -20.0F, -3.0F});
}

TEST_CASE("Lumière de modèle : l'écart se retrouve depuis la position, sous toute pose",
          "[mapedit]") {
  const std::array<ro::formats::RswModel, 3> poses = {
      Pose({4.0F, -1.0F, 9.0F}, {10.0F, 35.0F, -20.0F}, {1.5F, 0.5F, 2.0F}),
      Pose({}, {0.0F, 90.0F, 0.0F}, {1.0F, 1.0F, 1.0F}),
      Pose({-7.0F, 3.0F, 2.0F}, {180.0F, 0.0F, 45.0F}, {-1.0F, 1.0F, 1.0F})};
  const std::array<float, 3> offset{2.0F, -15.0F, -4.0F};
  for (const ro::formats::RswModel& pose : poses) {
    CheckNear(ro::mapedit::ModelLightOffset(pose, ro::mapedit::ModelLightPosition(pose, offset)),
              offset);
  }
}

TEST_CASE("Lumière de modèle : une échelle nulle écrase l'axe sans diviser par zéro", "[mapedit]") {
  const ro::formats::RswModel flat = Pose({}, {}, {1.0F, 0.0F, 1.0F});
  const std::array<float, 3> offset = ro::mapedit::ModelLightOffset(flat, {1.0F, -9.0F, 2.0F});
  CHECK(offset[1] == 0.0F);
  CHECK(std::isfinite(offset[0]));
}

TEST_CASE("Lumière de modèle : la clé d'un fichier ignore la casse et le sens des barres",
          "[mapedit]") {
  CHECK(ro::mapedit::ModelFileKey("Prontera/Lampadaire.RSM") ==
        ro::mapedit::ModelFileKey("prontera\\lampadaire.rsm"));
  CHECK(ro::mapedit::ModelFileKey("a\\b.rsm") != ro::mapedit::ModelFileKey("a\\c.rsm"));
}

TEST_CASE("Lumière de modèle : chaque pose du fichier la porte, sauf masquée ou éteinte",
          "[mapedit]") {
  const std::vector<ro::formats::RswModel> models = {
      Pose({0.0F, 0.0F, 0.0F}, {}, {1.0F, 1.0F, 1.0F}),
      Pose({100.0F, 0.0F, 0.0F}, {}, {1.0F, 1.0F, 1.0F}),
      Pose({200.0F, 0.0F, 0.0F}, {}, {1.0F, 1.0F, 1.0F}),
      Pose({300.0F, 0.0F, 0.0F}, {}, {1.0F, 1.0F, 1.0F}),
      Pose({400.0F, 0.0F, 0.0F}, {}, {1.0F, 1.0F, 1.0F})};
  const std::string lamp = ro::mapedit::ModelFileKey("prontera\\lampadaire.rsm");
  const std::vector<std::string> keys = {lamp, "prontera\\arbre.rsm", lamp, lamp, lamp};
  const std::vector<std::size_t> hidden = {2};
  const std::vector<std::size_t> unlit = {3};
  const std::vector<AddedLight> lights =
      ro::mapedit::ModelLightsOn(LampBook(), models, keys, hidden, unlit);
  REQUIRE(lights.size() == 2);
  CheckNear(lights[0].position, {0.0F, -20.0F, 0.0F});
  CheckNear(lights[1].position, {400.0F, -20.0F, 0.0F});
  CHECK(lights[1].why == "la lanterne du lampadaire");
}

TEST_CASE("Lumière de modèle : le document commun les écrit et les relit à l'identique",
          "[mapedit]") {
  const MapEdit book = LampBook();
  const std::string text = ro::mapedit::MapEditToYaml(book, kModelLightsDocument);
  auto reread = ro::mapedit::ParseMapEdit(text, kModelLightsDocument);
  REQUIRE(reread.has_value());
  CHECK(*reread == book);
}

TEST_CASE("Lumière de modèle : le document d'une carte n'en porte pas", "[mapedit]") {
  const std::string text = ro::mapedit::MapEditToYaml(LampBook(), "prontera");
  CHECK_FALSE(ro::mapedit::ParseMapEdit(text, "prontera").has_value());
}

TEST_CASE("Lumière de modèle : l'extinction d'une pose s'écrit et se relit", "[mapedit]") {
  MapEdit edit;
  edit.SetModel(ro::mapedit::ModelEdit{.model_index = 0,
                                       .model_file = "decor\\pont.rsm",
                                       .origin = {1.0F, 2.0F, 3.0F},
                                       .unlit = true,
                                       .why = "lanterne cassée"});
  edit.AddModel(ro::mapedit::AddedModel{
      .model_file = "decor\\lampe.rsm", .position = {}, .unlit = true, .why = "une lampe"});
  const std::string text = ro::mapedit::MapEditToYaml(edit, "prontera");
  auto reread = ro::mapedit::ParseMapEdit(text, "prontera");
  REQUIRE(reread.has_value());
  CHECK(*reread == edit);
}

TEST_CASE("Historique : un geste revient sur le document qu'il a changé", "[mapedit]") {
  ro::mapedit::EditHistory history;
  history.Record("carte", MapEdit(), MapEdit());
  history.Record("lumière", MapEdit(), LampBook(), std::string(kModelLightsDocument));
  CHECK(history.UndoDocument() == kModelLightsDocument);
  CHECK(history.Undo() == std::optional<MapEdit>(MapEdit()));
  CHECK(history.RedoDocument() == kModelLightsDocument);
  CHECK(history.UndoDocument().empty());
  CHECK(history.Redo() == std::optional<MapEdit>(LampBook()));
}
