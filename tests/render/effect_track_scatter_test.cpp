#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

using ro::render::AdvanceScatteringParticleTracks;
using ro::render::EffectScatterWorld;
using ro::render::EffectTrack;
using ro::render::kEffectTracksPerNode;

namespace {

constexpr std::size_t kSlotOfHeightGate = 6;
constexpr std::size_t kSlotOfStyle = 7;
constexpr std::size_t kSlotOfSpinDegrees = 8;
constexpr std::size_t kMarkOfTexture = 0;
constexpr std::size_t kMarkOfDrawing = 2;

// Les styles que la mesure distingue.
constexpr float kStyleGatedByHeight = 0.0F;
constexpr float kStyleOnTheGround = 3.0F;
constexpr float kStyleOrdinary = 7.0F;
constexpr float kStyleThatFreezes = 99.0F;

constexpr std::uint8_t kMarkThatCyclesFrames = 23;
constexpr std::int32_t kTicksBetweenFrames = 14;
constexpr std::int16_t kShortLifeTicks = 5;
// COMBIEN DE TICS AVANT QU'UNE PARTICULE RENAISSE, et ce n'est pas sa durée de
// vie. Son alpha MONTE de trois par tic jusqu'au quatre-vingtième et ne descend
// que d'un : il culmine vers cent soixante-cinq, et il faut autant de tics pour
// le ramener à zéro. Un test qui n'avancerait que de cent vingt tics ne verrait
// que la dérive et croirait éprouver la renaissance.
constexpr std::int32_t kTicksUntilRebirth = 400;

std::array<EffectTrack, kEffectTracksPerNode> OneLive(float style) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].value[kSlotOfStyle] = style;
  // L'azimut n'est pas un angle ici : c'est la durée au bout de laquelle la
  // piste commence à s'éteindre.
  tracks[0].azimuth_degrees = kShortLifeTicks;
  return tracks;
}

void Advance(std::array<EffectTrack, kEffectTracksPerNode>& tracks, std::int32_t ticks,
             const EffectScatterWorld& world, std::uint32_t& seed, glm::vec3 node = {}) {
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceScatteringParticleTracks(tracks, node, 0, 0, world, seed);
  }
}

}  // namespace

TEST_CASE("Dispersion : l'alpha MONTE puis la piste RENAÎT ailleurs", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneLive(kStyleOrdinary);
  EffectScatterWorld world;
  std::uint32_t seed = 1;

  Advance(tracks, 3, world, seed);
  CHECK(tracks[0].alpha > 0);

  // ⚠ C'EST LA RENAISSANCE QUI FAIT DURER LA NUÉE, et rien d'autre ne la
  // ressème : un mouvement qui laisserait la piste morte viderait l'écran au
  // bout de quelques dizaines de tics.
  const glm::vec3 start = tracks[0].anchor;
  Advance(tracks, kTicksUntilRebirth, world, seed);
  CHECK(tracks[0].anchor != start);
  CHECK(tracks[0].ticks >= 0);
}

TEST_CASE("Dispersion : un style GELÉ ne fait que respirer", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneLive(kStyleThatFreezes);
  EffectScatterWorld world;
  std::uint32_t seed = 2;
  Advance(tracks, 50, world, seed);
  // Ni alpha, ni tics, ni ancre : le client saute tout le corps du mouvement.
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[0].ticks == 0);
  CHECK(tracks[0].anchor == glm::vec3{0.0F, 0.0F, 0.0F});
  // Mais la respiration continue, et c'est ce qui l'empêche de paraître morte.
  CHECK(tracks[0].value[kSlotOfSpinDegrees] > 0.0F);
  CHECK(tracks[0].value[kSlotOfSpinDegrees] < 360.0F);
}

TEST_CASE("Dispersion : la PORTE du style zéro le garde éteint", "[render][track]") {
  // ⚠ LE SEUL STYLE DONT L'ALPHA DÉPEND D'UNE POSITION. Sa porte est écrite dans
  // le repère du client, qui descend : moins cent cinquante-deux, soit cent
  // cinquante-deux unités AU-DESSUS de l'origine. Un nœud plus bas qu'elle
  // garde la particule éteinte ; plus haut, elle s'allume. Comparer la porte à
  // notre hauteur sans la convertir inversait les deux.
  constexpr float kGate = -152.0F;
  std::array<EffectTrack, kEffectTracksPerNode> fermee = OneLive(kStyleGatedByHeight);
  fermee[0].value[kSlotOfHeightGate] = kGate;
  EffectScatterWorld world;
  std::uint32_t seed = 3;
  Advance(fermee, 40, world, seed, {0.0F, 100.0F, 0.0F});
  CHECK(fermee[0].alpha == 0);

  std::array<EffectTrack, kEffectTracksPerNode> opened = OneLive(kStyleGatedByHeight);
  opened[0].value[kSlotOfHeightGate] = kGate;
  std::uint32_t other = 3;
  Advance(opened, 3, world, other, {0.0F, 200.0F, 0.0F});
  CHECK(opened[0].alpha > 0);
}

