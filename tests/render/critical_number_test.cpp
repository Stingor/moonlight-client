// LE NOMBRE D'UN COUP CRITIQUE : l'étoile d'impact derrière, le nombre devant.
//
// ⚠ DEUX SPRITES-ENFANTS CHEZ LE CLIENT, un seul cadre chez nous — les genres
// quinze et seize —, et c'est tout l'objet de ces essais : ce que l'étoile
// prend du nombre (sa PLACE, parce que les deux genres partagent le même corps)
// et ce qu'elle garde en propre (sa taille, son image, son opacité). Les
// mélanger la faisait naître presque deux fois trop grosse.

#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/damage_mark.h"
#include "ro/render/floating_number.h"

#include "support/damage_sprites.h"

using ro::formats::ActAnimation;
using ro::formats::SprSprite;
using ro::render::ComposeCriticalNumber;
using ro::render::ComposedActor;
using ro::render::ComposeFloatingNumber;
using ro::render::DamageMark;
using ro::render::DamageMarkLifeMs;
using ro::render::DigitSprite;
using ro::render::FloatingNumber;
using ro::render::kNumberTickMs;
using ro::render::NumberStyle;
using ro::test_support::CenterX;
using ro::test_support::DigitsAnimation;
using ro::test_support::DigitsSprite;
using ro::test_support::kStarImages;
using ro::test_support::NearlyEqual;
using ro::test_support::Rise;
using ro::test_support::StarAnimation;
using ro::test_support::StarRise;
using ro::test_support::StarSprite;
using ro::test_support::Width;

TEST_CASE("Nombre critique : l'étoile se peint derrière le nombre", "[render][number]") {
  // Elle est le FOND du nombre, pas une marque à part. Deux cadres séparés la
  // laisseraient passer devant — le dernier posé se dessine par-dessus.
  const SprSprite digits_sprite = DigitsSprite();
  const ActAnimation digits_animation = DigitsAnimation();
  const DigitSprite digits{&digits_sprite, &digits_animation};
  const SprSprite star_sprite = StarSprite();
  const ActAnimation star_animation = StarAnimation();

  FloatingNumber number;
  number.value = 1234;
  number.style = NumberStyle::kCriticalBurst;

  const ComposedActor plain = ComposeFloatingNumber(digits, number, 0.0F);
  const ComposedActor critical =
      ComposeCriticalNumber(digits, number, star_sprite, star_animation, 0.0F);
  REQUIRE(!plain.quads.empty());
  REQUIRE(critical.quads.size() == plain.quads.size() + 1);
  REQUIRE(critical.sources.size() == critical.quads.size());

  // L'étoile EN TÊTE, les chiffres ensuite, et dans le même ordre que sans
  // elle.
  CHECK(critical.sources.front().sprite == &star_sprite);
  for (std::size_t index = 0; index < plain.quads.size(); ++index) {
    CHECK(critical.sources[index + 1].sprite == plain.sources[index].sprite);
    CHECK(critical.quads[index + 1].corners[0].x == Catch::Approx(plain.quads[index].corners[0].x));
    CHECK(critical.quads[index + 1].corners[0].y == Catch::Approx(plain.quads[index].corners[0].y));
  }
}

TEST_CASE("Nombre critique : l'étoile monte exactement comme le nombre", "[render][number]") {
  // C'est ce qui la distingue d'une marque posée à côté : elle ne reste pas sur
  // place pendant que le nombre s'envole.
  const SprSprite digits_sprite = DigitsSprite();
  const ActAnimation digits_animation = DigitsAnimation();
  const DigitSprite digits{&digits_sprite, &digits_animation};
  const SprSprite star_sprite = StarSprite();
  const ActAnimation star_animation = StarAnimation();

  FloatingNumber number;
  number.value = 7;
  number.style = NumberStyle::kCriticalBurst;

  for (const float ticks : {0.0F, 2.0F, 4.0F}) {
    const ComposedActor composed =
        ComposeCriticalNumber(digits, number, star_sprite, star_animation, ticks * kNumberTickMs);
    REQUIRE(composed.quads.size() == 2);
    CHECK(NearlyEqual(StarRise(composed), Rise(composed.quads[1]), 1e-2F));
  }

  // Et elle monte VRAIMENT : sans quoi l'égalité ci-dessus tiendrait aussi pour
  // deux choses immobiles.
  const ComposedActor born =
      ComposeCriticalNumber(digits, number, star_sprite, star_animation, 0.0F);
  const ComposedActor later =
      ComposeCriticalNumber(digits, number, star_sprite, star_animation, 7.0F * kNumberTickMs);
  CHECK(StarRise(later) > StarRise(born));
}

