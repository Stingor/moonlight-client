#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

using ro::render::AdvanceStyledBillboardTrack;
using ro::render::EffectTrack;

namespace {

// La case où la figure range l'angle dont elle tourne sur elle-même, et la
// marque qui porte son style.
constexpr std::size_t kSlotOfSpinDegrees = 8;
constexpr std::size_t kMarkOfStyle = 4;

EffectTrack SownBillboard(std::uint8_t style) {
  EffectTrack track;
  track.active = true;
  track.alpha = 0;
  track.mark[kMarkOfStyle] = style;
  return track;
}

void Advance(EffectTrack& track, std::int32_t ticks) {
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceStyledBillboardTrack(track);
  }
}

}  // namespace

TEST_CASE("Billboard stylé : une piste éteinte ne bouge pas", "[render][effecttrack]") {
  EffectTrack track = SownBillboard(1);
  track.active = false;
  Advance(track, 50);
  CHECK(track.ticks == 0);
  CHECK(track.alpha == 0);
}

TEST_CASE("Billboard stylé : le style à courbe MONTE puis redescend", "[render][effecttrack]") {
  // Dix par tic jusqu'à un plafond de deux cent vingt, puis cinq par tic
  // jusqu'à zéro. Les deux moitiés se relaient à un tic précis, et les mêler
  // donnerait une figure qui ne s'éteint pas.
  EffectTrack track = SownBillboard(1);
  Advance(track, 22);
  CHECK(track.alpha == 220);

  // LA BASCULE SE FAIT AU TIC DEUX CENTS LUI-MÊME, pas au suivant : le client
  // compare le compteur DÉJÀ avancé. Un tic d'écart ici et la figure tiendrait
  // sa pose cinq unités de trop, ce que personne ne verrait.
  Advance(track, 177);
  CHECK(track.ticks == 199);
  CHECK(track.alpha == 220);

  Advance(track, 1);
  CHECK(track.alpha == 215);

  Advance(track, 43);
  CHECK(track.alpha == 0);
}

TEST_CASE("Billboard stylé : un style POSE son alpha, il ne l'ajoute pas",
          "[render][effecttrack]") {
  // Le client ÉCRIT la valeur à chaque tic. La cumuler blanchirait la figure en
  // quelques tics, et cela ressemblerait à une montée voulue.
  EffectTrack track = SownBillboard(16);
  Advance(track, 1);
  CHECK(track.alpha == 100);
  Advance(track, 30);
  CHECK(track.alpha == 100);

  CHECK([] {
    EffectTrack other = SownBillboard(14);
    Advance(other, 10);
    return other.alpha;
  }() == 50);
}

TEST_CASE("Billboard stylé : un style SOUS DIX sans courbe ne pose RIEN", "[render][effecttrack]") {
  // Le client saute, et l'alpha garde ce que le tic d'avant lui a laissé. Poser
  // zéro à la place éteindrait une figure que rien n'éteint.
  EffectTrack track = SownBillboard(5);
  track.alpha = 77;
  Advance(track, 20);
  CHECK(track.alpha == 77);
}

TEST_CASE("Billboard stylé : le style qui PULSE ouvre son rayon tant qu'il se voit",
          "[render][effecttrack]") {
  // L'alpha monte de huit, puis redescend de huit, et le rayon ne croît que
  // pendant ce temps : une fois l'alpha tombé, la figure cesse de s'ouvrir au
  // lieu de grandir dans le vide.
  EffectTrack track = SownBillboard(2);
  track.radius_at_anchor = 10.0F;
  Advance(track, 40);
  CHECK(track.radius_at_anchor == Catch::Approx(10.0F));
  CHECK(track.alpha == 0);

  Advance(track, 10);
  CHECK(track.alpha == 80);
  CHECK(track.radius_at_anchor > 10.0F);

  const float ouvert = track.radius_at_anchor;
  Advance(track, 200);
  CHECK(track.alpha == 0);
  CHECK(track.radius_at_anchor > ouvert);

  const float ferme = track.radius_at_anchor;
  Advance(track, 50);
  CHECK(track.radius_at_anchor == Catch::Approx(ferme));
}