TEST_CASE("Dispersion : le style AU SOL prend l'altitude qu'on lui donne", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneLive(kStyleOnTheGround);
  EffectScatterWorld world;
  bool consulte = false;
  world.ground_height = [&consulte](float, float) {
    consulte = true;
    return 40.0F;
  };
  std::uint32_t seed = 4;
  Advance(tracks, kTicksUntilRebirth, world, seed);
  CHECK(consulte);
  // Le client retranche vingt à une altitude lue dans un repère qui DESCEND :
  // la particule se tient donc AU-DESSUS du sol, pas dessous.
  CHECK(tracks[0].anchor.y > 40.0F);
}

TEST_CASE("Dispersion : sans altitude, la particule ne s'invente pas de hauteur",
          "[render][track]") {
  // Un sol qu'on ne sait pas lire laisse la particule à la hauteur de son nœud.
  // C'est visiblement faux sur un relief, et préférable à une valeur inventée.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneLive(kStyleOnTheGround);
  const EffectScatterWorld muet;
  std::uint32_t seed = 5;
  Advance(tracks, kTicksUntilRebirth, muet, seed);
  CHECK(tracks[0].anchor.y == Catch::Approx(0.0F));
}

TEST_CASE("Dispersion : la boîte du style ordinaire ÉVITE son centre", "[render][track]") {
  // ⚠ C'EST CE QUI FAIT LE CHAMP D'ÉTOILES. Tirer dans un carré centré
  // donnerait une nuée dense au milieu, là où le client en creuse le cœur.
  EffectScatterWorld world;
  std::size_t nees = 0;
  for (std::uint32_t attempt = 0; attempt < 40; ++attempt) {
    std::array<EffectTrack, kEffectTracksPerNode> tracks = OneLive(kStyleOrdinary);
    std::uint32_t own = attempt + 1;
    Advance(tracks, kTicksUntilRebirth, world, own);
    if (tracks[0].anchor.x == 0.0F && tracks[0].anchor.z == 0.0F) {
      continue;
    }
    ++nees;
    // La dérive n'ajoute qu'une fraction par tic ; le creux tient largement.
    CHECK(std::abs(tracks[0].anchor.x) > 1.0F);
    CHECK(std::abs(tracks[0].anchor.z) > 1.0F);
  }
  CHECK(nees > 0);
}

TEST_CASE("Dispersion : la carte large DOUBLE la boîte", "[render][track]") {
  EffectScatterWorld etroite;
  EffectScatterWorld large;
  large.wide_scatter = true;
  float narrowest_max = 0.0F;
  float maxi_large = 0.0F;
  for (std::uint32_t attempt = 0; attempt < 60; ++attempt) {
    std::array<EffectTrack, kEffectTracksPerNode> here = OneLive(kStyleOrdinary);
    std::array<EffectTrack, kEffectTracksPerNode> la = OneLive(kStyleOrdinary);
    std::uint32_t a = attempt + 1;
    std::uint32_t b = attempt + 1;
    Advance(here, kTicksUntilRebirth, etroite, a);
    Advance(la, kTicksUntilRebirth, large, b);
    narrowest_max = std::max(narrowest_max, std::abs(here[0].anchor.x));
    maxi_large = std::max(maxi_large, std::abs(la[0].anchor.x));
  }
  CHECK(maxi_large > narrowest_max);
}

TEST_CASE("Dispersion : les trois angles errants restent dans le tour", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneLive(kStyleOrdinary);
  EffectScatterWorld world;
  std::uint32_t seed = 7;
  Advance(tracks, 500, world, seed);
  for (std::size_t pair = 0; pair < 3; ++pair) {
    INFO("couple " << pair);
    CHECK(tracks[0].value[2 * pair] >= 0.0F);
    CHECK(tracks[0].value[2 * pair] <= 359.0F);
    CHECK(tracks[0].value[2 * pair + 1] >= 0.0F);
    CHECK(tracks[0].value[2 * pair + 1] <= 359.0F);
  }
}

TEST_CASE("Dispersion : une seule marque fait DÉFILER son image", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> defile = OneLive(kStyleOrdinary);
  defile[0].mark[kMarkOfDrawing] = kMarkThatCyclesFrames;
  std::array<EffectTrack, kEffectTracksPerNode> fixed = OneLive(kStyleOrdinary);
  fixed[0].mark[kMarkOfDrawing] = kMarkThatCyclesFrames - 1;
  EffectScatterWorld world;
  std::uint32_t seed = 8;
  std::uint32_t other = 8;
  Advance(defile, kTicksBetweenFrames, world, seed);
  Advance(fixed, kTicksBetweenFrames, world, other);
  CHECK(defile[0].mark[kMarkOfTexture] == 1);
  CHECK(fixed[0].mark[kMarkOfTexture] == 0);
}

TEST_CASE("Dispersion : une piste éteinte ne bouge pas", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneLive(kStyleOrdinary);
  tracks[0].active = false;
  EffectScatterWorld world;
  std::uint32_t seed = 9;
  Advance(tracks, 50, world, seed);
  CHECK(tracks[0].ticks == 0);
  CHECK(tracks[0].value[kSlotOfSpinDegrees] == Catch::Approx(0.0F));
}
