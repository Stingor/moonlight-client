#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

using ro::render::AdvanceRisingParticleTrack;
using ro::render::EffectTrack;

namespace {

// La piste que le bâtisseur du sous-type 43 sème : allumée, alpha nul, rayon de
// douze, un pas de croissance d'une unité en première case, et un retard.
EffectTrack SownParticle(std::int32_t delay_ticks = -18, std::uint8_t mark = 0) {
  EffectTrack track;
  track.active = true;
  track.alpha = 0;
  track.radius_at_anchor = 12.0F;
  track.value[0] = 1.0F;
  track.ticks = delay_ticks;
  track.mark[0] = mark;
  return track;
}

void Advance(EffectTrack& track, std::int32_t ticks) {
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceRisingParticleTrack(track);
  }
}

}  // namespace

TEST_CASE("Particule montante : une piste éteinte ne bouge pas", "[render][effecttrack]") {
  EffectTrack track = SownParticle();
  track.active = false;
  Advance(track, 30);

  CHECK(track.ticks == -18);
  CHECK(track.alpha == 0);
}

TEST_CASE("Particule montante : le RETARD ne fait qu'avancer le compteur",
          "[render][effecttrack]") {
  // Le bâtisseur tire un retard de dix-huit à quarante-sept tics pour que les
  // quatre pistes n'arrivent pas ensemble. Pendant ce temps, rien d'autre ne se
  // passe : ni rayon, ni alpha.
  EffectTrack track = SownParticle(-18);
  Advance(track, 18);

  CHECK(track.ticks == 0);
  CHECK(track.alpha == 0);
  CHECK(track.radius_at_anchor == Catch::Approx(12.0F));
}

TEST_CASE("Particule montante : la comparaison de la montée est NON SIGNÉE",
          "[render][effecttrack]") {
  // C'est le piège du cas : le client compare le compteur sans signe, si bien
  // qu'un compteur encore négatif y paraît immense et tombe dans l'autre
  // branche. Le rendre signé ferait monter l'alpha pendant tout le retard, et
  // la particule paraîtrait avant son heure — à pleine teinte, qui plus est.
  EffectTrack track = SownParticle(-40);
  Advance(track, 10);

  CHECK(track.ticks == -30);
  CHECK(track.alpha == 0);
}

TEST_CASE("Particule montante : l'alpha monte DIX FOIS puis s'arrête", "[render][effecttrack]") {
  // Dix pas de quinze font cent cinquante, et la montée s'arrête là. Un pas de
  // plus, et l'alpha dépasserait ce que la teinte peut porter : c'est la durée
  // qui le borne, et non un plafond.
  EffectTrack track = SownParticle(0);
  Advance(track, 10);
  CHECK(track.alpha == 150);

  Advance(track, 5);
  CHECK(track.alpha == 150);
}

TEST_CASE("Particule montante : une piste MARQUÉE monte plus vite", "[render][effecttrack]") {
  // Toute marque non nulle fait monter de vingt-cinq au lieu de quinze. Les deux
  // valeurs sont du client, et la marque sert par ailleurs à choisir la couleur.
  EffectTrack track = SownParticle(0, 1);
  Advance(track, 10);

  CHECK(track.alpha == 250);
}

TEST_CASE("Particule montante : l'alpha redescend passé son seuil, jusqu'à ZÉRO",
          "[render][effecttrack]") {
  // Trois phases qui ne se recouvrent pas : la montée, la pose, la descente.
  // C'est ce qui donne à la particule son apparition franche et sa disparition
  // lente.
  EffectTrack track = SownParticle(0, 1);
  Advance(track, 25);
  CHECK(track.alpha == 250);

  Advance(track, 1);
  CHECK(track.alpha == 243);

  Advance(track, 100);
  CHECK(track.alpha == 0);
}

TEST_CASE("Particule montante : le rayon croît du pas que la PISTE porte",
          "[render][effecttrack]") {
  // Le pas n'est pas une constante du mouvement : il est semé en première case,
  // et deux effets peuvent monter à deux vitesses par le même code.
  EffectTrack track = SownParticle(0);
  track.value[0] = 0.5F;
  Advance(track, 8);

  CHECK(track.radius_at_anchor == Catch::Approx(12.0F + 4.0F));
}

TEST_CASE("Particule montante : seule la marque lente fait tourner l'AZIMUT",
          "[render][effecttrack]") {
  // L'azimut n'avance que pour une marque précise ; les autres pistes gardent
  // celui que le bâtisseur leur a tiré.
  EffectTrack immobile = SownParticle(0, 1);
  immobile.azimuth_degrees = 100;
  Advance(immobile, 10);
  CHECK(immobile.azimuth_degrees == 100);

  EffectTrack rotated = SownParticle(0, 2);
  rotated.azimuth_degrees = 100;
  Advance(rotated, 10);
  CHECK(rotated.azimuth_degrees == 130);
}

TEST_CASE("Particule montante : l'azimut SE REPLIE même sans marque lente",
          "[render][effecttrack]") {
  // Le client pose son repli APRÈS le test, donc il vaut pour toutes les pistes.
  // Un azimut semé tout près du tour entier déborderait sinon — et le bâtisseur
  // le tire jusqu'à trois cent cinquante-neuf.
  EffectTrack track = SownParticle(0, 1);
  track.azimuth_degrees = 370;
  Advance(track, 1);

  CHECK(track.azimuth_degrees == 10);
}

TEST_CASE("Particule montante : l'ANGLE avance à chaque tic et se replie",
          "[render][effecttrack]") {
  // La troisième case porte un angle, pas une longueur : c'est la figure qui
  // fait tourner son quad avec. Cinq degrés par tic, trois pour la marque lente.
  EffectTrack quick = SownParticle(0, 1);
  quick.value[2] = 0.0F;
  Advance(quick, 4);
  CHECK(quick.value[2] == Catch::Approx(20.0F));

  EffectTrack slow = SownParticle(0, 2);
  slow.value[2] = 0.0F;
  Advance(slow, 4);
  CHECK(slow.value[2] == Catch::Approx(12.0F));

  EffectTrack at_edge = SownParticle(0, 1);
  at_edge.value[2] = 357.0F;
  Advance(at_edge, 1);
  CHECK(at_edge.value[2] == Catch::Approx(2.0F));
}
