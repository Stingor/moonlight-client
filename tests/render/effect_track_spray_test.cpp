#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

using ro::render::AdvanceDualModeSprayTracks;
using ro::render::EffectTrack;
using ro::render::kEffectTrackSlots;
using ro::render::kEffectTracksPerNode;

namespace {

// Le mode que le client reconnaît à un nombre, et celui que nos treize
// identifiants prennent — le zéro du constructeur de nœud.
constexpr float kSlowMode = 99.0F;
constexpr float kModeOfOurSeeds = 0.0F;
// Le style qui monte deux fois moins vite, et celui qui s'éteint le plus vite.
constexpr std::int16_t kStyleThatRisesSlowly = 1;
constexpr std::int16_t kStyleThatFadesFastest = 2;

std::array<EffectTrack, kEffectTracksPerNode> FourLive(std::int16_t style = 0) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    track.alpha = 0;
    track.style = style;
    track.lift_key = 25.0F;
    track.radius_at_anchor = 2.0F;
    track.tilt_degrees = 80;
  }
  return tracks;
}

void Advance(std::array<EffectTrack, kEffectTracksPerNode>& tracks, std::int32_t ticks,
             float mode = kModeOfOurSeeds) {
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceDualModeSprayTracks(tracks, mode);
  }
}

}  // namespace

TEST_CASE("Gerbe : LES DEUX MODES ne tournent ni ne s'élargissent pareil", "[render][track]") {
  // ⚠ LE MODE SE LIT DANS UN CHAMP DU NŒUD, comparé à quatre-vingt-dix-neuf.
  // Nos semis gardent le zéro du constructeur et prennent donc le mode rapide ;
  // les confondre donnerait une gerbe deux fois trop lente.
  std::array<EffectTrack, kEffectTracksPerNode> fast = FourLive();
  std::array<EffectTrack, kEffectTracksPerNode> slow = FourLive();
  Advance(fast, 10);
  Advance(slow, 10, kSlowMode);
  CHECK(fast[0].azimuth_degrees == 50);
  CHECK(slow[0].azimuth_degrees == 30);
  CHECK(fast[0].radius_at_anchor > slow[0].radius_at_anchor);
}

TEST_CASE("Gerbe : l'inclinaison descend et S'ARRÊTE à un plancher", "[render][track]") {
  // ⚠ SANS LE PLANCHER, LA GERBE FINIRAIT COUCHÉE et disparaîtrait dans le sol.
  std::array<EffectTrack, kEffectTracksPerNode> fast = FourLive();
  std::array<EffectTrack, kEffectTracksPerNode> slow = FourLive();
  Advance(fast, 200);
  Advance(slow, 200, kSlowMode);
  CHECK(fast[0].tilt_degrees == 40);
  CHECK(slow[0].tilt_degrees == 30);
}

TEST_CASE("Gerbe : un STYLE monte deux fois moins vite", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> living = FourLive();
  std::array<EffectTrack, kEffectTracksPerNode> slow = FourLive(kStyleThatRisesSlowly);
  Advance(living, 10);
  Advance(slow, 10);
  CHECK(living[0].alpha == 200);
  CHECK(slow[0].alpha == 100);
}

TEST_CASE("Gerbe : la DESCENTE dépend du style, et de rien d'autre", "[render][track]") {
  // ⚠ C'EST CE QUI FAIT QU'UNE MÊME GERBE S'ÉTEINT VITE OU LENTEMENT selon
  // l'effet qui l'emploie. Une vitesse commune les uniformiserait.
  std::array<EffectTrack, kEffectTracksPerNode> quick = FourLive(kStyleThatFadesFastest);
  std::array<EffectTrack, kEffectTracksPerNode> doucement = FourLive(0);
  Advance(quick, 20);
  Advance(doucement, 20);
  CHECK(quick[0].alpha < doucement[0].alpha);
  // Huit par tic contre deux, sur les dix tics qui suivent la montée.
  CHECK(doucement[0].alpha - quick[0].alpha == 60);
}

TEST_CASE("Gerbe : l'alpha ne descend pas sous zéro", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive(kStyleThatFadesFastest);
  Advance(tracks, 200);
  CHECK(tracks[0].alpha == 0);
}

TEST_CASE("Gerbe : les segments dessinent l'ARCHE et RESPIRENT", "[render][track]") {
  // L'arche est celle du cône — neuf degrés par segment —, mais elle respire de
  // quinze centièmes au lieu de se figer. Les figer donnerait quatre couronnes
  // immobiles.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive();
  Advance(tracks, 1);
  const float at_first = tracks[0].value[10];
  CHECK(tracks[0].value[0] == Catch::Approx(0.0F).margin(0.001F));
  CHECK(tracks[0].value[10] > tracks[0].value[0]);
  CHECK(tracks[0].value[10] > tracks[0].value[kEffectTrackSlots - 1]);
  Advance(tracks, 90);
  CHECK(tracks[0].value[10] != Catch::Approx(at_first));
}

TEST_CASE("Gerbe : chaque piste respire un QUART DE TOUR après la précédente", "[render][track]") {
  // ⚠ QUATRE COURONNES QUI RESPIRENT ENSEMBLE n'en font qu'une, épaisse. C'est
  // le décalage qui fait la gerbe.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive();
  Advance(tracks, 1);
  CHECK(tracks[0].value[10] != Catch::Approx(tracks[1].value[10]));
  CHECK(tracks[1].value[10] != Catch::Approx(tracks[2].value[10]));
}

TEST_CASE("Gerbe : une piste éteinte ne bouge pas", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive();
  tracks[0].active = false;
  Advance(tracks, 30);
  CHECK(tracks[0].ticks == 0);
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[0].tilt_degrees == 80);
}
