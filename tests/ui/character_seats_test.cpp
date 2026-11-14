#include "ro/ui/character_seats.h"

#include <set>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using ro::ui::kBanquetSeats;
using ro::ui::kBenchSeats;
using ro::ui::kHonourSeats;
using ro::ui::kSeatCount;
using ro::ui::SeatedSlot;
using ro::ui::SeatFrameAt;
using ro::ui::SeatPages;

TEST_CASE("Banquet : sept ordonnées distinctes, et non cinq", "[ui][seats]") {
  // 🔴 C'EST EXACTEMENT CE QU'UNE TABLE « RANGÉE » EFFACE. Le petit trône est
  // neuf millièmes de hauteur SOUS le grand, et le bout droit deux millièmes
  // sous le bout gauche : à l'œil, deux paires qui devraient s'aligner. Les
  // aligner « proprement » donnerait une tablée qui ressemble à la bonne
  // sans être la même, et la perspective du décor demande cet écart.
  std::set<float> ordinates;
  for (const auto& seat : kBanquetSeats) {
    ordinates.insert(seat.y);
  }
  CHECK(ordinates.size() == 7);

  // Les deux paires qui ne s'alignent PAS, nommément.
  CHECK(kBanquetSeats[1].y > kBanquetSeats[0].y);    // petit trône sous le grand
  CHECK(kBanquetSeats[15].y > kBanquetSeats[14].y);  // bout droit sous le gauche
}

TEST_CASE("Banquet : les sièges d'honneur ne se paginent pas", "[ui][seats]") {
  // Ils portent les personnages 0 à 4 sur TOUTES les pages. Les paginer aussi
  // ferait disparaître le personnage principal dès qu'on tourne la page.
  for (int page = 0; page < 4; ++page) {
    for (int seat = 0; seat < kHonourSeats; ++seat) {
      CHECK(SeatedSlot(seat, page) == seat);
    }
  }
}

TEST_CASE("Banquet : les bancs défilent par vingt", "[ui][seats]") {
  // Page 0 : bancs = personnages 5 à 24. Page 1 : 25 à 44.
  CHECK(SeatedSlot(kHonourSeats, 0) == 5);
  CHECK(SeatedSlot(kSeatCount - 1, 0) == 24);
  CHECK(SeatedSlot(kHonourSeats, 1) == 25);
  CHECK(SeatedSlot(kSeatCount - 1, 1) == 44);
  CHECK(kBenchSeats == 20);
}

TEST_CASE("Banquet : le compte de pages couvre le dernier personnage", "[ui][seats]") {
  // ⚠ LE SERVEUR EN AUTORISE PLUS QUE LA TABLE N'A DE SIÈGES, et c'est toute
  // la raison d'être de la pagination. Une page de trop est sans conséquence ;
  // une de moins rend un personnage inatteignable.
  CHECK(SeatPages(0) == 1);
  CHECK(SeatPages(kHonourSeats) == 1);
  CHECK(SeatPages(kSeatCount) == 1);
  CHECK(SeatPages(kSeatCount + 1) == 2);
  CHECK(SeatPages(45) == 2);
  CHECK(SeatPages(60) == 3);

  // Le dernier emplacement doit être ASSIS quelque part sur la dernière page.
  for (const int slots : {25, 26, 45, 60}) {
    const int last_page = SeatPages(slots) - 1;
    int highest = -1;
    for (int seat = 0; seat < kSeatCount; ++seat) {
      const int slot = SeatedSlot(seat, last_page);
      if (slot < slots && slot > highest) {
        highest = slot;
      }
    }
    CHECK(highest == slots - 1);
  }
}

TEST_CASE("Banquet : la boîte du pantin s'arrête à la ligne du siège", "[ui][seats]") {
  // 🔴 SON BAS EST LA LIGNE DU SIÈGE, ET NON LE BAS DE LA ZONE CLIQUABLE. Les
  // deux rectangles se ressemblent et ne servent pas à la même chose : on
  // clique un peu SOUS les pieds — `kSeatGrabBelow` —, mais les pieds tombent
  // exactement sur la ligne. Aligner le pantin sur la zone cliquable
  // l'enfoncerait de deux centièmes d'écran dans le banc, sur les vingt-cinq
  // sièges à la fois.
  const ro::ui::SeatFrame frame = SeatFrameAt(kBanquetSeats[0], 1920.0F, 1080.0F);

  CHECK(frame.chair_y == 0.295F * 1080.0F);
  CHECK(frame.centre_x == 0.460F * 1920.0F);
  CHECK(frame.standard_height == 0.115F * 1080.0F);
  // La zone cliquable déborde SOUS la ligne ; la boîte du pantin, non.
  CHECK(frame.grab_bottom > frame.chair_y);
  CHECK(frame.grab_top == frame.chair_y - frame.standard_height);
}

TEST_CASE("Banquet : la boîte du pantin déborde du siège", "[ui][seats]") {
  // ⚠ C'EST CE QUI PERMET À UN GRAND CHAPEAU DE SORTIR DU CADRE au lieu de
  // rétrécir son porteur : toute la tablée est à UNE échelle, celle qui met un
  // personnage standard à la hauteur du siège, et la boîte doit avoir la place
  // de montrer ce qui dépasse. Une boîte à la taille du siège les couperait
  // tous.
  const ro::ui::SeatFrame frame = SeatFrameAt(kBanquetSeats[0], 1920.0F, 1080.0F);

  CHECK(frame.doll_height > frame.standard_height);
  CHECK(frame.doll_width > frame.grab_right - frame.grab_left);
}

