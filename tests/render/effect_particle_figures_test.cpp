#include "ro/render/effect_particle_figures.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"

using ro::render::Camera;
using ro::render::ComposeRisingParticleQuad;
using ro::render::EffectBillboardTiltDegrees;
using ro::render::EffectBillboardTurnDegrees;
using ro::render::EffectParticleAppearance;
using ro::render::EffectRisingParticleAppearance;
using ro::render::EffectRisingParticleIsDrawn;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;

namespace {

// La marque zéro : elle dessine, ne fait pas face à la caméra, et n'aplatit pas
// sa profondeur. De quoi éprouver la géométrie sans que la marque s'en mêle.
constexpr std::uint8_t kPlainMark = 0;
// La seule marque de la table qui ne figure pas : dix cas, donc dix est dehors.
constexpr std::uint8_t kMarkWithoutCase = 10;
// Une marque qui fait face à la caméra.
constexpr std::uint8_t kFacingMark = 2;

// Une piste que son mouvement a fait vivre : alpha monté, rayon grandi, et une
// demi-diagonale semée en deuxième case.
EffectTrack LivingParticle(std::uint8_t mark = kPlainMark) {
  EffectTrack track;
  track.active = true;
  track.alpha = 150;
  track.radius_at_anchor = 20.0F;
  track.degrees = 0;
  track.tilt_degrees = 0;
  track.azimuth_degrees = 0;
  track.value[1] = 2.0F;
  track.value[2] = 0.0F;
  track.mark[0] = mark;
  return track;
}

float AreaOf(const EffectWorldQuad& quad) {
  return glm::length(
      glm::cross(quad.corners[1] - quad.corners[0], quad.corners[3] - quad.corners[0]));
}

}  // namespace

TEST_CASE("Particule montante : une marque SANS CAS ne dessine rien", "[render][particlefigure]") {
  // Le cas par défaut du client saute l'émission entière. Poser un quad blanc à
  // la place montrerait ce qu'il cache, et une particule blanche ressemble à une
  // texture manquante bien plus qu'à une absence voulue.
  CHECK(EffectRisingParticleIsDrawn(0));
  CHECK(EffectRisingParticleIsDrawn(9));
  CHECK_FALSE(EffectRisingParticleIsDrawn(kMarkWithoutCase));
  CHECK_FALSE(ComposeRisingParticleQuad(LivingParticle(kMarkWithoutCase), 0, 0).has_value());
}

TEST_CASE("Particule montante : une piste éteinte ou transparente ne dessine rien",
          "[render][particlefigure]") {
  EffectTrack faded = LivingParticle();
  faded.active = false;
  CHECK_FALSE(ComposeRisingParticleQuad(faded, 0, 0).has_value());

  EffectTrack transparente = LivingParticle();
  transparente.alpha = 0;
  CHECK_FALSE(ComposeRisingParticleQuad(transparente, 0, 0).has_value());
}

TEST_CASE("Particule montante : la MARQUE décide trois choses à la fois",
          "[render][particlefigure]") {
  // Couleur, orientation et profondeur, sur un même octet, et aucune ne se
  // déduit des autres : la marque 6 partage sa couleur avec la 2 mais pas son
  // orientation, et la 8 partage la sienne avec la 3 sans partager la sienne.
  CHECK(EffectRisingParticleAppearance(4).color == std::array<std::uint8_t, 3>{255, 50, 50});
  CHECK(EffectRisingParticleAppearance(5).color == std::array<std::uint8_t, 3>{50, 255, 50});
  CHECK(EffectRisingParticleAppearance(2).color == EffectRisingParticleAppearance(6).color);
  CHECK(EffectRisingParticleAppearance(2).faces_camera);
  CHECK_FALSE(EffectRisingParticleAppearance(6).faces_camera);
  CHECK(EffectRisingParticleAppearance(3).color == EffectRisingParticleAppearance(8).color);
  CHECK_FALSE(EffectRisingParticleAppearance(3).faces_camera);
  CHECK(EffectRisingParticleAppearance(8).faces_camera);
  CHECK(EffectRisingParticleAppearance(7).flattened_depth);
  CHECK_FALSE(EffectRisingParticleAppearance(0).flattened_depth);
}

TEST_CASE("Particule montante : la teinte de la marque arrive au quad",
          "[render][particlefigure]") {
  const std::optional<EffectWorldQuad> quad = ComposeRisingParticleQuad(LivingParticle(4), 0, 0);
  REQUIRE(quad.has_value());
  CHECK(quad->tint[0] == 255);
  CHECK(quad->tint[1] == 50);
  CHECK(quad->tint[2] == 50);
  CHECK(quad->tint[3] == 150);
  // Le dessinateur passe le mode zéro : l'ADDITION, et non l'absence de mode.
  CHECK(quad->blend_source == 5);
  CHECK(quad->blend_destination == 2);
}

TEST_CASE("Particule montante : le centre s'ÉLOIGNE de l'ancre avec le rayon",
          "[render][particlefigure]") {
  // C'est tout le mouvement de la figure : quatre particules sur une sphère qui
  // grandit. Un centre fixe les laisserait toutes empilées sur l'ancre.
  EffectTrack near_point = LivingParticle();
  near_point.radius_at_anchor = 5.0F;
  EffectTrack far_point = LivingParticle();
  far_point.radius_at_anchor = 50.0F;

  const std::optional<EffectWorldQuad> un = ComposeRisingParticleQuad(near_point, 0, 0);
  const std::optional<EffectWorldQuad> other = ComposeRisingParticleQuad(far_point, 0, 0);
  REQUIRE(un.has_value());
  REQUIRE(other.has_value());
  CHECK(glm::length(other->corners[0]) > glm::length(un->corners[0]));
}

