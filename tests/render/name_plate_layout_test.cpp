// LA DISPOSITION D'UNE ÉTIQUETTE : l'image de tête, puis les lignes.
//
// 🔴 CE BANC GARDE DES NOMBRES QUI NE SE VOIENT QU'AU PIXEL. Un emblème décalé
// de quatre pixels, une seconde ligne recentrée au lieu d'alignée : l'étiquette
// reste lisible, et ressemble au client sans être la sienne.
//
// ⚠ LES ATTENDUS SONT DES LITTÉRAUX, tirés du relevé du client : les déduire
// des constantes qu'ils gardent ne garderait rien.

#include "ro/render/name_plate_layout.h"

#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using ro::render::NamePlateLayout;
using ro::render::NamePlateLead;
using ro::render::PlaceNamePlate;

namespace {

constexpr float kAnchorX = 200.0F;
constexpr float kTextTopY = 50.0F;
constexpr float kLineStep = 14.0F;

}  // namespace

// 🔴 LA POSE DE L'INSIGNE SE CONFRONTE À UNE CAPTURE DU CLIENT NATIF.
//
// Sur cette capture — un mini-boss de niveau cinquante, étiquette d'une ligne —
// les pixels donnent : texte rouge de l'abscisse 79 à 318, soit deux cent
// quarante de large, centré sur 198,5 ; insigne dont la partie utile occupe
// (60, 170)-(72, 180), et qui commence donc à (10, 5) près du coin de son
// carré, c'est-à-dire en (50, 165). Les pieds tombent alors en 186,5.
//
// ⚠ LA CAPTURE FIXE LES ABSCISSES, pas la hauteur du texte : les quatre pixels
// entre le haut du texte (169) et celui du widget viennent du relevé, qui pose
// le texte à `y = 4` de son widget.
TEST_CASE("Étiquette : l'insigne retombe sur la capture du client", "[render][boss]") {
  constexpr float kFeetX = 186.5F;
  constexpr float kWidestLine = 240.0F;
  const NamePlateLayout placed =
      PlaceNamePlate(kFeetX, 169.0F, kWidestLine, 1, kLineStep, NamePlateLead::kBossBadge);
  // Un demi-pixel : le bord gauche d'un glyphe ne se relève pas plus finement
  // qu'à la colonne près.
  CHECK(placed.lead_min.x == Catch::Approx(50.5F).margin(0.5));
  CHECK(placed.lead_min.y == Catch::Approx(165.0F));
  CHECK(placed.text_left_x + kWidestLine / 2.0F == Catch::Approx(198.5F).margin(0.5));
}

TEST_CASE("Étiquette : l'insigne décale le texte de sa demi-largeur", "[render][boss]") {
  // EXACTEMENT : un insigne de vingt-cinq pixels déplacerait l'une des deux.
  const NamePlateLayout court =
      PlaceNamePlate(100.0F, kTextTopY, 40.0F, 1, kLineStep, NamePlateLead::kBossBadge);
  const NamePlateLayout long_name =
      PlaceNamePlate(100.0F, kTextTopY, 400.0F, 1, kLineStep, NamePlateLead::kBossBadge);
  CHECK(court.text_left_x == 92.0F);
  CHECK(long_name.text_left_x == -88.0F);
}

TEST_CASE("Étiquette : avec l'insigne, un nom très court garde la largeur minimale",
          "[render][boss]") {
  // ⚠ LE WIDGET VAUT LA LARGEUR MINIMALE DU TEXTE PLUS L'INSIGNE : trente-deux
  // et vingt-quatre. Sans le minimum, un nom de dix pixels partirait sept
  // pixels plus à droite.
  const NamePlateLayout placed =
      PlaceNamePlate(kAnchorX, kTextTopY, 10.0F, 1, kLineStep, NamePlateLead::kBossBadge);
  CHECK(placed.lead_min.x == 172.0F);
  CHECK(placed.text_left_x == 200.0F);
}

