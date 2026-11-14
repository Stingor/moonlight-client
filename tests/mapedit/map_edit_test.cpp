// LE DOCUMENT D'ÉDITION D'UNE CARTE : sa lecture, son écriture, ce qu'il fait
// d'un RSW, et l'historique de l'éditeur.

#include "ro/mapedit/map_edit.h"

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/rsw.h"
#include "ro/mapedit/edit_history.h"

#include "support/map_builder.h"

using ro::mapedit::ApplyMapEdit;
using ro::mapedit::EditHistory;
using ro::mapedit::MapEdit;
using ro::mapedit::MapEditReport;
using ro::mapedit::ModelEdit;
using ro::mapedit::ParseMapEdit;

namespace {

// Les noms des essais sont en ASCII : l'encodage du RSW ne change rien.
std::string SameName(std::string_view raw) { return std::string(raw); }
constexpr ro::mapedit::FileNameCodec kSameNames{.to_utf8 = &SameName, .to_raw = &SameName};

// Trois modèles : un pont, un arbre, une maison.
ro::formats::RswScene ThreeModels() {
  ro::test_support::RswBuildOptions options;
  options.models = {
      {.name = "pont", .model_name = "decor\\pont.rsm", .position = {10.0F, -5.0F, 20.0F}},
      {.name = "arbre", .model_name = "decor\\arbre.rsm", .position = {30.0F, 0.0F, 40.0F}},
      {.name = "maison", .model_name = "decor\\maison.rsm", .position = {50.0F, 0.0F, 60.0F}},
  };
  auto rsw = ro::formats::RswScene::Parse(ro::test_support::BuildRsw(options));
  REQUIRE(rsw.has_value());
  return std::move(*rsw);
}

MapEdit EditFrom(std::string_view yaml) {
  auto edit = ParseMapEdit(yaml, "essai");
  REQUIRE(edit.has_value());
  return std::move(*edit);
}

ModelEdit Moved(std::size_t rank, float x) {
  ModelEdit edit;
  edit.model_index = rank;
  edit.model_file = "decor\\pont.rsm";
  edit.position = std::array<float, 3>{x, 0.0F, 0.0F};
  edit.why = "déplacé";
  return edit;
}

}  // namespace

TEST_CASE("Édition : le chemin d'un document est celui de sa carte", "[mapedit]") {
  CHECK(ro::mapedit::MapEditPath("gonryun") == "data\\map_edits\\gonryun.yml");
}

TEST_CASE("Édition : un modèle désigné se repose, le reste garde le RSW", "[mapedit]") {
  ro::formats::RswScene rsw = ThreeModels();
  const MapEdit edit = EditFrom(R"(
version: 1
map: essai
models:
  - index: 0
    file: 'decor\pont.rsm'
    origin: [10.0, -5.0, 20.0]
    position: [10.0, -7.5, 20.0]
    rotation: [0, 90, 0]
    why: "les planches s'enfoncent dans le sol"
)");
  const MapEditReport report = ApplyMapEdit(edit, rsw, kSameNames);
  CHECK(report.applied == 1);
  CHECK(report.refused.empty());
  CHECK(rsw.models()[0].position == std::array<float, 3>{10.0F, -7.5F, 20.0F});
  CHECK(rsw.models()[0].rotation == std::array<float, 3>{0.0F, 90.0F, 0.0F});
  // L'échelle, absente de la retouche, reste celle du RSW.
  CHECK(rsw.models()[0].scale == std::array<float, 3>{1.0F, 1.0F, 1.0F});
  CHECK(rsw.models()[1].position == std::array<float, 3>{30.0F, 0.0F, 40.0F});
}

TEST_CASE("Édition : une cible qui ne correspond plus n'est pas touchée", "[mapedit]") {
  ro::formats::RswScene rsw = ThreeModels();
  const MapEdit edit = EditFrom(R"(
version: 1
map: essai
models:
  - index: 1
    file: 'decor\pont.rsm'
    origin: [30.0, 0.0, 40.0]
    position: [0, 0, 0]
    why: "mauvais fichier"
  - index: 2
    file: 'decor\maison.rsm'
    origin: [50.5, 0.0, 60.0]
    position: [0, 0, 0]
    why: "la carte a bougé sous un patch"
  - index: 7
    file: 'decor\maison.rsm'
    origin: [0, 0, 0]
    hidden: true
    why: "hors de la liste"
)");
  const MapEditReport report = ApplyMapEdit(edit, rsw, kSameNames);
  CHECK(report.applied == 0);
  CHECK(report.refused.size() == 3);
  CHECK(rsw.models()[1].position == std::array<float, 3>{30.0F, 0.0F, 40.0F});
  CHECK(rsw.models()[2].position == std::array<float, 3>{50.0F, 0.0F, 60.0F});
}

