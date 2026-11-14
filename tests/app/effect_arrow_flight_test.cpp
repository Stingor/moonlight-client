// LA FLÈCHE D'UN ARC ET LA MUNITION D'UN MONSTRE, telles qu'elles se tirent et
// se peignent : un sprite d'acteur, opaque dès sa naissance, au mélange
// ordinaire, tourné à chaque image de la face de son vol vue de la caméra — la
// pointe de l'image, en bas, menant —, qui vole en ligne droite jusqu'à sa
// cible, hauteur comprise, et fait défiler ses images selon sa classe.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <numbers>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/sprite_renderer.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "support/client_vfs.h"
#include "support/grf_builder.h"
#include "support/sprite_builder.h"
#include "support/temp_directory.h"

using Catch::Approx;
using ro::render::ProjectileImageRule;
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

// Une image de 4 sur 6, comme la flèche du jeu : plus haute que large.
constexpr std::int32_t kArrowWidthPixels = 4;
constexpr std::int32_t kArrowHeightPixels = 6;
constexpr std::uint32_t kFirstFrameMs = 100;
// Un tic d'effet, en millisecondes : chaque image de plus en fait battre un.
constexpr auto kTickMs = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
// Le vol entier : huit pas de vingt-quatre millisecondes, en tics d'effet.
constexpr std::int32_t kFlightTicks = 12;
// Une vie assez longue pour voir l'avance des images jusqu'au bout.
constexpr std::int32_t kLongFlightTicks = 20;
// Deux bouts à la hauteur d'envol, le second au fond de l'écran de la caméra
// par défaut : quarante unités vers −z.
constexpr glm::vec3 kShooter{0.0F, 10.0F, 0.0F};
constexpr glm::vec3 kTargetFar{0.0F, 10.0F, -40.0F};
// Vers le joueur : la face zéro, où l'angle d'écran est le lacet seul.
constexpr glm::vec3 kTargetNear{0.0F, 10.0F, 40.0F};
// Une cible vingt unités plus haut que le tireur, au fond de l'écran.
constexpr glm::vec3 kTargetUphill{0.0F, 30.0F, -40.0F};

// Un projectile du client : cette règle d'images, à son horloge, sans angle ajouté.
ro::render::ProjectileFlight Flight(ProjectileImageRule rule) {
  return ro::render::ProjectileFlight{.image_rule = rule};
}

// Le trait de la baguette : voir `ro::render::ShootsWandBolts`.
constexpr ro::render::ProjectileFlight kWandBolt{.image_rule = ProjectileImageRule::kArrow,
                                                 .clock_slowdown = ro::render::kWandBoltSlowdown,
                                                 .turn_degrees = ro::render::kWandBoltTurnDegrees};

// `images` images pleines, la k-ième faite de l'entrée k+1 de la palette.
std::vector<std::uint8_t> ArrowSpr(std::size_t images) {
  SprBuildOptions options;
  options.palette.push_back(SpriteColor{});
  for (std::size_t image = 0; image < images; ++image) {
    options.indexed.push_back(SprIndexedFrameSpec{
        kArrowWidthPixels, kArrowHeightPixels,
        std::vector<std::uint8_t>(static_cast<std::size_t>(kArrowWidthPixels * kArrowHeightPixels),
                                  static_cast<std::uint8_t>(image + 1))});
    options.palette.push_back(SpriteColor{.red = 100, .green = 48, .blue = 54});
  }
  return BuildSpr(options);
}

// Une action de `images` images, chacune sur un calque qui montre l'image de
// son rang.
std::vector<std::uint8_t> ArrowAct(std::size_t images) {
  ActBuildOptions options;
  ActActionSpec action;
  for (std::size_t image = 0; image < images; ++image) {
    ActLayerSpec layer;
    layer.frame_index = static_cast<std::int32_t>(image);
    action.frames.push_back(ActFrameSpec{{layer}, {}, -1});
  }
  options.actions.push_back(action);
  options.speeds = {4.0F};
  return BuildAct(options);
}

void FillArcherClient(const TempDirectory& client, std::size_t images = 1) {
  const std::string base(ro::render::kArrowSpriteBase);
  client.WriteFile("fleches.grf", BuildGrf({GrfFileSpec{Cp949(base + ".spr"), ArrowSpr(images)},
                                            GrfFileSpec{Cp949(base + ".act"), ArrowAct(images)}}));
}