TEST_CASE("Nombre critique : l'étoile s'éteint AVEC le nombre, figée sur sa dernière image",
          "[render][number]") {
  // Sa durée est celle de son genre — soixante-dix tics, comme la cloche —, et
  // non celle de son fichier : elle atteint sa dernière image bien avant et y
  // reste, en pâlissant. Le premier port la faisait disparaître à la fin de son
  // animation, laissant le nombre finir seul.
  const SprSprite digits_sprite = DigitsSprite();
  const ActAnimation digits_animation = DigitsAnimation();
  const DigitSprite digits{&digits_sprite, &digits_animation};
  const SprSprite star_sprite = StarSprite();
  const ActAnimation star_animation = StarAnimation();

  FloatingNumber number;
  number.value = 7;
  number.style = NumberStyle::kCriticalBurst;

  const float star_life_ms = ro::render::DamageMarkLifeMs(star_animation, DamageMark::kCritical);
  CHECK(star_life_ms == Catch::Approx(ro::render::kStarLifeTicks * kNumberTickMs));

  // L'image tourne d'une demie par tic, et s'arrête sur la dernière : le
  // fichier d'essai en porte huit, atteintes au quatorzième tic.
  const auto frame_at = [&](float ticks) {
    const ComposedActor composed =
        ComposeCriticalNumber(digits, number, star_sprite, star_animation, ticks * kNumberTickMs);
    REQUIRE(composed.sources.size() == 2);
    return composed.sources.front().image_index;
  };
  CHECK(frame_at(0.0F) == 0);
  CHECK(frame_at(4.0F) == 2);
  CHECK(frame_at(static_cast<float>(2 * (kStarImages - 1))) ==
        static_cast<std::int32_t>(kStarImages - 1));
  CHECK(frame_at(60.0F) == static_cast<std::int32_t>(kStarImages - 1));

  // Figée, elle continue de pâlir — et les deux s'éteignent ensemble.
  const ComposedActor early =
      ComposeCriticalNumber(digits, number, star_sprite, star_animation, 20.0F * kNumberTickMs);
  const ComposedActor late =
      ComposeCriticalNumber(digits, number, star_sprite, star_animation, 60.0F * kNumberTickMs);
  REQUIRE(early.quads.size() == 2);
  REQUIRE(late.quads.size() == 2);
  CHECK(late.quads[0].tint[3] < early.quads[0].tint[3]);

  // Et les deux s'en vont ensemble. ⚠ LA BORNE EXACTE N'EST PAS ÉCRITE PAREIL
  // des deux côtés — le nombre vit jusqu'au terme inclus, la marque jusqu'au
  // terme exclu —, et ce que le jeu montre est ce qui se passe APRÈS : plus
  // rien, au lieu du nombre qui finissait seul.
  CHECK(ComposeCriticalNumber(digits, number, star_sprite, star_animation, star_life_ms + 1.0F)
            .quads.empty());
}

TEST_CASE("Nombre critique : l'étoile a SA taille, pas celle du nombre", "[render][number]") {
  // Deux sprites-enfants chez le client, et deux échelles : l'étoile naît à
  // trois quand le nombre naît à cinq, et rétrécit deux fois plus lentement
  // jusqu'à un — le nombre s'arrêtant à un et trois dixièmes. Lui donner la
  // pose du nombre la faisait naître presque deux fois trop grosse.
  const SprSprite digits_sprite = DigitsSprite();
  const ActAnimation digits_animation = DigitsAnimation();
  const DigitSprite digits{&digits_sprite, &digits_animation};
  const SprSprite star_sprite = StarSprite();
  const ActAnimation star_animation = StarAnimation();
  FloatingNumber number;
  number.value = 7;
  number.style = NumberStyle::kCriticalBurst;

  const auto star_scale = [&](float ticks) {
    const ComposedActor composed =
        ComposeCriticalNumber(digits, number, star_sprite, star_animation, ticks * kNumberTickMs);
    REQUIRE(composed.quads.size() == 2);
    return Width(composed.quads[0]);
  };
  const float born = star_scale(0.0F);
  CHECK(NearlyEqual(star_scale(5.0F),
                    born * (ro::render::kStarInitialScale - 5.0F * ro::render::kStarShrinkPerTick) /
                        ro::render::kStarInitialScale,
                    1e-2F));
  // Son plancher est le sien : passé le quatorzième tic elle ne rétrécit plus.
  const float floored = born * ro::render::kStarFinalScale / ro::render::kStarInitialScale;
  CHECK(NearlyEqual(star_scale(40.0F), floored, 1e-2F));
  CHECK(NearlyEqual(star_scale(60.0F), floored, 1e-2F));
}

TEST_CASE("Nombre critique : passé son terme, il ne reste rien", "[render][number]") {
  const SprSprite digits_sprite = DigitsSprite();
  const ActAnimation digits_animation = DigitsAnimation();
  const DigitSprite digits{&digits_sprite, &digits_animation};
  const SprSprite star_sprite = StarSprite();
  const ActAnimation star_animation = StarAnimation();

  FloatingNumber number;
  number.value = 7;
  number.style = NumberStyle::kCriticalBurst;
  const float dead_ms = 1000.0F * kNumberTickMs;
  CHECK(ComposeCriticalNumber(digits, number, star_sprite, star_animation, dead_ms).quads.empty());
  CHECK(ComposeCriticalNumber(digits, number, star_sprite, star_animation, -1.0F).quads.empty());
}

