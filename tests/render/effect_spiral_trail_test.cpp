#include "ro/render/effect_spiral_trail.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_spiral_trail_seed.h"
#include "ro/render/effect_track_motion.h"

using Catch::Approx;
using ro::render::AdvanceSwirlOrbitTracks;
using ro::render::ComposeSpiralTrailQuads;
using ro::render::EffectNodeLifeFor;
using ro::render::EffectSpiralTrailPattern;
using ro::render::EffectSpiralTrailPatternsFor;
using ro::render::EffectSpiralTrailPatternTable;
using ro::render::EffectSpiralTrailTintFor;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectLifeForever;
using ro::render::kEffectTracksPerNode;
using ro::render::kLastTickOfSpiralTrailClimb;
using ro::render::kMarkOfSpiralTrailSpark;
using ro::render::kMarkOfSpiralTrailTint;
using ro::render::kSpiralTrailAnchorShiftX;
using ro::render::kSpiralTrailDelayFloorTicks;
using ro::render::kSpiralTrailHalfSize;
using ro::render::kSpiralTrailImagesPerTrack;
using ro::render::kSpiralTrailTrailShrink;
using ro::render::kTailOfSpiralTrailRecent;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectSpiralTrailTracks;
using ro::render::SowEffectSpiralTrailNode;

namespace {

constexpr std::uint32_t kPokjuk = 297;
constexpr glm::vec3 kOrigin{0.0F, 0.0F, 0.0F};

// UNE PISTE POSÉE À LA MAIN, pour n'éprouver qu'une règle à la fois.
[[nodiscard]] EffectTrack Grain() {
  EffectTrack track;
  track.active = true;
  track.alpha = 200;
  track.lift_key = kSpiralTrailHalfSize;
  track.anchor = {10.0F, 20.0F, 30.0F};
  track.tail[kTailOfSpiralTrailRecent] = 11.0F;
  track.tail[kTailOfSpiralTrailRecent + 1] = 21.0F;
  track.tail[kTailOfSpiralTrailRecent + 2] = 31.0F;
  return track;
}

}  // namespace

TEST_CASE("la gerbe : cinq teintes, et la cinquième est le cas sans test",
          "[render][spiral_trail]") {
  CHECK(EffectSpiralTrailTintFor(0) == std::array<std::uint8_t, 3>{70, 70, 255});
  CHECK(EffectSpiralTrailTintFor(1) == std::array<std::uint8_t, 3>{255, 70, 70});
  CHECK(EffectSpiralTrailTintFor(2) == std::array<std::uint8_t, 3>{70, 255, 70});
  CHECK(EffectSpiralTrailTintFor(3) == std::array<std::uint8_t, 3>{255, 255, 90});
  // ⚠ LE SEMIS TIRE SUR CINQ : la quatrième marque existe et tombe ici.
  CHECK(EffectSpiralTrailTintFor(4) == std::array<std::uint8_t, 3>{255, 70, 255});
}

TEST_CASE("la gerbe : le grain, puis sa trace plus petite et deux fois plus pâle",
          "[render][spiral_trail]") {
  const EffectTrack track = Grain();
  const std::vector<EffectWorldQuad> spray = ComposeSpiralTrailQuads(track, 0, 0);
  REQUIRE(spray.size() == kSpiralTrailImagesPerTrack);

  // Le grain est à son ancre, la trace là où il était trois tics plus tôt.
  glm::vec3 centre_du_grain{0.0F, 0.0F, 0.0F};
  glm::vec3 centre_de_la_trace{0.0F, 0.0F, 0.0F};
  for (const glm::vec3& corner : spray.front().corners) {
    centre_du_grain += corner * 0.25F;
  }
  for (const glm::vec3& corner : spray.back().corners) {
    centre_de_la_trace += corner * 0.25F;
  }
  CHECK(centre_du_grain.x == Approx(track.anchor.x).margin(0.001));
  CHECK(centre_de_la_trace.x == Approx(11.0F).margin(0.001));

  // ⚠ LA TRACE EST PLUS PETITE, PAS PLUS LOIN : la demi-taille perd un demi.
  const float half_of_grain = spray.front().corners[0].x - centre_du_grain.x;
  const float half_of_trace = spray.back().corners[0].x - centre_de_la_trace.x;
  CHECK(half_of_grain == Approx(kSpiralTrailHalfSize).margin(0.001));
  CHECK(half_of_trace == Approx(kSpiralTrailHalfSize - kSpiralTrailTrailShrink).margin(0.001));

  // ⚠ ET SON ALPHA EST DIVISÉ PAR DEUX, pas retranché.
  CHECK(spray.front().tint[3] == 200);
  CHECK(spray.back().tint[3] == 100);
}