TEST_CASE("Étiquette : l'emblème à quatre pixels du bord, le texte vingt-huit plus loin",
          "[render][embleme]") {
  constexpr float kWidest = 100.0F;
  // Le widget vaut le texte, ses deux marges et vingt-cinq : 133 pixels, posé
  // à 200 − 133 / 2 en entiers, soit 134.
  constexpr float kWidgetLeft = 134.0F;
  const NamePlateLayout placed =
      PlaceNamePlate(kAnchorX, kTextTopY, kWidest, 1, kLineStep, NamePlateLead::kEmblem);
  CHECK(placed.lead_min.x == kWidgetLeft + 4.0F);
  CHECK(placed.text_left_x == kWidgetLeft + 32.0F);
  // ⚠ UN SEUL PIXEL À DROITE DU TEXTE : le widget est asymétrique, et c'est ce
  // qui décale tout le bloc vers la droite des pieds.
  CHECK(kWidgetLeft + 133.0F - (placed.text_left_x + kWidest) == 1.0F);
  // Une seule ligne : le widget vaut la case de l'emblème, qui monte donc de
  // quatre pixels au-dessus du texte.
  CHECK(placed.lead_min.y == 46.0F);
}

TEST_CASE("Étiquette : sur deux lignes, l'emblème se centre sur le bloc", "[render][embleme]") {
  const NamePlateLayout placed =
      PlaceNamePlate(kAnchorX, kTextTopY, 100.0F, 2, kLineStep, NamePlateLead::kEmblem);
  // Deux lignes de quatorze et six de marges : trente-quatre, soit cinq pixels
  // de part et d'autre de la case — un pixel sous le haut du texte.
  CHECK(placed.lead_min.y == 51.0F);
}

TEST_CASE("Étiquette : l'ancre se tronque, la demi-largeur se divise en entiers", "[render]") {
  // UNE LARGEUR IMPAIRE : 101 plus deux marges font 109, posés à 200 − 54.
  const NamePlateLayout odd =
      PlaceNamePlate(kAnchorX, kTextTopY, 101.0F, 1, kLineStep, NamePlateLead::kNone);
  CHECK(odd.text_left_x == 146.0F + 4.0F);
  // UNE ANCRE FRACTIONNAIRE sous le demi : tronquée, elle ne recule pas d'un
  // pixel comme le ferait un arrondi de l'ensemble.
  const NamePlateLayout shifted =
      PlaceNamePlate(200.3F, kTextTopY, 100.0F, 1, kLineStep, NamePlateLead::kEmblem);
  CHECK(shifted.lead_min.x == 138.0F);
}

TEST_CASE("Étiquette : le texte et l'image partagent la même grille", "[render]") {
  // 🔴 UN HAUT FRACTIONNAIRE s'arrondit UNE fois, pour les deux : sinon l'écart
  // entre l'image et le nom tremble d'un pixel quand la caméra bouge.
  for (const NamePlateLead lead : {NamePlateLead::kEmblem, NamePlateLead::kBossBadge}) {
    const NamePlateLayout low = PlaceNamePlate(kAnchorX, 50.4F, 100.0F, 1, kLineStep, lead);
    const NamePlateLayout high = PlaceNamePlate(kAnchorX, 50.6F, 100.0F, 1, kLineStep, lead);
    CHECK(low.text_top_y == 50.0F);
    CHECK(high.text_top_y == 51.0F);
    CHECK(low.lead_min.y == std::floor(low.lead_min.y));
    CHECK(high.lead_min.y - high.text_top_y == low.lead_min.y - low.text_top_y);
  }
}

TEST_CASE("Étiquette : sans image, les lignes partent du même bord, centrées sur la plus large",
          "[render]") {
  const NamePlateLayout placed =
      PlaceNamePlate(kAnchorX, kTextTopY, 100.0F, 2, kLineStep, NamePlateLead::kNone);
  CHECK(placed.text_left_x == 150.0F);
}

TEST_CASE("Étiquette : un nom très court remplit un widget de trente-deux pixels", "[render]") {
  // ⚠ IL N'EST PLUS CENTRÉ : il part à quatre pixels du bord d'un widget plus
  // large que lui. C'est ce que fait le client.
  const NamePlateLayout placed =
      PlaceNamePlate(kAnchorX, kTextTopY, 10.0F, 1, kLineStep, NamePlateLead::kNone);
  CHECK(placed.text_left_x == 188.0F);
}
