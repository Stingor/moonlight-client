// L'OPACITÉ D'UN MODÈLE DANS LE DOCUMENT D'ÉDITION : sa lecture, son écriture,
// ses bornes, et ce que l'application en rapporte.

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/rsw.h"
#include "ro/mapedit/map_edit.h"

#include "support/map_builder.h"

using ro::mapedit::AddedModel;
using ro::mapedit::ApplyMapEdit;
using ro::mapedit::MapEdit;
using ro::mapedit::MapEditReport;
using ro::mapedit::ModelEdit;
using ro::mapedit::ModelOpacity;
using ro::mapedit::ParseMapEdit;

namespace {

std::string SameName(std::string_view raw) { return std::string(raw); }
constexpr ro::mapedit::FileNameCodec kSameNames{.to_utf8 = &SameName, .to_raw = &SameName};

ro::formats::RswScene TwoModels() {
  ro::test_support::RswBuildOptions options;
  options.models = {
      {.name = "pont", .model_name = "decor\\pont.rsm", .position = {10.0F, -5.0F, 20.0F}},
      {.name = "arbre", .model_name = "decor\\arbre.rsm", .position = {30.0F, 0.0F, 40.0F}},
  };
  auto rsw = ro::formats::RswScene::Parse(ro::test_support::BuildRsw(options));
  REQUIRE(rsw.has_value());
  return std::move(*rsw);
}

ModelEdit Faded(std::size_t rank, std::string file, std::array<float, 3> origin, float opacity) {
  ModelEdit edit;
  edit.model_index = rank;
  edit.model_file = std::move(file);
  edit.origin = origin;
  edit.opacity = opacity;
  edit.why = "il cache l'entrée";
  return edit;
}

}  // namespace

TEST_CASE("Opacité : elle se lit, s'écrit et se relit à l'identique", "[mapedit]") {
  MapEdit edit;
  edit.SetModel(Faded(0, "decor\\pont.rsm", {10.0F, -5.0F, 20.0F}, 0.35F));
  edit.AddModel(AddedModel{.model_file = "decor\\lanterne.rsm",
                           .position = {1.0F, 2.0F, 3.0F},
                           .opacity = 0.5F,
                           .why = "une lanterne fantôme"});
  edit.AddModel(AddedModel{
      .model_file = "decor\\banc.rsm", .position = {4.0F, 5.0F, 6.0F}, .why = "un banc"});
  const std::string yaml = ro::mapedit::MapEditToYaml(edit, "essai");
  CHECK(yaml.find("opacity: 0.35") != std::string::npos);
  auto read = ParseMapEdit(yaml, "essai");
  REQUIRE(read.has_value());
  CHECK(*read == edit);
  CHECK_FALSE(read->added()[1].opacity.has_value());
}

TEST_CASE("Opacité : hors de 0 à 1, le document est refusé", "[mapedit]") {
  for (const std::string_view value : {"1.5", "-0.1", ".nan"}) {
    const std::string yaml = std::string(R"(
version: 1
map: essai
models:
  - index: 0
    file: 'decor\pont.rsm'
    origin: [10.0, -5.0, 20.0]
    opacity: )") + std::string(value) +
                             R"(
    why: "translucide"
)";
    CHECK_FALSE(ParseMapEdit(yaml, "essai").has_value());
  }
  const std::string added = R"(
version: 1
map: essai
models: []
added:
  - file: 'decor\banc.rsm'
    position: [0, 0, 0]
    opacity: 2
    why: "un banc"
)";
  CHECK_FALSE(ParseMapEdit(added, "essai").has_value());
}

TEST_CASE("Opacité : pleine, elle ne s'écrit pas ; réglée, elle reste dans ses bornes",
          "[mapedit]") {
  CHECK_FALSE(ro::mapedit::DocumentOpacity(1.0F).has_value());
  CHECK_FALSE(ro::mapedit::DocumentOpacity(1.2F).has_value());
  CHECK(ro::mapedit::DocumentOpacity(0.4F) == std::optional<float>(0.4F));
  CHECK(ro::mapedit::DocumentOpacity(-0.5F) == std::optional<float>(0.0F));
}

TEST_CASE("Opacité : l'application rapporte les translucides, ni masqués ni refusés", "[mapedit]") {
  ro::formats::RswScene rsw = TwoModels();
  MapEdit edit;
  edit.SetModel(Faded(0, "decor\\pont.rsm", {10.0F, -5.0F, 20.0F}, 0.25F));
  // Une cible qui ne correspond plus : rien ne se pose.
  edit.SetModel(Faded(1, "decor\\autre.rsm", {30.0F, 0.0F, 40.0F}, 0.5F));
  edit.AddModel(AddedModel{.model_file = "decor\\lanterne.rsm",
                           .position = {1.0F, 2.0F, 3.0F},
                           .hidden = true,
                           .opacity = 0.5F,
                           .why = "masquée"});
  edit.AddModel(AddedModel{.model_file = "decor\\banc.rsm",
                           .position = {4.0F, 5.0F, 6.0F},
                           .opacity = 0.75F,
                           .why = "un banc"});
  const MapEditReport report = ApplyMapEdit(edit, rsw, kSameNames);
  CHECK(report.refused.size() == 1);
  CHECK(report.opacities == std::vector<ModelOpacity>{{.model_index = 0, .opacity = 0.25F},
                                                      {.model_index = 3, .opacity = 0.75F}});
  // L'opacité seule ne déplace rien.
  CHECK(rsw.models()[0].position == std::array<float, 3>{10.0F, -5.0F, 20.0F});
}
