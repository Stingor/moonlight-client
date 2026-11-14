#include "ro/render/effect_track.h"

#include <array>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track_motion.h"

using Catch::Approx;
using ro::render::AdvanceUndulatingRingTrack;
using ro::render::EffectTrack;
using ro::render::kEffectTrackSlots;

namespace {

// La piste que le bâtisseur des quatre anneaux pose : balayage entier,
// inclinaison de quatre-vingts degrés, rayon de quatre et deux dixièmes, et
// vingt et un segments à zéro. C'est le sous-type 30, celui que quarante-quatre
// identifiants partagent.
EffectTrack RingsFromParam() {
  EffectTrack track;
  track.active = true;
  track.degrees = 360;
  track.azimuth_degrees = 180;
  track.tilt_degrees = 80;
  track.radius_at_anchor = 4.2F;
  // LA CLÉ : la base de l'onde, que le bâtisseur tire entre vingt et
  // vingt-cinq.
  track.lift_key = 20.0F;
  return track;
}

// La vie que le bâtisseur garantit : il relève la durée à cinquante-six tics
// quand elle est plus courte. En deçà de trente, l'anneau ne se rallume plus.
constexpr std::int32_t kBuilderLifeTicks = 56;
constexpr std::size_t kFirstRank = 0;

}  // namespace

TEST_CASE("Piste : éteinte, elle n'avance pas", "[render][effecttrack]") {
  // Un nœud n'allume pas toujours ses quatre pistes. Faire vieillir celles qu'il
  // laisse éteintes leur donnerait un rayon, donc une aire, donc une figure que
  // le client ne montre pas.
  EffectTrack track = RingsFromParam();
  track.active = false;

  AdvanceUndulatingRingTrack(track, kFirstRank, kBuilderLifeTicks);

  CHECK(track.ticks == 0);
  CHECK(track.radius_at_anchor == Catch::Approx(4.2F));
  CHECK(track.value[kEffectTrackSlots / 2] == Catch::Approx(0.0F));
}

TEST_CASE("Piste : un tic NÉGATIF est une attente", "[render][effecttrack]") {
  // Le bâtisseur échelonne ses quatre pistes en reculant leur compte de cinq
  // tics chacune. Les faire partir ensemble montrerait quatre anneaux
  // superposés au lieu d'une figure qui s'ouvre.
  EffectTrack track = RingsFromParam();
  track.ticks = -3;

  AdvanceUndulatingRingTrack(track, kFirstRank, kBuilderLifeTicks);

  CHECK(track.ticks == -2);
  CHECK(track.radius_at_anchor == Catch::Approx(4.2F));
  CHECK(track.value[kEffectTrackSlots / 2] == Catch::Approx(0.0F));
}

TEST_CASE("Piste : le rayon croît d'un pas fixe", "[render][effecttrack]") {
  EffectTrack track = RingsFromParam();
  track.radius_at_anchor = 1.0F;

  AdvanceUndulatingRingTrack(track, kFirstRank, kBuilderLifeTicks);

  CHECK(track.ticks == 1);
  CHECK(track.radius_at_anchor == Catch::Approx(1.07F));
}

TEST_CASE("Piste : l'inclinaison retombe jusqu'à un plancher, qui éteint",
          "[render][effecttrack]") {
  // C'est ce qui fait retomber la jupe de l'anneau. Sans le plancher, elle
  // passerait sous le sol et le ruban se retournerait.
  //
  // ⚠ LE PLANCHER SE JUGE APRÈS LE DÉCRÉMENT, et l'alpha qu'il annule REMONTE
  // dans le même tic : le client éteint d'abord, puis reprend sa montée plus
  // bas dans la même passe. Attendre un alpha nul en sortie ferait corriger un
  // code juste — c'est ce que la première version de ce test a fait.
  EffectTrack track = RingsFromParam();
  track.radius_at_anchor = 1.0F;
  track.tilt_degrees = 10;
  track.alpha = 200;

  AdvanceUndulatingRingTrack(track, kFirstRank, kBuilderLifeTicks);
  CHECK(track.tilt_degrees == 10);
  CHECK(track.alpha == 10);

  AdvanceUndulatingRingTrack(track, kFirstRank, kBuilderLifeTicks);
  CHECK(track.tilt_degrees == 10);
  CHECK(track.alpha == 10);
}

