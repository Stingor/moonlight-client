// LES OBJETS LANCÉS NAISSENT AU TIC QUE LEUR STYLE FIXE.
//
// EF_THROWITEM4 jette deux flacons d'un seul identifiant : le premier au
// cinquième tic de l'effet, le second au dixième. Aucun ne se montre avant son
// tic, chacun part de la place qu'occupe le lanceur à ce tic-là, et un effet
// retiré avant le dixième tic ne jette pas le second.

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_thrown_item_seed.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "support/client_vfs.h"
#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/temp_directory.h"

using ro::test_support::BmpPixel;
using ro::test_support::BuildBmp;
using ro::test_support::BuildGrf;
using ro::test_support::Cp949;
using ro::test_support::GrfFileSpec;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint32_t kTwoBottlesEffect = 539;
constexpr std::int32_t kFirstBirthTick = 5;
constexpr std::int32_t kSecondBirthTick = 10;
constexpr std::uint32_t kThrower = 7000;
constexpr std::int32_t kPlainParam = 1;
// Le flacon de potion vide : son style fait du paramètre de l'instance sa teinte.
constexpr std::uint32_t kTintedBottleEffect = 299;
// Au-delà du plafond de l'instance, et dont l'octet bas vaut un : non borné, il
// donnerait le flacon rouge.
constexpr std::int32_t kParamPastCap = 257;
constexpr std::uint8_t kCappedParam = 50;
constexpr std::int32_t kIconPixels = 8;
// L'instant du premier passage : il ne rattrape rien, il pose la référence.
constexpr std::uint32_t kFirstFrameMs = 100;
// Le pas que fait le lanceur avant le premier jet.
constexpr float kStepX = 40.0F;

// Un client qui porte les icônes que l'effet lance.
void FillClient(const TempDirectory& client, std::uint32_t effect_id = kTwoBottlesEffect) {
  const std::vector<BmpPixel> icone(static_cast<std::size_t>(kIconPixels * kIconPixels),
                                    BmpPixel{.red = 200, .green = 120, .blue = 60});
  std::vector<GrfFileSpec> fichiers;
  for (const ro::render::EffectThrownItemPattern& pattern :
       ro::render::EffectThrownItemPatternsFor(effect_id)) {
    fichiers.push_back(
        GrfFileSpec{Cp949(pattern.texture), BuildBmp(kIconPixels, kIconPixels, icone)});
  }
  client.WriteFile("objets.grf", BuildGrf(fichiers));
}

// Fait passer `ticks` tics à la troupe et à ses horloges, depuis l'instant `depuis_ms`.
std::uint32_t PassTicks(moonclient::EffectNodesInPlay& nodes, std::uint32_t from_ms,
                        std::int32_t ticks) {
  const ro::render::Camera camera;
  const std::uint32_t jusqu_a_ms =
      from_ms + static_cast<std::uint32_t>(ticks * ro::render::kEffectTickMs);
  static_cast<void>(nodes.Drawn(jusqu_a_ms, camera));
  return jusqu_a_ms;
}

std::vector<const ro::render::PooledEffectNode*> ThrownItems(
    const moonclient::EffectNodesInPlay& nodes) {
  std::vector<const ro::render::PooledEffectNode*> objets;
  for (const ro::render::PooledEffectNode& node : nodes.living_nodes()) {
    if (node.shape == ro::render::PooledNodeShape::kThrownItem) {
      objets.push_back(&node);
    }
  }
  return objets;
}

}  // namespace