TEST_CASE("Banquet : la mesure suit la taille d'écran", "[ui][seats]") {
  // Les coordonnées sont des fractions, et le décor est étiré plein cadre :
  // une tablée calée en 1920 × 1080 doit valoir en 1280 × 720 sans retouche.
  const ro::ui::SeatFrame large = SeatFrameAt(kBanquetSeats[12], 1920.0F, 1080.0F);
  const ro::ui::SeatFrame small = SeatFrameAt(kBanquetSeats[12], 1280.0F, 720.0F);

  CHECK(small.centre_x * 1.5F == large.centre_x);
  CHECK(small.chair_y * 1.5F == large.chair_y);
  CHECK(small.standard_height * 1.5F == large.standard_height);
  // Et le cadre cliquable garde le rapport d'un sprite debout, aux deux
  // tailles.
  CHECK((large.grab_right - large.grab_left) ==
        Catch::Approx(large.standard_height * ro::ui::kSeatBoxAspect));
  CHECK((small.grab_right - small.grab_left) ==
        Catch::Approx(small.standard_height * ro::ui::kSeatBoxAspect));
}

TEST_CASE("Banquet : aucun siège hors du cadre", "[ui][seats]") {
  // Les coordonnées sont des fractions d'écran. Une valeur hors [0, 1] poserait
  // un personnage que personne ne peut cliquer — et le bout gauche, à 0,041,
  // montre que la marge est mince.
  for (const auto& seat : kBanquetSeats) {
    CHECK(seat.x > 0.0F);
    CHECK(seat.x < 1.0F);
    CHECK(seat.y > 0.0F);
    CHECK(seat.y < 1.0F);
    CHECK(seat.height > 0.0F);
  }
}

namespace {

// LA TABLÉE D'USINE EN 1920 × 1080, un convive à chaque siège donné ;
// l'emplacement vaut le siège, comme sur la première page.
std::vector<ro::ui::SeatPoint> SeatedAt(std::initializer_list<int> seats) {
  std::vector<ro::ui::SeatPoint> points;
  for (const int seat : seats) {
    const ro::ui::SeatFrame frame =
        SeatFrameAt(kBanquetSeats[static_cast<std::size_t>(seat)], 1920.0F, 1080.0F);
    points.push_back({seat, frame.centre_x, frame.chair_y});
  }
  return points;
}

}  // namespace

TEST_CASE("Banquet : les flèches suivent la rangée", "[ui][seats]") {
  using ro::ui::NearestSeatToward;
  using ro::ui::SeatStep;
  // Rangée haute : index 7 à 9. Rangée basse : index 19 et 20.
  const auto seats = SeatedAt({7, 8, 9, 19, 20});
  CHECK(NearestSeatToward(seats, 8, SeatStep::kRight) == 9);
  CHECK(NearestSeatToward(seats, 8, SeatStep::kLeft) == 7);
  // L'index 20 est sous le 9 à un millième près : la flèche y descend droit,
  // et non vers le 19, plus à gauche.
  CHECK(NearestSeatToward(seats, 9, SeatStep::kDown) == 20);
  CHECK(NearestSeatToward(seats, 20, SeatStep::kUp) == 9);
}

TEST_CASE("Banquet : au bout, la flèche boucle", "[ui][seats]") {
  using ro::ui::NearestSeatToward;
  using ro::ui::SeatStep;
  const auto seats = SeatedAt({7, 8, 9, 19, 20});
  // Au bout de la rangée haute, « droite » revient à son autre bout, et non
  // au voisin d'à côté.
  CHECK(NearestSeatToward(seats, 9, SeatStep::kRight) == 7);
  CHECK(NearestSeatToward(seats, 7, SeatStep::kLeft) == 9);
  // En haut de la colonne, « haut » repart d'en bas : l'index 19 est assez
  // près sous le 7 pour en être, le 20 trop à droite.
  CHECK(NearestSeatToward(seats, 7, SeatStep::kUp) == 19);
  // Le retour ne revient pas forcément d'où l'on vient : l'index 19 est
  // presque à l'aplomb du 8, et c'est lui que « bas » rejoint.
  CHECK(NearestSeatToward(seats, 19, SeatStep::kDown) == 8);
  // Seul sur sa ligne, il n'y a rien où revenir.
  const auto alone = SeatedAt({0, 20});
  CHECK(NearestSeatToward(alone, 20, SeatStep::kRight) == -1);
}

TEST_CASE("Banquet : une flèche ne glisse pas de travers", "[ui][seats]") {
  using ro::ui::NearestSeatToward;
  using ro::ui::SeatStep;
  // Le bout droit est deux millièmes SOUS le gauche, mais à l'autre bout de
  // la table : « bas » ne doit pas y mener.
  const auto ends = SeatedAt({14, 15});
  CHECK(NearestSeatToward(ends, 14, SeatStep::kDown) == -1);
  CHECK(NearestSeatToward(ends, 14, SeatStep::kRight) == 15);
  // Et en bouclant non plus : « droite » depuis le bout droit revient au
  // gauche, mais « haut » depuis le gauche ne trouve rien.
  CHECK(NearestSeatToward(ends, 15, SeatStep::kRight) == 14);
  CHECK(NearestSeatToward(ends, 14, SeatStep::kUp) == -1);
}

TEST_CASE("Banquet : sans désigné, la flèche prend le premier emplacement", "[ui][seats]") {
  using ro::ui::NearestSeatToward;
  using ro::ui::SeatStep;
  const auto seats = SeatedAt({12, 3, 20});
  CHECK(NearestSeatToward(seats, -1, SeatStep::kRight) == 3);
  // Un désigné resté sur une autre page compte comme aucun.
  CHECK(NearestSeatToward(seats, 40, SeatStep::kDown) == 3);
  CHECK(NearestSeatToward({}, -1, SeatStep::kDown) == -1);
}