// DU MILIEU DU BORD HAUT DE L'IMAGE AU MILIEU DE SON BORD BAS, en pixels de
// sprite, l'ordonnée vers le bas : là où la pointe de la flèche se retrouve.
glm::vec2 HeadVector(const ro::render::SpriteQuad& quad) {
  const glm::vec2 tail = (quad.corners[0] + quad.corners[1]) * 0.5F;
  const glm::vec2 head = (quad.corners[2] + quad.corners[3]) * 0.5F;
  return head - tail;
}

// L'ANGLE D'ÉCRAN DE LA POINTE, en degrés dans le sens des aiguilles d'une
// montre depuis le bas de l'image : celui dont le quad a été tourné.
float HeadAngleDegrees(const ro::render::SpriteQuad& quad) {
  const glm::vec2 head = HeadVector(quad);
  return std::atan2(-head.x, head.y) * 180.0F / std::numbers::pi_v<float>;
}

ro::render::Camera GameCameraTurnedBy(float yaw_degrees) {
  ro::render::Camera camera;
  camera.yaw_degrees = yaw_degrees;
  camera.pitch_degrees = ro::render::kGameElevationDegrees;
  return camera;
}

// L'UNIQUE QUAD DE L'UNIQUE ACTEUR DESSINÉ à cet instant, devant cette caméra.
const ro::render::SpriteQuad& OnlyQuadDrawnAt(moonclient::EffectNodesInPlay& nodes,
                                              std::uint32_t now_ms,
                                              const ro::render::Camera& camera) {
  const std::span<const ro::render::DrawnActor> drawn = nodes.Drawn(now_ms, camera);
  REQUIRE(drawn.size() == 1);
  REQUIRE(drawn.front().actor != nullptr);
  REQUIRE(drawn.front().actor->quads.size() == 1);
  return drawn.front().actor->quads.front();
}

// L'IMAGE QUE LE PROJECTILE MONTRE à cet instant.
std::size_t ImageShownAt(moonclient::EffectNodesInPlay& nodes, std::uint32_t now_ms) {
  return static_cast<std::size_t>(
      OnlyQuadDrawnAt(nodes, now_ms, GameCameraTurnedBy(0.0F)).image_index);
}

}  // namespace

TEST_CASE("la flèche d'un arc part opaque, au mot du dessinateur des acteurs",
          "[effect_arrow_flight]") {
  const TempDirectory client;
  FillArcherClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"fleches.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.ShootSprite(ro::render::kArrowSpriteBase, kShooter, kTargetFar, kFlightTicks,
                            Flight(ProjectileImageRule::kArrow)));
  REQUIRE(nodes.living_nodes().size() == 1);
  const ro::render::PooledEffectNode& arrow = nodes.living_nodes().front();

  // Elle tourne de la face de son vol, et non de sa projection.
  CHECK(arrow.turns_with_flight_heading);
  CHECK_FALSE(arrow.turns_with_flight);
  // Un sprite d'acteur : mélange ordinaire, épreuve de profondeur, sous les .str.
  REQUIRE(arrow.node.render_word.has_value());
  CHECK(*arrow.node.render_word == ro::render::kEffectRenderWordOfActorSprite);
  // Et pleine dès sa naissance : pas de montée d'alpha.
  CHECK(arrow.node.alpha == Approx(ro::render::kEffectNodeChannelMax));

  const std::span<const ro::render::DrawnActor> drawn =
      nodes.Drawn(kFirstFrameMs, GameCameraTurnedBy(0.0F));
  REQUIRE(drawn.size() == 1);
  std::size_t batches_holding_it = 0;
  for (const moonclient::EffectNodesInPlay::SpriteNodeBatch& batch : nodes.SpriteBatches()) {
    if (batch.actors.empty()) {
      continue;
    }
    ++batches_holding_it;
    CHECK_FALSE(batch.additive);
    CHECK(batch.depth_tested);
    CHECK(batch.paints_before_str);
  }
  CHECK(batches_holding_it == 1);
}

