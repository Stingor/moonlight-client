#include "ro/render/effect_particle_quad.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using ro::render::ComposeEffectParticleQuad;
using ro::render::EffectBlendPair;
using ro::render::EffectOwnerTint;
using ro::render::EffectParticleBlend;
using ro::render::EffectParticleQuadRequest;
using ro::render::EffectWorldQuad;
using ro::render::kEffectOwnerTintAbsent;

namespace {

// Un carré d'une unité de côté, posé dans le plan du sol : de quoi éprouver
// l'ancrage et la texture sans que la forme entre en jeu.
EffectParticleQuadRequest UnitQuad() {
  EffectParticleQuadRequest request;
  request.corners = {glm::vec3{0.0F, 0.0F, 0.0F}, glm::vec3{1.0F, 0.0F, 0.0F},
                     glm::vec3{1.0F, 0.0F, 1.0F}, glm::vec3{0.0F, 0.0F, 1.0F}};
  request.color = {10, 20, 30};
  request.alpha = 200;
  return request;
}

}  // namespace

TEST_CASE("Quad projeté : le mode ZÉRO est l'ADDITION, pas une absence", "[render][particlequad]") {
  // Deux cent cinquante-six sites sur trois cent cinquante-huit passent zéro.
  // Le prendre pour « pas de mode » et poser le mélange ordinaire peindrait un
  // carré noir autour de chaque éclair — c'est exactement la panne que l'ADR
  // sur le mélange raconte, et elle ne se voit pas sur un fond noir.
  const EffectBlendPair blend = EffectParticleBlend(0, kEffectOwnerTintAbsent);
  CHECK(blend.source == 5);
  CHECK(blend.destination == 2);
}

TEST_CASE("Quad projeté : un mode non nul mélange ORDINAIREMENT", "[render][particlequad]") {
  // Le deuxième cas le plus fréquent, quatre-vingt-un sites.
  const EffectBlendPair blend = EffectParticleBlend(1, kEffectOwnerTintAbsent);
  CHECK(blend.source == 5);
  CHECK(blend.destination == 6);
}

TEST_CASE("Quad projeté : le mode dix EFFACE ce qu'il couvre", "[render][particlequad]") {
  // Un sur zéro : la destination ne compte pas. C'est le seul mode qui change
  // aussi la source, et le seul par lequel un quad d'effet est opaque.
  const EffectBlendPair blend = EffectParticleBlend(10, kEffectOwnerTintAbsent);
  CHECK(blend.source == 2);
  CHECK(blend.destination == 1);
}

TEST_CASE("Quad projeté : le propriétaire impose sa DESTINATION, jamais sa source",
          "[render][particlequad]") {
  // L'écrasement ne porte que sur le facteur de destination. L'étendre à la
  // source rendrait opaques des quads que le client additionne.
  const EffectBlendPair impose = EffectParticleBlend(0, 6);
  CHECK(impose.source == 5);
  CHECK(impose.destination == 6);
}

TEST_CASE("Quad projeté : le mode quatre-vingt-dix-neuf IGNORE le propriétaire",
          "[render][particlequad]") {
  // Le client lit l'écrasement puis teste ce mode, qui a le dernier mot.
  // L'ordre inverse laisserait le propriétaire gagner, et un seul site l'emploie
  // — donc personne ne s'en apercevrait avant de le voir en jeu.
  const EffectBlendPair force = EffectParticleBlend(99, 2);
  CHECK(force.source == 5);
  CHECK(force.destination == 6);
}

TEST_CASE("Quad projeté : le mode dix l'emporte AUSSI sur le propriétaire",
          "[render][particlequad]") {
  const EffectBlendPair opaque = EffectParticleBlend(10, 6);
  CHECK(opaque.source == 2);
  CHECK(opaque.destination == 1);
}

TEST_CASE("Quad projeté : des coins RELATIFS partent de l'origine du nœud",
          "[render][particlequad]") {
  // Le cas le plus fréquent. Les traiter comme absolus poserait toute la
  // famille à l'origine du monde, c'est-à-dire au coin de la carte.
  const glm::vec3 origin{10.0F, 2.0F, -4.0F};
  const EffectWorldQuad quad = ComposeEffectParticleQuad(UnitQuad(), origin);

  CHECK(quad.corners[0].x == Catch::Approx(origin.x));
  CHECK(quad.corners[0].y == Catch::Approx(origin.y));
  CHECK(quad.corners[0].z == Catch::Approx(origin.z));
  CHECK(quad.corners[2].x == Catch::Approx(origin.x + 1.0F));
  CHECK(quad.corners[2].z == Catch::Approx(origin.z + 1.0F));
}