TEST_CASE("Nombre critique : l'étoile suit la durée du réglage sans doubler l'échelle",
          "[render][number]") {
  const SprSprite digits_sprite = DigitsSprite();
  const ActAnimation digits_animation = DigitsAnimation();
  const DigitSprite digits{&digits_sprite, &digits_animation};
  const SprSprite star_sprite = StarSprite();
  const ActAnimation star_animation = StarAnimation();
  FloatingNumber number;
  number.value = 7;
  number.style = NumberStyle::kCriticalBurst;

  // Doubler la taille double l'étoile UNE fois : le réglage multiplie l'échelle
  // de son genre, il ne s'y ajoute pas une seconde fois par la pose du nombre.
  ro::render::NumberLook look;
  look.scale = 2.0F;
  const ComposedActor plain =
      ComposeCriticalNumber(digits, number, star_sprite, star_animation, 0.0F);
  const ComposedActor doubled =
      ComposeCriticalNumber(digits, number, star_sprite, star_animation, 0.0F, look);
  REQUIRE(plain.quads.size() == 2);
  REQUIRE(doubled.quads.size() == 2);
  CHECK(NearlyEqual(Width(doubled.quads[0]), Width(plain.quads[0]) * 2.0F));

  // Et la vie réglée étire la sienne comme celle du nombre : à son terme du
  // client, les deux sont éteints ; à vie doublée, les deux sont encore là.
  look = ro::render::ClientLookOf(number.style);
  look.life->life_ms *= 2.0F;
  const float star_life_ms = ro::render::DamageMarkLifeMs(star_animation, DamageMark::kCritical);
  CHECK(ComposeCriticalNumber(digits, number, star_sprite, star_animation, star_life_ms + 1.0F)
            .quads.empty());
  CHECK(
      ComposeCriticalNumber(digits, number, star_sprite, star_animation, star_life_ms + 1.0F, look)
          .quads.size() == 2);
}

TEST_CASE("Nombre critique : l'étoile rejoint le point d'arrivée avec le nombre",
          "[render][number]") {
  const SprSprite digits_sprite = DigitsSprite();
  const ActAnimation digits_animation = DigitsAnimation();
  const DigitSprite digits{&digits_sprite, &digits_animation};
  const SprSprite star_sprite = StarSprite();
  const ActAnimation star_animation = StarAnimation();
  FloatingNumber number;
  number.value = 7;
  number.style = NumberStyle::kCriticalBurst;
  ro::render::NumberLook look;
  look.arrival_x_pixels = 20.0F;

  // À un cinquième de la cloche — quatorze tics sur soixante-dix —, l'étoile vit
  // encore, et elle a fait le même chemin de côté que le chiffre : un
  // cinquième, à vitesse égale.
  const ComposedActor midway = ComposeCriticalNumber(digits, number, star_sprite, star_animation,
                                                     14.0F * kNumberTickMs, look);
  REQUIRE(midway.quads.size() == 2);
  CHECK(NearlyEqual(CenterX(midway.quads[0]), 20.0F * 0.2F));
  CHECK(NearlyEqual(CenterX(midway.quads[1]), 20.0F * 0.2F));
}

TEST_CASE("Nombre d'un coup de pied : l'étoile dessous, qui monte avec lui et ne tourne pas",
          "[render][number]") {
  // Le genre vingt-cinq sous le genre vingt-deux : l'étoile part la première,
  // donc se peint dessous, et prend du nombre sa PLACE — les deux montent du
  // même dixième d'unité par tic depuis la même hauteur.
  const SprSprite digits_sprite = DigitsSprite();
  const ActAnimation digits_animation = DigitsAnimation();
  const DigitSprite digits{&digits_sprite, &digits_animation};
  const SprSprite star_sprite = StarSprite();
  const ActAnimation star_animation = StarAnimation();

  FloatingNumber number;
  number.value = 7;
  number.style = NumberStyle::kRising;

  for (const float ticks : {0.0F, 10.0F, 60.0F}) {
    const ComposedActor composed = ro::render::ComposeRisingStarNumber(
        digits, number, star_sprite, star_animation, ticks * kNumberTickMs);
    REQUIRE(composed.quads.size() == 2);
    CHECK(composed.sources.front().sprite == &star_sprite);
    CHECK(composed.sources.front().image_index == 0);
    CHECK(NearlyEqual(StarRise(composed), Rise(composed.quads[1]), 1e-2F));
  }
  const ComposedActor born =
      ro::render::ComposeRisingStarNumber(digits, number, star_sprite, star_animation, 0.0F);
  const ComposedActor later = ro::render::ComposeRisingStarNumber(
      digits, number, star_sprite, star_animation, 60.0F * kNumberTickMs);
  CHECK(StarRise(later) > StarRise(born));
}
