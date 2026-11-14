// LES VIGNETTES GARDÉES SUR LE DISQUE : la première ouverture rend, la suivante
// relit ; un modèle changé dans l'installation se refait ; un modèle illisible
// garde son échec.

#include "ro/render/decor_vignette_cache.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/core/utf8_path.h"
#include "ro/render/decor_vignette.h"
#include "ro/vfs/vfs.h"

#include "support/grf_builder.h"
#include "support/model_builder.h"
#include "support/temp_directory.h"

using ro::render::DecorVignetteCachePath;
using ro::render::DecorVignetteCacheRoot;
using ro::render::DecorVignetteCacheRootOrNone;
using ro::render::DecorVignetteKey;
using ro::render::ListDecorModels;
using ro::render::LoadCachedDecorVignette;
using ro::render::VignetteOrigin;
using ro::test_support::BuildGrf;
using ro::test_support::BuildRsm;
using ro::test_support::Bytes;
using ro::test_support::RsmBuildOptions;
using ro::test_support::RsmFaceSpec;
using ro::test_support::RsmMeshSpec;
using ro::test_support::TempDirectory;

namespace {

constexpr std::int32_t kSidePx = 32;
constexpr const char* kArchive = "data.grf";
constexpr const char* kModel = "maison\\toit.rsm";
constexpr const char* kModelPath = "data\\model\\maison\\toit.rsm";

// Un triangle, sans texture : ses faces se peignent en gris. `width` change
// sa forme ; `spare_vertices` ajoute des sommets sans face, ce qui change la
// TAILLE du fichier.
std::vector<std::uint8_t> Triangle(float width, int spare_vertices = 0) {
  RsmMeshSpec mesh;
  mesh.name = "corps";
  mesh.texture_indices = {0};
  mesh.vertices = {{0.0F, 0.0F, 0.0F}, {width, 0.0F, 0.0F}, {0.0F, width, 0.0F}};
  for (int spare = 0; spare < spare_vertices; ++spare) {
    mesh.vertices.push_back({width, width, width});
  }
  mesh.texture_vertices = {{0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 1.0F}};
  mesh.faces = {RsmFaceSpec{.vertex_ids = {0, 1, 2}, .texture_vertex_ids = {0, 1, 2}}};
  RsmBuildOptions options;
  options.textures = {"absente.bmp"};
  options.main_mesh_names = {"corps"};
  options.meshes = {mesh};
  return BuildRsm(options);
}

void WriteArchive(const TempDirectory& client, const std::vector<std::uint8_t>& model) {
  client.WriteFile(kArchive, BuildGrf({{kModelPath, model},
                                       {"data\\model\\maison\\ancien.rsm2", Triangle(1.0F)},
                                       {"data\\model\\vide.rsm", Bytes("pas un modèle")},
                                       {"data\\texture\\autre.bmp", Bytes("x")}}));
}

ro::core::Result<ro::vfs::Vfs> Mount(const TempDirectory& client) {
  return ro::vfs::Vfs::Mount({.root = client.path(), .archives = {kArchive}});
}

}  // namespace

TEST_CASE("Vignettes gardées : un fichier par modèle, sous l'arborescence de data\\model",
          "[render][vignette]") {
  const std::filesystem::path root = ro::core::PathFromUtf8("cache");
  CHECK(DecorVignetteCachePath(root, "maison\\toit.rsm", kSidePx) ==
        root / "32" / "maison" / "toit.rsm.vgn");
  // Un segment qui remonterait ne sort pas du dossier des vignettes.
  CHECK(DecorVignetteCachePath(root, "..\\..\\x.rsm", kSidePx) == root / "32" / "x.rsm.vgn");
}

TEST_CASE("Vignettes gardées : sous le dossier de cache du client, ou nulle part",
          "[render][vignette]") {
  const std::filesystem::path client_cache = ro::core::PathFromUtf8("Moonlight-Destiny");
  const std::filesystem::path root = client_cache / "model_vignettes";
  CHECK(DecorVignetteCacheRoot(client_cache) == root);
  CHECK(DecorVignetteCacheRootOrNone(client_cache) == root);
  CHECK(DecorVignetteCachePath(root, "toit.rsm", kSidePx) == root / "32" / "toit.rsm.vgn");
  // Sans dossier de cache, un chemin vide : rien ne se garde (voir plus bas).
  const ro::core::Result<std::filesystem::path> missing =
      ro::core::Fail(ro::core::ErrorCode::kNotFound, "pas de dossier");
  CHECK(DecorVignetteCacheRootOrNone(missing).empty());
}

