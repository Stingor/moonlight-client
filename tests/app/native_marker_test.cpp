// La flèche de sélection : ce qu'elle fait à chaque image, au-dessus de quoi,
// et où exactement. Hors du jeu, rien d'autre ne la voit.
//
// 🔴 « GARDER » EST UN ORDRE À PART ENTIÈRE, et le plus facile à perdre : le
// confondre avec « cacher » éteindrait la flèche d'une cible qui sort de la vue,
// là où le jeu la laisse figée en l'air à sa dernière place.

#include "world/native_marker.h"

#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "ro/sim/world.h"

#include "world/mouse_engagement.h"

using Catch::Matchers::WithinAbs;
using moonclient::ApplyMarker;
using moonclient::MarkerCall;
using moonclient::MarkerCell;
using moonclient::MarkerOrder;
using moonclient::MarkerOver;
using moonclient::MarkerPose;
using moonclient::MouseEngagement;
using moonclient::NativeMarkerCall;
using moonclient::UnderTargetingMode;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::FractionalCell;
using ro::sim::World;

namespace {

constexpr std::uint32_t kSelf = 150001;
constexpr std::uint32_t kWolf = 2000123;
constexpr std::uint32_t kRogue = 150002;
constexpr std::uint32_t kPortal = 110001;
constexpr std::uint32_t kGone = 2000999;

// Une grille paire, puis une impaire : le centre tombe sur un bord de cellule,
// ou au milieu d'une.
constexpr int kEvenCells = 400;
constexpr int kOddCells = 301;
constexpr float kCellUnits = 5.0F;
// Bien sous le pas d'une unité, qui vaut un cinquième de cellule.
constexpr double kCellTolerance = 1e-3;

[[nodiscard]] Actor Spawned(std::uint32_t id, ActorKind kind, bool player_sprite) {
  Actor actor;
  actor.id = id;
  actor.kind = kind;
  actor.player_sprite = player_sprite;
  return actor;
}

[[nodiscard]] World Peopled() {
  World world;
  world.Spawn(Spawned(kSelf, ActorKind::kPlayer, true));
  world.set_player_id(kSelf);
  world.Spawn(Spawned(kWolf, ActorKind::kMonster, false));
  world.Spawn(Spawned(kRogue, ActorKind::kPlayer, true));
  Actor portal = Spawned(kPortal, ActorKind::kNonPlayer, false);
  portal.look.job = ro::sim::kPortalJob;
  world.Spawn(portal);
  return world;
}

[[nodiscard]] MouseEngagement EngagedOn(std::uint32_t id) {
  MouseEngagement engagement;
  engagement.engaged = true;
  engagement.clicked_id = id;
  return engagement;
}

}  // namespace

TEST_CASE("Flèche : non engagée, elle se cache", "[app][fleche]") {
  const World world = Peopled();
  MouseEngagement engagement = EngagedOn(kWolf);
  engagement.engaged = false;
  CHECK(NativeMarkerCall(engagement, world).order == MarkerOrder::kHide);
}

TEST_CASE("Flèche : une entité introuvable la laisse telle quelle", "[app][fleche]") {
  const World world = Peopled();
  CHECK(NativeMarkerCall(EngagedOn(kGone), world).order == MarkerOrder::kKeep);
  // Le personnage lui-même n'est jamais trouvé : se viser ne la déplace pas.
  CHECK(NativeMarkerCall(EngagedOn(kSelf), world).order == MarkerOrder::kKeep);
  CHECK(NativeMarkerCall(EngagedOn(0), world).order == MarkerOrder::kKeep);

  // Et garder, c'est vraiment ne rien toucher : ni la place, ni la visibilité.
  MarkerPose pose;
  ApplyMarker(pose, MarkerOrder::kShow, {1.0F, 2.0F, 3.0F});
  ApplyMarker(pose, MarkerOrder::kKeep, {9.0F, 9.0F, 9.0F});
  CHECK(pose.shown);
  CHECK(pose.ground == std::array<float, 3>{1.0F, 2.0F, 3.0F});
  ApplyMarker(pose, MarkerOrder::kHide, {9.0F, 9.0F, 9.0F});
  CHECK_FALSE(pose.shown);
}

