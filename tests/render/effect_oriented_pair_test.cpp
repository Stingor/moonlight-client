#include "ro/render/effect_oriented_pair.h"

#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::ComposeOrientedPairQuads;
using ro::render::EffectOrientedPairRequest;
using ro::render::EffectOrientedPairRequestsOf;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;

namespace {

[[nodiscard]] EffectOrientedPairRequest Request(std::uint8_t mark, bool second = false) {
  EffectOrientedPairRequest request;
  request.radius = 10.0F;
  request.alpha = 120;
  request.mark = mark;
  request.is_second_position = second;
  // Les deux angles a ZERO : la rotation partagee de la voie A est alors
  // l'identite, et les coins se lisent tels que la figure les pose. C'est la
  // seule facon d'eprouver la GEOMETRIE sans eprouver aussi la rotation, qui a
  // ses propres tests.
  request.turn_degrees = 0;
  request.tilt_degrees = 0;
  return request;
}

}  // namespace

TEST_CASE("la paire peint DEUX quads, dont un pale", "[render][effect][pair]") {
  const std::vector<EffectWorldQuad> quads = ComposeOrientedPairQuads(Request(0), {});
  REQUIRE(quads.size() == 2);
  // ⚠ LE PALE N'A QUE LE TIERS DE L'ALPHA, et il passe le melange UN quand le
  // franc passe zero. Les peindre au meme alpha doublerait l'opacite.
  CHECK(quads[0].tint[3] == 40);
  CHECK(quads[1].tint[3] == 120);
  CHECK(quads[0].tint[0] == 255);
  CHECK(quads[0].tint[2] == 255);
}

TEST_CASE("la teinte du quad franc suit la marque", "[render][effect][pair]") {
  // Rouge pour la marque un, bleu pour toutes les autres.
  CHECK(ComposeOrientedPairQuads(Request(1), {})[1].tint[0] == 255);
  CHECK(ComposeOrientedPairQuads(Request(1), {})[1].tint[2] == 50);
  CHECK(ComposeOrientedPairQuads(Request(0), {})[1].tint[0] == 50);
  CHECK(ComposeOrientedPairQuads(Request(0), {})[1].tint[2] == 255);
}

TEST_CASE("deux marques ne peignent qu'UN quad, retourne", "[render][effect][pair]") {
  const std::vector<EffectWorldQuad> two = ComposeOrientedPairQuads(Request(2), {});
  REQUIRE(two.size() == 1);
  CHECK(two.front().tint[3] == 120);
  // ⚠ SES COINS SONT ECHANGES DEUX A DEUX : l'image se retourne. Garder l'ordre
  // ordinaire remettrait ces deux marques a l'endroit, et elles ne se
  // distingueraient plus des autres.
  const std::vector<EffectWorldQuad> straight = ComposeOrientedPairQuads(Request(0), {});
  CHECK(two.front().corners[0] == straight.back().corners[1]);
  CHECK(two.front().corners[1] == straight.back().corners[0]);
}

TEST_CASE("la marque trois est MUETTE a la seconde position", "[render][effect][pair]") {
  // ⚠ LA PREMIERE LA TRAITE COMME LA DEUX, LA SECONDE L'ECARTE. Traiter les
  // deux pareil ajouterait un carre que le client ne montre pas.
  CHECK(ComposeOrientedPairQuads(Request(3, false), {}).size() == 1);
  CHECK(ComposeOrientedPairQuads(Request(3, true), {}).empty());
  // Et la marque deux, elle, parle aux deux positions.
  CHECK(ComposeOrientedPairQuads(Request(2, true), {}).size() == 1);
}

TEST_CASE("les quatre coins sont un carre tourne d'un huitieme de tour", "[render][effect][pair]") {
  const EffectWorldQuad quad = ComposeOrientedPairQuads(Request(0), {}).front();
  // Les deux diagonales valent deux rayons, et les quatre cotes sont egaux.
  const auto length = [](const glm::vec3& a, const glm::vec3& b) { return glm::length(a - b); };
  CHECK(length(quad.corners[0], quad.corners[2]) == Approx(20.0F));
  CHECK(length(quad.corners[0], quad.corners[1]) ==
        Approx(length(quad.corners[1], quad.corners[2])));
  // ⚠ ET LE PREMIER COIN EST A QUARANTE-CINQ DEGRES, non a zero : un carre
  // DROIT aurait un coin sur l'axe. Celui-ci a ses COTES parallèles aux axes.
  const float expected = 10.0F * std::cos(45.0F * 3.14159265F / 180.0F);
  CHECK(std::abs(quad.corners[0].x) == Approx(expected).margin(1e-3));
}

TEST_CASE("une position sans alpha ni rayon ne rend rien", "[render][effect][pair]") {
  EffectOrientedPairRequest silent = Request(0);
  silent.alpha = 0;
  CHECK(ComposeOrientedPairQuads(silent, {}).empty());
  silent = Request(0);
  silent.radius = 0.0F;
  CHECK(ComposeOrientedPairQuads(silent, {}).empty());
}

TEST_CASE("une piste donne DEUX positions et DEUX alphas", "[render][effect][pair]") {
  EffectTrack track;
  track.active = true;
  track.radius_at_anchor = 5.0F;
  track.anchor = {1.0F, 2.0F, 3.0F};
  track.tail[3] = 7.0F;
  track.tail[4] = 8.0F;
  track.tail[5] = 9.0F;
  track.value[12] = 200.0F;
  track.value[13] = 60.0F;
  track.mark[0] = 1;
  const std::array<EffectOrientedPairRequest, 2> requests =
      EffectOrientedPairRequestsOf(track, 10, 20);
  // ⚠ LA PREMIERE VIENT DE LA QUEUE, la seconde de l'ANCRE. Une seule position
  // en perdrait la moitie, et un seul alpha les eteindrait ensemble.
  CHECK(requests[0].center == glm::vec3{7.0F, 8.0F, 9.0F});
  CHECK(requests[1].center == glm::vec3{1.0F, 2.0F, 3.0F});
  CHECK(requests[0].alpha == 200);
  CHECK(requests[1].alpha == 60);
  CHECK_FALSE(requests[0].is_second_position);
  CHECK(requests[1].is_second_position);

  // Une piste eteinte rend deux positions muettes.
  track.active = false;
  CHECK(EffectOrientedPairRequestsOf(track, 10, 20)[0].alpha == 0);
}