TEST_CASE("Vignettes gardées : la liste porte les deux familles, relatives à data\\model",
          "[render][vignette]") {
  TempDirectory client;
  WriteArchive(client, Triangle(1.0F));
  auto vfs = Mount(client);
  REQUIRE(vfs.has_value());
  CHECK(ListDecorModels(*vfs) ==
        std::vector<std::string>{"maison\\ancien.rsm2", "maison\\toit.rsm", "vide.rsm"});
  CHECK(ListDecorModels(*vfs, "maison\\") ==
        std::vector<std::string>{"maison\\ancien.rsm2", "maison\\toit.rsm"});
}

TEST_CASE("Vignettes gardées : la clé suit l'état du fichier, archive ou disque",
          "[render][vignette]") {
  TempDirectory client;
  WriteArchive(client, Triangle(1.0F));
  {
    auto vfs = Mount(client);
    REQUIRE(vfs.has_value());
    const auto key = DecorVignetteKey(*vfs, kModel, kSidePx);
    REQUIRE(key.has_value());
    CHECK(key->stamp.origin == ro::core::FileStamp::Origin::kArchive);
    CHECK(key->stamp.container == kArchive);
    CHECK(key->stamp.size == Triangle(1.0F).size());
    CHECK_FALSE(DecorVignetteKey(*vfs, "absent.rsm", kSidePx).has_value());
  }
  // Une copie dans data\ passe avant l'archive, et la clé le dit.
  client.WriteFile("data/model/maison/toit.rsm", Triangle(2.0F));
  auto vfs = Mount(client);
  REQUIRE(vfs.has_value());
  const auto key = DecorVignetteKey(*vfs, kModel, kSidePx);
  REQUIRE(key.has_value());
  CHECK(key->stamp.origin == ro::core::FileStamp::Origin::kDisk);
  CHECK(key->stamp.container.empty());
  CHECK(key->stamp.written_ticks != 0);
}

TEST_CASE("Vignettes gardées : rendue une fois, relue ensuite, refaite quand le modèle change",
          "[render][vignette]") {
  TempDirectory client;
  TempDirectory cache;
  WriteArchive(client, Triangle(1.0F));
  {
    auto vfs = Mount(client);
    REQUIRE(vfs.has_value());
    const auto first = LoadCachedDecorVignette(*vfs, kModel, kSidePx, cache.path());
    REQUIRE(first.has_value());
    CHECK(first->origin == VignetteOrigin::kRendered);
    CHECK(std::filesystem::exists(DecorVignetteCachePath(cache.path(), kModel, kSidePx)));

    const auto second = LoadCachedDecorVignette(*vfs, kModel, kSidePx, cache.path());
    REQUIRE(second.has_value());
    CHECK(second->origin == VignetteOrigin::kDisk);
    CHECK(second->image.pixels == first->image.pixels);

    // Un autre côté est une autre vignette.
    const auto larger = LoadCachedDecorVignette(*vfs, kModel, kSidePx * 2, cache.path());
    REQUIRE(larger.has_value());
    CHECK(larger->origin == VignetteOrigin::kRendered);
  }
  // L'installation change le modèle : sa taille dans l'archive change, et la
  // vignette gardée ne correspond plus.
  WriteArchive(client, Triangle(3.0F, 1));
  auto vfs = Mount(client);
  REQUIRE(vfs.has_value());
  const auto changed = LoadCachedDecorVignette(*vfs, kModel, kSidePx, cache.path());
  REQUIRE(changed.has_value());
  CHECK(changed->origin == VignetteOrigin::kRendered);
}

TEST_CASE("Vignettes gardées : un modèle illisible garde son échec", "[render][vignette]") {
  TempDirectory client;
  TempDirectory cache;
  WriteArchive(client, Triangle(1.0F));
  auto vfs = Mount(client);
  REQUIRE(vfs.has_value());
  CHECK_FALSE(LoadCachedDecorVignette(*vfs, "vide.rsm", kSidePx, cache.path()).has_value());
  const auto key = DecorVignetteKey(*vfs, "vide.rsm", kSidePx);
  REQUIRE(key.has_value());
  const auto kept = ro::render::ReadCachedDecorVignette(
      DecorVignetteCachePath(cache.path(), "vide.rsm", kSidePx), *key);
  REQUIRE(kept.has_value());
  CHECK(kept->empty());
  CHECK_FALSE(LoadCachedDecorVignette(*vfs, "vide.rsm", kSidePx, cache.path()).has_value());
}

TEST_CASE("Vignettes gardées : sans dossier, rien ne se garde", "[render][vignette]") {
  TempDirectory client;
  WriteArchive(client, Triangle(1.0F));
  auto vfs = Mount(client);
  REQUIRE(vfs.has_value());
  const auto rendered = LoadCachedDecorVignette(*vfs, kModel, kSidePx, {});
  REQUIRE(rendered.has_value());
  CHECK(rendered->origin == VignetteOrigin::kRendered);
  CHECK(rendered->image.width == kSidePx);
}
