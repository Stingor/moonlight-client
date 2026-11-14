#include "ro/render/effect_track_figures.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"
#include "ro/render/effect_trail_palette.h"

using ro::render::ComposeEffectTrackSkirt;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTrackMaxQuads;
using ro::render::kEffectTrackSlots;

namespace {

// La variante que le sous-type 30 emploie le plus, et que la palette laisse
// blanche : la figure s'éprouve sans que la teinte entre en jeu.
constexpr std::uint8_t kPlainVariant = 5;

// La piste du sous-type 30, une fois que son mouvement lui a donné des
// longueurs : tour entier, inclinaison forte, rayon de quatre.
EffectTrack OpenedSkirt() {
  EffectTrack track;
  track.active = true;
  track.alpha = 200;
  track.degrees = 360;
  track.azimuth_degrees = 0;
  track.tilt_degrees = 80;
  track.radius_at_anchor = 4.0F;
  track.value.fill(2.0F);
  return track;
}

// L'aire d'un quad, par le produit vectoriel de ses deux côtés. C'est le
// critère du banc : une figure qui n'a pas d'aire ne se voit pas.
float AreaOf(const EffectWorldQuad& quad) {
  const glm::vec3 cote = quad.corners[1] - quad.corners[0];
  const glm::vec3 other = quad.corners[3] - quad.corners[0];
  return glm::length(glm::cross(cote, other));
}

}  // namespace

TEST_CASE("Jupe : une piste éteinte ne dessine RIEN", "[render][effecttrack]") {
  // Un nœud n'allume pas ses quatre pistes. Dessiner celles qu'il laisse
  // éteintes montrerait quatre anneaux là où le client en montre un.
  EffectTrack track = OpenedSkirt();
  track.active = false;

  CHECK(ComposeEffectTrackSkirt(track, kPlainVariant, {}).empty());
}

TEST_CASE("Jupe : un balayage NÉGATIF n'est pas un arc à l'envers", "[render][effecttrack]") {
  // Le client teste le bit de signe et abandonne la piste. Le prendre pour un
  // sens de rotation ferait tourner un ruban qui ne doit pas paraître.
  EffectTrack track = OpenedSkirt();
  track.degrees = -90;

  CHECK(ComposeEffectTrackSkirt(track, kPlainVariant, {}).empty());
}

TEST_CASE("Jupe : un alpha éteint ne coûte aucun quad", "[render][effecttrack]") {
  // Le mouvement éteint la piste entre deux pulsations du portail. Soumettre
  // ses quads pendant ce temps paierait un lot de rendu pour rien.
  EffectTrack track = OpenedSkirt();
  track.alpha = 0;

  CHECK(ComposeEffectTrackSkirt(track, kPlainVariant, {}).empty());
}

TEST_CASE("Jupe : le tour entier donne un quad par intervalle", "[render][effecttrack]") {
  // Vingt et un segments font vingt quads : un segment ne fait un quad qu'avec
  // son voisin.
  const std::vector<EffectWorldQuad> quads =
      ComposeEffectTrackSkirt(OpenedSkirt(), kPlainVariant, {});

  CHECK(quads.size() == kEffectTrackMaxQuads);
}

TEST_CASE("Jupe : le ruban SE REFERME exactement sur le tour", "[render][effecttrack]") {
  // Le client remet l'angle de départ au dernier segment au lieu de
  // l'accumuler. Sans cela, vingt pas de dix-huit degrés laisseraient une fente
  // large de l'arrondi entre le premier quad et le dernier.
  const std::vector<EffectWorldQuad> quads =
      ComposeEffectTrackSkirt(OpenedSkirt(), kPlainVariant, {});
  REQUIRE(quads.size() >= 2);

  const glm::vec3 start = quads.front().corners[0];
  const glm::vec3 arrivee = quads.back().corners[1];
  CHECK(arrivee.x == Catch::Approx(start.x));
  CHECK(arrivee.y == Catch::Approx(start.y));
  CHECK(arrivee.z == Catch::Approx(start.z));
}

TEST_CASE("Jupe : CHAQUE quad a une aire", "[render][effecttrack]") {
  // Une entrée peut être complète au sens du crible et ne rien dessiner quand
  // même : c'est ce que le test de la table des patrons a appris au lot 5.
  const std::vector<EffectWorldQuad> quads =
      ComposeEffectTrackSkirt(OpenedSkirt(), kPlainVariant, {});
  REQUIRE(!quads.empty());

  for (const EffectWorldQuad& quad : quads) {
    CHECK(AreaOf(quad) > 0.0F);
  }
}