TEST_CASE("Piste : passé le seuil, l'anneau s'éteint puis SE RALLUME", "[render][effecttrack]") {
  // Un portail pulse, et aucune boucle n'est décrite nulle part : l'anneau
  // grossit jusqu'au seuil, s'efface, puis repart d'un rayon posé en dur. Ne
  // pas le rallumer montrerait un portail qui s'ouvre une fois et meurt.
  EffectTrack track = RingsFromParam();
  track.radius_at_anchor = 5.0F;
  track.alpha = 2;

  AdvanceUndulatingRingTrack(track, kFirstRank, kBuilderLifeTicks);

  CHECK(track.alpha == 0);
  CHECK(track.radius_at_anchor == Catch::Approx(3.37F));
  CHECK(track.tilt_degrees == 74);
}

TEST_CASE("Piste : sans temps à vivre, l'anneau NE se rallume PAS", "[render][effecttrack]") {
  // Un anneau rallumé au dernier tic naîtrait et disparaîtrait dans la même
  // image.
  EffectTrack track = RingsFromParam();
  track.radius_at_anchor = 5.0F;
  track.alpha = 2;
  track.ticks = kBuilderLifeTicks - 1;

  AdvanceUndulatingRingTrack(track, kFirstRank, kBuilderLifeTicks);

  CHECK(track.alpha == 0);
  CHECK(track.radius_at_anchor == Catch::Approx(5.07F));
}

TEST_CASE("Piste : l'ondulation s'annule AUX DEUX BOUTS du ruban", "[render][effecttrack]") {
  // Les segments balaient un demi-tour de sinus : le premier et le dernier
  // valent la CLÉ nue — la base de l'onde, et non le rayon. C'est ce qui permet
  // à l'anneau de se refermer sans marche visible à la jointure.
  EffectTrack track = RingsFromParam();
  track.radius_at_anchor = 1.0F;

  AdvanceUndulatingRingTrack(track, kFirstRank, kBuilderLifeTicks);

  CHECK(track.value.front() == Catch::Approx(track.lift_key));
  CHECK(track.value.back() == Catch::Approx(track.lift_key).margin(1e-4F));
  // Et elle ne vaut PAS le rayon : les rubans font vingt unités, pas un.
  CHECK(track.value[kEffectTrackSlots / 2] > 10.0F);
}

TEST_CASE("Piste : après un tic, les segments ont une LONGUEUR", "[render][effecttrack]") {
  // Le bâtisseur les pose à zéro, et un ruban de longueur nulle n'a pas
  // d'aire : c'est le mouvement, et lui seul, qui donne sa figure à l'anneau.
  EffectTrack track = RingsFromParam();
  track.radius_at_anchor = 1.0F;

  AdvanceUndulatingRingTrack(track, kFirstRank, kBuilderLifeTicks);

  for (const float length : track.value) {
    CHECK(length > 0.0F);
  }
}

TEST_CASE("Piste : le RANG change la phase, et pas au même rythme", "[render][effecttrack]") {
  // Les quatre pistes d'un nœud n'ondulent pas ensemble, et les deux dernières
  // ondulent DEUX FOIS plus vite que les deux premières. C'est une asymétrie du
  // client, reproduite telle quelle ; la lisser ferait battre les quatre pistes
  // à l'unisson, ce qui se voit.
  const std::size_t in_middle = kEffectTrackSlots / 2;
  std::array<float, ro::render::kEffectTracksPerNode> au_centre = {};
  for (std::size_t rang = 0; rang < ro::render::kEffectTracksPerNode; ++rang) {
    EffectTrack track = RingsFromParam();
    track.radius_at_anchor = 1.0F;
    track.ticks = 6;
    AdvanceUndulatingRingTrack(track, rang, kBuilderLifeTicks);
    au_centre[rang] = track.value[in_middle];
  }

  CHECK(au_centre[0] != Catch::Approx(au_centre[1]));
  CHECK(au_centre[1] != Catch::Approx(au_centre[2]));
  CHECK(au_centre[2] != Catch::Approx(au_centre[3]));
}