TEST_CASE("la gerbe : l'étincelle change l'image, la teinte ET l'alpha", "[render][spiral_trail]") {
  EffectTrack track = Grain();
  track.mark[kMarkOfSpiralTrailTint] = 1;
  const std::vector<EffectWorldQuad> sans = ComposeSpiralTrailQuads(track, 0, 0);
  REQUIRE_FALSE(sans.empty());
  CHECK(sans.front().texture == 0);
  CHECK(sans.front().tint[0] == 255);
  CHECK(sans.front().tint[1] == 70);

  track.mark[kMarkOfSpiralTrailSpark] = 2;
  const std::vector<EffectWorldQuad> with = ComposeSpiralTrailQuads(track, 0, 0);
  REQUIRE_FALSE(with.empty());
  // L'image est celle que la marque désigne, et la teinte redevient BLANCHE.
  CHECK(with.front().texture == 2);
  CHECK(with.front().tint[0] == 255);
  CHECK(with.front().tint[1] == 255);
  CHECK(with.front().tint[2] == 255);
  // Et l'alpha perd les trois quarts.
  CHECK(with.front().tint[3] == 50);
}

TEST_CASE("la gerbe : le grain monte INVISIBLE, puis s'allume d'un coup",
          "[render][spiral_trail]") {
  std::uint32_t generator = 7U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpiralTrailTracks(kOrigin, generator);
  // ⚠ TOUTES LES PISTES SONT VIVES ET TOUTES SONT RETARDÉES : leur compteur part
  // de moins trois cents au moins.
  for (const EffectTrack& track : tracks) {
    CHECK(track.active);
    CHECK(track.alpha == 0);
    CHECK(track.ticks <= kSpiralTrailDelayFloorTicks);
    CHECK(track.anchor.x == Approx(kSpiralTrailAnchorShiftX));
  }

  // On remet la première piste au premier tic pour suivre sa montée sans jouer
  // trois cents tics de retard.
  tracks.front().ticks = 0;
  const float start = tracks.front().anchor.y;
  for (std::int32_t tick = 0; tick < 100; ++tick) {
    AdvanceSwirlOrbitTracks(tracks, 0, 0, kOrigin, generator);
  }
  // Elle s'est ÉLEVÉE d'un quart par tic — le client retranche dans son repère
  // qui descend —, et n'a toujours pas d'alpha.
  CHECK(tracks.front().anchor.y == Approx(start + 25.0F).margin(0.001));
  CHECK(tracks.front().alpha == 0);
  CHECK(ComposeSpiralTrailQuads(tracks.front(), 0, 0).empty());

  // ⚠ AU CENT CINQUANTE-CINQUIÈME TIC ELLE S'ALLUME, et le vol commence.
  for (std::int32_t tick = 0; tick < 55; ++tick) {
    AdvanceSwirlOrbitTracks(tracks, 0, 0, kOrigin, generator);
  }
  CHECK(tracks.front().ticks == 155);
  CHECK(tracks.front().alpha > 0);
  CHECK_FALSE(ComposeSpiralTrailQuads(tracks.front(), 0, 0).empty());
}

TEST_CASE("la gerbe : la trace suit de trois tics, et la piste RENAÎT", "[render][spiral_trail]") {
  std::uint32_t generator = 3U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpiralTrailTracks(kOrigin, generator);
  tracks.front().ticks = 0;
  tracks.front().mark[1] = 1;  // en vol, pour que l'ancre bouge
  tracks.front().alpha = 250;

  AdvanceSwirlOrbitTracks(tracks, 0, 0, kOrigin, generator);
  AdvanceSwirlOrbitTracks(tracks, 0, 0, kOrigin, generator);
  const glm::vec3 after_two_ticks = tracks.front().anchor;
  AdvanceSwirlOrbitTracks(tracks, 0, 0, kOrigin, generator);
  // ⚠ LE DÉCALAGE SE FAIT AU TROISIÈME TIC, ET AVANT QUE CE TIC-LÀ NE BOUGE
  // L'ANCRE : la trace porte donc la position de la fin du DEUXIÈME tic, pas
  // celle du troisième. Décaler après le déplacement ferait une trace collée au
  // grain, c'est-à-dire aucune trace.
  CHECK(tracks.front().tail[kTailOfSpiralTrailRecent] == Approx(after_two_ticks.x).margin(0.001));
  CHECK(tracks.front().anchor.x != Approx(after_two_ticks.x).margin(0.001));

  // ⚠ ET LA PISTE NE MEURT PAS : au deux millième tic elle repart de zéro, en
  // montée, avec une direction neuve.
  tracks.front().ticks = 2000;
  AdvanceSwirlOrbitTracks(tracks, 0, 0, kOrigin, generator);
  CHECK(tracks.front().ticks == 0);
  CHECK(tracks.front().mark[1] == 0);
  CHECK(tracks.front().active);
  CHECK(tracks.front().anchor.x == Approx(kSpiralTrailAnchorShiftX));
}

