// LE HALO DES MONSTRES QUI PARAISSENT, sous-type 153.
//
// Chaque monstre se peint deux fois : à son mélange, puis élargi d'un pixel
// d'écran de chaque côté et ajouté. La seconde pose porte l'élargissement ; la
// première n'en porte aucun.

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_glow_sprite_seed.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "support/client_vfs.h"
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
using ro::test_support::Cp949;
using ro::test_support::GrfFileSpec;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::SpriteColor;
using ro::test_support::TempDirectory;

namespace {

constexpr std::int32_t kSpritePixels = 8;
constexpr std::uint32_t kFirstFrameMs = 100;

// Un `.act` de huit actions d'une image : le style peut tirer sa direction.
std::vector<std::uint8_t> EightActionAct() {
  ActBuildOptions options;
  constexpr std::size_t kActions = 64;
  for (std::size_t action = 0; action < kActions; ++action) {
    options.actions.push_back(ActActionSpec{{ActFrameSpec{{ActLayerSpec{}}, {}, -1}}});
    options.speeds.push_back(1.0F);
  }
  return BuildAct(options);
}

std::vector<std::uint8_t> OneFrameSpr() {
  SprBuildOptions options;
  options.indexed.push_back(SprIndexedFrameSpec{
      kSpritePixels, kSpritePixels,
      std::vector<std::uint8_t>(static_cast<std::size_t>(kSpritePixels * kSpritePixels), 1)});
  options.palette = {SpriteColor{}, SpriteColor{.red = 200, .green = 60, .blue = 200}};
  return BuildSpr(options);
}

}  // namespace

TEST_CASE("un monstre qui paraît se peint deux fois, la seconde élargie et ajoutée",
          "[effect_glow_halo]") {
  const ro::render::EffectGlowSpriteNodePattern& pattern =
      ro::render::EffectGlowSpriteNodePatternTable().front();
  const ro::render::EffectGlowSpriteStyleSeed* seed =
      ro::render::EffectGlowSpriteStyleSeedFor(pattern.style);
  REQUIRE(seed != nullptr);

  const TempDirectory client;
  const std::string base(seed->sprite_base);
  client.WriteFile("monstres.grf", BuildGrf({GrfFileSpec{Cp949(base + ".spr"), OneFrameSpr()},
                                             GrfFileSpec{Cp949(base + ".act"), EightActionAct()}}));
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"monstres.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.PlayProcedural(pattern.effect_id, glm::vec3(0.0F, 0.0F, 0.0F)));

  // Le monstre naît éteint : quelques tics lui donnent son alpha.
  const ro::render::Camera camera;
  constexpr std::int32_t kTicks = 10;
  static_cast<void>(nodes.Drawn(kFirstFrameMs, camera));
  static_cast<void>(nodes.Drawn(
      kFirstFrameMs + static_cast<std::uint32_t>(kTicks * ro::render::kEffectTickMs), camera));

  std::size_t ordinaires = 0;
  std::size_t halos = 0;
  for (const moonclient::EffectNodesInPlay::SpriteNodeBatch& lot : nodes.SpriteBatches()) {
    for (const ro::render::DrawnActor& actor : lot.actors) {
      if (actor.grow_screen_pixels == ro::render::kEffectGlowSpriteHaloScreenPixels) {
        // Le halo s'ajoute, quel que soit le mélange de la première passe.
        CHECK(lot.additive);
        ++halos;
      } else {
        CHECK(actor.grow_screen_pixels == 0.0F);
        ++ordinaires;
      }
    }
  }
  CHECK(ordinaires >= 1);
  CHECK(halos == ordinaires);
}