TEST_CASE("Édition : un modèle masqué reste à son rang", "[mapedit]") {
  ro::formats::RswScene rsw = ThreeModels();
  const MapEdit edit = EditFrom(R"(
version: 1
map: essai
models:
  - index: 1
    file: 'decor\arbre.rsm'
    origin: [30.0, 0.0, 40.0]
    hidden: true
    why: "il traverse un mur"
  - index: 2
    file: 'decor\maison.rsm'
    origin: [50.0, 0.0, 60.0]
    scale: [2, 2, 2]
    why: "trop petite"
)");
  const MapEditReport report = ApplyMapEdit(edit, rsw, kSameNames);
  CHECK(report.applied == 2);
  REQUIRE(rsw.models().size() == 3);
  CHECK(report.hidden == std::vector<std::size_t>{1});
  CHECK(rsw.models()[2].scale == std::array<float, 3>{2.0F, 2.0F, 2.0F});
}

TEST_CASE("Édition : un document malformé est refusé", "[mapedit]") {
  CHECK_FALSE(ParseMapEdit("pas: [du YAML", "essai").has_value());
  CHECK_FALSE(ParseMapEdit("version: 2\nmap: essai\n", "essai").has_value());
  // Un document qui nomme une autre carte.
  CHECK_FALSE(ParseMapEdit("version: 1\nmap: prontera\nmodels: []\n", "essai").has_value());
  // Sans raison, puis une raison vide, puis une position à deux composantes.
  CHECK_FALSE(ParseMapEdit(R"(
version: 1
map: essai
models:
  - index: 0
    file: 'decor\pont.rsm'
    origin: [10.0, -5.0, 20.0]
    position: [0, 0, 0]
)",
                           "essai")
                  .has_value());
  CHECK_FALSE(ParseMapEdit(R"(
version: 1
map: essai
models:
  - index: 0
    file: 'decor\pont.rsm'
    origin: [10.0, -5.0, 20.0]
    why: ""
)",
                           "essai")
                  .has_value());
  CHECK_FALSE(ParseMapEdit(R"(
version: 1
map: essai
models:
  - index: 0
    file: 'decor\pont.rsm'
    origin: [10.0, -5.0, 20.0]
    position: [1, 2]
    why: "deux composantes"
)",
                           "essai")
                  .has_value());
}

TEST_CASE("Édition : le document écrit se relit à l'identique", "[mapedit]") {
  MapEdit edit;
  ModelEdit bridge;
  bridge.model_index = 879;
  bridge.model_file = "외부소품\\나무다리.rsm";
  bridge.origin = {50.279F, -52.445F, -492.465F};
  bridge.position = std::array<float, 3>{50.279F, -55.0F, -492.465F};
  bridge.why = "les planches \"s'enfoncent\"\npar endroits";
  ModelEdit tree;
  tree.model_index = 12;
  tree.model_file = "l'arbre.rsm";
  tree.origin = {1.0F, 2.0F, 3.0F};
  tree.hidden = true;
  tree.why = "il traverse un mur";
  edit.SetModel(bridge);
  edit.SetModel(tree);
  auto read_back = ParseMapEdit(ro::mapedit::MapEditToYaml(edit, "gonryun"), "gonryun");
  REQUIRE(read_back.has_value());
  // Rangées par rang de modèle, quel que soit l'ordre de pose.
  REQUIRE(read_back->models().size() == 2);
  CHECK(read_back->models()[0] == tree);
  CHECK(read_back->models()[1] == bridge);
  CHECK(*read_back == edit);
  // Un document vide s'écrit et se relit encore.
  auto empty = ParseMapEdit(ro::mapedit::MapEditToYaml(MapEdit(), "gonryun"), "gonryun");
  REQUIRE(empty.has_value());
  CHECK(empty->empty());
}

TEST_CASE("Édition : une retouche reposée remplace l'ancienne, une retirée disparaît",
          "[mapedit]") {
  MapEdit edit;
  edit.SetModel(Moved(3, 1.0F));
  edit.SetModel(Moved(3, 2.0F));
  REQUIRE(edit.models().size() == 1);
  CHECK((*edit.ModelAt(3)->position)[0] == 2.0F);
  edit.RemoveModel(3);
  CHECK(edit.empty());
  CHECK(edit.ModelAt(3) == nullptr);
}

TEST_CASE("Historique : annuler et rétablir rendent les documents de chaque geste", "[mapedit]") {
  EditHistory history;
  CHECK_FALSE(history.CanUndo());
  MapEdit first;
  first.SetModel(Moved(1, 1.0F));
  MapEdit second = first;
  second.SetModel(Moved(2, 2.0F));
  history.Record("déplacer 1", MapEdit(), first);
  history.Record("déplacer 2", first, second);
  CHECK(history.UndoLabel() == "déplacer 2");

  CHECK(history.Undo() == std::optional<MapEdit>(first));
  CHECK(history.Undo() == std::optional<MapEdit>(MapEdit()));
  CHECK_FALSE(history.Undo().has_value());
  CHECK(history.RedoLabel() == "déplacer 1");
  CHECK(history.Redo() == std::optional<MapEdit>(first));

  // Un geste neuf efface ce qui pouvait se rétablir.
  history.Record("masquer", first, MapEdit());
  CHECK_FALSE(history.CanRedo());
  CHECK(history.UndoLabel() == "masquer");
}