TEST_CASE("la gerbe : en vol, sa direction tiree est un cap du client", "[render][spiral_trail]") {
  // ⚠ L'ANCRE EST DANS NOTRE REPÈRE ET LE CAP DANS CELUI DU CLIENT : un quart de
  // tour l'envoie vers NOTRE profondeur négative. Le tirage couvre le tour, si
  // bien que le miroir ne se voyait qu'à graine égale.
  std::uint32_t generator = 3U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpiralTrailTracks(kOrigin, generator);
  EffectTrack& grain = tracks.front();
  grain.ticks = 0;
  grain.mark[1] = 1;  // en vol
  grain.alpha = 250;
  grain.degrees = 0;  // à plat : tout le pas va dans le plan du sol
  grain.tilt_degrees = 90;
  grain.radius_at_anchor = 2.0F;
  const glm::vec3 before = grain.anchor;
  AdvanceSwirlOrbitTracks(tracks, 0, 0, kOrigin, generator);
  CHECK(grain.anchor.z - before.z == Approx(-2.0F).margin(0.001));
  CHECK(grain.anchor.x - before.x == Approx(0.0F).margin(0.001));
}

TEST_CASE("la table des gerbes porte ses trois images dans l'ordre du client",
          "[render][spiral_trail]") {
  const std::span<const EffectSpiralTrailPattern> table = EffectSpiralTrailPatternTable();
  REQUIRE(table.size() == 1);
  CHECK(EffectSpiralTrailPatternsFor(kPokjuk).size() == 1);
  CHECK(EffectSpiralTrailPatternsFor(1U).empty());

  const EffectSpiralTrailPattern& pattern = table.front();
  REQUIRE(pattern.textures.size() == 3);
  // ⚠ L'ORDRE EST CELUI DU CLIENT, et la marque d'étincelle l'indexe : échanger
  // les deux dernières donnerait la mauvaise étincelle.
  CHECK(pattern.textures[0].ends_with("pok1.tga"));
  CHECK(pattern.textures[1].ends_with("pok3.tga"));
  CHECK(pattern.textures[2].ends_with("pok2.tga"));

  std::uint32_t generator = 11U;
  const ro::render::PooledEffectNode node =
      SowEffectSpiralTrailNode(pattern, glm::vec3{1.0F, 2.0F, 3.0F}, 60, generator);
  CHECK(node.shape == PooledNodeShape::kSpiralTrail);
  CHECK(node.resource_paths.size() == 3);
  CHECK(node.node.life_ticks == 60);
}

TEST_CASE("la gerbe vit SANS FIN, et il lui faut ça pour paraître", "[render][spiral_trail]") {
  // ⚠ SA VIE VIENT DE L'EFFET, PAS DU BÂTISSEUR : le bâtisseur copie la durée
  // de l'effet dans le nœud, et cette valeur-là se lit dans une table que le
  // client garnit au démarrage. 99999999 y est l'écriture de l'infini. ADR 0042.
  const EffectSpiralTrailPattern& pattern = EffectSpiralTrailPatternTable().front();
  const std::int32_t life = EffectNodeLifeFor(pattern.effect_id);
  CHECK(life == kEffectLifeForever);

  // ⚠ ET UNE VIE BORNÉE LA TUERAIT AVANT QU'ELLE PEIGNE, ce qui est le vrai
  // objet de ce test. Son retard est d'au moins trois cents tics, et le grain
  // monte encore cent cinquante-quatre tics avant que le mouvement ne l'allume.
  // Les cent tics du constructeur de nœud n'y suffisent pas — c'est ce qui l'a
  // fait passer pour une figure cassée, puis pour un effet que le jeu ne montre
  // pas. Il le montre.
  std::uint32_t generator = 3U;
  const ro::render::PooledEffectNode node =
      SowEffectSpiralTrailNode(pattern, glm::vec3{}, life, generator);
  for (const ro::render::EffectTrack& track : node.tracks) {
    REQUIRE(track.active);
    CHECK(track.ticks <= kSpiralTrailDelayFloorTicks);
    // Le tic où elle s'allume, compté depuis zéro, reste très en deçà de sa vie.
    CHECK(-track.ticks + kLastTickOfSpiralTrailClimb < node.node.life_ticks);
  }
}
