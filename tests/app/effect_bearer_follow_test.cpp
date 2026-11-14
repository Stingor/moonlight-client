// UN EFFET JOUÉ SUR UN ACTEUR : il lui appartient toujours, il ne le suit que
// si son identifiant le veut.
//
// La torche recopie la place de son porteur à chaque tic ; la bouffée de poison
// ne la recopie jamais. Les deux sont accrochées au même acteur, et disparaissent
// avec lui.

#include "ro/render/effect_bearer_follow.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_hit_cross_seed.h"
#include "ro/render/effect_lens_flare_seed.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_screen_burst_seed.h"
#include "ro/render/effect_screen_seed.h"
#include "ro/render/effect_split_beam.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "support/client_vfs.h"
#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/sprite_builder.h"
#include "support/temp_directory.h"

using ro::test_support::ActActionSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildGrf;
using ro::test_support::BuildSpr;
using ro::test_support::BuildTga;
using ro::test_support::Cp949;
using ro::test_support::GrfFileSpec;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::SpriteColor;
using ro::test_support::TempDirectory;
using ro::test_support::TgaPixel;

namespace {

constexpr std::uint32_t kTorchEffect = 47;
constexpr std::uint32_t kPoisonHitEffect = 53;
// EF_WARP : il reste où il est né, et sème une étincelle tous les deux tics.
constexpr std::uint32_t kWarpEffect = 8;
// LES ÉTINCELLES DU WARP : l'effet rejoint son porteur aux tics un et deux, puis
// plus jamais, et sème une étincelle tous les dix tics.
constexpr std::uint32_t kWarpZoneEffect = 321;
constexpr std::int32_t kWarpZoneLastRejoinTick = 2;
constexpr std::int32_t kWarpZoneSparkPeriodTicks = 10;
// LES CINQ FAMILLES DE NŒUDS D'ÉCRAN, une par effet : la couronne et l'éclat de
// Bash, le nœud d'Endure, la gerbe de reflets d'un coup, la croix d'un impact,
// l'étoile de glace.
constexpr std::uint32_t kBashEffect = 16;
constexpr std::uint32_t kEndureEffect = 11;
constexpr std::uint32_t kHitLensFlareEffect = 1;
constexpr std::uint32_t kHitCrossEffect = 4;
constexpr std::uint32_t kIceSmokeColumnEffect = 51;
// LA GERBE BLEUTÉE : le seul nœud d'écran qui reste où il est né.
constexpr std::uint32_t kBluishLensFlareEffect = 183;
// Le moindre déplacement à l'écran, en pixels, qui compte pour « a suivi ».
constexpr float kMinScreenShiftPixels = 1.0F;
constexpr float kViewportWidth = 640.0F;
constexpr float kViewportHeight = 480.0F;
// Assez loin pour qu'aucune dispersion d'étincelle ne l'atteigne.
constexpr float kFarStepX = 200.0F;
// L'instant du premier passage : il ne rattrape rien, il pose la référence.
constexpr std::uint32_t kFirstFrameMs = 100;
// Les couples `.spr`/`.act` que nomment les patrons engendrés des deux effets.
constexpr std::string_view kTorchSprite = "data\\sprite\\이팩트\\torch_01";
constexpr std::string_view kPoisonHitSprite = "data\\sprite\\이팩트\\PoisonHit";
constexpr std::int32_t kSpritePixels = 8;
constexpr std::uint32_t kBearer = 5000;
constexpr std::int32_t kPlainParam = 1;
// Le pas que fait le porteur entre la naissance des effets et l'image suivante.
constexpr float kStepX = 40.0F;

std::vector<std::uint8_t> OneFrameSpr() {
  SprBuildOptions options;
  options.indexed.push_back(SprIndexedFrameSpec{
      kSpritePixels, kSpritePixels,
      std::vector<std::uint8_t>(static_cast<std::size_t>(kSpritePixels * kSpritePixels), 1)});
  options.palette = {SpriteColor{}, SpriteColor{.red = 0, .green = 200, .blue = 60}};
  return BuildSpr(options);
}

std::vector<std::uint8_t> OneFrameAct() {
  ActBuildOptions options;
  options.actions.push_back(ActActionSpec{{ActFrameSpec{{ActLayerSpec{}}, {}, -1}}});
  options.speeds = {1.0F};
  return BuildAct(options);
}

// Un client qui ne porte que les deux sprites, sous les chemins que les patrons
// nomment.
void FillClient(const TempDirectory& client) {
  std::vector<GrfFileSpec> fichiers;
  for (const std::string_view sprite : {kTorchSprite, kPoisonHitSprite}) {
    fichiers.push_back(GrfFileSpec{Cp949(std::string(sprite) + ".spr"), OneFrameSpr()});
    fichiers.push_back(GrfFileSpec{Cp949(std::string(sprite) + ".act"), OneFrameAct()});
  }
  client.WriteFile("effets.grf", BuildGrf(fichiers));
}

// Un client qui porte de quoi montrer chaque patron de ces effets : un couple
// `.spr`/`.act` pour un sprite, une texture pour une figure du monde.
void FillClientFor(const TempDirectory& client, std::initializer_list<std::uint32_t> effects) {
  const std::vector<TgaPixel> texture(static_cast<std::size_t>(kSpritePixels * kSpritePixels),
                                      TgaPixel{.red = 200, .green = 200, .blue = 60});
  std::vector<GrfFileSpec> fichiers;
  for (const std::uint32_t each_effect_id : effects) {
    for (const ro::render::EffectNodePattern& pattern :
         ro::render::EffectNodePatternsFor(each_effect_id)) {
      for (const std::string_view name : ro::render::EffectNodePatternTextures(pattern)) {
        if (pattern.shape == ro::render::EffectNodeShape::kSprite) {
          fichiers.push_back(GrfFileSpec{Cp949(std::string(name) + ".spr"), OneFrameSpr()});
          fichiers.push_back(GrfFileSpec{Cp949(std::string(name) + ".act"), OneFrameAct()});
        } else {
          fichiers.push_back(
              GrfFileSpec{Cp949(name), BuildTga(kSpritePixels, kSpritePixels, texture)});
        }
      }
    }
  }
  client.WriteFile("effets.grf", BuildGrf(fichiers));
}

// Un client qui porte les images des nœuds d'écran de cet effet, toutes familles.
void FillClientWithScreenNodes(const TempDirectory& client, std::uint32_t each_effect_id) {
  const std::vector<TgaPixel> texture(static_cast<std::size_t>(kSpritePixels * kSpritePixels),
                                      TgaPixel{.red = 200, .green = 200, .blue = 60});
  std::vector<std::string> paths;
  for (const auto& pattern : ro::render::EffectScreenBurstPatternsFor(each_effect_id)) {
    paths.emplace_back(pattern.texture);
  }
  for (const auto& pattern : ro::render::EffectScreenPatternsFor(each_effect_id)) {
    for (std::string& path : ro::render::EffectScreenTexturePaths(pattern)) {
      paths.push_back(std::move(path));
    }
  }
  for (const auto& pattern : ro::render::EffectLensFlarePatternsFor(each_effect_id)) {
    paths.emplace_back(pattern.even_texture);
    paths.emplace_back(pattern.odd_texture);
  }
  for (const auto& pattern : ro::render::EffectHitCrossPatternsFor(each_effect_id)) {
    paths.emplace_back(pattern.texture);
  }
  for (const auto& pattern : ro::render::EffectSplitBeamPatternsFor(each_effect_id)) {
    paths.emplace_back(pattern.texture);
  }
  std::ranges::sort(paths);
  const auto [fin, _] = std::ranges::unique(paths);
  paths.erase(fin, paths.end());
  std::vector<GrfFileSpec> fichiers;
  for (const std::string& path : paths) {
    fichiers.push_back(GrfFileSpec{Cp949(path), BuildTga(kSpritePixels, kSpritePixels, texture)});
  }
  client.WriteFile("effets.grf", BuildGrf(fichiers));
}

ro::render::Camera ScreenCamera() {
  ro::render::Camera camera;
  camera.look_at = {0.0F, 0.0F, 0.0F};
  camera.distance = 100.0F;
  camera.pitch_degrees = 45.0F;
  camera.yaw_degrees = 0.0F;
  camera.fov_degrees = 45.0F;
  return camera;
}

// L'abscisse moyenne, en pixels, de tous les coins des quads d'écran.
float MeanScreenX(moonclient::EffectNodesInPlay& nodes, const ro::render::Camera& camera) {
  float somme = 0.0F;
  std::size_t corners = 0;
  for (const ro::render::DrawnScreenQuads& lot :
       nodes.ScreenQuads(camera, kViewportWidth, kViewportHeight)) {
    for (const ro::render::EffectScreenQuad& quad : lot.quads) {
      for (const glm::vec2 corner : quad.corners) {
        somme += corner.x;
        ++corners;
      }
    }
  }
  REQUIRE(corners > 0);
  return somme / static_cast<float>(corners);
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

const ro::render::PooledEffectNode* NodeOf(const moonclient::EffectNodesInPlay& nodes,
                                           std::uint32_t effect_id) {
  for (const ro::render::PooledEffectNode& node : nodes.living_nodes()) {
    if (node.sown_by_effect_id == effect_id) {
      return &node;
    }
  }
  return nullptr;
}

}  // namespace

TEST_CASE("un effet accroché suit son porteur si son identifiant le veut, et seulement alors",
          "[effect_bearer_follow]") {
  const TempDirectory client;
  FillClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effets.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  const glm::vec3 birth{0.0F, 0.0F, 0.0F};
  REQUIRE(nodes.PlayProcedural(kTorchEffect, birth, std::nullopt, 0.0F, kBearer, kPlainParam));
  REQUIRE(nodes.PlayProcedural(kPoisonHitEffect, birth, std::nullopt, 0.0F, kBearer, kPlainParam));
  const ro::render::PooledEffectNode* torche = NodeOf(nodes, kTorchEffect);
  const ro::render::PooledEffectNode* bouffee = NodeOf(nodes, kPoisonHitEffect);
  REQUIRE(torche != nullptr);
  REQUIRE(bouffee != nullptr);
  const float torche_x = torche->ground_position.x;
  const float bouffee_x = bouffee->ground_position.x;

  nodes.FollowActor(kBearer, birth + glm::vec3{kStepX, 0.0F, 0.0F}, 0.0F);

  // 🔴 LA TORCHE MARCHE AVEC LUI, LA BOUFFÉE RESTE. Le cas de la bouffée de
  // poison ne recopie jamais la place de son porteur : elle reste où elle est
  // née, et c'était l'écart que l'on corrige ici.
  torche = NodeOf(nodes, kTorchEffect);
  bouffee = NodeOf(nodes, kPoisonHitEffect);
  REQUIRE(torche != nullptr);
  REQUIRE(bouffee != nullptr);
  CHECK(torche->ground_position.x == Catch::Approx(torche_x + kStepX));
  CHECK(bouffee->ground_position.x == Catch::Approx(bouffee_x));

  // ⚠ MAIS ELLE LUI APPARTIENT TOUJOURS : un porteur parti emporte les deux.
  nodes.ForgetAbsentBearers([](std::uint32_t) { return false; });
  CHECK(nodes.living_nodes().empty());
}

TEST_CASE("l'horloge d'un effet qui reste sème à sa place de naissance, même si le porteur marche",
          "[effect_bearer_follow]") {
  // 🔴 LES SEMIS À VENIR AUSSI. Ce que l'effet a déjà semé reste en place, et
  // ce qu'il sèmera ensuite naît au même endroit : l'effet n'a pas bougé, et ses
  // nœuds se placent sur lui.
  REQUIRE(ro::render::EffectBearerFollowFor(kWarpEffect) == nullptr);
  const TempDirectory client;
  FillClientFor(client, {kWarpEffect});
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effets.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  const glm::vec3 birth{0.0F, 0.0F, 0.0F};
  REQUIRE(nodes.PlayProcedural(kWarpEffect, birth, std::nullopt, 0.0F, kBearer, kPlainParam));
  const std::size_t at_start = nodes.living_nodes().size();

  nodes.FollowActor(kBearer, birth + glm::vec3{kFarStepX, 0.0F, 0.0F}, 0.0F);
  const std::uint32_t maintenant = PassTicks(nodes, kFirstFrameMs, 0);
  static_cast<void>(PassTicks(nodes, maintenant, kWarpZoneSparkPeriodTicks));

  REQUIRE(nodes.living_nodes().size() > at_start);
  for (const ro::render::PooledEffectNode& node : nodes.living_nodes()) {
    INFO("né au tic " << node.sown_at_effect_tick);
    CHECK(node.ground_position.x < kFarStepX / 2.0F);
  }
}

TEST_CASE("un effet qui rejoint son porteur à des tics précis ne le suit plus ensuite",
          "[effect_bearer_follow]") {
  // Les étincelles du warp : l'effet reprend la place de son porteur aux tics
  // un et deux, puis la garde. Les étincelles des tics dix et vingt naissent là
  // où le porteur se tenait au deuxième tic — pas à sa naissance, et pas là où
  // il est allé depuis.
  const ro::render::EffectBearerFollow* follow = ro::render::EffectBearerFollowFor(kWarpZoneEffect);
  REQUIRE(follow != nullptr);
  REQUIRE(follow->last_tick == kWarpZoneLastRejoinTick);
  const TempDirectory client;
  FillClientFor(client, {kWarpZoneEffect});
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effets.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  const glm::vec3 birth{0.0F, 0.0F, 0.0F};
  static_cast<void>(
      nodes.PlayProcedural(kWarpZoneEffect, birth, std::nullopt, 0.0F, kBearer, kPlainParam));

  // Avant le premier tic, le porteur fait un pas ; l'effet l'y rejoint aux tics
  // un et deux.
  const glm::vec3 rejointe = birth + glm::vec3{kFarStepX, 0.0F, 0.0F};
  nodes.FollowActor(kBearer, rejointe, 0.0F);
  std::uint32_t maintenant = PassTicks(nodes, kFirstFrameMs, 0);
  maintenant = PassTicks(nodes, maintenant, kWarpZoneLastRejoinTick + 1);
  // Puis il s'éloigne encore : l'effet, lui, ne bouge plus.
  nodes.FollowActor(kBearer, rejointe + glm::vec3{kFarStepX, 0.0F, 0.0F}, 0.0F);
  static_cast<void>(PassTicks(nodes, maintenant, 2 * kWarpZoneSparkPeriodTicks));

  std::size_t tardifs = 0;
  for (const ro::render::PooledEffectNode& node : nodes.living_nodes()) {
    if (node.sown_at_effect_tick < kWarpZoneSparkPeriodTicks) {
      continue;
    }
    ++tardifs;
    INFO("né au tic " << node.sown_at_effect_tick);
    CHECK(node.ground_position.x > kFarStepX / 2.0F);
    CHECK(node.ground_position.x < kFarStepX * 3.0F / 2.0F);
  }
  CHECK(tardifs > 0);
}

// Cet acteur fait-il partie de ceux que le moteur fait suivre ?
bool Bears(const moonclient::EffectNodesInPlay& nodes, std::uint32_t actor_id) {
  std::vector<std::uint32_t> ids;
  nodes.CollectBearerIds(ids);
  return std::ranges::find(ids, actor_id) != ids.end();
}

// De combien de pixels les nœuds d'écran d'un effet porté se déplacent quand
// leur porteur fait un pas vers l'est. `suivi` dit si ces nœuds sont censés
// lui rester accrochés.
float ScreenShiftWhenBearerSteps(std::uint32_t each_effect_id, bool follow) {
  const TempDirectory client;
  FillClientWithScreenNodes(client, each_effect_id);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effets.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  const ro::render::Camera camera = ScreenCamera();
  const glm::vec3 birth{0.0F, 0.0F, 0.0F};
  REQUIRE(nodes.PlayProcedural(each_effect_id, birth, std::nullopt, 0.0F, kBearer, kPlainParam));
  // ⚠ C'EST CE QUI FAIT APPELER `FollowActor` : sans lui, rien ne suivrait.
  CHECK(Bears(nodes, kBearer) == follow);
  // UN PREMIER PASSAGE : avant lui, le client n'a encore rien semé à dessiner.
  static_cast<void>(PassTicks(nodes, PassTicks(nodes, kFirstFrameMs, 0), 1));
  const float before = MeanScreenX(nodes, camera);
  nodes.FollowActor(kBearer, birth + glm::vec3{kStepX, 0.0F, 0.0F}, 0.0F);
  const float after = MeanScreenX(nodes, camera);
  // Un autre acteur ne les emmène pas.
  nodes.FollowActor(kBearer + 1, birth - glm::vec3{kFarStepX, 0.0F, 0.0F}, 0.0F);
  CHECK(MeanScreenX(nodes, camera) == Catch::Approx(after));
  return after - before;
}

TEST_CASE("les nœuds d'écran d'un effet porté suivent leur porteur, même un impact",
          "[effect_bearer_follow]") {
  // 🔴 LE CLIENT REPROJETTE LE PORTEUR À CHAQUE TIC, pour les cinq familles. Bash
  // est un impact, que la table de suivi laisse où il est né dans le monde ; sa
  // gerbe d'écran, elle, suit l'acteur qui marche.
  for (const std::uint32_t each_effect_id :
       {kBashEffect, kEndureEffect, kHitLensFlareEffect, kHitCrossEffect, kIceSmokeColumnEffect}) {
    INFO("effet " << each_effect_id);
    CHECK(ScreenShiftWhenBearerSteps(each_effect_id, true) > kMinScreenShiftPixels);
  }
}

TEST_CASE("la gerbe bleutée reste où elle est née", "[effect_bearer_follow]") {
  // ⚠ SA MISE À JOUR NE PROJETTE SON PORTEUR QU'AU TIC ZÉRO : ses reflets ne le
  // suivent pas, quand ceux du coup ordinaire, du même bâtisseur, le suivent.
  CHECK(ScreenShiftWhenBearerSteps(kBluishLensFlareEffect, false) == Catch::Approx(0.0F));
}

TEST_CASE("les nœuds d'écran partent avec leur porteur, et un effet posé là n'en a pas",
          "[effect_bearer_follow]") {
  const TempDirectory client;
  FillClientWithScreenNodes(client, kBashEffect);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effets.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  const ro::render::Camera camera = ScreenCamera();
  REQUIRE(nodes.PlayProcedural(kBashEffect, glm::vec3(0.0F, 0.0F, 0.0F)));
  // Zéro ne désigne aucun acteur, et ne se fait pas suivre.
  CHECK_FALSE(Bears(nodes, 0));
  CHECK_FALSE(Bears(nodes, kBearer));
  // ⚠ CHAQUE NAISSANCE ATTEND SON PREMIER PASSAGE pour se dessiner.
  std::uint32_t maintenant = PassTicks(nodes, PassTicks(nodes, kFirstFrameMs, 0), 1);
  const std::size_t bearerless_count =
      nodes.ScreenQuads(camera, kViewportWidth, kViewportHeight).size();
  REQUIRE(bearerless_count > 0);

  REQUIRE(nodes.PlayProcedural(kBashEffect, glm::vec3(0.0F, 0.0F, 0.0F), std::nullopt, 0.0F,
                               kBearer, kPlainParam));
  CHECK(nodes.ScreenQuads(camera, kViewportWidth, kViewportHeight).size() == bearerless_count);
  static_cast<void>(PassTicks(nodes, maintenant, 1));
  REQUIRE(nodes.ScreenQuads(camera, kViewportWidth, kViewportHeight).size() > bearerless_count);
  // Le porteur s'en va : ses nœuds d'écran avec lui, ceux posés là restent.
  nodes.ForgetAbsentBearers([](std::uint32_t) { return false; });
  CHECK(nodes.ScreenQuads(camera, kViewportWidth, kViewportHeight).size() == bearerless_count);
  CHECK_FALSE(Bears(nodes, kBearer));
}

TEST_CASE("la liste des porteurs tient l'horloge, puis ce qu'elle a semé, et jamais zéro",
          "[effect_bearer_follow]") {
  const TempDirectory client;
  FillClientWithScreenNodes(client, kBashEffect);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effets.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  const glm::vec3 birth{0.0F, 0.0F, 0.0F};
  REQUIRE(nodes.PlayProcedural(kBashEffect, birth));
  REQUIRE(nodes.PlayProcedural(kBashEffect, birth, std::nullopt, 0.0F, kBearer, kPlainParam));

  const auto listed = [&nodes] {
    std::vector<std::uint32_t> ids;
    nodes.CollectBearerIds(ids);
    return ids;
  };
  CHECK(listed() == std::vector<std::uint32_t>{kBearer});
  static_cast<void>(PassTicks(nodes, PassTicks(nodes, kFirstFrameMs, 0), 1));
  CHECK(listed() == std::vector<std::uint32_t>{kBearer});

  nodes.ForgetAbsentBearers([](std::uint32_t) { return false; });
  CHECK(listed().empty());
}

TEST_CASE("suivre tous les porteurs revient à les suivre un par un", "[effect_bearer_follow]") {
  const TempDirectory client;
  FillClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effets.grf"}});
  REQUIRE(vfs.has_value());
  const glm::vec3 birth{0.0F, 0.0F, 0.0F};
  const glm::vec3 place = birth + glm::vec3{kStepX, 0.0F, 0.0F};

  const auto play = [&](moonclient::EffectNodesInPlay& nodes) {
    REQUIRE(nodes.PlayProcedural(kTorchEffect, birth, std::nullopt, 0.0F, kBearer, kPlainParam));
    REQUIRE(
        nodes.PlayProcedural(kPoisonHitEffect, birth, std::nullopt, 0.0F, kBearer, kPlainParam));
    REQUIRE(nodes.PlayProcedural(kTorchEffect, birth));
  };
  moonclient::EffectNodesInPlay one_by_one(*vfs);
  play(one_by_one);
  moonclient::EffectNodesInPlay all_at_once(*vfs);
  play(all_at_once);

  one_by_one.FollowActor(kBearer, place, 0.0F);
  all_at_once.FollowBearers(
      [&](std::uint32_t bearer_id) -> std::optional<ro::render::EffectBearerPlace> {
        return ro::render::EffectBearerPlace{.actor_id = bearer_id, .ground_position = place};
      });

  const auto expected = one_by_one.living_nodes();
  const auto got = all_at_once.living_nodes();
  REQUIRE(got.size() == expected.size());
  bool any_moved = false;
  for (std::size_t rank = 0; rank < expected.size(); ++rank) {
    CHECK(got[rank].ground_position == expected[rank].ground_position);
    CHECK(got[rank].owner_position == expected[rank].owner_position);
    any_moved = any_moved || got[rank].owner_position.x == place.x;
  }
  CHECK(any_moved);
}
