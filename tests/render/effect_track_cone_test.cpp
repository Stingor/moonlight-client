#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

using ro::render::AdvanceParticleConeSpawnTracks;
using ro::render::EffectTrack;
using ro::render::kEffectTrackSlots;
using ro::render::kEffectTracksPerNode;

namespace {

constexpr std::size_t kLastTrack = kEffectTracksPerNode - 1;
constexpr std::int32_t kLifeTicks = 70;
// Le style qui empêche l'alpha de monter, et la teinte qui n'anime que la
// dernière piste.
constexpr std::int16_t kStyleThatNeverRises = 1;
constexpr float kTrailVariantThatKeepsOnlyTheLast = 3.0F;
// La sortie commence quarante tics avant la fin de la vie du noeud.
constexpr std::int32_t kTicksOfFadeMargin = 40;

std::array<EffectTrack, kEffectTracksPerNode> FourLive(float amplitude = 25.0F) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    track.alpha = 0;
    track.lift_key = amplitude;
  }
  return tracks;
}

void Advance(std::array<EffectTrack, kEffectTracksPerNode>& tracks, std::int32_t ticks,
             float tint = 0.0F) {
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceParticleConeSpawnTracks(tracks, kLifeTicks, tint);
  }
}

}  // namespace

TEST_CASE("Cône : les TROIS PREMIÈRES pistes tournent, la dernière non", "[render][track]") {
  // ⚠ C'EST CE QUI OUVRE LE CÔNE. Chaque piste tourne plus vite que la
  // précédente — trois, quatre, cinq degrés par tic — et la quatrième reste
  // fixe : c'est elle le halo. Un pas commun les garderait alignées.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive();
  Advance(tracks, 10);
  CHECK(tracks[0].azimuth_degrees == 30);
  CHECK(tracks[1].azimuth_degrees == 40);
  CHECK(tracks[2].azimuth_degrees == 50);
  CHECK(tracks[kLastTrack].azimuth_degrees == 0);
}

TEST_CASE("Cône : l'azimut reste DANS LE TOUR", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive();
  Advance(tracks, 200);
  for (std::size_t rang = 0; rang < kLastTrack; ++rang) {
    INFO("piste " << rang);
    CHECK(tracks[rang].azimuth_degrees >= 0);
    CHECK(tracks[rang].azimuth_degrees < ro::render::kEffectTrackFullTurnDegrees);
  }
}

TEST_CASE("Cône : la dernière piste monte MOINS HAUT que les autres", "[render][track]") {
  // Dix par tic jusqu'à cent quatre-vingts pour les trois premières, huit
  // jusqu'à soixante pour le halo. Les aligner le noierait dans les autres.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive();
  Advance(tracks, 19);
  CHECK(tracks[0].alpha == 180);
  CHECK(tracks[kLastTrack].alpha == 60);
}

TEST_CASE("Cône : un STYLE empêche la montée", "[render][track]") {
  // ⚠ LA PISTE RESTE À L'ALPHA DE SON SEMIS. L'oublier ferait remonter à cent
  // quatre-vingts une piste que le client laisse où elle est.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive();
  tracks[0].style = kStyleThatNeverRises;
  tracks[0].alpha = 40;
  Advance(tracks, 19);
  CHECK(tracks[0].alpha == 40);
  CHECK(tracks[1].alpha == 180);
}

TEST_CASE("Cône : la sortie commence QUARANTE TICS avant la fin", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive();
  Advance(tracks, kLifeTicks - kTicksOfFadeMargin);
  const std::int16_t at_peak = tracks[0].alpha;
  Advance(tracks, 5);
  CHECK(tracks[0].alpha < at_peak);
  // La dernière s'efface plus lentement : deux par tic contre cinq.
  CHECK(at_peak - tracks[0].alpha == 25);
}