TEST_CASE("Historique : au-delà de sa capacité, les plus vieux gestes s'oublient", "[mapedit]") {
  EditHistory history;
  for (std::size_t step = 0; step < EditHistory::kMaxSteps + 1; ++step) {
    history.Record("geste", MapEdit(), MapEdit());
  }
  std::size_t undone = 0;
  while (history.Undo().has_value()) {
    ++undone;
  }
  CHECK(undone == EditHistory::kMaxSteps);
}

TEST_CASE("Édition : un modèle ajouté prend rang après ceux du RSW", "[mapedit]") {
  ro::formats::RswScene rsw = ThreeModels();
  const MapEdit edit = EditFrom(R"(
version: 1
map: essai
added:
  - file: 'decor\lanterne.rsm'
    position: [7, -1, 8]
    rotation: [0, 45, 0]
    why: "une lanterne au carrefour"
  - file: 'decor\banc.rsm'
    position: [1, 0, 1]
    hidden: true
    why: "un banc, en attente"
)");
  REQUIRE(edit.added().size() == 2);
  const MapEditReport report = ApplyMapEdit(edit, rsw, kSameNames);
  CHECK(report.applied == 2);
  REQUIRE(rsw.models().size() == 5);
  CHECK(rsw.models()[3].model_name == "decor\\lanterne.rsm");
  CHECK(rsw.models()[3].position == std::array<float, 3>{7.0F, -1.0F, 8.0F});
  CHECK(rsw.models()[3].rotation == std::array<float, 3>{0.0F, 45.0F, 0.0F});
  // L'échelle absente vaut un ; le masqué garde son rang.
  CHECK(rsw.models()[3].scale == std::array<float, 3>{1.0F, 1.0F, 1.0F});
  CHECK(report.hidden == std::vector<std::size_t>{4});

  // Écrit puis relu, à l'identique ; un ajout sans raison est refusé.
  auto read_back = ParseMapEdit(ro::mapedit::MapEditToYaml(edit, "essai"), "essai");
  REQUIRE(read_back.has_value());
  CHECK(*read_back == edit);
  CHECK_FALSE(ParseMapEdit("version: 1\nmap: essai\nadded:\n  - file: 'a.rsm'\n"
                           "    position: [0, 0, 0]\n",
                           "essai")
                  .has_value());
}

TEST_CASE("Édition : retirer un ajout fait remonter les suivants", "[mapedit]") {
  MapEdit edit;
  edit.AddModel({.model_file = "a.rsm", .why = "a"});
  edit.AddModel({.model_file = "b.rsm", .why = "b"});
  edit.AddModel({.model_file = "c.rsm", .why = "c"});
  edit.RemoveAdded(1, 0);
  REQUIRE(edit.added().size() == 2);
  CHECK(edit.added()[1].model_file == "c.rsm");
  edit.SetAdded(0, {.model_file = "z.rsm", .why = "z"});
  CHECK(edit.added()[0].model_file == "z.rsm");
  edit.RemoveAdded(7, 0);
  CHECK(edit.added().size() == 2);
}

TEST_CASE("Édition : un ajout retiré emporte son maillage, les suivants le leur", "[mapedit]") {
  // Une carte de dix modèles d'origine : les ajouts prennent les rangs 10 à 12.
  constexpr std::size_t kOriginalCount = 10;
  MapEdit edit;
  edit.AddModel({.model_file = "a.rsm", .why = "a"});
  edit.AddModel({.model_file = "b.rsm", .why = "b"});
  edit.AddModel({.model_file = "c.rsm", .why = "c"});
  edit.SetMesh({.model_index = 3, .model_file = "origine.rsm", .why = "origine"});
  edit.SetMesh({.model_index = 11, .model_file = "b.rsm", .why = "b"});
  edit.SetMesh({.model_index = 12, .model_file = "c.rsm", .why = "c"});
  edit.RemoveAdded(1, kOriginalCount);
  // La retouche de la pose retirée part avec elle.
  REQUIRE(edit.meshes().size() == 2);
  // Celle d'un modèle d'origine ne bouge pas ; celle de c suit sa pose au rang 11.
  CHECK(edit.MeshAt(3) != nullptr);
  REQUIRE(edit.MeshAt(11) != nullptr);
  CHECK(edit.MeshAt(11)->model_file == "c.rsm");
  CHECK(edit.MeshAt(12) == nullptr);
}