TEST_CASE("Jupe : le bord extérieur MONTE quand l'inclinaison est forte", "[render][effecttrack]") {
  // C'est ce qui fait la jupe, et c'est ce que le nom « Ring_0 » ne dit pas :
  // à quatre-vingts degrés, le segment lève le bord bien plus qu'il ne
  // l'éloigne. Un dessinateur qui poserait les deux bords au même plan
  // montrerait une couronne plate.
  const std::vector<EffectWorldQuad> quads =
      ComposeEffectTrackSkirt(OpenedSkirt(), kPlainVariant, {});
  REQUIRE(!quads.empty());

  const EffectWorldQuad& quad = quads.front();
  const glm::vec3 inner = quad.corners[0];
  const glm::vec3 outer = quad.corners[3];
  CHECK(outer.y > inner.y);

  const float rise = outer.y - inner.y;
  const float ecarte =
      glm::length(glm::vec2{outer.x, outer.z}) - glm::length(glm::vec2{inner.x, inner.z});
  CHECK(rise > ecarte);
}

TEST_CASE("Jupe : la CLÉ de décalage soulève les deux bords", "[render][effecttrack]") {
  // Le client compare la clé à deux flottants exacts. Vingt-cinq — ce que pose
  // le sous-type 30 — n'en est pas un, et ne décale rien : la lire comme une
  // hauteur soulèverait l'anneau de vingt-cinq unités.
  EffectTrack without_key = OpenedSkirt();
  without_key.lift_key = 25.0F;
  const std::vector<EffectWorldQuad> pose = ComposeEffectTrackSkirt(without_key, kPlainVariant, {});

  EffectTrack with_key = OpenedSkirt();
  with_key.lift_key = 6.001F;
  const std::vector<EffectWorldQuad> lifted = ComposeEffectTrackSkirt(with_key, kPlainVariant, {});

  REQUIRE(!pose.empty());
  REQUIRE(pose.size() == lifted.size());
  CHECK(pose.front().corners[0].y == Catch::Approx(0.0F));
  CHECK(lifted.front().corners[0].y == Catch::Approx(2.0F));
  CHECK(lifted.front().corners[3].y == Catch::Approx(pose.front().corners[3].y + 2.0F));
}

TEST_CASE("Jupe : la teinte vient de la VARIANTE, et son alpha l'emporte",
          "[render][effecttrack]") {
  // La teinte appartient à la primitive de ruban, pas à la figure. La variante
  // 18 est le rouge pur, la 3 impose son alpha quoi que porte la piste.
  const std::vector<EffectWorldQuad> rouges = ComposeEffectTrackSkirt(OpenedSkirt(), 18, {});
  REQUIRE(!rouges.empty());
  CHECK(rouges.front().tint[0] == 255);
  CHECK(rouges.front().tint[1] == 0);
  CHECK(rouges.front().tint[2] == 0);
  CHECK(rouges.front().tint[3] == 200);

  const std::vector<EffectWorldQuad> imposes = ComposeEffectTrackSkirt(OpenedSkirt(), 3, {});
  REQUIRE(!imposes.empty());
  CHECK(imposes.front().tint[3] == 80);
}

TEST_CASE("Jupe : la texture court LE LONG du ruban, pas au travers", "[render][effecttrack]") {
  // L'abscisse avance d'un segment au suivant ; l'ordonnée traverse la bande,
  // un au bord intérieur et zéro à l'extérieur. Les échanger étirerait l'image
  // sur tout le tour au lieu de la dérouler.
  const std::vector<EffectWorldQuad> quads =
      ComposeEffectTrackSkirt(OpenedSkirt(), kPlainVariant, {});
  REQUIRE(quads.size() >= 2);

  const EffectWorldQuad& first = quads.front();
  CHECK(first.texture_uv[0].y == Catch::Approx(1.0F));
  CHECK(first.texture_uv[3].y == Catch::Approx(0.0F));
  CHECK(first.texture_uv[1].x > first.texture_uv[0].x);
  CHECK(quads[1].texture_uv[0].x > first.texture_uv[0].x);

  const float last_u = quads.back().texture_uv[1].x;
  CHECK(last_u <= 1.0F);
  CHECK(last_u == Catch::Approx(static_cast<float>(kEffectTrackSlots - 1) /
                                static_cast<float>(kEffectTrackSlots)));
}
