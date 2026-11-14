#include "ro/render/effect_rainbow_trail.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_particle_figures.h"
#include "ro/render/effect_rainbow_trail_seed.h"
#include "ro/render/effect_track_motion.h"

using Catch::Approx;
using ro::render::AdvanceCameraAlignedArcTracks;
using ro::render::ComposeRainbowTrailQuads;
using ro::render::EffectRainbowTrailBandTint;
using ro::render::EffectRainbowTrailPattern;
using ro::render::EffectRainbowTrailPatternsFor;
using ro::render::EffectRainbowTrailPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kRainbowTrailBands;
using ro::render::kRainbowTrailRadius;
using ro::render::kRainbowTrailSweepDegrees;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectRainbowTrailTracks;
using ro::render::SowEffectRainbowTrailNode;

namespace {

constexpr std::uint32_t kRainbow = 410;
constexpr glm::vec3 kOrigin{0.0F, 0.0F, 0.0F};

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Advance(std::int32_t count) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectRainbowTrailTracks(0);
  for (std::int32_t tick = 0; tick < count; ++tick) {
    AdvanceCameraAlignedArcTracks(tracks);
  }
  return tracks;
}

}  // namespace

TEST_CASE("l'arc-en-ciel : sept couleurs, dans l'ordre du spectre", "[render][rainbow_trail]") {
  CHECK(EffectRainbowTrailBandTint(0) == std::array<std::uint8_t, 3>{255, 0, 0});
  CHECK(EffectRainbowTrailBandTint(1) == std::array<std::uint8_t, 3>{255, 126, 0});
  CHECK(EffectRainbowTrailBandTint(2) == std::array<std::uint8_t, 3>{255, 255, 0});
  CHECK(EffectRainbowTrailBandTint(3) == std::array<std::uint8_t, 3>{0, 255, 0});
  CHECK(EffectRainbowTrailBandTint(4) == std::array<std::uint8_t, 3>{0, 0, 255});
  CHECK(EffectRainbowTrailBandTint(5) == std::array<std::uint8_t, 3>{115, 50, 200});
  CHECK(EffectRainbowTrailBandTint(6) == std::array<std::uint8_t, 3>{255, 0, 255});
}

TEST_CASE("l'arc-en-ciel : il naît éteint et ne rend rien", "[render][rainbow_trail]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectRainbowTrailTracks(0);
  CHECK(tracks.front().active);
  CHECK(tracks.front().alpha == 0);
  CHECK(tracks.front().radius_at_anchor == Approx(kRainbowTrailRadius));
  CHECK(tracks.front().degrees == kRainbowTrailSweepDegrees);
  CHECK(tracks.front().azimuth_degrees == 0);
  // ⚠ LES TROIS AUTRES PISTES DORMENT, et elles rendent un lot vide d'elles-mêmes.
  CHECK_FALSE(tracks[1].active);
  CHECK(ComposeRainbowTrailQuads(tracks.front(), kOrigin).empty());
  CHECK(ComposeRainbowTrailQuads(tracks[1], kOrigin).empty());
}

TEST_CASE("l'arc-en-ciel : sept bandes de vingt et un segments", "[render][rainbow_trail]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = Advance(10);
  const std::vector<EffectWorldQuad> arc = ComposeRainbowTrailQuads(tracks.front(), kOrigin);
  // Un demi-tour coupé en vingt donne un pas de neuf, et vingt et un segments.
  REQUIRE(arc.size() == kRainbowTrailBands * 21);

  // La première bande est ROUGE, la dernière VIOLETTE.
  CHECK(arc.front().tint[0] == 255);
  CHECK(arc.front().tint[1] == 0);
  CHECK(arc.back().tint[0] == 255);
  CHECK(arc.back().tint[2] == 255);

  // ⚠ L'ORDONNÉE EST DOUBLÉE : au sommet de l'arc — quatre-vingt-dix degrés —,
  // la hauteur vaut DEUX fois le rayon, pas une. Le client écrit deux fois le
  // sinus, et lire un cercle écraserait l'arc de moitié.
  //
  // ⚠ ET AU PREMIER DEMI-TOUR DE L'AZIMUT, CE SOMMET EST SOUS LE NŒUD : le sinus
  // positif du client descend dans son repère.
  float lowest = 0.0F;
  for (const EffectWorldQuad& quad : arc) {
    for (const glm::vec3& corner : quad.corners) {
      lowest = std::min(lowest, corner.y);
    }
  }
  CHECK(lowest == Approx(-2.0F * kRainbowTrailRadius).margin(0.5));
}

