#include "ro/render/effect_marching_sower.h"

#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

using Catch::Approx;
using ro::render::EffectGroundDistance;
using ro::render::EffectMarchingSower;
using ro::render::EffectMarchingSowerFor;
using ro::render::EffectMarchingSowerLastSowTick;
using ro::render::EffectMarchingSowerPoint;

namespace {

// 🔴 LE CAP PASSE PAR LES TABLES DU CLIENT : une cible plein est tombe sur un
// cap de moins quatre-vingt-dix, qu'elles lisent un degré de travers. La marche
// part donc d'un degré à côté, et son abscisse ne vaut que le cosinus d'un degré
// de chaque pas.
const float kUnDegreDeTravers = std::cos(glm::radians(1.0F));

// Le trait de Frost Diver, tel que la table le porte.
const EffectMarchingSower& TraitDeFrostDiver() {
  const EffectMarchingSower* found = EffectMarchingSowerFor(27);
  REQUIRE(found != nullptr);
  return *found;
}

}  // namespace

TEST_CASE("Semeur qui marche : le trait de Frost Diver part à cinq et avance de deux",
          "[render][effect]") {
  const EffectMarchingSower& trait = TraitDeFrostDiver();
  CHECK(trait.first_distance == Approx(5.0F));
  CHECK(trait.distance_per_tick == Approx(2.0F));
  CHECK(trait.arrival_distance == Approx(2.5F));

  // ⚠ LA MARCHE EST LINÉAIRE, et c'est un piège : le client écrit un
  // pas de départ ET une accélération, ce qui donnerait cinq, sept, onze, dix-
  // sept si on recopiait les deux étages. Il les RETRANCHE l'un à l'autre à
  // chaque tic, et l'écart entre deux pointes vaut deux, toujours.
  const glm::vec3 source{0.0F, 0.0F, 0.0F};
  const glm::vec3 target{100.0F, 0.0F, 0.0F};
  CHECK(EffectMarchingSowerPoint(trait, source, target, 0).x == Approx(5.0F * kUnDegreDeTravers));
  CHECK(EffectMarchingSowerPoint(trait, source, target, 1).x == Approx(7.0F * kUnDegreDeTravers));
  CHECK(EffectMarchingSowerPoint(trait, source, target, 2).x == Approx(9.0F * kUnDegreDeTravers));
  CHECK(EffectMarchingSowerPoint(trait, source, target, 10).x == Approx(25.0F * kUnDegreDeTravers));
}

TEST_CASE("Semeur qui marche : il va DROIT SUR LA CIBLE, et reste à l'horizontale",
          "[render][effect]") {
  const EffectMarchingSower& trait = TraitDeFrostDiver();
  // Une cible en diagonale, et PLUS HAUTE que la source : le pas du client naît
  // d'une rotation en lacet seule, si bien qu'il n'a pas d'ordonnée. Une pointe
  // semée à mi-chemin garde la hauteur de la SOURCE.
  const glm::vec3 source{10.0F, 3.0F, 10.0F};
  const glm::vec3 target{10.0F + 30.0F, 50.0F, 10.0F + 40.0F};
  const glm::vec3 first = EffectMarchingSowerPoint(trait, source, target, 0);
  CHECK(first.y == Approx(3.0F));
  // Cinq unités vers un triangle trois-quatre-cinq — trente-six degrés et
  // quatre-vingt-sept centièmes —, mais le cap tronqué au degré puis lu un
  // degré de travers : trente-cinq.
  const float cap = glm::radians(35.0F);
  CHECK(first.x == Approx(10.0F + 5.0F * std::sin(cap)));
  CHECK(first.z == Approx(10.0F + 5.0F * std::cos(cap)));
  CHECK(EffectGroundDistance(source, first) == Approx(5.0F));

  // ET LA DISTANCE SE MESURE AU SOL, sans l'ordonnée, comme chez le client.
  CHECK(EffectGroundDistance(source, target) == Approx(50.0F));
}