TEST_CASE("Piste : les deux blocs de cases ont le MÊME compte", "[render][effecttrack]") {
  // Vingt et une valeurs et vingt et un octets, et ce n'est pas une coïncidence
  // : les deux zones se suivent dans les cent quatre-vingts octets d'une piste,
  // et un bâtisseur écrit dans les deux au même index. Les désaccorder ferait
  // lire la marque d'une case à côté de sa valeur.
  const ro::render::EffectTrack effect_track;
  CHECK(effect_track.value.size() == ro::render::kEffectTrackSlots);
  CHECK(effect_track.mark.size() == ro::render::kEffectTrackSlots);
}

TEST_CASE("Piste : une piste neuve s'ancre AU NŒUD", "[render][effecttrack]") {
  // Le bâtisseur de l'anneau ne pose pas d'ancre : ses quatre pistes partent du
  // nœud. Celui du billboard en pose une par piste. Un défaut non nul ferait
  // partir de travers tout ce qui n'en pose pas.
  const ro::render::EffectTrack effect_track;
  CHECK(effect_track.anchor.x == 0.0F);
  CHECK(effect_track.anchor.y == 0.0F);
  CHECK(effect_track.anchor.z == 0.0F);
  for (const std::uint8_t mark : effect_track.mark) {
    CHECK(mark == 0);
  }
}

TEST_CASE("Piste : la QUEUE clot les cent quatre-vingts octets", "[render][effecttrack]") {
  // Neuf cases apres l'ancre, et ce compte se lit deux fois : cent
  // quatre-vingts octets moins les cent quarante-quatre qui precedent en font
  // trente-six, soit neuf flottants ; et les cent onze dessinateurs en lisent
  // exactement neuf, aucun au-dela. Les compter absentes parce que deux
  // familles les ignorent aurait fait croire la piste finie.
  const ro::render::EffectTrack effect_track;
  CHECK(effect_track.tail.size() == ro::render::kEffectTrackTailSlots);
  for (const float tail_value : effect_track.tail) {
    CHECK(tail_value == 0.0F);
  }
}

TEST_CASE("Sous-type 139 : l'onde bat autour de la CLE, pas du rayon", "[render][effecttrack]") {
  // ⚠ C'EST LA SEULE CHOSE QUI LE SEPARE DU CAS 30 dans les pistes, et elle se
  // voit : un anneau qui ondulerait autour de son rayon croissant s'elargirait
  // sans cesse. Celui-ci bat autour d'une hauteur fixe.
  std::array<EffectTrack, ro::render::kEffectTracksPerNode> tracks;
  tracks[0] = RingsFromParam();
  tracks[0].lift_key = 20.0F;
  // ⚠ SOUS LE SEUIL QUI ÉTEINT. Le rayon de quatre et deux dixièmes que le
  // bâtisseur du sous-type 30 pose est DÉJÀ au-dessus des quatre qui font
  // s'effacer l'anneau : partir de là rallume la piste au premier tic et remet
  // le rayon à 3,37, ce qui ne dit rien de l'onde.
  tracks[0].radius_at_anchor = 1.0F;
  std::uint8_t scale = 0;
  ro::render::AdvanceOscillatingRadiusParticleTracks(tracks, kBuilderLifeTicks, scale);

  // Le segment du milieu porte la base plus son ondulation ; les deux bouts la
  // portent nue, leur sinus le long du ruban etant nul.
  CHECK(tracks[0].value[0] == Approx(20.0F));
  CHECK(tracks[0].value[kEffectTrackSlots - 1] == Approx(20.0F).margin(1e-4F));
  CHECK(tracks[0].value[10] > 20.0F);
  // Et le rayon, lui, a bien avance : il n'est pas la base, mais il vit.
  CHECK(tracks[0].radius_at_anchor == Approx(1.07F));
}