TEST_CASE("Quad projeté : des coins ABSOLUS ignorent l'origine du nœud", "[render][particlequad]") {
  // Quatre-vingt-quatre sites la posent. Ajouter l'origine quand même ferait
  // partir la figure deux fois plus loin que le nœud.
  EffectParticleQuadRequest request = UnitQuad();
  request.absolute = true;
  const EffectWorldQuad quad = ComposeEffectParticleQuad(request, glm::vec3{10.0F, 2.0F, -4.0F});

  CHECK(quad.corners[0].x == Catch::Approx(0.0F));
  CHECK(quad.corners[0].y == Catch::Approx(0.0F));
  CHECK(quad.corners[0].z == Catch::Approx(0.0F));
}

TEST_CASE("Quad projeté : chaque coin reçoit SES deux nombres de texture",
          "[render][particlequad]") {
  // A et D partagent une abscisse, B et C l'autre ; A et B partagent une
  // ordonnée, C et D l'autre. C'est ce que le client répartit sur ses six
  // sommets, et les intervertir ferait pivoter l'image d'un quart de tour.
  EffectParticleQuadRequest request = UnitQuad();
  request.u_at_a_and_d = 0.25F;
  request.u_at_b_and_c = 0.75F;
  request.v_at_a_and_b = 0.125F;
  request.v_at_c_and_d = 0.875F;
  const EffectWorldQuad quad = ComposeEffectParticleQuad(request, {});

  CHECK(quad.texture_uv[0].x == Catch::Approx(0.25F));
  CHECK(quad.texture_uv[3].x == Catch::Approx(0.25F));
  CHECK(quad.texture_uv[1].x == Catch::Approx(0.75F));
  CHECK(quad.texture_uv[2].x == Catch::Approx(0.75F));
  CHECK(quad.texture_uv[0].y == Catch::Approx(0.125F));
  CHECK(quad.texture_uv[1].y == Catch::Approx(0.125F));
  CHECK(quad.texture_uv[2].y == Catch::Approx(0.875F));
  CHECK(quad.texture_uv[3].y == Catch::Approx(0.875F));
}

TEST_CASE("Quad projeté : une texture RETOURNÉE le reste", "[render][particlequad]") {
  // Un dessinateur retourne son image en échangeant deux des quatre nombres.
  // Les ranger en minimum et maximum effacerait le retournement, et la figure
  // resterait plausible.
  EffectParticleQuadRequest request = UnitQuad();
  request.u_at_a_and_d = 1.0F;
  request.u_at_b_and_c = 0.0F;
  const EffectWorldQuad quad = ComposeEffectParticleQuad(request, {});

  CHECK(quad.texture_uv[0].x > quad.texture_uv[1].x);
}

TEST_CASE("Quad projeté : le propriétaire écrase CANAL PAR CANAL", "[render][particlequad]") {
  // Chaque canal a son propre marqueur d'absence. Traiter l'écrasement comme
  // tout ou rien teindrait en noir les deux canaux que le propriétaire laisse
  // au dessinateur.
  EffectOwnerTint owner;
  owner.green = 128;
  const EffectWorldQuad quad = ComposeEffectParticleQuad(UnitQuad(), {}, owner);

  CHECK(quad.tint[0] == 10);
  CHECK(quad.tint[1] == 128);
  CHECK(quad.tint[2] == 30);
}

TEST_CASE("Quad projeté : ZÉRO est une teinte, pas une absence", "[render][particlequad]") {
  // C'est pourquoi le client dit l'absence par moins un. Un propriétaire qui
  // impose du noir doit obtenir du noir.
  EffectOwnerTint owner;
  owner.red = 0;
  owner.green = 0;
  owner.blue = 0;
  const EffectWorldQuad quad = ComposeEffectParticleQuad(UnitQuad(), {}, owner);

  CHECK(quad.tint[0] == 0);
  CHECK(quad.tint[1] == 0);
  CHECK(quad.tint[2] == 0);
  CHECK(quad.tint[3] == 200);
}

TEST_CASE("Quad projeté : l'ALPHA n'est jamais écrasé", "[render][particlequad]") {
  // Le propriétaire ne porte que trois canaux ; l'alpha reste celui que le
  // dessinateur passe, et c'est par lui que la figure s'éteint.
  EffectOwnerTint owner;
  owner.red = 1;
  owner.green = 2;
  owner.blue = 3;
  EffectParticleQuadRequest request = UnitQuad();
  request.alpha = 7;
  CHECK(ComposeEffectParticleQuad(request, {}, owner).tint[3] == 7);
}