TEST_CASE("l'arc-en-ciel : au plus fort de son alpha, il passe AU-DESSUS du noeud",
          "[render][rainbow_trail]") {
  // 🔴 À CENT QUATRE-VINGTS TICS, l'azimut a fait un demi-tour : l'arc couvre le
  // second demi-cercle, dont le sinus est négatif chez le client — vers le haut.
  // Garder la hauteur du client sans la nier le dessinait sous le nœud.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = Advance(180);
  const std::vector<EffectWorldQuad> arc = ComposeRainbowTrailQuads(tracks.front(), kOrigin);
  REQUIRE_FALSE(arc.empty());
  float highest = 0.0F;
  float lowest = 0.0F;
  for (const EffectWorldQuad& quad : arc) {
    for (const glm::vec3& corner : quad.corners) {
      highest = std::max(highest, corner.y);
      lowest = std::min(lowest, corner.y);
    }
  }
  CHECK(highest == Approx(2.0F * kRainbowTrailRadius).margin(0.5));
  // Le dernier segment dépasse le demi-tour d'un pas de neuf degrés, et lui
  // seul repasse un peu sous le nœud.
  CHECK(lowest > -0.5F * kRainbowTrailRadius);
}

TEST_CASE("l'arc-en-ciel : les bandes s'emboîtent sans fente", "[render][rainbow_trail]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = Advance(10);
  const std::vector<EffectWorldQuad> arc = ComposeRainbowTrailQuads(tracks.front(), kOrigin);
  REQUIRE(arc.size() == kRainbowTrailBands * 21);
  // ⚠ LE BORD EXTÉRIEUR D'UNE BANDE EST LE BORD INTÉRIEUR DE LA SUIVANTE. Le
  // premier segment de la bande zéro et celui de la bande un partagent donc deux
  // coins ; leur donner chacune son épaisseur laisserait sept fentes.
  const EffectWorldQuad& red = arc[0];
  const EffectWorldQuad& orange = arc[21];
  CHECK(red.corners[1].x == Approx(orange.corners[0].x).margin(0.001));
  CHECK(red.corners[1].y == Approx(orange.corners[0].y).margin(0.001));
  CHECK(red.corners[2].x == Approx(orange.corners[3].x).margin(0.001));
}

TEST_CASE("l'arc-en-ciel : il s'ouvre, puis se referme, puis se fige", "[render][rainbow_trail]") {
  // ⚠ L'ALPHA ET LE BALAYAGE SONT LE MÊME GESTE : tant que l'un monte, l'autre
  // avance.
  const std::array<EffectTrack, kEffectTracksPerNode> a_cent = Advance(100);
  CHECK(a_cent.front().alpha == 100);
  CHECK(a_cent.front().azimuth_degrees == 100);

  const std::array<EffectTrack, kEffectTracksPerNode> at_peak = Advance(180);
  CHECK(at_peak.front().alpha == 180);
  CHECK(at_peak.front().azimuth_degrees == 180);

  // Passé le cent quatre-vingtième tic il redescend, et l'azimut avance encore.
  const std::array<EffectTrack, kEffectTracksPerNode> after = Advance(200);
  CHECK(after.front().alpha == 160);
  CHECK(after.front().azimuth_degrees == 200);

  // ⚠ ET AU TROIS CENT SOIXANTIÈME TIC TOUT S'ARRÊTE : la piste reste vive, mais
  // rien ne bouge plus. Ce mouvement n'éteint jamais une piste.
  const std::array<EffectTrack, kEffectTracksPerNode> a_la_fin = Advance(360);
  const std::array<EffectTrack, kEffectTracksPerNode> long_after = Advance(500);
  CHECK(a_la_fin.front().alpha == 0);
  CHECK(long_after.front().alpha == a_la_fin.front().alpha);
  CHECK(long_after.front().azimuth_degrees == a_la_fin.front().azimuth_degrees);
  CHECK(long_after.front().active);
}