TEST_CASE("Sous-type 139 : l'echelle du noeud saute par quart de cycle", "[render][effecttrack]") {
  // ⚠ QUATRE MARCHES, PAS UNE RAMPE. Entre deux quarts l'echelle ne bouge pas ;
  // la lisser ferait gonfler l'anneau sans a-coup la ou le client le fait sauter.
  std::array<EffectTrack, ro::render::kEffectTracksPerNode> tracks;
  tracks[0] = RingsFromParam();
  tracks[0].lift_key = 20.0F;
  std::uint8_t scale = 0;
  std::vector<std::int32_t> walk_ticks;
  std::vector<int> walk_values;
  std::uint8_t before = scale;
  for (std::int32_t tick = 0; tick < 101; ++tick) {
    ro::render::AdvanceOscillatingRadiusParticleTracks(tracks, 400, scale);
    if (scale != before) {
      walk_ticks.push_back(tracks[0].ticks);
      walk_values.push_back(static_cast<int>(scale));
      before = scale;
    }
  }
  // ⚠ LA PREMIÈRE MARCHE N'EST PAS AU PREMIER TIC. Le client incrémente son
  // compteur PUIS lit son reste : le premier reste nul tombe au centième, et
  // avant cela seuls les trois quarts se présentent. L'échelle que le bâtisseur
  // a semée tient donc les vingt-quatre premiers tics.
  REQUIRE(walk_ticks.size() == 4);
  CHECK(walk_ticks[0] == 25);
  CHECK(walk_values[0] == 6);
  CHECK(walk_ticks[1] == 50);
  CHECK(walk_values[1] == 7);
  CHECK(walk_ticks[2] == 75);
  CHECK(walk_values[2] == 8);
  CHECK(walk_ticks[3] == 100);
  CHECK(walk_values[3] == 4);
}

TEST_CASE("Sous-type 108 : un segment ouvert se FIGE", "[render][effecttrack]") {
  // ⚠ SANS LE VERROU, L'EVENTAIL SE REFERMERAIT au retour du sinus du temps.
  // Le client ramene le segment a sa pleine longueur et pose sa marque a un ; le
  // segment ne bouge plus, quoi que fasse le temps.
  std::array<EffectTrack, ro::render::kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].tail[1] = 10.0F;
  std::size_t figes = 0;
  for (std::int32_t tick = 0; tick < 120; ++tick) {
    ro::render::AdvanceRadialSpreadParticleTracks(tracks);
  }
  std::vector<float> longueurs(tracks[0].value.begin(), tracks[0].value.end());
  for (const std::uint8_t mark : tracks[0].mark) {
    figes += mark == 1 ? 1 : 0;
  }
  CHECK(figes > 0);
  // Cent tics de plus ne bougent plus un seul segment fige.
  for (std::int32_t tick = 0; tick < 100; ++tick) {
    ro::render::AdvanceRadialSpreadParticleTracks(tracks);
  }
  for (std::size_t segment = 0; segment < kEffectTrackSlots; ++segment) {
    if (tracks[0].mark[segment] == 1) {
      CHECK(tracks[0].value[segment] == Approx(longueurs[segment]));
    }
  }
  // Et son alpha est monte puis redescendu : au cent vingtieme tic il decroit.
  CHECK(tracks[0].alpha < 180);
}

TEST_CASE("Sous-type 118 : la QUATRIEME piste n'est pas comme les trois autres",
          "[render][effecttrack]") {
  // ⚠ LE CLIENT LA TRAITE A PART par une comparaison sur le rang : sa cle croit
  // cinq fois plus vite et son azimut derive deux fois moins. L'aligner sur ses
  // soeurs ferait quatre spirales identiques.
  std::array<EffectTrack, ro::render::kEffectTracksPerNode> tracks;
  for (EffectTrack& effect_track : tracks) {
    effect_track.active = true;
    effect_track.lift_key = 10.0F;
  }
  std::uint32_t seed = 1;
  ro::render::AdvanceSpiralParticleTracks(tracks, seed);
  CHECK(tracks[0].lift_key == Approx(10.05F));
  CHECK(tracks[3].lift_key == Approx(10.25F));
  CHECK(tracks[3].lift_key > tracks[0].lift_key);
}