TEST_CASE("la flèche d'un arc vole la pointe en avant, et suit la caméra qui tourne",
          "[effect_arrow_flight]") {
  const TempDirectory client;
  FillArcherClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"fleches.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.ShootSprite(ro::render::kArrowSpriteBase, kShooter, kTargetFar, kFlightTicks,
                            Flight(ProjectileImageRule::kArrow)));

  // 🔴 L'IMAGE PORTE SA POINTE EN BAS. Devant la caméra par défaut, un vol vers
  // le fond de l'écran la retourne : le bord bas de l'image se retrouve en
  // haut. Une image supposée pointer vers le haut ferait voler la flèche
  // empennage en avant.
  const glm::vec2 toward_far =
      HeadVector(OnlyQuadDrawnAt(nodes, kFirstFrameMs, GameCameraTurnedBy(0.0F)));
  CHECK(toward_far.y < 0.0F);
  CHECK(std::abs(toward_far.x) < 1e-3F);
  CHECK(glm::length(toward_far) == Approx(static_cast<float>(kArrowHeightPixels)));

  // LA CAMÉRA TOURNE D'UN QUART DE TOUR : le fond de l'écran passe à droite, et
  // la flèche, recomposée à cette image, pointe à droite avec lui.
  const glm::vec2 toward_right =
      HeadVector(OnlyQuadDrawnAt(nodes, kFirstFrameMs + 1, GameCameraTurnedBy(90.0F)));
  CHECK(toward_right.x > 0.0F);
  CHECK(std::abs(toward_right.y) < 1e-3F);
}

TEST_CASE(
    "la flèche à plusieurs images tient la première huit tics, puis avance jusqu'à la "
    "dernière",
    "[effect_arrow_flight]") {
  // 🔴 LA CLASSE DE LA FLÈCHE compte ses mises à jour : huit sur la première
  // image, puis une image par mise à jour, figée sur la dernière. Le boulet du
  // Stone Shooter et la pêche de l'arbre sont de cette classe.
  constexpr std::size_t kImages = 4;
  const TempDirectory client;
  FillArcherClient(client, kImages);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"fleches.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.ShootSprite(ro::render::kArrowSpriteBase, kShooter, kTargetFar, kLongFlightTicks,
                            Flight(ProjectileImageRule::kArrow)));
  REQUIRE(nodes.living_nodes().front().node.texture_count == kImages);

  // La première image sans tic, puis huit tics durant.
  CHECK(ImageShownAt(nodes, kFirstFrameMs) == 0);
  for (std::uint32_t tick = 1; tick <= 8; ++tick) {
    CHECK(ImageShownAt(nodes, kFirstFrameMs + kTickMs * tick) == 0);
  }
  // Puis une image par tic, jusqu'à la dernière, qui tient.
  CHECK(ImageShownAt(nodes, kFirstFrameMs + kTickMs * 9) == 1);
  CHECK(ImageShownAt(nodes, kFirstFrameMs + kTickMs * 10) == 2);
  CHECK(ImageShownAt(nodes, kFirstFrameMs + kTickMs * 11) == 3);
  CHECK(ImageShownAt(nodes, kFirstFrameMs + kTickMs * 12) == 3);
  CHECK(ImageShownAt(nodes, kFirstFrameMs + kTickMs * 13) == 3);
  // Et la flèche vit toujours : la dernière image ne la tue pas.
  CHECK(nodes.living_nodes().size() == 1);
}

TEST_CASE("la munition d'un monstre change d'image à chaque tic, en boucle",
          "[effect_arrow_flight]") {
  // 🔴 LA CLASSE DE LA MUNITION EN SPRITE avance son image à chaque mise à jour,
  // dès la première, et repart à la première après la dernière : c'est ce qui
  // fait tourner le boulet de canon en vol.
  constexpr std::size_t kImages = 3;
  const TempDirectory client;
  FillArcherClient(client, kImages);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"fleches.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.ShootSprite(ro::render::kArrowSpriteBase, kShooter, kTargetFar, kLongFlightTicks,
                            Flight(ProjectileImageRule::kSpriteBullet)));

  CHECK(ImageShownAt(nodes, kFirstFrameMs) == 0);
  CHECK(ImageShownAt(nodes, kFirstFrameMs + kTickMs * 1) == 1);
  CHECK(ImageShownAt(nodes, kFirstFrameMs + kTickMs * 2) == 2);
  CHECK(ImageShownAt(nodes, kFirstFrameMs + kTickMs * 3) == 0);
  CHECK(ImageShownAt(nodes, kFirstFrameMs + kTickMs * 4) == 1);
  CHECK(nodes.living_nodes().size() == 1);
}

