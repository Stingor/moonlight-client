#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

using ro::render::AdvanceCascadingTrailTracks;
using ro::render::EffectTrack;
using ro::render::kEffectTracksPerNode;

namespace {

constexpr std::size_t kSlotOfSpinDegrees = 8;
constexpr std::size_t kMarkOfTexture = 0;
constexpr std::size_t kMarkOfDrawing = 2;
constexpr std::size_t kTailSlotOfHeight = 4;

// Ce que la mesure donne, et que les tests relisent.
constexpr std::int32_t kTicksBetweenShifts = 6;
constexpr float kRadiusGrowthPerShift = 0.5F;
constexpr std::int16_t kAlphaDropPerShift = 50;
constexpr std::uint8_t kMarkOfTrail = 2;
constexpr std::uint8_t kMarkThatSpins = 7;

// Un nœud à QUATRE pistes vives, comme les bâtisseurs du sous-type 59 en sèment.
std::array<EffectTrack, kEffectTracksPerNode> FourLive() {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    track.alpha = 255;
    track.radius_at_anchor = 10.0F;
  }
  tracks[0].degrees = 40;
  tracks[0].mark[kMarkOfTexture] = 3;
  tracks[0].mark[kMarkOfDrawing] = 13;
  return tracks;
}

// Un nœud à UNE seule piste vive, comme le sous-type 47.
std::array<EffectTrack, kEffectTracksPerNode> OneLive() {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].alpha = 200;
  tracks[0].radius_at_anchor = 3.0F;
  tracks[0].degrees = 45;
  return tracks;
}

void Advance(std::array<EffectTrack, kEffectTracksPerNode>& tracks, std::int32_t ticks,
             glm::vec3 owner = {}) {
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceCascadingTrailTracks(tracks, owner, 0, 0);
  }
}

}  // namespace

TEST_CASE("Cascade : les angles se rangent MÊME quand le nœud dort", "[render][track]") {
  // Le client les écrit avant de regarder si sa première piste est allumée. Un
  // nœud qui se réveille trouve donc des angles à jour.
  std::array<EffectTrack, kEffectTracksPerNode> endormies;
  AdvanceCascadingTrailTracks(endormies, {}, 137, 291);
  CHECK(endormies[0].style == 137);
  CHECK(endormies[1].style == 291);
  CHECK_FALSE(endormies[0].active);
}

TEST_CASE("Cascade : rien ne bouge avant le sixième tic", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive();
  const std::int16_t before = tracks[1].alpha;
  Advance(tracks, kTicksBetweenShifts - 1);
  CHECK(tracks[1].alpha == before);
  Advance(tracks, 1);
  CHECK(tracks[1].alpha == before - kAlphaDropPerShift);
}

TEST_CASE("Cascade : la traînée s'OUVRE et s'EFFACE en s'éloignant de la tête", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive();
  // Trois sauts suffisent à remplir les trois pistes derrière la tête.
  Advance(tracks, kTicksBetweenShifts * 3);
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    INFO("piste " << rang);
    // Chaque cran est plus large et plus pâle que celui qui le précède.
    CHECK(tracks[rang].radius_at_anchor >
          tracks[rang - 1].radius_at_anchor + kRadiusGrowthPerShift / 2.0F);
    CHECK(tracks[rang].alpha < tracks[rang - 1].alpha);
  }
}

TEST_CASE("Cascade : la cascade va de la DERNIÈRE à la PREMIÈRE", "[render][track]") {
  // ⚠ L'ORDRE DÉCIDE DE TOUT. À l'envers, la tête serait copiée dans les quatre
  // pistes d'un seul tic, et la traînée deviendrait quatre copies superposées.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive();
  Advance(tracks, kTicksBetweenShifts);
  // Après UN saut, seule la deuxième porte l'alpha de la tête ; les suivantes
  // portent encore le leur, diminué une fois.
  CHECK(tracks[1].alpha == 255 - kAlphaDropPerShift);
  CHECK(tracks[2].alpha == 255 - kAlphaDropPerShift);
  CHECK(tracks[1].radius_at_anchor == Catch::Approx(10.0F + kRadiusGrowthPerShift));
  // La tête, elle, n'a pas grossi : rien ne lui est passé.
  CHECK(tracks[0].radius_at_anchor == Catch::Approx(10.0F));
}

