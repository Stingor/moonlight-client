// CE QUE LE COMBAT DU CHAMP DE BATAILLE MONTRE : la marque des ennemis, le
// score à peindre, la couleur d'un camp.

#include "world/battlefield_view.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_pick.h"
#include "ro/sim/actor.h"
#include "ro/sim/map_mode.h"
#include "ro/sim/world.h"

using moonclient::BattleScoreOf;
using moonclient::BattleSideColor;
using moonclient::EnemyMarkersOnScreen;
using ro::render::BattleScoreHighlight;
using ro::sim::BattleSide;
using ro::sim::World;

namespace {

constexpr std::uint32_t kSelf = 150001;
constexpr std::uint32_t kMate = 150002;
constexpr std::uint32_t kFoe = 150003;
constexpr std::uint32_t kStranger = 150004;
// Deux camps quelconques : le plus petit est celui du lion.
constexpr std::uint16_t kLionCamp = 3;
constexpr std::uint16_t kEagleCamp = 4;

constexpr float kWidth = 1280.0F;
constexpr float kHeight = 960.0F;
constexpr float kAspect = kWidth / kHeight;
constexpr std::int32_t kMarkerWidth = 20;
constexpr std::int32_t kMarkerHeight = 12;
constexpr std::size_t kRgbaBytes = 4;
constexpr std::uint8_t kOpaqueByte = 0xFF;

[[nodiscard]] ro::sim::MapMode Battlefield() {
  ro::sim::MapMode mode;
  mode.battlefield = true;
  return mode;
}

[[nodiscard]] ro::sim::Actor Player(std::uint32_t id, std::uint32_t option = 0) {
  ro::sim::Actor actor;
  actor.id = id;
  actor.kind = ro::sim::ActorKind::kPlayer;
  actor.option = option;
  return actor;
}

// Un champ de bataille où nous sommes `own_camp`, avec un coéquipier, un
// ennemi et un joueur dont le camp n'est pas dit.
[[nodiscard]] World Field(std::uint16_t own_camp, std::uint16_t foe_camp) {
  World world;
  world.set_player_id(kSelf);
  world.TellMapMode(Battlefield());
  world.Spawn(Player(kSelf));
  world.Spawn(Player(kMate));
  world.Spawn(Player(kFoe));
  world.Spawn(Player(kStranger));
  world.battlefield().TellCamp(kSelf, own_camp, kSelf);
  world.battlefield().TellCamp(kMate, own_camp, kSelf);
  world.battlefield().TellCamp(kFoe, foe_camp, kSelf);
  return world;
}

[[nodiscard]] ro::render::BattlefieldArt Art() {
  ro::render::BattlefieldArt art;
  art.enemy_marker.width = kMarkerWidth;
  art.enemy_marker.height = kMarkerHeight;
  art.enemy_marker.pixels.assign(
      static_cast<std::size_t>(kMarkerWidth) * static_cast<std::size_t>(kMarkerHeight) * kRgbaBytes,
      kOpaqueByte);
  return art;
}

// La caméra du jeu, au recul du jeu : un pixel de sprite y vaut un pixel.
[[nodiscard]] ro::render::Camera GameCamera() {
  ro::render::Camera camera;
  camera.distance = ro::render::kGameViewDistance;
  camera.fov_degrees = ro::render::kGameFovDegrees;
  return camera;
}

// Tous les acteurs aux pieds de la caméra, au centre de l'écran.
struct Scene {
  std::vector<ro::render::DrawnActor> drawn;
  std::vector<std::uint32_t> ids;
};

[[nodiscard]] Scene AllAtCenter(std::initializer_list<std::uint32_t> ids) {
  Scene scene;
  for (const std::uint32_t id : ids) {
    scene.drawn.push_back(ro::render::DrawnActor{});
    scene.ids.push_back(id);
  }
  return scene;
}

[[nodiscard]] std::vector<ro::render::ScreenQuad> Markers(
    const World& world, const Scene& scene, const ro::render::BattlefieldArt& art,
    std::span<const moonclient::ConcealedPlace> concealed = {}, bool sees_concealed = false) {
  return EnemyMarkersOnScreen(GameCamera(), kAspect, scene.drawn, scene.ids, concealed,
                              sees_concealed, world, art, kWidth, kHeight);
}

}  // namespace