TEST_CASE("les deux flacons d'EF_THROWITEM4 partent aux tics cinq et dix, du lanceur tel qu'il est",
          "[effect_thrown_item]") {
  const std::span<const ro::render::EffectThrownItemPattern> patterns =
      ro::render::EffectThrownItemPatternsFor(kTwoBottlesEffect);
  REQUIRE(patterns.size() == 2);
  const TempDirectory client;
  FillClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"objets.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  const glm::vec3 birth{0.0F, 0.0F, 0.0F};
  const glm::vec3 target{100.0F, 0.0F, 30.0F};

  // 🔴 L'EFFET EST JOUÉ, MAIS RIEN N'EST ENCORE LANCÉ : c'est l'horloge qui le dit.
  REQUIRE(nodes.PlayProcedural(kTwoBottlesEffect, birth, target, 0.0F, kThrower, kPlainParam));
  CHECK(ThrownItems(nodes).empty());

  // ⚠ LE BÂTISSEUR SÈME À L'ÂGE QU'IL A AU DÉBUT DU PASSAGE : le flacon du tic k
  // part au passage k + 1 compté depuis la naissance, le premier voyant l'âge 0.
  std::uint32_t maintenant = PassTicks(nodes, kFirstFrameMs, 0);
  maintenant = PassTicks(nodes, maintenant, kFirstBirthTick);
  CHECK(ThrownItems(nodes).empty());

  // Le lanceur a fait un pas : le premier flacon part de là où il est.
  const glm::vec3 caster = birth + glm::vec3{kStepX, 0.0F, 0.0F};
  nodes.FollowActor(kThrower, caster, 0.0F);
  maintenant = PassTicks(nodes, maintenant, 1);
  std::vector<const ro::render::PooledEffectNode*> objets = ThrownItems(nodes);
  REQUIRE(objets.size() == 1);
  CHECK(objets[0]->sown_at_effect_tick == kFirstBirthTick);
  CHECK(objets[0]->owner_position.x == Catch::Approx(caster.x));
  REQUIRE(objets[0]->resource_paths.size() == 1);
  CHECK(objets[0]->resource_paths[0] == patterns[0].texture);

  maintenant = PassTicks(nodes, maintenant, kSecondBirthTick - kFirstBirthTick - 1);
  CHECK(ThrownItems(nodes).size() == 1);
  static_cast<void>(PassTicks(nodes, maintenant, 1));
  objets = ThrownItems(nodes);
  REQUIRE(objets.size() == 2);
  const ro::render::PooledEffectNode* second =
      objets[0]->sown_at_effect_tick == kSecondBirthTick ? objets[0] : objets[1];
  CHECK(second->sown_at_effect_tick == kSecondBirthTick);
  CHECK(second->resource_paths[0] == patterns[1].texture);
}

TEST_CASE("un effet retiré avant le dixième tic ne lance pas son second flacon",
          "[effect_thrown_item]") {
  const TempDirectory client;
  FillClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"objets.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  const glm::vec3 birth{0.0F, 0.0F, 0.0F};
  const glm::vec3 target{100.0F, 0.0F, 30.0F};
  REQUIRE(nodes.PlayProcedural(kTwoBottlesEffect, birth, target, 0.0F, kThrower, kPlainParam));
  std::uint32_t maintenant = PassTicks(nodes, kFirstFrameMs, 0);
  maintenant = PassTicks(nodes, maintenant, kFirstBirthTick + 1);
  REQUIRE(ThrownItems(nodes).size() == 1);

  // Le lanceur est retiré : l'instance meurt avec lui, et son horloge aussi.
  nodes.StopEffect(kTwoBottlesEffect, kThrower);
  static_cast<void>(PassTicks(nodes, maintenant, kSecondBirthTick));
  CHECK(ThrownItems(nodes).empty());
}

TEST_CASE("la teinte d'un flacon lit le paramètre BORNÉ par l'instance, non son octet bas",
          "[effect_thrown_item]") {
  const TempDirectory client;
  FillClient(client, kTintedBottleEffect);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"objets.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.PlayProcedural(kTintedBottleEffect, glm::vec3{0.0F}, glm::vec3{100.0F, 0.0F, 30.0F},
                               0.0F, kThrower, kParamPastCap));
  const std::vector<const ro::render::PooledEffectNode*> objets = ThrownItems(nodes);
  REQUIRE(objets.size() == 1);
  CHECK(objets[0]->tracks[0].mark[ro::render::kMarkOfThrownExtra] == kCappedParam);
}

TEST_CASE("sans cible, aucun flacon ne part, même à son tic", "[effect_thrown_item]") {
  const TempDirectory client;
  FillClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"objets.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  static_cast<void>(nodes.PlayProcedural(kTwoBottlesEffect, glm::vec3{0.0F}, std::nullopt, 0.0F,
                                         kThrower, kPlainParam));
  const std::uint32_t maintenant = PassTicks(nodes, kFirstFrameMs, 0);
  static_cast<void>(PassTicks(nodes, maintenant, kSecondBirthTick));
  CHECK(ThrownItems(nodes).empty());
}