TEST_CASE("Semeur qui marche : il sème ENCORE au tic de son arrivée", "[render][effect]") {
  const EffectMarchingSower& trait = TraitDeFrostDiver();
  // Cinquante unités à couvrir : le premier pas qui entre dans la bande des deux
  // et demie est le vingt-deuxième — cinq plus deux fois vingt-deux vaut
  // quarante-neuf, à une unité du but.
  CHECK(EffectMarchingSowerLastSowTick(trait, 50.0F) == 22);
  // ⚠ ET IL SÈME CE TIC-LÀ : le client lève son drapeau d'arrivée AVANT de bâtir
  // et bâtit quand même. Un semeur qui s'arrêterait au tic d'avant
  // laisserait un trou de deux unités devant sa cible.
  CHECK(
      EffectMarchingSowerPoint(trait, glm::vec3{0.0F, 0.0F, 0.0F}, glm::vec3{50.0F, 0.0F, 0.0F}, 22)
          .x == Approx(49.0F * kUnDegreDeTravers));

  // Une cible juste au premier pas : il arrive d'emblée, et sème une fois.
  CHECK(EffectMarchingSowerLastSowTick(trait, 5.0F) == 0);
  // Et une cible dans la bande d'arrivée du premier pas aussi — de deux et demie
  // à sept et demie.
  CHECK(EffectMarchingSowerLastSowTick(trait, 7.0F) == 0);
  CHECK(EffectMarchingSowerLastSowTick(trait, 3.0F) == 0);
}

TEST_CASE("Semeur qui marche : une cible hors d'atteinte le fait marcher jusqu'au bout",
          "[render][effect]") {
  const EffectMarchingSower& trait = TraitDeFrostDiver();
  // 🔴 UNE CIBLE TROP PROCHE EST DERRIÈRE LE PREMIER PAS. À deux unités, le
  // semeur naît déjà trois unités trop loin et s'en écarte ensuite : il
  // n'arrivera jamais, et le client le laisse marcher jusqu'à la fin de la vie
  // de l'effet. Rendre zéro ici — l'arrivée la plus proche — le ferait semer une
  // seule fois, ce qui ressemble à une figure oubliée plutôt qu'à ce cas-ci.
  CHECK(EffectMarchingSowerLastSowTick(trait, 2.0F) == trait.last_tick);
  CHECK(EffectMarchingSowerLastSowTick(trait, 0.0F) == trait.last_tick);
  // Et une cible si lointaine que la vie de l'effet s'épuise avant l'arrivée.
  CHECK(EffectMarchingSowerLastSowTick(trait, 10000.0F) == trait.last_tick);
}

TEST_CASE("Semeur qui marche : deux effets seulement, et le reste ne marche pas",
          "[render][effect]") {
  // La variante d'apparat partage la fonction du client, et ses chiffres avec.
  const EffectMarchingSower* apparat = EffectMarchingSowerFor(801);
  REQUIRE(apparat != nullptr);
  CHECK(apparat->first_distance == Approx(TraitDeFrostDiver().first_distance));
  CHECK(apparat->distance_per_tick == Approx(TraitDeFrostDiver().distance_per_tick));

  // ⚠ LES POINTES D'IMPACT NE MARCHENT PAS : l'effet 28 pose sa couronne d'un
  // coup sur la victime. Les confondre ferait partir la gerbe en procession.
  CHECK(EffectMarchingSowerFor(28) == nullptr);
  CHECK(EffectMarchingSowerFor(51) == nullptr);
  CHECK(EffectMarchingSowerFor(0) == nullptr);
}

TEST_CASE("Semeur qui marche : sans direction, il reste à sa source", "[render][effect]") {
  const EffectMarchingSower& trait = TraitDeFrostDiver();
  // Une cible confondue avec la source ne donne aucune direction : le point du
  // semis reste la source, plutôt que de partir sur une division par zéro.
  const glm::vec3 source{7.0F, 2.0F, -4.0F};
  const glm::vec3 pose = EffectMarchingSowerPoint(trait, source, source, 5);
  CHECK(pose.x == Approx(source.x));
  CHECK(pose.y == Approx(source.y));
  CHECK(pose.z == Approx(source.z));
}