TEST_CASE("la munition d'un monstre tourne en degrés réels, la flèche au degré",
          "[effect_arrow_flight]") {
  // Un vol vers le joueur a la face zéro : l'angle d'écran est le lacet seul.
  // Le dessinateur des acteurs le tronque au degré pour la flèche ; celui des
  // particules le prend tel quel pour la munition.
  constexpr float kHalfDegreeYaw = 10.5F;
  constexpr float kTableMarginDegrees = 0.05F;
  const TempDirectory client;
  FillArcherClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"fleches.grf"}});
  REQUIRE(vfs.has_value());

  moonclient::EffectNodesInPlay arrows(*vfs);
  REQUIRE(arrows.ShootSprite(ro::render::kArrowSpriteBase, kShooter, kTargetNear, kFlightTicks,
                             Flight(ProjectileImageRule::kArrow)));
  CHECK_FALSE(arrows.living_nodes().front().node.spins_by_exact_degrees);
  CHECK(HeadAngleDegrees(
            OnlyQuadDrawnAt(arrows, kFirstFrameMs, GameCameraTurnedBy(kHalfDegreeYaw))) ==
        Approx(10.0F).margin(kTableMarginDegrees));

  moonclient::EffectNodesInPlay bullets(*vfs);
  REQUIRE(bullets.ShootSprite(ro::render::kArrowSpriteBase, kShooter, kTargetNear, kFlightTicks,
                              Flight(ProjectileImageRule::kSpriteBullet)));
  CHECK(bullets.living_nodes().front().node.spins_by_exact_degrees);
  CHECK(HeadAngleDegrees(
            OnlyQuadDrawnAt(bullets, kFirstFrameMs, GameCameraTurnedBy(kHalfDegreeYaw))) ==
        Approx(kHalfDegreeYaw).margin(kTableMarginDegrees));
}

TEST_CASE("le projectile vole en ligne droite jusqu'à sa cible, hauteur comprise",
          "[effect_arrow_flight]") {
  // 🔴 LE CLIENT MÈNE LE VOL DES PIEDS DU TIREUR AUX PIEDS DE LA CIBLE en trois
  // dimensions : vers une cible vingt unités plus haut, la flèche monte de
  // vingt unités en douze tics. À plat, elle arrivait sous sa cible.
  const TempDirectory client;
  FillArcherClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"fleches.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.ShootSprite(ro::render::kArrowSpriteBase, kShooter, kTargetUphill, kFlightTicks,
                            Flight(ProjectileImageRule::kArrow)));
  static_cast<void>(nodes.Drawn(kFirstFrameMs, GameCameraTurnedBy(0.0F)));
  constexpr std::uint32_t kHalfWayTicks = 6;
  for (std::uint32_t tick = 1; tick <= kHalfWayTicks; ++tick) {
    static_cast<void>(nodes.Drawn(kFirstFrameMs + kTickMs * tick, GameCameraTurnedBy(0.0F)));
  }
  REQUIRE(nodes.living_nodes().size() == 1);
  const glm::vec3 half_way = nodes.living_nodes().front().ground_position;
  CHECK(half_way.y == Approx(20.0F).margin(0.01F));
  CHECK(half_way.z == Approx(-20.0F).margin(0.01F));
  CHECK(half_way.x == Approx(0.0F).margin(0.01F));
  // Et la hauteur ne change rien à la face : la pointe mène toujours vers le
  // fond de l'écran.
  const glm::vec2 toward_far = HeadVector(OnlyQuadDrawnAt(
      nodes, kFirstFrameMs + kTickMs * (kHalfWayTicks + 1), GameCameraTurnedBy(0.0F)));
  CHECK(toward_far.y < 0.0F);
  CHECK(std::abs(toward_far.x) < 1e-3F);
}

