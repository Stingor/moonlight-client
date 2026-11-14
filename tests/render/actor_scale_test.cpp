#include "ro/render/actor_scale.h"

#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using ro::render::BodyScaleFor;

TEST_CASE("Échelle : tout le monde a sa taille, sauf les bébés", "[render][scale]") {
  // La table est celle du client, et ces valeurs sont les siennes, écrites en clair.
  CHECK(BodyScaleFor(0) == 1.0F);     // novice
  CHECK(BodyScaleFor(4055) == 1.0F);  // high wizard
  CHECK(BodyScaleFor(1002) == 1.0F);  // Poring

  // Première classe : trois quarts.
  CHECK(BodyScaleFor(4023) == 0.75F);  // bébé novice
  CHECK(BodyScaleFor(4029) == 0.75F);  // bébé voleur
  CHECK(BodyScaleFor(4045) == 0.75F);  // bébé super novice
  CHECK(BodyScaleFor(4191) == 0.75F);  // bébé super novice étendu

  // Le bébé invocateur a sa valeur à lui.
  CHECK(BodyScaleFor(4220) == 0.8F);

  // Tous les autres bébés : quatre-vingt-deux centièmes.
  CHECK(BodyScaleFor(4030) == 0.82F);  // bébé chevalier
  CHECK(BodyScaleFor(4044) == 0.82F);  // bébé alchimiste
  CHECK(BodyScaleFor(4096) == 0.82F);  // bébé rune knight
  CHECK(BodyScaleFor(4112) == 0.82F);
  CHECK(BodyScaleFor(4248) == 0.82F);
}

TEST_CASE("Échelle : la table du client se recopie jusque dans ses trous", "[render][scale]") {
  // Le bébé maître des esprits (4315) n'est dans aucune plage du client, et
  // 4221 tombe entre deux : tous deux gardent leur taille. Les corriger serait
  // dessiner autre chose que le jeu.
  CHECK(BodyScaleFor(4315) == 1.0F);
  CHECK(BodyScaleFor(4221) == 1.0F);
  CHECK(BodyScaleFor(4022) == 1.0F);
  CHECK(BodyScaleFor(4249) == 1.0F);
}

TEST_CASE("Échelle : un corps va d'une taille à l'autre par une courbe", "[render][scale]") {
  // Le client fait avancer un angle de quatre degrés par PAS, de quatre-vingt-dix
  // à deux cent soixante-dix, et prend le sinus : le départ et l'arrivée sont
  // plats, le milieu rapide. Le pas est un compteur d'octet sur l'acteur, que
  // l'effet en cours y écrit — ce n'est pas une durée.
  using ro::render::kBigBodyScale;
  using ro::render::kSizeChangeSteps;
  using ro::render::kSmallBodyScale;
  using ro::render::SizeChangeScale;

  // Aux deux bouts : la taille ordinaire, puis la taille visée.
  CHECK(SizeChangeScale(kSmallBodyScale, 0) == Catch::Approx(1.0F));
  CHECK(SizeChangeScale(kSmallBodyScale, kSizeChangeSteps) == Catch::Approx(kSmallBodyScale));
  CHECK(SizeChangeScale(kBigBodyScale, 0) == Catch::Approx(1.0F));
  CHECK(SizeChangeScale(kBigBodyScale, kSizeChangeSteps * 10) == Catch::Approx(kBigBodyScale));

  // La courbe est SYMÉTRIQUE autour de son milieu : ce qu'elle a parcouru au
  // pas k et ce qu'il lui reste au pas 45 − k sont la même part du trajet. Le
  // milieu exact, lui, tombe entre deux pas — quarante-cinq est impair — et
  // les deux qui l'encadrent encadrent aussi la moitié du chemin.
  for (std::int32_t step = 0; step <= kSizeChangeSteps; ++step) {
    CHECK(SizeChangeScale(kSmallBodyScale, step) +
              SizeChangeScale(kSmallBodyScale, kSizeChangeSteps - step) ==
          Catch::Approx(1.0F + kSmallBodyScale));
  }
  CHECK(SizeChangeScale(kSmallBodyScale, kSizeChangeSteps / 2) > 0.75F);
  CHECK(SizeChangeScale(kSmallBodyScale, kSizeChangeSteps / 2 + 1) < 0.75F);
  CHECK(SizeChangeScale(kBigBodyScale, kSizeChangeSteps / 2) < 1.25F);
  CHECK(SizeChangeScale(kBigBodyScale, kSizeChangeSteps / 2 + 1) > 1.25F);

  // Et elle ne rebrousse jamais chemin.
  float previous = 1.0F;
  for (std::int32_t step = 0; step <= kSizeChangeSteps; ++step) {
    const float scale = SizeChangeScale(kSmallBodyScale, step);
    CHECK(scale <= previous);
    previous = scale;
  }

  // Un pas négatif ne remonte pas le temps.
  CHECK(SizeChangeScale(kSmallBodyScale, -100) == Catch::Approx(1.0F));
}