TEST_CASE("l'arc-en-ciel : son plan est FIGÉ à la naissance", "[render][rainbow_trail]") {
  // ⚠ L'INCLINAISON EST LE TOUR DE PANNEAU AU TIC DU SEMIS, pris tel quel — il
  // est déjà `360 − lacet` du client —, et le mouvement ne la touche jamais :
  // l'arc ne pivote pas quand le joueur tourne l'écran.
  const std::array<EffectTrack, kEffectTracksPerNode> face_a_l_est =
      SeedEffectRainbowTrailTracks(0);
  const std::array<EffectTrack, kEffectTracksPerNode> face_au_sud =
      SeedEffectRainbowTrailTracks(270);
  CHECK(face_a_l_est.front().tilt_degrees == 0);
  CHECK(face_au_sud.front().tilt_degrees == 270);

  std::array<EffectTrack, kEffectTracksPerNode> tracks = face_au_sud;
  for (std::int32_t tick = 0; tick < 50; ++tick) {
    AdvanceCameraAlignedArcTracks(tracks);
  }
  CHECK(tracks.front().tilt_degrees == 270);
}

TEST_CASE("l'arc-en-ciel : son plan fait face à la caméra du semis", "[render][rainbow_trail]") {
  // ⚠ LE COMPLÉMENT NE SE PREND QU'UNE FOIS, et le cap revient au nôtre par `S` :
  // l'arc s'étend le long de la droite de l'écran, perpendiculaire à la visée,
  // quel que soit le lacet. Pris deux fois, il se présentait par la tranche dès
  // qu'on avait tourné d'un quart.
  for (const float yaw : {0.0F, 37.0F, 90.0F, 200.0F, -75.0F}) {
    ro::render::Camera camera;
    camera.yaw_degrees = yaw;
    camera.pitch_degrees = 50.0F;
    std::array<EffectTrack, kEffectTracksPerNode> tracks =
        SeedEffectRainbowTrailTracks(ro::render::EffectBillboardTurnDegrees(camera));
    for (std::int32_t tick = 0; tick < 10; ++tick) {
      AdvanceCameraAlignedArcTracks(tracks);
    }
    const std::vector<EffectWorldQuad> arc = ComposeRainbowTrailQuads(tracks.front(), kOrigin);
    REQUIRE_FALSE(arc.empty());
    const glm::vec3 sight = camera.Position() - camera.look_at;
    const glm::vec3 level_sight = glm::normalize(glm::vec3{sight.x, 0.0F, sight.z});
    for (const EffectWorldQuad& quad : arc) {
      for (const glm::vec3& corner : quad.corners) {
        const glm::vec3 level{corner.x, 0.0F, corner.z};
        CHECK(glm::dot(level, level_sight) == Approx(0.0F).margin(0.05));
      }
    }
  }
}

TEST_CASE("la table des arcs porte son unique effet", "[render][rainbow_trail]") {
  const std::span<const EffectRainbowTrailPattern> table = EffectRainbowTrailPatternTable();
  REQUIRE(table.size() == 1);
  CHECK(EffectRainbowTrailPatternsFor(kRainbow).size() == 1);
  CHECK(EffectRainbowTrailPatternsFor(1U).empty());

  const ro::render::PooledEffectNode node =
      SowEffectRainbowTrailNode(table.front(), glm::vec3{1.0F, 2.0F, 3.0F}, 60, 45);
  CHECK(node.shape == PooledNodeShape::kRainbowTrail);
  CHECK(node.node.life_ticks == 60);
  CHECK(node.resource_paths.size() == 1);
  CHECK(node.tracks.front().tilt_degrees == 45);
}
