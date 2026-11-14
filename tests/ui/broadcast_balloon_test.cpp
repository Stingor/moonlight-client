#include "ro/ui/broadcast_balloon.h"

#include <catch2/catch_test_macros.hpp>

// LA GÉOMÉTRIE DES BULLES D'ANNONCE : celle du client, en fonctions pures.

using ro::ui::BalloonAnchor;

TEST_CASE("Bulle d'annonce : la taille ajoute les marges du client", "[ui][balloon]") {
  // Une rangée de 121 pixels sous un pas de 16 (12 de texte, 4 d'interligne) :
  // 133 de large, comme la bulle relevée sur une capture du jeu, et 4 + 16 de
  // haut.
  const ImVec2 size = ro::ui::BalloonSize(121.0F, 16.0F);
  CHECK(size.x == 133.0F);
  CHECK(size.y == 20.0F);
  // Une largeur fractionnaire s'arrondit vers le HAUT : vers le bas, elle
  // rognerait le dernier glyphe.
  CHECK(ro::ui::BalloonSize(120.2F, 16.0F).x == 133.0F);
}

TEST_CASE("Bulle d'annonce : les bandeaux se posent à la place du client", "[ui][balloon]") {
  const BalloonAnchor native;
  CHECK(ro::ui::AnnouncementBalloonTop(native) == 51.0F);
  // Celui des chasses suit la hauteur de l'écran : 130 pixels sur 480.
  CHECK(ro::ui::HuntBalloonTop(native, 480.0F) == 130.0F);
  CHECK(ro::ui::HuntBalloonTop(native, 1022.0F) == 276.0F);
  // Centrés sur l'écran.
  CHECK(ro::ui::BalloonCenterX(native, 1762.0F) == 881.0F);

  // Placés à la main, ils gardent leur place, quel que soit l'écran.
  const BalloonAnchor placed{.center_x = 300, .top_y = 90};
  CHECK(ro::ui::AnnouncementBalloonTop(placed) == 90.0F);
  CHECK(ro::ui::HuntBalloonTop(placed, 1022.0F) == 90.0F);
  CHECK(ro::ui::BalloonCenterX(placed, 1762.0F) == 300.0F);
}

TEST_CASE("Bulle d'annonce : le corps suit le réglage, entier et borné", "[ui][balloon]") {
  CHECK(ro::ui::BalloonFontPixels(12, 100) == 12.0F);
  CHECK(ro::ui::BalloonFontPixels(14, 150) == 21.0F);
  // 12 × 1,1 = 13,2 : un corps fractionnaire rend le texte pâteux.
  CHECK(ro::ui::BalloonFontPixels(12, 110) == 13.0F);
  // Hors bornes, le pourcentage se ramène à elles.
  CHECK(ro::ui::BalloonFontPixels(12, 1) ==
        ro::ui::BalloonFontPixels(12, ro::ui::kLeastBalloonFontPercent));
}