TEST_CASE("Combat du champ : hors d'un champ de bataille, aucun score", "[app][battleground]") {
  World world;
  CHECK_FALSE(BattleScoreOf(world, true).has_value());
  // ⚠ MÊME UN SCORE DIT ne se montre pas ailleurs : c'est la carte qui décide.
  world.battlefield().TellScore(2, 3);
  CHECK_FALSE(BattleScoreOf(world, true).has_value());
}

TEST_CASE("Combat du champ : rien tant que le serveur n'a pas dit le score",
          "[app][battleground]") {
  World world = Field(kLionCamp, kEagleCamp);
  CHECK_FALSE(BattleScoreOf(world, true).has_value());
  world.battlefield().TellScore(0, 0);
  const std::optional<ro::render::BattleScoreFigures> figures = BattleScoreOf(world, false);
  REQUIRE(figures.has_value());
  CHECK(figures->lion == 0);
  CHECK(figures->eagle == 0);
}

TEST_CASE("Combat du champ : le score suit le serveur, lion puis aigle", "[app][battleground]") {
  World world = Field(kLionCamp, kEagleCamp);
  world.battlefield().TellScore(7, 12);
  const std::optional<ro::render::BattleScoreFigures> figures = BattleScoreOf(world, false);
  REQUIRE(figures.has_value());
  CHECK(figures->lion == 7);
  CHECK(figures->eagle == 12);
  CHECK(figures->highlight == BattleScoreHighlight::kNone);
}

TEST_CASE("Combat du champ : le surlignage suit notre camp et le réglage", "[app][battleground]") {
  SECTION("Lion : notre camp est le plus petit") {
    World world = Field(kLionCamp, kEagleCamp);
    world.battlefield().TellScore(1, 1);
    CHECK(BattleScoreOf(world, true)->highlight == BattleScoreHighlight::kLion);
    CHECK(BattleScoreOf(world, false)->highlight == BattleScoreHighlight::kNone);
  }
  SECTION("Aigle : notre camp est le plus grand") {
    World world = Field(kEagleCamp, kLionCamp);
    world.battlefield().TellScore(1, 1);
    CHECK(BattleScoreOf(world, true)->highlight == BattleScoreHighlight::kEagle);
  }
  SECTION("Aucun adversaire vu : la règle ne tranche pas, rien n'est surligné") {
    World world;
    world.set_player_id(kSelf);
    world.TellMapMode(Battlefield());
    world.battlefield().TellCamp(kSelf, kLionCamp, kSelf);
    world.battlefield().TellScore(1, 1);
    CHECK(BattleScoreOf(world, true)->highlight == BattleScoreHighlight::kNone);
  }
}

TEST_CASE("Combat du champ : trois couleurs de camp, toutes distinctes", "[app][battleground]") {
  const ImU32 lion = BattleSideColor(BattleSide::kLion);
  const ImU32 eagle = BattleSideColor(BattleSide::kEagle);
  const ImU32 unknown = BattleSideColor(BattleSide::kUnknown);
  CHECK(lion != eagle);
  CHECK(lion != unknown);
  CHECK(eagle != unknown);
  // Opaques : un point de minimap translucide se perd sur une carte claire.
  CHECK((lion >> IM_COL32_A_SHIFT) == kOpaqueByte);
  CHECK((eagle >> IM_COL32_A_SHIFT) == kOpaqueByte);
}

TEST_CASE("Marque d'ennemi : l'ennemi seul, ni coéquipier ni camp inconnu ni soi",
          "[app][battleground]") {
  const World world = Field(kLionCamp, kEagleCamp);
  const ro::render::BattlefieldArt art = Art();
  const std::vector<ro::render::ScreenQuad> markers =
      Markers(world, AllAtCenter({kSelf, kMate, kFoe, kStranger}), art);
  REQUIRE(markers.size() == 1);
  // ⚠ L'IMAGE EST DÉSIGNÉE, PAS RECOPIÉE : la couche d'écran la reconnaît par
  // son adresse.
  CHECK(markers[0].atlas == &art.enemy_marker);
}

