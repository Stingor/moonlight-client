// LE PRIMITIF 44, la fumée que sème un semis direct : d'une à quatre bouffées,
// tirées à chaque lancement. Un effet 44 annoncé sur un acteur crée, lui, la
// classe des cheminées — voir `effect_chimney_notice_test.cpp`.

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/encoding.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "support/grf_builder.h"
#include "support/sprite_builder.h"
#include "support/temp_directory.h"

using ro::test_support::ActActionSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildGrf;
using ro::test_support::BuildSpr;
using ro::test_support::GrfFileSpec;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::SpriteColor;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint32_t kSmokeEffect = 44;
// Le couple que le patron de la fumée nomme.
constexpr std::string_view kSmokeSprite = "data\\sprite\\이팩트\\굴뚝연기";
constexpr std::int32_t kPuffPixels = 8;
constexpr std::size_t kFewestPuffs = 1;
constexpr std::size_t kMostPuffs = 4;
// Assez de lancements pour que le tirage montre plus d'un compte.
constexpr int kLaunches = 24;

std::vector<std::uint8_t> PuffSpr() {
  SprBuildOptions options;
  options.indexed.push_back(SprIndexedFrameSpec{
      kPuffPixels, kPuffPixels,
      std::vector<std::uint8_t>(static_cast<std::size_t>(kPuffPixels * kPuffPixels), 1)});
  options.palette = {SpriteColor{}, SpriteColor{.red = 200, .green = 200, .blue = 200}};
  return BuildSpr(options);
}

std::vector<std::uint8_t> PuffAct() {
  ActBuildOptions options;
  options.actions.push_back(ActActionSpec{{ActFrameSpec{{ActLayerSpec{}}, {}, -1}}});
  options.speeds = {4.0F};
  return BuildAct(options);
}

void FillSmokeClient(const TempDirectory& client) {
  const auto cp949 = [](std::string_view utf8) {
    auto converti = ro::vfs::Utf8ToCp949(utf8);
    REQUIRE(converti.has_value());
    return *converti;
  };
  client.WriteFile("fumee.grf",
                   BuildGrf({GrfFileSpec{cp949(std::string(kSmokeSprite) + ".spr"), PuffSpr()},
                             GrfFileSpec{cp949(std::string(kSmokeSprite) + ".act"), PuffAct()}}));
}

}  // namespace

TEST_CASE("la fumée lâche une à quatre bouffées, un compte par lancement", "[effect_smoke]") {
  const TempDirectory client;
  FillSmokeClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"fumee.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);

  std::set<std::size_t> comptes;
  for (int lancement = 0; lancement < kLaunches; ++lancement) {
    const std::size_t before = nodes.living_nodes().size();
    REQUIRE(nodes.PlayProcedural(kSmokeEffect, glm::vec3(0.0F, 0.0F, 0.0F)));
    const std::size_t sown_count = nodes.living_nodes().size() - before;
    INFO("lancement " << lancement);
    CHECK(sown_count >= kFewestPuffs);
    CHECK(sown_count <= kMostPuffs);
    comptes.insert(sown_count);
  }
  // ⚠ UN COMPTE TIRÉ, ET NON FIXE : un seul compte sur tant de lancements
  // voudrait dire que la borne haute n'est pas lue.
  CHECK(comptes.size() > 1);
}

TEST_CASE("chaque bouffée tourne sur elle-même", "[effect_smoke]") {
  const TempDirectory client;
  FillSmokeClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"fumee.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.PlayProcedural(kSmokeEffect, glm::vec3(0.0F, 0.0F, 0.0F)));
  REQUIRE_FALSE(nodes.living_nodes().empty());
  for (const auto& alive : nodes.living_nodes()) {
    CHECK(alive.node.draws_rotated);
    CHECK(alive.node.spin_speed_per_tick > 0.0F);
    // Et ses calques tournent avec lui : c'est un nœud d'effet.
    CHECK(alive.node.draws_layer_angles);
  }
}
