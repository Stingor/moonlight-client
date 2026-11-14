// LA MISE EN PAGE VIVANTE DE LA TABLÉE : ce que le mode « Personnaliser »
// manipule, sans fenêtre.

#include "ro/ui/seat_layout.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

using ro::ui::CharacterSeat;
using ro::ui::kAutomaticFrame;
using ro::ui::kBanquetSeats;
using ro::ui::kSeatAnchorsMax;
using ro::ui::SeatHeldFrame;
using ro::ui::SeatLayout;

namespace {

// Assez d'images pour que rien ne se borne.
constexpr int kPlentyOfFrames = 8;

}  // namespace

TEST_CASE("Mise en page : l'usine est la tablée du banquet", "[ui][seats][layout]") {
  const SeatLayout layout;
  for (std::size_t seat = 0; seat < layout.seats.size(); ++seat) {
    CHECK(layout.seats[seat].x == kBanquetSeats[seat].x);
    CHECK(layout.seats[seat].y == kBanquetSeats[seat].y);
    CHECK(layout.seats[seat].height == kBanquetSeats[seat].height);
    // Les convives sont assis, de face, la tête dans l'axe.
    CHECK(layout.seats[seat].animation == ro::ui::kSeatAnimSit);
    CHECK(layout.seats[seat].direction == 0);
    CHECK(layout.seats[seat].head_turn == ro::ui::kHeadAlongBody);
    CHECK(layout.seats[seat].frame == kAutomaticFrame);
    CHECK_FALSE(layout.seats[seat].animated);
  }
  CHECK(layout.anchors.empty());
  CHECK(layout.background == "lobby_hall.bmp");
  CHECK_FALSE(layout.hide_free_seats);
  CHECK(layout.free_seat_alpha == 1.0F);
}

TEST_CASE("Mise en page : le décor se nomme relativement à data\\texture", "[ui][seats][layout]") {
  // 🔴 C'EST LA FORME DU FICHIER HÉRITÉ, et c'est ce qui le laisse relire :
  // le chemin qu'il garde n'a pas `data\texture\` en tête, on l'ajoute ici.
  SeatLayout layout;
  CHECK(ro::ui::BackgroundPath(layout) == "data\\texture\\lobby_hall.bmp");
  layout.background = "lobby\\taverne.bmp";
  CHECK(ro::ui::BackgroundPath(layout) == "data\\texture\\lobby\\taverne.bmp");
  // Vide vaut l'usine, pas un chemin vers le dossier.
  layout.background.clear();
  CHECK(ro::ui::BackgroundPath(layout) == "data\\texture\\lobby_hall.bmp");
}

TEST_CASE("Mise en page : un chemin de décor bricolé est refusé", "[ui][seats][layout]") {
  CHECK(ro::ui::ValidBackgroundPath("lobby\\taverne.bmp"));
  CHECK(ro::ui::ValidBackgroundPath("lobby_hall.bmp"));
  CHECK_FALSE(ro::ui::ValidBackgroundPath(""));
  CHECK_FALSE(ro::ui::ValidBackgroundPath("..\\..\\windows\\win.ini"));
  CHECK_FALSE(ro::ui::ValidBackgroundPath("C:\\fond.bmp"));
  CHECK_FALSE(ro::ui::ValidBackgroundPath("\\lobby\\fond.bmp"));
  CHECK_FALSE(ro::ui::ValidBackgroundPath("/lobby/fond.bmp"));
  CHECK_FALSE(
      ro::ui::ValidBackgroundPath("lobby\\d\xC3\xA9"
                                  "cor.bmp"));
  CHECK_FALSE(ro::ui::ValidBackgroundPath(std::string(ro::ui::kBackgroundPathMax + 1, 'a')));
}

TEST_CASE("Mise en page : les bornes de lecture sont généreuses mais fermées",
          "[ui][seats][layout]") {
  // Un siège qui entre par le bord est une mise en page ; quarante est un
  // fichier corrompu.
  CHECK(ro::ui::ValidSeatFraction(-0.2F));
  CHECK(ro::ui::ValidSeatFraction(1.2F));
  CHECK_FALSE(ro::ui::ValidSeatFraction(40.0F));
  CHECK(ro::ui::ValidSeatHeight(0.115F));
  CHECK_FALSE(ro::ui::ValidSeatHeight(0.01F));
  CHECK_FALSE(ro::ui::ValidSeatHeight(0.7F));
}

TEST_CASE("Mise en page : une ancre s'enregistre une fois, à sa place d'usine",
          "[ui][seats][layout]") {
  SeatLayout layout;
  ro::ui::SeatAnchor* title = ro::ui::AnchorOf(layout, ro::ui::kTitleAnchor, 0.5F, 0.02F);
  REQUIRE(title != nullptr);
  CHECK(title->x == 0.5F);
  title->x = 0.3F;
  // ⚠ LE SECOND APPEL NE REMET PAS LA PLACE D'USINE : c'est ce qui fait
  // qu'une pastille glissée reste où on l'a posée.
  ro::ui::SeatAnchor* again = ro::ui::AnchorOf(layout, ro::ui::kTitleAnchor, 0.5F, 0.02F);
  REQUIRE(again != nullptr);
  CHECK(again->x == 0.3F);
  CHECK(layout.anchors.size() == 1);
}