TEST_CASE("Marque d'ennemi : un ennemi caché, camouflé ou invisible n'en porte pas",
          "[app][battleground]") {
  const ro::render::BattlefieldArt art = Art();
  for (const std::uint32_t option :
       {ro::sim::kOptionHide, ro::sim::kOptionCloak, ro::sim::kOptionInvisible}) {
    World world = Field(kLionCamp, kEagleCamp);
    world.Spawn(Player(kFoe, option));
    CHECK(Markers(world, AllAtCenter({kFoe}), art).empty());
    // Même mis à sa place de dissimulé : il faut être administrateur.
    const std::vector<moonclient::ConcealedPlace> concealed{{.actor_id = kFoe}};
    CHECK(Markers(world, Scene{}, art, concealed, false).empty());
  }
}

TEST_CASE("Marque d'ennemi : un administrateur voit celle d'un ennemi caché, à sa place",
          "[app][battleground]") {
  const ro::render::BattlefieldArt art = Art();
  World world = Field(kLionCamp, kEagleCamp);
  world.Spawn(Player(kFoe, ro::sim::kOptionCloak));
  world.Spawn(Player(kMate, ro::sim::kOptionCloak));
  const std::vector<moonclient::ConcealedPlace> concealed{{.actor_id = kFoe}, {.actor_id = kMate}};
  // Le coéquipier caché ne se marque pas plus qu'un coéquipier visible.
  const std::vector<ro::render::ScreenQuad> markers = Markers(world, Scene{}, art, concealed, true);
  REQUIRE(markers.size() == 1);

  // La même place qu'un corps ordinaire dessiné là.
  World visible = Field(kLionCamp, kEagleCamp);
  const std::vector<ro::render::ScreenQuad> drawn = Markers(visible, AllAtCenter({kFoe}), art);
  REQUIRE(drawn.size() == 1);
  CHECK(markers[0].min.x == Catch::Approx(drawn[0].min.x));
  CHECK(markers[0].min.y == Catch::Approx(drawn[0].min.y));
}

TEST_CASE("Marque d'ennemi : rien hors d'un champ de bataille, ni sans image",
          "[app][battleground]") {
  World world = Field(kLionCamp, kEagleCamp);
  CHECK(Markers(world, AllAtCenter({kFoe}), ro::render::BattlefieldArt{}).empty());
  world.TellMapMode(ro::sim::MapMode{});
  CHECK(Markers(world, AllAtCenter({kFoe}), Art()).empty());
}

TEST_CASE("Marque d'ennemi : au-dessus de la tête, décalée selon l'écran et la taille du corps",
          "[app][battleground]") {
  const World world = Field(kLionCamp, kEagleCamp);
  const ro::render::BattlefieldArt art = Art();
  Scene scene = AllAtCenter({kFoe});
  const std::optional<glm::vec2> feet = ro::render::ProjectToScreen(
      GameCamera(), kAspect, scene.drawn[0].ground_position, kWidth, kHeight);
  REQUIRE(feet.has_value());
  // Le décalage de la marque, à cette fenêtre, pour un corps ordinaire.
  const glm::vec2 shift = ro::render::EnemyMarkerShiftPx({kWidth, kHeight});
  const glm::vec2 feet_px = glm::trunc(*feet);

  std::vector<ro::render::ScreenQuad> markers = Markers(world, scene, art);
  REQUIRE(markers.size() == 1);
  CHECK(markers[0].min.x == Catch::Approx(std::trunc(feet_px.x + shift.x)));
  CHECK(markers[0].min.y == Catch::Approx(std::trunc(feet_px.y + shift.y)));
  CHECK(markers[0].min.y < feet->y);
  CHECK(markers[0].max.x - markers[0].min.x == Catch::Approx(kMarkerWidth));
  CHECK(markers[0].max.y - markers[0].min.y == Catch::Approx(kMarkerHeight));

  // UN CORPS DEUX FOIS PLUS GRAND porte sa marque deux fois plus haut ;
  // l'image, elle, garde sa taille.
  scene.drawn[0].scale = 2.0F;
  markers = Markers(world, scene, art);
  REQUIRE(markers.size() == 1);
  CHECK(markers[0].min.y == Catch::Approx(std::trunc(feet_px.y + 2.0F * shift.y)));
  CHECK(markers[0].max.y - markers[0].min.y == Catch::Approx(kMarkerHeight));
}
