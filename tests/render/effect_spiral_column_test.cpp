#include "ro/render/effect_spiral_column.h"

#include <cmath>
#include <numbers>
#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_scene.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_burst.h"

using Catch::Approx;
using ro::render::AdvanceSpiralColumnTracks;
using ro::render::ComposeSpiralColumnQuads;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectColumnFullAtTick;
using ro::render::kEffectColumnOpensAtTick;
using ro::render::kEffectTracksPerNode;
using ro::render::kSpiralColumnInnerAlphaDivisor;
using ro::render::kSpiralColumnQuadsPerPass;

namespace {

// UNE PISTE COMME LE BÂTISSEUR LA SÈME : un cercle minuscule, une clé énorme,
// une inclinaison droite.
[[nodiscard]] EffectTrack Semee() {
  EffectTrack track;
  track.active = true;
  track.degrees = 360;
  track.azimuth_degrees = 0;
  track.tilt_degrees = 90;
  track.alpha = 150;
  track.radius_at_anchor = 0.2F;
  track.lift_key = 120.0F;
  track.ticks = 60;
  return track;
}

[[nodiscard]] EffectTrack Advanced(std::int32_t ticks, EffectTrack track = Semee()) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = track;
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceSpiralColumnTracks(tracks);
  }
  return tracks[0];
}

}  // namespace

TEST_CASE("la colonne ne montre rien avant le centieme tic", "[render][effect][column]") {
  // ⚠ ELLE EXISTE, TOURNE ET PORTE SON ALPHA, et ne se dessine pas : les vingt
  // et une cases valent zéro. Croire à une panne de semis ferait chercher au
  // mauvais endroit — c'est une ATTENTE, et elle est voulue.
  const EffectTrack before = Advanced(39);
  CHECK(before.ticks == 99);
  CHECK(before.value[0] == 0.0F);
  CHECK(ComposeSpiralColumnQuads(before, 4, {}).empty());
}

TEST_CASE("la colonne s'ouvre en un quart de sinus", "[render][effect][column]") {
  // Au centième tic exactement, le sinus de zéro : encore rien.
  CHECK(Advanced(40).ticks == kEffectColumnOpensAtTick);
  CHECK(Advanced(40).value[0] == Approx(0.0F).margin(1e-4));

  // À mi-chemin — huit tics plus tard —, le sinus de quarante-huit degrés.
  const float expected = 120.0F * std::sin(48.0F * std::numbers::pi_v<float> / 180.0F);
  CHECK(Advanced(48).value[0] == Approx(expected));

  // ⚠ ET LES DEUX BRANCHES SE REJOIGNENT SANS SAUT au cent quinzième tic : le
  // sinus de quatre-vingt-dix vaut un, et la branche suivante rend la clé
  // entière. C'est cette continuité qui prouve que la « table » du client est
  // un sinus, et non une donnée qu'on n'aurait pas su lire.
  CHECK(Advanced(55).ticks == kEffectColumnFullAtTick);
  CHECK(Advanced(55).value[0] == Approx(120.0F));
  CHECK(Advanced(56).value[0] == Approx(120.0F));
}

TEST_CASE("la colonne tourne d'un degre par tic", "[render][effect][column]") {
  CHECK(Advanced(10).azimuth_degrees == 10);
  // Et l'azimut se ramène dans le tour au lieu de déborder.
  EffectTrack presque = Semee();
  presque.azimuth_degrees = 358;
  CHECK(Advanced(5, presque).azimuth_degrees == 3);
}

TEST_CASE("la colonne s'eteint par l'alpha ET par la hauteur", "[render][effect][column]") {
  // Au cent vingt-cinquième tic, les deux commencent à tomber ensemble.
  const EffectTrack au_seuil = Advanced(65);
  CHECK(au_seuil.ticks == 125);
  CHECK(au_seuil.alpha == 150 - 15);
  CHECK(au_seuil.lift_key == Approx(115.0F));
  // Et la hauteur suit la clé : c'est elle que les cases reçoivent.
  CHECK(au_seuil.value[0] == Approx(115.0F));

  // Aucune des deux ne passe sous zéro.
  const EffectTrack late = Advanced(100);
  CHECK(late.alpha == 0);
  CHECK(late.lift_key == Approx(0.0F));
}

