// LA NUÉE DE SIGHT BLASTER, effet 601.
//
// Elle tourne tant que l'état 198 tient, et c'est le RETRAIT de l'état qui
// l'éteint : le client déclare à cet effet une durée de six heures quarante,
// quand le buff en dure quinze minutes.

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/state_effect.h"
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

constexpr std::uint32_t kSightBlasterEffect = 601;
constexpr std::uint32_t kSightEffect = 22;
constexpr std::uint16_t kSightBlasterState = 198;
constexpr std::string_view kSightSprite = "data\\sprite\\이팩트\\Sight";
constexpr std::int32_t kSpritePixels = 8;
// Le porteur dont la nuée suit les pas.
constexpr std::uint32_t kCaster = 4242;
constexpr std::uint32_t kOtherCaster = 777;

void FillSightClient(const TempDirectory& client) {
  const auto cp949 = [](std::string_view utf8) {
    auto converti = ro::vfs::Utf8ToCp949(utf8);
    REQUIRE(converti.has_value());
    return *converti;
  };
  SprBuildOptions sprite;
  sprite.indexed.push_back(SprIndexedFrameSpec{
      kSpritePixels, kSpritePixels,
      std::vector<std::uint8_t>(static_cast<std::size_t>(kSpritePixels * kSpritePixels), 1)});
  sprite.palette = {SpriteColor{}, SpriteColor{.red = 255, .green = 120, .blue = 0}};
  ActBuildOptions animation;
  animation.actions.push_back(ActActionSpec{{ActFrameSpec{{ActLayerSpec{}}, {}, -1}}});
  animation.speeds = {1.0F};
  client.WriteFile(
      "sight.grf",
      BuildGrf({GrfFileSpec{cp949(std::string(kSightSprite) + ".spr"), BuildSpr(sprite)},
                GrfFileSpec{cp949(std::string(kSightSprite) + ".act"), BuildAct(animation)}}));
}

const ro::render::EffectNodePattern& PatternOf(std::uint32_t effect_id, std::size_t rank) {
  const std::span<const ro::render::EffectNodePattern> patterns =
      ro::render::EffectNodePatternsFor(effect_id);
  REQUIRE(patterns.size() > rank);
  return patterns[rank];
}

// Sème la nuée sur un porteur, et rend la troupe pour qu'on l'interroge.
void PlayOn(moonclient::EffectNodesInPlay& nodes, std::uint32_t actor_id) {
  REQUIRE(nodes.PlayProcedural(kSightBlasterEffect, glm::vec3(0.0F, 0.0F, 0.0F), std::nullopt, 0.0F,
                               actor_id));
}

std::size_t CountFrom(const moonclient::EffectNodesInPlay& nodes, std::uint32_t actor_id) {
  std::size_t count = 0;
  for (const ro::render::PooledEffectNode& node : nodes.living_nodes()) {
    if (node.anchor_id == actor_id) {
      ++count;
    }
  }
  return count;
}

}  // namespace

TEST_CASE("la nuée de Sight Blaster reprend celle de Sight, sans son ombre", "[sight_blaster]") {
  const ro::render::EffectNodePattern& blaster = PatternOf(kSightBlasterEffect, 0);
  const ro::render::EffectNodePattern& sight = PatternOf(kSightEffect, 0);

  // 🔴 LE CAS EST LE JUMEAU DU PREMIER NŒUD DE SIGHT, champ pour
  // champ — même cercle, même hauteur, même vie, même cadence, même échelle.
  CHECK(blaster.shape == sight.shape);
  CHECK(blaster.life_ticks == sight.life_ticks);
  CHECK(blaster.period_ticks == sight.period_ticks);
  CHECK(blaster.sprite_scale == Catch::Approx(sight.sprite_scale));
  CHECK(blaster.sprite_frame_hold_ticks == sight.sprite_frame_hold_ticks);
  CHECK(blaster.birth_sweep_radius == Catch::Approx(sight.birth_sweep_radius));
  CHECK(blaster.birth_sweep_degrees_per_tick == Catch::Approx(sight.birth_sweep_degrees_per_tick));
  CHECK(blaster.launch_drop == Catch::Approx(sight.launch_drop));

  // ⚠ SAUF L'ALPHA — cinquante contre cent cinquante — ET L'OMBRE : l'effet 22
  // pose un second nœud, `Shadow`, aux pieds de chaque boule ; le 601 non.
  CHECK(blaster.alpha_start == Catch::Approx(50.0F));
  CHECK(sight.alpha_start == Catch::Approx(150.0F));
  CHECK(ro::render::EffectNodePatternsFor(kSightBlasterEffect).size() == 1);
  CHECK(ro::render::EffectNodePatternsFor(kSightEffect).size() == 2);
}

