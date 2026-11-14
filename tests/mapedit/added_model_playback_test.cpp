// COMMENT JOUE UN MODÈLE AJOUTÉ : en boucle sauf avis contraire, comme le
// document l'écrit et comme l'application le pose dans le RSW.

#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/rsw.h"
#include "ro/mapedit/map_edit.h"

#include "support/map_builder.h"

using ro::mapedit::AddedModel;
using ro::mapedit::MapEdit;
using ro::mapedit::ParseMapEdit;

namespace {

std::string SameName(std::string_view raw) { return std::string(raw); }
constexpr ro::mapedit::FileNameCodec kSameNames{.to_utf8 = &SameName, .to_raw = &SameName};

ro::formats::RswScene NoModel() {
  auto rsw = ro::formats::RswScene::Parse(ro::test_support::BuildRsw({}));
  REQUIRE(rsw.has_value());
  return std::move(*rsw);
}

// Un document qui ajoute un banc, avec `fields` sous son entrée.
std::string AddedBench(std::string_view fields) {
  return std::string(R"(
version: 1
map: essai
models: []
added:
  - file: 'decor\banc.rsm'
    position: [0, 0, 0]
)") + std::string(fields) +
         R"(
    why: "un banc"
)";
}

}  // namespace

TEST_CASE("Jeu d'un ajout : un tourne-broche ajouté tourne dans le RSW", "[mapedit]") {
  ro::formats::RswScene rsw = NoModel();
  MapEdit edit;
  edit.AddModel(AddedModel{
      .model_file = "decor\\broche.rsm", .position = {1.0F, 2.0F, 3.0F}, .why = "un barbecue"});
  edit.AddModel(AddedModel{.model_file = "decor\\moulin.rsm",
                           .position = {4.0F, 5.0F, 6.0F},
                           .animation_type = ro::formats::kRswModelPlayOnce,
                           .animation_speed = 0.5F,
                           .why = "un moulin"});
  static_cast<void>(ro::mapedit::ApplyMapEdit(edit, rsw, kSameNames));
  REQUIRE(rsw.models().size() == 2);
  CHECK(rsw.models()[0].animation_type == ro::formats::kRswModelPlayLooped);
  CHECK(rsw.models()[0].animation_speed == 1.0F);
  CHECK(rsw.models()[1].animation_type == ro::formats::kRswModelPlayOnce);
  CHECK(rsw.models()[1].animation_speed == 0.5F);
}

TEST_CASE("Jeu d'un ajout : le défaut ne s'écrit pas, le reste se relit à l'identique",
          "[mapedit]") {
  MapEdit edit;
  edit.AddModel(AddedModel{.model_file = "decor\\broche.rsm", .why = "un barbecue"});
  edit.AddModel(AddedModel{.model_file = "decor\\moulin.rsm",
                           .animation_type = ro::formats::kRswModelPlayOnce,
                           .animation_speed = 2.5F,
                           .why = "un moulin"});
  edit.AddModel(AddedModel{.model_file = "decor\\horloge.rsm",
                           .animation_type = ro::formats::kRswModelStill,
                           .why = "une horloge arrêtée"});
  const std::string yaml = ro::mapedit::MapEditToYaml(edit, "essai");
  CHECK(yaml.find("animation: loop") == std::string::npos);
  CHECK(yaml.find("animation: once") != std::string::npos);
  CHECK(yaml.find("animation: still") != std::string::npos);
  CHECK(yaml.find("animation_speed: 2.5") != std::string::npos);
  auto read = ParseMapEdit(yaml, "essai");
  REQUIRE(read.has_value());
  CHECK(*read == edit);
}

TEST_CASE("Jeu d'un ajout : un document sans la clé joue en boucle", "[mapedit]") {
  auto read = ParseMapEdit(AddedBench(""), "essai");
  REQUIRE(read.has_value());
  REQUIRE(read->added().size() == 1);
  CHECK(read->added()[0].animation_type == ro::formats::kRswModelPlayLooped);
  CHECK(read->added()[0].animation_speed == 1.0F);
}

TEST_CASE("Jeu d'un ajout : un mot inconnu ou une vitesse hors bornes refuse le document",
          "[mapedit]") {
  for (const std::string_view fields :
       {"    animation: spin", "    animation_speed: 0", "    animation_speed: -1",
        "    animation_speed: 101", "    animation_speed: .nan"}) {
    CHECK_FALSE(ParseMapEdit(AddedBench(fields), "essai").has_value());
  }
}
