// LE MAGASIN DES DOCUMENTS D'ÉDITION, entre le dossier du client et ce que le
// miroir livre dans l'installation (docs/adr/0128) : la révision la plus haute
// l'emporte, celle du poste à égalité. Le document livré est écrit comme le
// serveur l'exporte : sa révision en tête, puis le texte publié tel quel.

#include "world/map_edit_store.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/mapedit/map_edit.h"

#include "support/client_vfs.h"
#include "support/temp_directory.h"

using moonclient::MapEditStore;

namespace {

constexpr const char* kMap = "prontera";

ro::mapedit::MapEdit Edit(const char* why) {
  ro::mapedit::MapEdit edit;
  ro::mapedit::ModelEdit model;
  model.model_index = 0;
  model.model_file = "pont.rsm";
  model.origin = {1.0F, 2.0F, 3.0F};
  model.hidden = true;
  model.why = why;
  edit.SetModel(model);
  return edit;
}

std::span<const std::uint8_t> BytesOf(std::string_view text) {
  return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

// Le document que le serveur exporte dans le miroir : « revision: N », puis le
// texte que le staff a publié.
void WriteServed(const ro::test_support::TempDirectory& client, const ro::mapedit::MapEdit& edit,
                 std::uint32_t revision) {
  const std::string text =
      "revision: " + std::to_string(revision) + "\n" + ro::mapedit::MapEditToYaml(edit, kMap);
  client.WriteFile(std::filesystem::path("data") / "map_edits_served" / "prontera.yml",
                   BytesOf(text));
}

// Le document que le poste a écrit lui-même, sous `revision`.
void WriteOwn(const MapEditStore& store, const ro::mapedit::MapEdit& edit, std::uint32_t revision,
              const ro::test_support::TempDirectory& client) {
  const std::string text = ro::mapedit::RevisedMapEditToYaml(edit, kMap, revision);
  client.WriteFile(std::filesystem::relative(store.FileOf(kMap), client.path()), BytesOf(text));
}

// ⚠ LE CLIENT SE MONTE APRÈS QUE SES FICHIERS SONT ÉCRITS, comme chez le joueur,
// où la mise à jour passe avant le lancement : le système de fichiers ne lit
// le disque sous `data\` que si ce dossier existait au montage.
struct Bench {
  ro::test_support::TempDirectory client;
  MapEditStore store;

  Bench() { store.Open(client.path()); }
  void Load() {
    ro::vfs::Vfs vfs = ro::test_support::EmptyClient(client);
    store.Load(kMap, vfs);
  }
};

}  // namespace

TEST_CASE("Magasin d'éditions : sans document du poste, celui du miroir sert, sans s'écrire",
          "[app][map_edit_store]") {
  Bench bench;
  WriteServed(bench.client, Edit("servi"), 3);
  bench.Load();
  CHECK(bench.store.Document(kMap) == Edit("servi"));
  CHECK(bench.store.Revision(kMap) == 3);
  CHECK_FALSE(bench.store.Unsaved(kMap));
  CHECK_FALSE(std::filesystem::exists(bench.store.FileOf(kMap)));
}

TEST_CASE("Magasin d'éditions : la révision la plus haute l'emporte", "[app][map_edit_store]") {
  SECTION("celle du miroir") {
    Bench bench;
    WriteOwn(bench.store, Edit("poste"), 2, bench.client);
    WriteServed(bench.client, Edit("servi"), 4);
    bench.Load();
    CHECK(bench.store.Document(kMap) == Edit("servi"));
    CHECK(bench.store.Revision(kMap) == 4);
  }
  SECTION("celle du poste") {
    Bench bench;
    WriteOwn(bench.store, Edit("poste"), 5, bench.client);
    WriteServed(bench.client, Edit("servi"), 3);
    bench.Load();
    CHECK(bench.store.Document(kMap) == Edit("poste"));
    CHECK(bench.store.Revision(kMap) == 5);
  }
}

TEST_CASE("Magasin d'éditions : à révision égale, le poste garde ses retouches enregistrées",
          "[app][map_edit_store]") {
  Bench bench;
  WriteOwn(bench.store, Edit("retouché sur la 4"), 4, bench.client);
  WriteServed(bench.client, Edit("servi"), 4);
  bench.Load();
  CHECK(bench.store.Document(kMap) == Edit("retouché sur la 4"));
}

TEST_CASE("Magasin d'éditions : un document du miroir sans révision ou illisible est écarté",
          "[app][map_edit_store]") {
  SECTION("sans révision") {
    Bench bench;
    bench.client.WriteFile(std::filesystem::path("data") / "map_edits_served" / "prontera.yml",
                           BytesOf(ro::mapedit::MapEditToYaml(Edit("servi"), kMap)));
    bench.Load();
    CHECK(bench.store.Document(kMap).empty());
    CHECK(bench.store.Revision(kMap) == 0);
  }
  SECTION("illisible") {
    Bench bench;
    bench.client.WriteFile(std::filesystem::path("data") / "map_edits_served" / "prontera.yml",
                           BytesOf("revision: 3\nversion: [\n"));
    bench.Load();
    CHECK(bench.store.Document(kMap).empty());
    CHECK(bench.store.Revision(kMap) == 0);
  }
}