TEST_CASE("l'état de Sight Blaster pose son effet ET le retire", "[sight_blaster]") {
  // 🔴 LE RETRAIT EST CE QUI L'ARRÊTE, et rien d'autre : le client déclare à cet
  // effet 999 999 tics, six heures quarante, là où le buff dure neuf cents
  // secondes. Sans lui, la nuée tournerait jusqu'au changement de carte.
  const auto carries = ro::render::StateEffectsFor(kSightBlasterState);
  REQUIRE(carries.size() == 1);
  CHECK(carries.front().effect_id == kSightBlasterEffect);
  CHECK(carries.front().moment == ro::render::StateEffectMoment::kOnApply);

  const auto retire = ro::render::StateRemovalsFor(kSightBlasterState);
  REQUIRE(retire.size() == 1);
  CHECK(retire.front().effect_id == kSightBlasterEffect);
}

TEST_CASE("couper un effet procédural emporte ce qu'il a semé", "[sight_blaster]") {
  const TempDirectory client;
  FillSightClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"sight.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);

  PlayOn(nodes, kCaster);
  REQUIRE(CountFrom(nodes, kCaster) == 1);

  nodes.StopEffect(kSightBlasterEffect, kCaster);
  CHECK(CountFrom(nodes, kCaster) == 0);
}

TEST_CASE("couper un effet procédural emporte aussi son horloge", "[sight_blaster]") {
  const TempDirectory client;
  FillSightClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"sight.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  const ro::render::Camera camera;

  PlayOn(nodes, kCaster);
  // ⚠ LE PREMIER PASSAGE NE RATTRAPE RIEN : il pose l'instant de référence. Le
  // temps ne passe qu'au second.
  static_cast<void>(nodes.Drawn(100, camera));
  static_cast<void>(nodes.Drawn(300, camera));
  REQUIRE(CountFrom(nodes, kCaster) > 1);

  // 🔴 RETIRER LES NŒUDS SANS L'HORLOGE NE SUFFIT PAS : elle en resèmerait au
  // tic suivant, et l'effet repousserait derrière la coupe.
  nodes.StopEffect(kSightBlasterEffect, kCaster);
  REQUIRE(CountFrom(nodes, kCaster) == 0);
  static_cast<void>(nodes.Drawn(500, camera));
  CHECK(CountFrom(nodes, kCaster) == 0);
}

TEST_CASE("couper n'emporte que le porteur nommé", "[sight_blaster]") {
  const TempDirectory client;
  FillSightClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"sight.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);

  PlayOn(nodes, kCaster);
  PlayOn(nodes, kOtherCaster);
  REQUIRE(CountFrom(nodes, kCaster) == 1);
  REQUIRE(CountFrom(nodes, kOtherCaster) == 1);

  // ⚠ LE COUPLE, ET NON L'EFFET SEUL : deux joueurs peuvent porter la même
  // nuée, et couper la sienne ne doit pas éteindre celle du voisin.
  nodes.StopEffect(kSightBlasterEffect, kCaster);
  CHECK(CountFrom(nodes, kCaster) == 0);
  CHECK(CountFrom(nodes, kOtherCaster) == 1);
}
