#include "world/broadcast_balloons.h"

#include <cstdint>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/world.h"

// LES BULLES D'ANNONCE, CÔTÉ ÉTAT VIVANT : la pile de chaque famille, la vie
// d'une bulle, son fondu, et l'ordre où les bandeaux les reçoivent.

using moonclient::BalloonAlpha;
using moonclient::BalloonDecor;
using moonclient::BalloonExpired;
using moonclient::BroadcastBalloons;
using moonclient::MostBalloonsOf;
using ro::sim::BalloonFamily;
using ro::sim::BalloonNotice;
using ro::ui::BroadcastBalloonLook;

namespace {

[[nodiscard]] BalloonNotice Notice(BalloonFamily family, std::string text, std::uint32_t heard_ms) {
  BalloonNotice notice{.family = family, .heard_ms = heard_ms};
  notice.lines.push_back(ro::sim::BalloonLine{.text = std::move(text), .color = 0xFFFF00});
  return notice;
}

// Dépose puis compose, comme une image de la partie.
void Post(ro::sim::World& world, BroadcastBalloons& balloons, const BroadcastBalloonLook& look,
          BalloonNotice notice) {
  world.balloon_news().Post(std::move(notice));
  balloons.Notice(world, look);
}

}  // namespace

TEST_CASE("Bulles : le plafond de la famille, et la chasse n'en garde qu'une", "[app][balloon]") {
  const BroadcastBalloonLook look;
  CHECK(MostBalloonsOf(BalloonFamily::kAnnouncement, look) == 3);
  CHECK(MostBalloonsOf(BalloonFamily::kStaffWhisper, look) == 3);
  CHECK(MostBalloonsOf(BalloonFamily::kHunt, look) == 1);
  BroadcastBalloonLook many = look;
  many.most_per_family = 5;
  CHECK(MostBalloonsOf(BalloonFamily::kAnnouncement, many) == 5);
  CHECK(MostBalloonsOf(BalloonFamily::kHunt, many) == 1);
}

TEST_CASE("Bulles : vingt secondes de vie, et le fondu sur la fin", "[app][balloon]") {
  BroadcastBalloonLook look;
  CHECK_FALSE(BalloonExpired(1000, 20999, look));
  CHECK(BalloonExpired(1000, 21000, look));
  // Pleine jusqu'au fondu, puis décroissante.
  CHECK(BalloonAlpha(0, 10000, look) == 1.0F);
  CHECK(BalloonAlpha(0, static_cast<std::uint32_t>(20000 - look.fade_ms / 2), look) == 0.5F);
  // Le client l'éteint net : sans fondu, toujours pleine.
  look.fade = false;
  CHECK(BalloonAlpha(0, 19999, look) == 1.0F);
  // Une horloge qui recule ne vieillit rien.
  CHECK_FALSE(BalloonExpired(5000, 1000, look));
}

TEST_CASE("Bulles : la plus ancienne part pour la nouvelle", "[app][balloon]") {
  const BroadcastBalloonLook look;
  ro::sim::World world;
  BroadcastBalloons balloons;
  for (std::uint32_t rank = 0; rank < 4; ++rank) {
    Post(world, balloons, look,
         Notice(BalloonFamily::kAnnouncement, "annonce " + std::to_string(rank), rank));
  }
  balloons.Compose(10, look, BalloonDecor{}, false, false);
  REQUIRE(balloons.announcements().size() == 3);
  CHECK(balloons.announcements()[0].lines[0].text == "annonce 1");
  CHECK(balloons.announcements()[2].lines[0].text == "annonce 3");
  CHECK(balloons.hunts().empty());
}

TEST_CASE("Bulles : les familles dans l'ordre, la chasse à part", "[app][balloon]") {
  const BroadcastBalloonLook look;
  ro::sim::World world;
  BroadcastBalloons balloons;
  Post(world, balloons, look, Notice(BalloonFamily::kStaffWhisper, "[ GM ] : bonjour", 0));
  Post(world, balloons, look, Notice(BalloonFamily::kAnnouncement, "annonce", 1));
  Post(world, balloons, look, Notice(BalloonFamily::kHunt, "Poring [ 1 / 5 ]", 2));
  Post(world, balloons, look, Notice(BalloonFamily::kHunt, "Poring [ 2 / 5 ]", 3));
  balloons.Compose(10, look, BalloonDecor{}, false, false);
  // ⚠ LES ANNONCES AVANT LE CHUCHOTEMENT, quel que soit l'ordre d'arrivée, et
  // l'écart s'élargit entre les deux familles.
  REQUIRE(balloons.announcements().size() == 2);
  CHECK(balloons.announcements()[0].lines[0].text == "annonce");
  CHECK_FALSE(balloons.announcements()[0].new_family);
  CHECK(balloons.announcements()[1].lines[0].text == "[ GM ] : bonjour");
  CHECK(balloons.announcements()[1].new_family);
  // La chasse remplace la précédente.
  REQUIRE(balloons.hunts().size() == 1);
  CHECK(balloons.hunts()[0].lines[0].text == "Poring [ 2 / 5 ]");
  // La couleur arrive pleine.
  CHECK(balloons.hunts()[0].lines[0].color == IM_COL32(0xFF, 0xFF, 0x00, 0xFF));
}

TEST_CASE("Bulles : éteintes, elles ne se gardent pas et la file se vide", "[app][balloon]") {
  BroadcastBalloonLook look;
  look.shown = false;
  ro::sim::World world;
  BroadcastBalloons balloons;
  Post(world, balloons, look, Notice(BalloonFamily::kAnnouncement, "annonce", 0));
  CHECK(world.balloon_news().Take().empty());
  balloons.Compose(10, look, BalloonDecor{}, false, false);
  CHECK(balloons.announcements().empty());
}

TEST_CASE("Bulles : une ancre déverrouillée montre un exemple sur un bandeau vide",
          "[app][balloon]") {
  const BroadcastBalloonLook look;
  ro::sim::World world;
  BroadcastBalloons balloons;
  balloons.Compose(0, look, BalloonDecor{}, true, true);
  CHECK(balloons.announcements().size() == 1);
  CHECK(balloons.hunts().size() == 1);
  // Une vraie bulle prend la place de l'exemple.
  Post(world, balloons, look, Notice(BalloonFamily::kAnnouncement, "annonce", 0));
  balloons.Compose(0, look, BalloonDecor{}, true, false);
  REQUIRE(balloons.announcements().size() == 1);
  CHECK(balloons.announcements()[0].lines[0].text == "annonce");
  CHECK(balloons.hunts().empty());
}

TEST_CASE("Bulles : une bulle éteinte quitte la pile", "[app][balloon]") {
  const BroadcastBalloonLook look;
  ro::sim::World world;
  BroadcastBalloons balloons;
  Post(world, balloons, look, Notice(BalloonFamily::kAnnouncement, "annonce", 0));
  balloons.Compose(19999, look, BalloonDecor{}, false, false);
  CHECK(balloons.announcements().size() == 1);
  balloons.Compose(20000, look, BalloonDecor{}, false, false);
  CHECK(balloons.announcements().empty());
}