TEST_CASE("Sous-type 118 : passe le quatre-vingt-dixieme tic, le segment est MARQUE",
          "[render][effecttrack]") {
  // ⚠ ET IL N'EST PLUS ECRIT. Le recalculer le ferait redescendre avec le sinus
  // du temps ; la marque est ce que la figure lit pour le savoir.
  std::array<EffectTrack, ro::render::kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].lift_key = 1.0F;
  std::uint32_t seed = 7;
  for (std::int32_t tick = 0; tick < 91; ++tick) {
    ro::render::AdvanceSpiralParticleTracks(tracks, seed);
  }
  const float fige = tracks[0].value[10];
  for (std::int32_t tick = 0; tick < 30; ++tick) {
    ro::render::AdvanceSpiralParticleTracks(tracks, seed);
  }
  CHECK(tracks[0].mark[10] == 1);
  CHECK(tracks[0].value[10] == Approx(fige));
}

TEST_CASE("Sous-type 24 : l'onde MULTIPLIE la base au lieu de s'y ajouter",
          "[render][effecttrack]") {
  // ⚠ C'EST CE QUI LE SEPARE DU CAS 30. Le trente pose un rayon constant sur
  // tous ses segments ; celui-ci donne a chacun une hauteur en demi-arche, et
  // fait respirer l'arche entiere. Les deux bouts restent donc NULS quoi qu'il
  // arrive — une addition les aurait souleves.
  std::array<EffectTrack, ro::render::kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].lift_key = 10.0F;
  ro::render::AdvanceTurningArchTracks(tracks, 200);
  CHECK(tracks[0].value.front() == Approx(0.0F).margin(1e-5F));
  CHECK(tracks[0].value.back() == Approx(0.0F).margin(1e-5F));
  CHECK(tracks[0].value[10] > 9.0F);
}

TEST_CASE("Sous-type 24 : les rangs pairs tournent deux fois moins vite", "[render][effecttrack]") {
  std::array<EffectTrack, ro::render::kEffectTracksPerNode> tracks;
  for (EffectTrack& effect_track : tracks) {
    effect_track.active = true;
    effect_track.lift_key = 10.0F;
  }
  for (std::int32_t tick = 0; tick < 10; ++tick) {
    ro::render::AdvanceTurningArchTracks(tracks, 200);
  }
  CHECK(tracks[0].azimuth_degrees == 10);
  CHECK(tracks[1].azimuth_degrees == 20);
  CHECK(tracks[2].azimuth_degrees == 10);
  CHECK(tracks[3].azimuth_degrees == 20);
}

TEST_CASE("Sous-type 24 : l'alpha ne retombe qu'a VINGT TICS DE LA FIN", "[render][effecttrack]") {
  // ⚠ CE N'EST PAS UN TIC FIXE MAIS LA VIE DU NOEUD. Un noeud court s'eteint
  // donc sans jamais atteindre son plafond, et c'est voulu : le client compare
  // le compteur a « la vie moins vingt ».
  std::array<EffectTrack, ro::render::kEffectTracksPerNode> brief;
  brief[0].active = true;
  brief[0].lift_key = 10.0F;
  for (std::int32_t tick = 0; tick < 12; ++tick) {
    ro::render::AdvanceTurningArchTracks(brief, 10);
  }
  // Vie de dix tics : le seuil est a moins dix, donc franchi des le premier.
  CHECK(brief[0].alpha == 0);

  std::array<EffectTrack, ro::render::kEffectTracksPerNode> longue;
  longue[0].active = true;
  longue[0].lift_key = 10.0F;
  for (std::int32_t tick = 0; tick < 12; ++tick) {
    ro::render::AdvanceTurningArchTracks(longue, 200);
  }
  CHECK(longue[0].alpha == 12);
}
