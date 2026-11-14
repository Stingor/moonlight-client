#include "ro/audio/sound_placement.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using ro::audio::kHearingRange;
using ro::audio::kNearDistance;
using ro::audio::PlaceSound;
using ro::audio::SoundPlacement;

namespace {

// L'auditeur à l'origine, regardant vers le nord : sa droite est l'axe des x.
constexpr std::array<float, 3> kListener = {0.0F, 0.0F, 0.0F};
constexpr std::array<float, 3> kRight = {1.0F, 0.0F, 0.0F};

}  // namespace

TEST_CASE("Placement d'un son : le volume tombe en inverse de la distance", "[audio][placement]") {
  // Plein volume en deçà de la proximité.
  CHECK(PlaceSound(kListener, kRight, {0.0F, 0.0F, 0.0F}).gain == Catch::Approx(1.0F));
  CHECK(PlaceSound(kListener, kRight, {kNearDistance, 0.0F, 0.0F}).gain == Catch::Approx(1.0F));

  // Puis proximité / distance, et non une droite : deux fois plus loin que la
  // proximité, moitié moins fort ; quatre fois plus loin, le quart. C'est ce
  // que calcule le client, et une droite donnerait ici 0,81 puis 0,43.
  CHECK(PlaceSound(kListener, kRight, {kNearDistance * 2.0F, 0.0F, 0.0F}).gain ==
        Catch::Approx(0.5F));
  CHECK(PlaceSound(kListener, kRight, {kNearDistance * 4.0F, 0.0F, 0.0F}).gain ==
        Catch::Approx(0.25F));
}

TEST_CASE("Placement d'un son : la coupure de portée est franche", "[audio][placement]") {
  // Juste avant la portée le son s'entend encore à seize pour cent, et il
  // tombe à rien d'un coup. La marche est VOULUE : c'est celle du client, et
  // c'est elle qui garde les voix pour ce qu'on entend vraiment.
  const float almost = kHearingRange - 1.0F;
  CHECK(PlaceSound(kListener, kRight, {almost, 0.0F, 0.0F}).gain ==
        Catch::Approx(kNearDistance / almost));
  CHECK(PlaceSound(kListener, kRight, {kHearingRange, 0.0F, 0.0F}).gain == 0.0F);
  CHECK(PlaceSound(kListener, kRight, {kHearingRange * 2.0F, 0.0F, 0.0F}).gain == 0.0F);
}

TEST_CASE("Placement d'un son : la hauteur ne compte pas", "[audio][placement]") {
  // Le client mesure la distance AU SOL et passe zéro pour l'axe vertical. Une
  // source à la verticale de l'auditeur est donc sur lui, si haut soit-elle.
  CHECK(PlaceSound(kListener, kRight, {0.0F, 500.0F, 0.0F}).gain == Catch::Approx(1.0F));
  // Et un dénivelé n'éloigne pas : sur une carte en pente, deux cases d'écart
  // valent parfois trente unités de hauteur.
  const float far = kNearDistance * 2.0F;
  CHECK(PlaceSound(kListener, kRight, {far, 200.0F, 0.0F}).gain ==
        Catch::Approx(PlaceSound(kListener, kRight, {far, 0.0F, 0.0F}).gain));
}

TEST_CASE("Placement d'un son : le côté suit l'axe droit de l'auditeur", "[audio][placement]") {
  // C'est l'axe de la CAMÉRA : un son passe d'une oreille à l'autre quand on
  // fait pivoter la vue, et non quand le monde bouge.
  CHECK(PlaceSound(kListener, kRight, {50.0F, 0.0F, 0.0F}).pan == Catch::Approx(1.0F));
  CHECK(PlaceSound(kListener, kRight, {-50.0F, 0.0F, 0.0F}).pan == Catch::Approx(-1.0F));
  // Droit devant, au centre.
  CHECK(PlaceSound(kListener, kRight, {0.0F, 0.0F, -50.0F}).pan == Catch::Approx(0.0F));
  // La même source, l'auditeur tourné : le côté s'inverse.
  constexpr std::array<float, 3> turned = {-1.0F, 0.0F, 0.0F};
  CHECK(PlaceSound(kListener, turned, {50.0F, 0.0F, 0.0F}).pan == Catch::Approx(-1.0F));
}

TEST_CASE("Placement d'un son : sur place, il ne panoramique pas", "[audio][placement]") {
  // Un écart nul ne désigne aucun côté ; le rapporter à la distance
  // diviserait par zéro.
  const SoundPlacement placement = PlaceSound(kListener, kRight, kListener);
  CHECK(placement.gain == Catch::Approx(1.0F));
  CHECK(placement.pan == 0.0F);
}