TEST_CASE("Flèche : un portail la refuse, quelle que soit son espèce", "[app][fleche]") {
  const World world = Peopled();
  CHECK(NativeMarkerCall(EngagedOn(kPortal), world).order == MarkerOrder::kHide);
  Actor odd = Spawned(kWolf, ActorKind::kMonster, false);
  odd.look.job = ro::sim::kPortalJob;
  CHECK(MarkerOver(odd).order == MarkerOrder::kHide);
}

TEST_CASE("Flèche : seul un sprite de joueur caché la refuse", "[app][fleche]") {
  // 🔴 UN MONSTRE CACHÉ LA GARDE, et un joueur caché la perd : c'est le type
  // d'objet du paquet d'apparition qui décide, pas l'espèce.
  Actor wolf = Spawned(kWolf, ActorKind::kMonster, false);
  wolf.option = ro::sim::kOptionCloak;
  const MarkerCall over_wolf = MarkerOver(wolf);
  CHECK(over_wolf.order == MarkerOrder::kShow);
  CHECK(over_wolf.on == &wolf);

  Actor rogue = Spawned(kRogue, ActorKind::kPlayer, true);
  for (const std::uint32_t option :
       {ro::sim::kOptionHide, ro::sim::kOptionCloak, ro::sim::kOptionInvisible}) {
    rogue.option = option;
    CHECK(MarkerOver(rogue).order == MarkerOrder::kHide);
  }
  rogue.option = 0;
  CHECK(MarkerOver(rogue).order == MarkerOrder::kShow);

  // Un joueur déguisé en autre chose est annoncé sans sprite de joueur : caché,
  // il la garde.
  Actor disguised = Spawned(kRogue, ActorKind::kPlayer, false);
  disguised.option = ro::sim::kOptionHide;
  CHECK(MarkerOver(disguised).order == MarkerOrder::kShow);
}

TEST_CASE("Flèche : le staff ne la garde pas sur un joueur @hide", "[app][fleche]") {
  // Le droit de groupe le plus haut ne voit pas à travers : le serveur annonce
  // l'option à tous, et le jeu n'en fait d'exception pour personne.
  constexpr std::uint8_t kHighestGroupLevel = 99;
  World world = Peopled();
  world.session().group_level = kHighestGroupLevel;
  Actor hidden_staff = Spawned(kRogue, ActorKind::kPlayer, true);
  hidden_staff.option = ro::sim::kOptionInvisible;
  world.Spawn(hidden_staff);
  CHECK(NativeMarkerCall(EngagedOn(kRogue), world).order == MarkerOrder::kHide);
}

TEST_CASE("Flèche : un cadavre la garde", "[app][fleche]") {
  World world = Peopled();
  REQUIRE(world.SetPosture(kWolf, ro::sim::Posture::kDead, 0));
  const MarkerCall call = NativeMarkerCall(EngagedOn(kWolf), world);
  CHECK(call.order == MarkerOrder::kShow);
  REQUIRE(call.on != nullptr);
  CHECK(call.on->id == kWolf);
}

TEST_CASE("Flèche : le suivi passe avant l'engagement", "[app][fleche][suivi]") {
  const World world = Peopled();
  // Désengagée, la flèche se pose quand même sur le joueur suivi.
  MouseEngagement idle;
  const MarkerCall on_rogue = NativeMarkerCall(idle, world, kRogue);
  CHECK(on_rogue.order == MarkerOrder::kShow);
  REQUIRE(on_rogue.on != nullptr);
  CHECK(on_rogue.on->id == kRogue);
  // Et un monstre engagé ne la lui reprend pas.
  const MarkerCall engaged_elsewhere = NativeMarkerCall(EngagedOn(kWolf), world, kRogue);
  REQUIRE(engaged_elsewhere.on != nullptr);
  CHECK(engaged_elsewhere.on->id == kRogue);
}

