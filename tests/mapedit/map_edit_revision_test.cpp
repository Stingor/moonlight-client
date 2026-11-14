// LA RÉVISION D'UN DOCUMENT D'ÉDITION — le numéro que le serveur lui donne,
// écrit en tête du YAML et relu (ADR 0124).

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/mapedit/map_edit.h"

using ro::mapedit::MapEdit;
using ro::mapedit::ParseMapEdit;
using ro::mapedit::ParseRevisedMapEdit;
using ro::mapedit::RevisedMapEditToYaml;

namespace {

constexpr std::uint32_t kRevision = 42;

MapEdit OneModel() {
  MapEdit edit;
  ro::mapedit::ModelEdit model;
  model.model_index = 3;
  model.model_file = "pont.rsm";
  model.origin = {1.0F, 2.0F, 3.0F};
  model.hidden = true;
  model.why = "les planches s'enfoncent";
  edit.SetModel(model);
  return edit;
}

}  // namespace

TEST_CASE("Révision : elle s'écrit en tête et se relit", "[mapedit][map_edit_revision]") {
  const MapEdit edit = OneModel();
  const std::string text = RevisedMapEditToYaml(edit, "prontera", kRevision);
  CHECK(text.find("revision: 42\n") != std::string::npos);
  auto revised = ParseRevisedMapEdit(text, "prontera");
  REQUIRE(revised.has_value());
  CHECK(revised->revision == kRevision);
  CHECK(revised->edit == edit);
  // Le lecteur d'avant la lit aussi, et ignore la révision.
  auto plain = ParseMapEdit(text, "prontera");
  REQUIRE(plain.has_value());
  CHECK(*plain == edit);
}

TEST_CASE("Révision : absente, elle vaut zéro et ne s'écrit pas", "[mapedit][map_edit_revision]") {
  const std::string text = RevisedMapEditToYaml(OneModel(), "prontera", 0);
  CHECK(text.find("revision:") == std::string::npos);
  CHECK(text == ro::mapedit::MapEditToYaml(OneModel(), "prontera"));
  auto revised = ParseRevisedMapEdit(text, "prontera");
  REQUIRE(revised.has_value());
  CHECK(revised->revision == 0);
}

TEST_CASE("Révision : une révision négative refuse le document", "[mapedit][map_edit_revision]") {
  const std::string text = "version: 1\nmap: 'prontera'\nrevision: -3\nmodels: []\n";
  CHECK_FALSE(ParseRevisedMapEdit(text, "prontera").has_value());
  CHECK(ParseRevisedMapEdit("version: 1\nmap: 'prontera'\nrevision: 3\nmodels: []\n", "prontera")
            .has_value());
}
