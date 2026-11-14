#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

using ro::render::AdvanceRadialParticleGroupTracks;
using ro::render::EffectTrack;
using ro::render::kEffectTrackSlots;
using ro::render::kEffectTracksPerNode;

namespace {

// Les marques que le client distingue, et celle qui fait regarder la caméra.
constexpr std::uint8_t kMarkThatTurnsFastest = 1;
constexpr std::uint8_t kMarkThatTurnsByHalves = 2;
constexpr std::uint8_t kMarkThatTurnsMedium = 3;
constexpr std::uint8_t kMarkThatFacesTheCamera = 33;
constexpr std::size_t kTrackThatTakesTheCameraTurn = 2;
// Une marque de case qui éteint pour de bon passé quarante-cinq tics.
constexpr std::uint8_t kSlotMarkThatBlinks = 2;
// Un style NÉGATIF veut dire « ne s'efface jamais ».
constexpr std::int16_t kStyleThatNeverFades = -1;

std::array<EffectTrack, kEffectTracksPerNode> FourLive(std::uint8_t mark,
                                                       std::int16_t style = kStyleThatNeverFades) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    track.alpha = 0;
    track.style = style;
    track.lift_key = 20.0F;
    track.mark[0] = mark;
  }
  return tracks;
}

void Advance(std::array<EffectTrack, kEffectTracksPerNode>& tracks, std::int32_t ticks,
             std::int16_t tour = 0) {
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceRadialParticleGroupTracks(tracks, tour);
  }
}

}  // namespace

TEST_CASE("Groupes radiaux : la marque choisit la VITESSE, et le rang s'y ajoute",
          "[render][track]") {
  // ⚠ CHAQUE PISTE AJOUTE SON RANG au pas de sa marque. Sans cela les quatre
  // couronnes tourneraient ensemble et ne feraient qu'une.
  std::array<EffectTrack, kEffectTracksPerNode> quick = FourLive(kMarkThatTurnsFastest);
  Advance(quick, 10);
  CHECK(quick[0].azimuth_degrees == 80);
  CHECK(quick[1].azimuth_degrees == 90);
  CHECK(quick[2].azimuth_degrees == 100);

  // Celle-ci ralentit passé la deuxième piste : quatre, puis deux.
  std::array<EffectTrack, kEffectTracksPerNode> moities = FourLive(kMarkThatTurnsByHalves);
  Advance(moities, 10);
  CHECK(moities[0].azimuth_degrees == 40);
  CHECK(moities[2].azimuth_degrees == 40);
  CHECK(moities[3].azimuth_degrees == 50);
}

TEST_CASE("Groupes radiaux : une marque inconnue NE TOURNE PAS", "[render][track]") {
  // Le client n'a que trois cas de rotation ; tout le reste laisse l'azimut où
  // il est. Lui donner un pas par défaut ferait tourner ce qui doit rester fixe.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive(0);
  Advance(tracks, 20);
  CHECK(tracks[0].azimuth_degrees == 0);
}

TEST_CASE("Groupes radiaux : l'azimut reste DANS LE TOUR", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive(kMarkThatTurnsFastest);
  Advance(tracks, 300);
  for (const EffectTrack& track : tracks) {
    CHECK(track.azimuth_degrees >= 0);
    CHECK(track.azimuth_degrees < ro::render::kEffectTrackFullTurnDegrees);
  }
}

TEST_CASE("Groupes radiaux : la marque choisit aussi la MONTÉE de l'alpha", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> quick = FourLive(kMarkThatTurnsFastest);
  std::array<EffectTrack, kEffectTracksPerNode> moities = FourLive(kMarkThatTurnsByHalves);
  std::array<EffectTrack, kEffectTracksPerNode> moyenne = FourLive(kMarkThatTurnsMedium);
  Advance(quick, 15);
  Advance(moities, 15);
  Advance(moyenne, 15);
  CHECK(quick[0].alpha == 75);
  CHECK(moities[0].alpha == 45);
  CHECK(moyenne[0].alpha == 30);
}

TEST_CASE("Groupes radiaux : le STYLE est une DURÉE, et négatif il vaut JAMAIS",
          "[render][track]") {
  // ⚠ LE PRENDRE POUR UN ZÉRO ferait s'effacer d'emblée une piste qui doit tenir.
  std::array<EffectTrack, kEffectTracksPerNode> jamais = FourLive(kMarkThatTurnsFastest);
  std::array<EffectTrack, kEffectTracksPerNode> brève = FourLive(kMarkThatTurnsFastest, 10);
  Advance(jamais, 40);
  Advance(brève, 40);
  // Quinze montees de cinq — la montee court sur les tics un a quinze.
  CHECK(jamais[0].alpha == 75);
  CHECK(brève[0].alpha < jamais[0].alpha);
}

TEST_CASE("Groupes radiaux : une marque fait REGARDER LA CAMÉRA", "[render][track]") {
  // ⚠ ELLE RANGE LE TOUR DANS LE CHAMP QUE LA TROISIÈME PISTE EMPLOIE COMME
  // DURÉE — le même champ, deux sens, et c'est le client qui le veut ainsi.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive(kMarkThatFacesTheCamera);
  Advance(tracks, 1, 217);
  CHECK(tracks[kTrackThatTakesTheCameraTurn].style == 217);

  std::array<EffectTrack, kEffectTracksPerNode> autres = FourLive(kMarkThatTurnsFastest);
  Advance(autres, 1, 217);
  CHECK(autres[kTrackThatTakesTheCameraTurn].style == kStyleThatNeverFades);
}

TEST_CASE("Groupes radiaux : CHAQUE CASE suit sa propre règle", "[render][track]") {
  // ⚠ C'EST CE QUI FAIT LES GROUPES DU NOM. Une seule règle pour les vingt et une
  // donnerait une couronne uniforme là où le client en fait des paquets.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive(0);
  tracks[0].mark[5] = kSlotMarkThatBlinks;
  Advance(tracks, 50);
  // La case clignotante s'éteint pour de bon passé quarante-cinq tics ; ses
  // voisines tiennent.
  CHECK(tracks[0].value[5] == Catch::Approx(0.0F));
  CHECK(tracks[0].value[4] > 0.0F);
  CHECK(tracks[0].value[6] > 0.0F);
}

TEST_CASE("Groupes radiaux : la longueur ONDULE autour d'une part FIXE", "[render][track]") {
  // Trois quarts de la clé tiennent, un quart ondule : la couronne bat sans
  // jamais disparaître.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive(0);
  Advance(tracks, 120);
  for (std::size_t segment = 0; segment < kEffectTrackSlots; ++segment) {
    INFO("case " << segment);
    CHECK(tracks[0].value[segment] >= Catch::Approx(20.0F * 0.5F).margin(0.001F));
    CHECK(tracks[0].value[segment] <= Catch::Approx(20.0F));
  }
}

TEST_CASE("Groupes radiaux : une piste éteinte ne bouge pas", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive(kMarkThatTurnsFastest);
  tracks[0].active = false;
  Advance(tracks, 30);
  CHECK(tracks[0].ticks == 0);
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[0].azimuth_degrees == 0);
}