TEST_CASE("Cascade : la marque de la traînée est IMPOSÉE, elle ne se recopie pas",
          "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive();
  REQUIRE(tracks[0].mark[kMarkOfDrawing] != kMarkOfTrail);
  Advance(tracks, kTicksBetweenShifts);
  // Le sillage a toujours la même parure ; seule la tête garde la sienne.
  CHECK(tracks[1].mark[kMarkOfDrawing] == kMarkOfTrail);
  CHECK(tracks[0].mark[kMarkOfDrawing] != kMarkOfTrail);
  // La TEXTURE, elle, suit : c'est la même image qui traîne.
  CHECK(tracks[1].mark[kMarkOfTexture] == tracks[0].mark[kMarkOfTexture]);
}

TEST_CASE("Cascade : une piste ÉTEINTE ne reçoit rien", "[render][track]") {
  // ⚠ C'EST CE QUI DONNE AU SOUS-TYPE 47 SA FIGURE. Il n'allume qu'une piste,
  // et n'a donc aucune traînée : copier quand même en inventerait une.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneLive();
  Advance(tracks, kTicksBetweenShifts * 4);
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    INFO("piste " << rang);
    CHECK_FALSE(tracks[rang].active);
    CHECK(tracks[rang].alpha == 0);
    CHECK(tracks[rang].radius_at_anchor == Catch::Approx(0.0F));
  }
}

TEST_CASE("Cascade : l'alpha d'une traînée ne passe jamais SOUS zéro", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = FourLive();
  Advance(tracks, kTicksBetweenShifts * 12);
  for (const EffectTrack& track : tracks) {
    CHECK(track.alpha >= 0);
  }
}

TEST_CASE("Cascade : la tête se RÉ-ANCRE sur son propriétaire", "[render][track]") {
  // ⚠ LE MANQUE COMMUN DE LA FAMILLE EST ICI COMBLÉ : c'est ce mouvement qui
  // fait suivre l'effet quand celui qui le porte se déplace.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneLive();
  Advance(tracks, 1, {10.0F, 0.0F, 20.0F});
  CHECK(tracks[0].anchor.x == Catch::Approx(10.0F));
  CHECK(tracks[0].anchor.z == Catch::Approx(20.0F));
  Advance(tracks, 1, {40.0F, 0.0F, -5.0F});
  CHECK(tracks[0].anchor.x == Catch::Approx(40.0F));
  CHECK(tracks[0].anchor.z == Catch::Approx(-5.0F));
}

TEST_CASE("Cascade : la tête se tient AU-DESSUS de son propriétaire", "[render][track]") {
  // Le client retranche six dans un repère qui descend : la tête est donc plus
  // haute que le point qu'elle suit, et son balancement l'entoure.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneLive();
  const glm::vec3 owner{0.0F, 12.0F, 0.0F};
  Advance(tracks, 1, owner);
  CHECK(tracks[0].tail[kTailSlotOfHeight] > owner.y);
  CHECK(tracks[0].anchor.y > owner.y);
}

TEST_CASE("Cascade : la tête se BALANCE autour de sa hauteur", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneLive();
  float top = -1e9F;
  float bottom = 1e9F;
  // Un tour entier d'azimut, un degré par tic.
  for (std::int32_t tick = 0; tick < 360; ++tick) {
    Advance(tracks, 1, {0.0F, 0.0F, 0.0F});
    top = std::max(top, tracks[0].anchor.y);
    bottom = std::min(bottom, tracks[0].anchor.y);
  }
  CHECK(top > bottom);
  // Le balancement encadre la hauteur : il ne la décale pas.
  CHECK(tracks[0].tail[kTailSlotOfHeight] < top);
  CHECK(tracks[0].tail[kTailSlotOfHeight] > bottom);
}

TEST_CASE("Cascade : le rayon RESPIRE, et son angle propre reste dans le tour", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneLive();
  Advance(tracks, 200);
  CHECK(tracks[0].value[kSlotOfSpinDegrees] >= 0.0F);
  CHECK(tracks[0].value[kSlotOfSpinDegrees] < 360.0F);
}

TEST_CASE("Cascade : une SEULE marque fait tourner la tête sur elle-même", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> rotated = OneLive();
  rotated[0].mark[kMarkOfDrawing] = kMarkThatSpins;
  std::array<EffectTrack, kEffectTracksPerNode> fixed = OneLive();
  fixed[0].mark[kMarkOfDrawing] = kMarkThatSpins - 1;
  const std::int16_t start = rotated[0].degrees;
  Advance(rotated, 5);
  Advance(fixed, 5);
  CHECK(rotated[0].degrees != start);
  CHECK(fixed[0].degrees == start);
  // Et l'angle reste dans le tour, quoi qu'il arrive.
  Advance(rotated, 400);
  CHECK(rotated[0].degrees >= 0);
  CHECK(rotated[0].degrees < 360);
}