TEST_CASE("Mise en page : le registre des ancres est borné", "[ui][seats][layout]") {
  SeatLayout layout;
  for (std::size_t rank = 0; rank < kSeatAnchorsMax; ++rank) {
    CHECK(ro::ui::AnchorOf(layout, "ancre" + std::to_string(rank), 0.0F, 0.0F) != nullptr);
  }
  CHECK(ro::ui::AnchorOf(layout, "de_trop", 0.0F, 0.0F) == nullptr);
  CHECK(layout.anchors.size() == kSeatAnchorsMax);
}

TEST_CASE("Mise en page : replacer garde le décor, tout restaurer le rend", "[ui][seats][layout]") {
  SeatLayout layout;
  layout.seats[3].x = 0.9F;
  layout.seats[3].animation = ro::ui::kSeatAnimDie;
  layout.background = "lobby\\taverne.bmp";
  layout.hide_free_seats = true;
  layout.free_seat_alpha = 0.25F;
  static_cast<void>(ro::ui::AnchorOf(layout, ro::ui::kTitleAnchor, 0.5F, 0.1F));

  ro::ui::ResetSeatPlacement(layout);
  CHECK(layout.seats[3].x == kBanquetSeats[3].x);
  CHECK(layout.seats[3].animation == ro::ui::kSeatAnimSit);
  CHECK(layout.anchors.empty());
  CHECK(layout.background == "lobby\\taverne.bmp");

  ro::ui::ResetSeatLayout(layout);
  CHECK(layout.background == "lobby_hall.bmp");
  // ⚠ LES PLACES LIBRES SONT UN GOÛT, pas un placement : aucune des deux
  // remises n'y touche.
  CHECK(layout.hide_free_seats);
  CHECK(layout.free_seat_alpha == 0.25F);
}

TEST_CASE("Mise en page : une pose s'applique partout, pas une place", "[ui][seats][layout]") {
  SeatLayout layout;
  CharacterSeat& source = layout.seats[7];
  source.x = 0.33F;
  source.animation = ro::ui::kSeatAnimFight;
  source.animated = true;
  source.head_turn = 2;
  source.direction = 5;
  source.frame = 1;
  // ⚠ LA SOURCE EST DANS LA TABLE QU'ON ÉCRIT : une référence serait écrasée
  // dès la première place.
  ro::ui::ApplyPoseEverywhere(layout, source);
  for (std::size_t seat = 0; seat < layout.seats.size(); ++seat) {
    CHECK(layout.seats[seat].animation == ro::ui::kSeatAnimFight);
    CHECK(layout.seats[seat].animated);
    CHECK(layout.seats[seat].head_turn == 2);
    CHECK(layout.seats[seat].direction == 5);
    CHECK(layout.seats[seat].frame == 1);
    if (seat != 7) {
      CHECK(layout.seats[seat].x == kBanquetSeats[seat].x);
    }
  }
}

TEST_CASE("Mise en page : l'image que tient un convive", "[ui][seats][layout]") {
  CharacterSeat seat;
  // Assis, figé : l'image zéro.
  CHECK(SeatHeldFrame(seat, kPlentyOfFrames) == 0);
  // 🔴 LA TÊTE TOURNÉE EST UNE IMAGE, celle de tout l'acteur.
  seat.head_turn = 2;
  CHECK(SeatHeldFrame(seat, kPlentyOfFrames) == 2);
  // Un corps qui n'a qu'une image ne la dépasse pas : sinon plus rien ne se
  // dessinerait.
  CHECK(SeatHeldFrame(seat, 1) == 0);

  // « Animé » ne fait défiler ni l'attente ni l'assise…
  seat.animated = true;
  CHECK(SeatHeldFrame(seat, kPlentyOfFrames) == 2);
  // …mais la marche, oui — et la tête ne se tourne plus.
  seat.animation = ro::ui::kSeatAnimWalk;
  CHECK(SeatHeldFrame(seat, kPlentyOfFrames) == kAutomaticFrame);

  // L'image imposée l'emporte sur tout, et se borne.
  seat.frame = 3;
  CHECK(SeatHeldFrame(seat, kPlentyOfFrames) == 3);
  CHECK(SeatHeldFrame(seat, 3) == 0);
  // Zéro image connue ne borne rien : les sprites ne répondent pas encore.
  CHECK(SeatHeldFrame(seat, 0) == 3);
}

TEST_CASE("Mise en page : le menu de pose offre quinze poses", "[ui][seats][layout]") {
  CHECK(ro::ui::kSeatPoseChoices.size() == 15);
  CHECK(ro::ui::kSeatPoseChoices.front().animation == ro::ui::kSeatAnimSit);
  for (const ro::ui::SeatPoseChoice& choice : ro::ui::kSeatPoseChoices) {
    CHECK(choice.label != nullptr);
    CHECK(choice.animation >= 0);
    CHECK(choice.animation < ro::ui::kSeatAnimationTypes);
  }
  CHECK(ro::ui::kHeadTurnChoices.size() == 3);
}