TEST_CASE("Particule montante : deux ANGLES de piste envoient dans deux directions",
          "[render][particlefigure]") {
  // Le bâtisseur tire ces deux angles au sort pour chacune des quatre pistes.
  // Les ignorer ferait partir les quatre dans la même direction.
  EffectTrack est = LivingParticle();
  EffectTrack nord = LivingParticle();
  nord.tilt_degrees = 90;

  const std::optional<EffectWorldQuad> one = ComposeRisingParticleQuad(est, 0, 0);
  const std::optional<EffectWorldQuad> other = ComposeRisingParticleQuad(nord, 0, 0);
  REQUIRE(one.has_value());
  REQUIRE(other.has_value());
  CHECK(one->corners[0].x != Catch::Approx(other->corners[0].x));
  CHECK(one->corners[0].z != Catch::Approx(other->corners[0].z));
}

TEST_CASE("Particule montante : l'ANCRE porte la figure, sans se retourner",
          "[render][particlefigure]") {
  // L'ancre est dans notre repère, le reste dans celui du client. Retourner
  // l'ancre aussi renverrait toute la figure sous le sol.
  EffectTrack track = LivingParticle();
  track.anchor = glm::vec3{10.0F, 7.0F, -3.0F};
  const std::optional<EffectWorldQuad> quad = ComposeRisingParticleQuad(track, 0, 0);
  REQUIRE(quad.has_value());

  glm::vec3 centre{0.0F, 0.0F, 0.0F};
  for (const glm::vec3& corner : quad->corners) {
    centre += corner;
  }
  centre /= 4.0F;
  CHECK(centre.y == Catch::Approx(7.0F));
  CHECK(centre.x == Catch::Approx(30.0F));
}

TEST_CASE("Particule montante : le carré a une AIRE, et de la demi-diagonale",
          "[render][particlefigure]") {
  // La deuxième case porte la demi-diagonale, et c'est la seule case qui donne
  // sa taille au quad. Une figure sans aire ne se distingue pas d'une absence.
  const std::optional<EffectWorldQuad> little = ComposeRisingParticleQuad(LivingParticle(), 0, 0);
  REQUIRE(little.has_value());
  CHECK(AreaOf(*little) > 0.0F);

  EffectTrack gros_track = LivingParticle();
  gros_track.value[1] = 4.0F;
  const std::optional<EffectWorldQuad> gros = ComposeRisingParticleQuad(gros_track, 0, 0);
  REQUIRE(gros.has_value());
  CHECK(AreaOf(*gros) == Catch::Approx(AreaOf(*little) * 4.0F));
}

TEST_CASE("Particule montante : un carré DEBOUT tourne sur place", "[render][particlefigure]") {
  // Sept marques sur dix donnent un carré debout, que son propre angle fait
  // tourner autour de la verticale. Sans lui, la particule serait figée face à
  // une direction que rien ne choisit.
  EffectTrack start = LivingParticle();
  EffectTrack tournee = LivingParticle();
  tournee.value[2] = 90.0F;

  const std::optional<EffectWorldQuad> un = ComposeRisingParticleQuad(start, 0, 0);
  const std::optional<EffectWorldQuad> other = ComposeRisingParticleQuad(tournee, 0, 0);
  REQUIRE(un.has_value());
  REQUIRE(other.has_value());
  CHECK(AreaOf(*other) == Catch::Approx(AreaOf(*un)));
  CHECK(un->corners[0].x != Catch::Approx(other->corners[0].x));
}

TEST_CASE("Particule montante : un carré COUCHÉ suit la caméra", "[render][particlefigure]") {
  // Trois marques sur dix donnent un billboard. Les deux angles entrent alors
  // dans la figure, et deux caméras différentes donnent deux quads différents.
  const std::optional<EffectWorldQuad> devant =
      ComposeRisingParticleQuad(LivingParticle(kFacingMark), 0, 0);
  const std::optional<EffectWorldQuad> de_cote =
      ComposeRisingParticleQuad(LivingParticle(kFacingMark), 90, 45);
  REQUIRE(devant.has_value());
  REQUIRE(de_cote.has_value());
  CHECK(AreaOf(*de_cote) == Catch::Approx(AreaOf(*devant)));
  CHECK(devant->corners[0].z != Catch::Approx(de_cote->corners[0].z));

  // Un carré DEBOUT, lui, ne bouge pas quand la caméra tourne.
  const std::optional<EffectWorldQuad> fixed = ComposeRisingParticleQuad(LivingParticle(), 90, 45);
  const std::optional<EffectWorldQuad> pareil = ComposeRisingParticleQuad(LivingParticle(), 0, 0);
  REQUIRE(fixed.has_value());
  REQUIRE(pareil.has_value());
  CHECK(fixed->corners[0].x == Catch::Approx(pareil->corners[0].x));
}

TEST_CASE("Particule montante : les angles sont les COMPLÉMENTS de ceux de la caméra",
          "[render][particlefigure]") {
  // Le client pose trois cent soixante moins SON lacet, et quatre cent
  // cinquante moins SON altitude. Son lacet est l'opposé du nôtre, son altitude
  // l'opposé de notre élévation : `360 + 30` et `450 + 45`, repliés.
  Camera camera;
  camera.yaw_degrees = 30.0F;
  camera.pitch_degrees = 45.0F;
  CHECK(EffectBillboardTurnDegrees(camera) == 30);
  CHECK(EffectBillboardTiltDegrees(camera) == 135);

  // Et ils se replient sur un tour : une rotation nulle ne doit pas donner
  // trois cent soixante, qui indexerait au-delà de la table du client.
  camera.yaw_degrees = 0.0F;
  CHECK(EffectBillboardTurnDegrees(camera) == 0);
}