TEST_CASE("la colonne sort par DEUX voies", "[render][effect][column]") {
  const EffectTrack opened = Advanced(56);
  const std::vector<EffectWorldQuad> quads = ComposeSpiralColumnQuads(opened, 4, {});
  // ⚠ QUATRE QUADS PAR PASSE, ET DEUX PASSES. N'en porter qu'une donnerait la
  // moitié de la figure, et rien ne le dirait.
  REQUIRE(quads.size() == 2 * kSpiralColumnQuadsPerPass);

  // La passe intérieure est ROUGE en dur, et son alpha vaut le cinquième.
  CHECK(quads.front().tint[0] == 255);
  CHECK(quads.front().tint[1] == 10);
  CHECK(quads.front().tint[2] == 10);
  CHECK(quads.front().tint[3] == opened.alpha / kSpiralColumnInnerAlphaDivisor);

  // La passe extérieure prend sa teinte dans la palette, et l'alpha entier.
  CHECK(quads.back().tint[3] == opened.alpha);
}

TEST_CASE("la passe exterieure est plus large et tournee", "[render][effect][column]") {
  EffectTrack track = Advanced(56);
  track.radius_at_anchor = 10.0F;
  const std::vector<EffectWorldQuad> quads = ComposeSpiralColumnQuads(track, 4, {});
  REQUIRE(quads.size() == 2 * kSpiralColumnQuadsPerPass);
  // Le premier coin de chaque passe est sur son propre cercle : celui de
  // l'extérieure est une fois et demie plus loin de l'axe.
  const auto radius = [](const glm::vec3& point) {
    return std::sqrt(point.x * point.x + point.z * point.z);
  };
  CHECK(radius(quads[kSpiralColumnQuadsPerPass].corners[0]) ==
        Approx(1.5F * radius(quads[0].corners[0])));
  // Et elle ne part pas du même angle : quarante-cinq degrés plus loin.
  CHECK(quads[kSpiralColumnQuadsPerPass].corners[0].z != Approx(quads[0].corners[0].z));
}

TEST_CASE("la colonne monte au lieu de s'enfoncer", "[render][effect][column]") {
  // ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND. À quatre-vingt-dix degrés
  // d'inclinaison, le sommet d'un segment est AU-DESSUS de sa base ; recopier
  // le signe du client enfoncerait la colonne dans le sol.
  const std::vector<EffectWorldQuad> quads = ComposeSpiralColumnQuads(Advanced(56), 4, {});
  REQUIRE_FALSE(quads.empty());
  // Les coins deux et trois sont les sommets, les coins zéro et un les bases.
  CHECK(quads.front().corners[3].y > quads.front().corners[0].y);
  CHECK(quads.front().corners[3].y == Approx(120.0F));
}

TEST_CASE("la colonne penche vers sa cible", "[render][effect][column]") {
  // ⚠ LES TROIS DERNIÈRES CASES DE LA QUEUE portent le vecteur vers la cible —
  // c'est la SEULE différence entre les deux bâtisseurs du sous-type. Les
  // ignorer poserait toutes les colonnes droites.
  EffectTrack right = Advanced(56);
  EffectTrack penchee = right;
  penchee.tail[6] = 30.0F;
  penchee.tail[8] = -20.0F;
  const glm::vec3 a = ComposeSpiralColumnQuads(right, 4, {}).front().corners[0];
  const glm::vec3 b = ComposeSpiralColumnQuads(penchee, 4, {}).front().corners[0];
  CHECK(b.x == Approx(a.x + 30.0F));
  CHECK(b.z == Approx(a.z - 20.0F));
}

TEST_CASE("une piste eteinte ou muette ne rend rien", "[render][effect][column]") {
  EffectTrack track = Advanced(56);
  track.active = false;
  CHECK(ComposeSpiralColumnQuads(track, 4, {}).empty());
  track = Advanced(56);
  track.alpha = 0;
  CHECK(ComposeSpiralColumnQuads(track, 4, {}).empty());
  track = Advanced(56);
  track.degrees = -1;
  CHECK(ComposeSpiralColumnQuads(track, 4, {}).empty());
}