TEST_CASE("le trait d'une baguette vole trois fois plus lentement, images comprises",
          "[effect_arrow_flight]") {
  // ⚠ ÉCART VOULU AU CLIENT D'ORIGINE : l'horloge du trait
  // est divisée par trois. Le vol dure trois fois plus, sur le même trajet, et
  // chaque tenue d'image avec lui : vingt-quatre tics sur la première, puis
  // trois par image, figée sur la dernière.
  constexpr std::size_t kImages = 4;
  constexpr std::int32_t kSlowdown = ro::render::kWandBoltSlowdown;
  const TempDirectory client;
  FillArcherClient(client, kImages);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"fleches.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.ShootSprite(ro::render::kArrowSpriteBase, kShooter, kTargetFar, kFlightTicks,
                            kWandBolt));
  REQUIRE(nodes.living_nodes().size() == 1);
  CHECK(nodes.living_nodes().front().node.life_ticks == kFlightTicks * kSlowdown);

  constexpr auto kFirstHoldTicks =
      static_cast<std::uint32_t>(ro::render::kArrowFirstImageHoldTicks * kSlowdown);
  CHECK(ImageShownAt(nodes, kFirstFrameMs) == 0);
  for (std::uint32_t tick = 1; tick <= kFirstHoldTicks; ++tick) {
    CHECK(ImageShownAt(nodes, kFirstFrameMs + kTickMs * tick) == 0);
  }
  CHECK(ImageShownAt(nodes, kFirstFrameMs + kTickMs * (kFirstHoldTicks + 1)) == 1);
  CHECK(ImageShownAt(nodes, kFirstFrameMs + kTickMs * (kFirstHoldTicks + 3)) == 1);
  CHECK(ImageShownAt(nodes, kFirstFrameMs + kTickMs * (kFirstHoldTicks + 4)) == 2);
  CHECK(ImageShownAt(nodes, kFirstFrameMs + kTickMs * (kFirstHoldTicks + 7)) == 3);
  CHECK(ImageShownAt(nodes, kFirstFrameMs + kTickMs * (kFirstHoldTicks + 10)) == 3);
  CHECK(nodes.living_nodes().size() == 1);
}

TEST_CASE("le trait d'une baguette arrive à mi-chemin à la moitié de son vol ralenti",
          "[effect_arrow_flight]") {
  const TempDirectory client;
  FillArcherClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"fleches.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.ShootSprite(ro::render::kArrowSpriteBase, kShooter, kTargetFar, kFlightTicks,
                            kWandBolt));
  static_cast<void>(nodes.Drawn(kFirstFrameMs, GameCameraTurnedBy(0.0F)));
  constexpr auto kHalfWayTicks =
      static_cast<std::uint32_t>(kFlightTicks * ro::render::kWandBoltSlowdown / 2);
  for (std::uint32_t tick = 1; tick <= kHalfWayTicks; ++tick) {
    static_cast<void>(nodes.Drawn(kFirstFrameMs + kTickMs * tick, GameCameraTurnedBy(0.0F)));
  }
  REQUIRE(nodes.living_nodes().size() == 1);
  const glm::vec3 half_way = nodes.living_nodes().front().ground_position;
  CHECK(half_way.y == Approx(10.0F).margin(0.01F));
  CHECK(half_way.z == Approx(-20.0F).margin(0.01F));
  CHECK(half_way.x == Approx(0.0F).margin(0.01F));
}

TEST_CASE("le trait d'une baguette tourne de trois quarts de tour de plus que la flèche",
          "[effect_arrow_flight]") {
  // Son `.act` n'est pas dessiné sur l'axe de la flèche : à toute face de vol
  // et toute caméra, son image tourne de 270 degrés de plus.
  constexpr float kTableMarginDegrees = 0.05F;
  constexpr float kFullTurn = 360.0F;
  const TempDirectory client;
  FillArcherClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"fleches.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay arrows(*vfs);
  REQUIRE(arrows.ShootSprite(ro::render::kArrowSpriteBase, kShooter, kTargetNear, kLongFlightTicks,
                             Flight(ProjectileImageRule::kArrow)));
  moonclient::EffectNodesInPlay bolts(*vfs);
  REQUIRE(bolts.ShootSprite(ro::render::kArrowSpriteBase, kShooter, kTargetNear, kLongFlightTicks,
                            kWandBolt));
  CHECK(bolts.living_nodes().front().flight_heading_turn_degrees == 270);
  CHECK(arrows.living_nodes().front().flight_heading_turn_degrees == 0);

  std::uint32_t now_ms = kFirstFrameMs;
  for (const float yaw_degrees : {0.0F, 10.5F, 90.0F, 200.0F}) {
    CAPTURE(yaw_degrees);
    const float arrow_degrees =
        HeadAngleDegrees(OnlyQuadDrawnAt(arrows, now_ms, GameCameraTurnedBy(yaw_degrees)));
    const float bolt_degrees =
        HeadAngleDegrees(OnlyQuadDrawnAt(bolts, now_ms, GameCameraTurnedBy(yaw_degrees)));
    const float turned = std::fmod(bolt_degrees - arrow_degrees + 2.0F * kFullTurn, kFullTurn);
    CHECK(turned == Approx(270.0F).margin(kTableMarginDegrees));
    now_ms += kTickMs;
  }
}