TEST_CASE("Cône : chaque segment se FIGE à son plafond", "[render][track]") {
  // ⚠ SANS CE GEL, LE CÔNE RESPIRERAIT AU LIEU DE S'OUVRIR UNE FOIS. Le sinus du
  // temps redescendrait après son quart de tour, et l'anneau se refermerait.
  // ⚠ LE RELEVÉ PREND LA MESURE APRÈS LE QUATRE-VINGT-DIXIÈME TIC, et non avant :
  // le sinus du temps n'atteint un que là, et une mesure prise au
  // quatre-vingt-neuvième compare un segment encore en croissance à sa fin.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive();
  Advance(tracks, 91);
  const std::array<float, kEffectTrackSlots> at_peak = tracks[0].value;
  Advance(tracks, 40);
  for (std::size_t segment = 0; segment < kEffectTrackSlots; ++segment) {
    INFO("segment " << segment);
    CHECK(tracks[0].value[segment] == Catch::Approx(at_peak[segment]));
  }
}

TEST_CASE("Cône : les segments dessinent une ARCHE le long du ruban", "[render][track]") {
  // Neuf degrés par segment, vingt et un segments : cent quatre-vingt-neuf
  // degrés. Le sinus y est nul aux deux bouts et plein au milieu — c'est la
  // forme du cône, et la plate ferait un cylindre.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive();
  Advance(tracks, 89);
  CHECK(tracks[0].value[0] == Catch::Approx(0.0F).margin(0.001F));
  CHECK(tracks[0].value[10] > tracks[0].value[0]);
  CHECK(tracks[0].value[10] > tracks[0].value[kEffectTrackSlots - 1]);
}

TEST_CASE("Cône : une TEINTE n'anime que la dernière piste", "[render][track]") {
  // ⚠ LE CLIENT SAUTE LES TROIS PREMIÈRES DÈS L'ENTRÉE DE SA BOUCLE. Les animer
  // quand même montrerait trois couronnes que cet effet-là ne montre pas.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive();
  Advance(tracks, 10, kTrailVariantThatKeepsOnlyTheLast);
  CHECK(tracks[0].ticks == 0);
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[kLastTrack].ticks == 10);
  CHECK(tracks[kLastTrack].alpha > 0);
}

TEST_CASE("Cône : une piste éteinte ne bouge pas", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive();
  tracks[0].active = false;
  Advance(tracks, 30);
  CHECK(tracks[0].ticks == 0);
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[0].azimuth_degrees == 0);
}

TEST_CASE("Cone : le jumeau du sous-type 25 fait onduler sa derniere piste sans fin",
          "[render][track_cone]") {
  // 🔴 LES HALOS VERT ET VIOLET (56, 59) NE SONT PAS LE CÔNE : leur dernière
  // piste monte jusqu'à cent, s'efface de trois, et ses segments figés battent
  // entre 55 et 100 % de leur arche au lieu de se tenir.
  constexpr float kAmplitude = 25.0F;
  constexpr std::int32_t kLongLife = 400;
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive(kAmplitude);
  tracks[kLastTrack].style = kStyleThatNeverRises;
  for (std::int32_t tick = 0; tick < 19; ++tick) {
    AdvanceParticleConeSpawnTracks(tracks, kLongLife, 0.0F, true);
  }
  // Aucun style ne retient la montée, et le plafond est cent.
  CHECK(tracks[kLastTrack].alpha == 100);
  for (std::int32_t tick = 19; tick < 120; ++tick) {
    AdvanceParticleConeSpawnTracks(tracks, kLongLife, 0.0F, true);
  }
  // Au tic 120, figé, le segment du milieu bat à 0,55 + 0,45 × sin(120°).
  const float arch = kAmplitude * std::sin(90.0F * 3.14159265F / 180.0F);
  const float expected = arch * 0.55F + arch * 0.45F * std::sin(120.0F * 3.14159265F / 180.0F);
  CHECK(tracks[kLastTrack].value[10] == Catch::Approx(expected).margin(1e-3F));
  // Les trois premières, elles, se tiennent comme dans le cône.
  CHECK(tracks[0].value[10] == Catch::Approx(arch).margin(1e-3F));
}