TEST_CASE("Flèche : le joueur suivi hors de vue la laisse telle quelle", "[app][fleche][suivi]") {
  const World world = Peopled();
  CHECK(NativeMarkerCall(MouseEngagement{}, world, kGone).order == MarkerOrder::kKeep);
  // Un acteur de cet identifiant qui n'est pas un joueur ne compte pas.
  CHECK(NativeMarkerCall(EngagedOn(kWolf), world, kWolf).order == MarkerOrder::kKeep);
}

TEST_CASE("Flèche : le joueur suivi caché la perd, comme un engagé", "[app][fleche][suivi]") {
  World world;
  Actor rogue = Spawned(kRogue, ActorKind::kPlayer, true);
  rogue.option = ro::sim::kOptionHide;
  world.Spawn(rogue);
  CHECK(NativeMarkerCall(MouseEngagement{}, world, kRogue).order == MarkerOrder::kHide);
}

TEST_CASE("Flèche : le mode Ciblage ne réécrit que le masquage", "[app][fleche]") {
  const Actor hud = Spawned(kRogue, ActorKind::kPlayer, true);
  const Actor wolf = Spawned(kWolf, ActorKind::kMonster, false);

  const MarkerCall hidden{MarkerOrder::kHide, nullptr};
  const MarkerCall rewritten = UnderTargetingMode(hidden, &hud);
  CHECK(rewritten.order == MarkerOrder::kShow);
  CHECK(rewritten.on == &hud);
  // Sans cible à montrer, le masquage reste un masquage.
  CHECK(UnderTargetingMode(hidden, nullptr).order == MarkerOrder::kHide);

  // L'entité engagée garde la flèche, même quand le HUD en suit une autre.
  const MarkerCall shown{MarkerOrder::kShow, &wolf};
  CHECK(UnderTargetingMode(shown, &hud).on == &wolf);
  // Et garder reste garder : aucun ordre ne part, rien ne se réécrit.
  CHECK(UnderTargetingMode(MarkerCall{}, &hud).order == MarkerOrder::kKeep);
}

TEST_CASE("Flèche : la cellule est tronquée vers le centre de la carte", "[app][fleche]") {
  // 🔴 UNE ENTITÉ ARRÊTÉE SE TIENT AU CENTRE DE SA CELLULE, et sur une grille
  // paire ce centre tombe à une demi-unité d'une unité ronde : la flèche se pose
  // un dixième de cellule plus près du centre de la carte. Vers le bas à droite
  // du centre, vers le haut à gauche.
  const FractionalCell east = MarkerCell({250.0F, 250.0F}, kEvenCells, kEvenCells, kCellUnits);
  CHECK_THAT(east.x, WithinAbs(249.9, kCellTolerance));
  CHECK_THAT(east.y, WithinAbs(249.9, kCellTolerance));
  const FractionalCell west = MarkerCell({150.0F, 150.0F}, kEvenCells, kEvenCells, kCellUnits);
  CHECK_THAT(west.x, WithinAbs(150.1, kCellTolerance));
  CHECK_THAT(west.y, WithinAbs(150.1, kCellTolerance));
  // Les deux axes se tronquent chacun de leur côté du centre.
  const FractionalCell mixed = MarkerCell({250.0F, 150.0F}, kEvenCells, kEvenCells, kCellUnits);
  CHECK_THAT(mixed.x, WithinAbs(249.9, kCellTolerance));
  CHECK_THAT(mixed.y, WithinAbs(150.1, kCellTolerance));

  // En marche, elle avance par pas d'une unité, un cinquième de cellule.
  const FractionalCell walking = MarkerCell({250.35F, 150.35F}, kEvenCells, kEvenCells, kCellUnits);
  CHECK_THAT(walking.x, WithinAbs(250.3, kCellTolerance));
  CHECK_THAT(walking.y, WithinAbs(150.5, kCellTolerance));

  // Sur une grille impaire, le centre de la carte est celui d'une cellule : une
  // entité arrêtée tombe sur une unité ronde, et la flèche ne bouge pas.
  const FractionalCell odd = MarkerCell({200.0F, 100.0F}, kOddCells, kOddCells, kCellUnits);
  CHECK_THAT(odd.x, WithinAbs(200.0, kCellTolerance));
  CHECK_THAT(odd.y, WithinAbs(100.0, kCellTolerance));
}