TEST_CASE("Billboard stylé : l'angle avance UN TIC SUR DEUX", "[render][effecttrack]") {
  // Le faire à chaque tic ferait tourner la figure deux fois trop vite — une
  // erreur qui reste parfaitement plausible à l'écran.
  EffectTrack advance = SownBillboard(13);
  Advance(advance, 20);
  CHECK(advance.degrees == 10);

  EffectTrack recule = SownBillboard(11);
  recule.degrees = 100;
  Advance(recule, 20);
  CHECK(recule.degrees == 90);
}

TEST_CASE("Billboard stylé : chaque sens ne replie que SON côté", "[render][effecttrack]") {
  // Le client pose deux replis distincts, un par branche. Les fondre en un seul
  // marcherait presque, et l'angle d'une figure qui recule finirait par sortir
  // par le bas sans que rien ne le ramène.
  EffectTrack advance = SownBillboard(13);
  advance.degrees = 358;
  Advance(advance, 6);
  CHECK(advance.degrees == 1);

  EffectTrack recule = SownBillboard(12);
  recule.degrees = 2;
  Advance(recule, 6);
  CHECK(recule.degrees == 359);
}

TEST_CASE("Billboard stylé : le style dix-huit se REPLIE sans tourner", "[render][effecttrack]") {
  // Le client le fait sauter par-dessus le pas, droit au repli. Le ranger avec
  // ceux qui ne touchent à rien laisserait passer un angle négatif.
  EffectTrack track = SownBillboard(18);
  track.degrees = -20;
  Advance(track, 4);
  CHECK(track.degrees == 340);
}

TEST_CASE("Billboard stylé : le DÉCLIN FINAL ne vaut que pour les styles bas",
          "[render][effecttrack]") {
  // Passé son seuil, le style ne pilote plus ; les styles bas s'éteignent alors
  // d'eux-mêmes, les autres non — c'est la mort du nœud qui les retire.
  // Le style zéro monte d'une unité par tic : c'est le seul style bas qui tienne
  // encore une teinte au moment où le déclin commence.
  EffectTrack bottom = SownBillboard(0);
  Advance(bottom, 300);
  CHECK(bottom.alpha == 255);
  Advance(bottom, 20);
  CHECK(bottom.alpha == 235);

  EffectTrack top = SownBillboard(21);
  Advance(top, 320);
  CHECK(top.alpha == 250);
}

TEST_CASE("Billboard stylé : la figure tourne AUSSI sur elle-même", "[render][effecttrack]") {
  // Deux angles distincts, et deux rythmes : celui-ci avance à chaque tic. Les
  // confondre ferait tourner la figure et son orbite ensemble.
  EffectTrack track = SownBillboard(1);
  Advance(track, 10);
  CHECK(track.value[kSlotOfSpinDegrees] == Catch::Approx(20.0F));

  Advance(track, 180);
  CHECK(track.value[kSlotOfSpinDegrees] < 360.0F);

  EffectTrack fixed = SownBillboard(30);
  Advance(fixed, 10);
  CHECK(fixed.value[kSlotOfSpinDegrees] == Catch::Approx(0.0F));
}

TEST_CASE("Billboard stylé : l'alpha reste dans ses bornes", "[render][effecttrack]") {
  // Deux cent cinquante tient sous le plafond, mais le style zéro monte d'une
  // unité par tic pendant trois cents tics : sans bornage il déborderait de ce
  // qu'une teinte peut porter.
  EffectTrack track = SownBillboard(0);
  Advance(track, 400);
  CHECK(track.alpha <= 255);
  CHECK(track.alpha >= 0);
}
