#include "ro/render/effect_breathing_quad.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_breathing_quad_seed.h"
#include "ro/render/effect_node_pool.h"

using Catch::Approx;
using ro::render::ComposeBreathingQuad;
using ro::render::EffectBreathingQuadPattern;
using ro::render::EffectBreathingQuadPatternsFor;
using ro::render::EffectBreathingQuadPatternTable;
using ro::render::EffectBreathingQuadSeedFor;
using ro::render::EffectBreathingQuadStyleFor;
using ro::render::EffectNodePool;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kBreathingQuadDegreesPerTrack;
using ro::render::kEffectTracksPerNode;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectBreathingQuadTracks;
using ro::render::SowEffectBreathingQuadNode;

namespace {

constexpr glm::vec3 kAtTheOrigin{0.0F, 0.0F, 0.0F};

[[nodiscard]] float QuadArea(const EffectWorldQuad& quad) {
  const glm::vec3 ab = quad.corners[1] - quad.corners[0];
  const glm::vec3 ac = quad.corners[2] - quad.corners[0];
  const glm::vec3 ad = quad.corners[3] - quad.corners[0];
  return (glm::length(glm::cross(ab, ac)) + glm::length(glm::cross(ac, ad))) * 0.5F;
}

[[nodiscard]] EffectTrack TrackFromByte(std::uint8_t octet) {
  const EffectBreathingQuadPattern pattern{0, "essai", "image.tga", octet};
  std::uint32_t seed = 77U;
  return SeedEffectBreathingQuadTracks(pattern, seed).front();
}

}  // namespace

TEST_CASE("le carré qui respire a QUATRE coins sur un cercle", "[render][breathing_quad]") {
  // ⚠ IL N'Y A AUCUN RAYON : c'est un carré posé
  // à plat, ses quatre coins à quatre-vingt-dix degrés, tous à la même hauteur.
  const EffectTrack track = TrackFromByte(2);
  const std::vector<EffectWorldQuad> quads =
      ComposeBreathingQuad(track, EffectBreathingQuadSeedFor(2).node_style, 0, kAtTheOrigin);
  REQUIRE(quads.size() == 1);
  CHECK(QuadArea(quads.front()) > 0.0F);

  const EffectWorldQuad& quad = quads.front();
  // ⚠ DEUX UNITÉS AU-DESSUS de l'ancre : le semis garde le moins deux du
  // client, dont le repère descend, et la figure le retourne dans le nôtre.
  for (const glm::vec3& corner : quad.corners) {
    CHECK(corner.y == Approx(2.0F));
  }
  // Les coins opposés sont diamétralement opposés : leur milieu est le centre.
  CHECK(quad.corners[0].x + quad.corners[2].x == Approx(0.0F).margin(0.001F));
  CHECK(quad.corners[1].z + quad.corners[3].z == Approx(0.0F).margin(0.001F));
}

TEST_CASE("le rayon respire entre quatre cinquièmes et un, jamais au-delà",
          "[render][breathing_quad]") {
  // ⚠ LE CLIENT AJOUTE UN AU SINUS avant de le multiplier, si bien que le terme
  // oscillant reste POSITIF. Oublier ce « plus un » ferait battre le carré
  // autour de sa taille de repos au lieu de la frôler par en dessous.
  const float repos = EffectBreathingQuadSeedFor(2).radius_at_anchor;
  float smallest = repos * 2.0F;
  float plus_grand = 0.0F;
  for (std::int16_t tilt = 0; tilt < 360; ++tilt) {
    EffectTrack track = TrackFromByte(2);
    track.tilt_degrees = tilt;
    const std::vector<EffectWorldQuad> quads =
        ComposeBreathingQuad(track, EffectBreathingQuadSeedFor(2).node_style, 0, kAtTheOrigin);
    REQUIRE_FALSE(quads.empty());
    const float radius = std::hypot(quads.front().corners[0].x, quads.front().corners[0].z);
    smallest = std::min(smallest, radius);
    plus_grand = std::max(plus_grand, radius);
  }
  CHECK(smallest == Approx(repos * 0.8F).margin(0.01F));
  CHECK(plus_grand == Approx(repos).margin(0.01F));
}

TEST_CASE("l'angle de départ vient de DEUX endroits selon le style", "[render][breathing_quad]") {
  // ⚠ LE PIÈGE DE CETTE FIGURE. Pour cinq styles il vient de l'azimut ; pour les
  // autres, du RANG de la piste. Prendre toujours l'azimut superposerait les
  // quatre pistes.
  CHECK(EffectBreathingQuadStyleFor(111, 3.0F).starts_at_azimuth);
  CHECK(EffectBreathingQuadStyleFor(100, 0.0F).starts_at_azimuth);
  CHECK(EffectBreathingQuadStyleFor(10, 0.0F).starts_at_azimuth);
  CHECK(EffectBreathingQuadStyleFor(0, 0.0F).starts_at_azimuth);
  CHECK_FALSE(EffectBreathingQuadStyleFor(5, 2.0F).starts_at_azimuth);

  // ⚠ ET LE STYLE QUATRE N'Y A DROIT QUE SI LA VARIANTE VAUT AUSSI QUATRE. Le
  // client teste les deux ensemble ; n'en tester qu'un le rangerait du mauvais
  // côté la moitié du temps.
  CHECK(EffectBreathingQuadStyleFor(4, 4.0F).starts_at_azimuth);
  CHECK_FALSE(EffectBreathingQuadStyleFor(4, 2.0F).starts_at_azimuth);

  // Et le rang écarte vraiment les pistes quand il sert.
  const EffectTrack track = TrackFromByte(2);
  const std::int32_t style = EffectBreathingQuadSeedFor(2).node_style;
  const std::vector<EffectWorldQuad> first = ComposeBreathingQuad(track, style, 0, kAtTheOrigin);
  const std::vector<EffectWorldQuad> second = ComposeBreathingQuad(track, style, 1, kAtTheOrigin);
  REQUIRE_FALSE(first.empty());
  REQUIRE_FALSE(second.empty());
  CHECK(first.front().corners[0].x != Approx(second.front().corners[0].x));
  CHECK(kBreathingQuadDegreesPerTrack == 23);
}

TEST_CASE("le style décide de la teinte, du mélange et du nombre de couches",
          "[render][breathing_quad]") {
  SECTION("deux styles se MÉLANGENT, tous les autres s'ajoutent") {
    CHECK(EffectBreathingQuadStyleFor(0, 0.0F).blend_mode == 1);
    CHECK(EffectBreathingQuadStyleFor(110, 0.0F).blend_mode == 1);
    for (const std::int32_t style : {15, 17, 18, 100, 101, 102, 111, 112, 113, 114}) {
      INFO("style " << style);
      CHECK(EffectBreathingQuadStyleFor(style, 0.0F).blend_mode == 0);
    }
  }
  SECTION("deux styles peignent DEUX fois le même quad") {
    CHECK(EffectBreathingQuadStyleFor(100, 0.0F).layers == 2);
    CHECK(EffectBreathingQuadStyleFor(101, 0.0F).layers == 2);
    CHECK(EffectBreathingQuadStyleFor(111, 0.0F).layers == 1);
  }
  SECTION("un style inconnu est BLANC et S'AJOUTE, ce n'est pas une absence") {
    CHECK(EffectBreathingQuadStyleFor(5, 0.0F).color == std::array<std::uint8_t, 3>{255, 255, 255});
    CHECK(EffectBreathingQuadStyleFor(5, 0.0F).blend_mode == 0);
  }
}

TEST_CASE("l'octet choisit quatre valeurs d'un coup", "[render][breathing_quad]") {
  // ⚠ ET ELLES NE SUIVENT AUCUNE RÈGLE COMMUNE : le client les écrit branche par
  // branche. Les déduire l'une de l'autre donnerait six carrés au même rayon.
  std::set<float> radii;
  for (const std::uint8_t octet : std::array<std::uint8_t, 7>{1, 2, 3, 4, 10, 11, 12}) {
    radii.insert(EffectBreathingQuadSeedFor(octet).radius_at_anchor);
  }
  CHECK(radii.size() >= 5);
  CHECK(EffectBreathingQuadSeedFor(3).azimuth_degrees == 225);
  CHECK(EffectBreathingQuadSeedFor(3).alpha == 250);
  CHECK(EffectBreathingQuadSeedFor(3).node_style == 111);

  // ⚠ TROIS OCTETS POSENT LEUR INCLINAISON au lieu de la tirer, et comme c'est
  // elle qui mène le souffle, leur carré part à son rayon plein.
  for (const std::uint8_t octet : std::array<std::uint8_t, 3>{10, 11, 12}) {
    INFO("octet " << static_cast<int>(octet));
    CHECK(EffectBreathingQuadSeedFor(octet).tilt_is_fixed);
    CHECK(TrackFromByte(octet).tilt_degrees == 360);
  }
  CHECK_FALSE(EffectBreathingQuadSeedFor(2).tilt_is_fixed);
}

TEST_CASE("le mouvement ne fait avancer que ce que l'octet lui dit", "[render][breathing_quad]") {
  const auto after = [](std::uint8_t octet, int ticks) {
    const EffectBreathingQuadPattern pattern{0, "essai", "image.tga", octet};
    std::uint32_t seed = 5U;
    std::array<EffectTrack, kEffectTracksPerNode> tracks =
        SeedEffectBreathingQuadTracks(pattern, seed);
    for (int tick = 0; tick < ticks; ++tick) {
      ro::render::AdvanceBreathingQuadTracks(tracks);
    }
    return tracks.front();
  };

  // L'octet quatre tourne vite, les trois derniers lentement, les autres pas.
  CHECK(after(4, 10).azimuth_degrees == 60);
  CHECK(after(11, 10).azimuth_degrees == 30);
  CHECK(after(2, 10).azimuth_degrees == 0);

  // L'inclinaison avance d'un pour l'octet deux, de trois par défaut, et pas du
  // tout pour onze et douze.
  //
  // ⚠ ON COMPARE DEUX ÉTATS DU MÊME TIRAGE, et pas deux tirages : l'inclinaison
  // de départ est tirée au semis, si bien que deux graines différentes donnent
  // deux points de départ. Ce cas de test l'avait d'abord ignoré.
  CHECK(after(2, 10).tilt_degrees == after(2, 0).tilt_degrees + 10);
  CHECK(after(1, 10).tilt_degrees == after(1, 0).tilt_degrees + 30);

  // ⚠ ET L'INCLINAISON POSÉE À TROIS CENT SOIXANTE RETOMBE À ZÉRO au premier
  // tic, sans que rien ne l'ait incrémentée : la normalisation du client est
  // HORS des branches. C'est pourquoi le semis peut la poser hors du tour.
  CHECK(after(11, 0).tilt_degrees == 360);
  CHECK(after(11, 1).tilt_degrees == 0);
  CHECK(after(11, 10).tilt_degrees == 0);

  // ⚠ ET UN SEUL OCTET FAIT RESPIRER LE RAYON LUI-MÊME. Son azimut part à zéro
  // et monte de trois : tant qu'il n'a pas passé le demi-tour, le rayon PERD.
  CHECK(after(10, 10).radius_at_anchor < EffectBreathingQuadSeedFor(10).radius_at_anchor);
  CHECK(after(11, 10).radius_at_anchor == Approx(EffectBreathingQuadSeedFor(11).radius_at_anchor));
}

TEST_CASE("tous les carrés relevés sont en table, chacun avec SON image",
          "[render][breathing_quad]") {
  const std::span<const EffectBreathingQuadPattern> table = EffectBreathingQuadPatternTable();
  REQUIRE_FALSE(table.empty());
  CHECK(std::ranges::is_sorted(table, {}, &EffectBreathingQuadPattern::effect_id));

  for (const EffectBreathingQuadPattern& pattern : table) {
    INFO("effet " << pattern.effect_id);
    CHECK_FALSE(pattern.texture.empty());
    const EffectTrack track = TrackFromByte(pattern.builder_argument);
    const std::vector<EffectWorldQuad> quads = ComposeBreathingQuad(
        track, EffectBreathingQuadSeedFor(pattern.builder_argument).node_style, 0, kAtTheOrigin);
    REQUIRE_FALSE(quads.empty());
    for (const EffectWorldQuad& quad : quads) {
      CHECK(QuadArea(quad) > 0.0F);
    }
  }
}

TEST_CASE("le 828 pose TROIS carrés, et chacun a la sienne", "[render][breathing_quad]") {
  // ⚠ LE 828 N'EST PAS EXCLU : ce qui se garde est trois nœuds, trois octets,
  // trois images DIFFÉRENTES. Une seule image pour les trois passerait le
  // compte et montrerait le même carré trois fois.
  const std::span<const EffectBreathingQuadPattern> own_entries =
      EffectBreathingQuadPatternsFor(828);
  REQUIRE(own_entries.size() == 3);
  std::set<std::uint8_t> octets;
  std::set<std::string_view> images;
  for (const EffectBreathingQuadPattern& pattern : own_entries) {
    octets.insert(pattern.builder_argument);
    images.insert(pattern.texture);
    CHECK(pattern.texture.starts_with("data"));
    // ⚠ ET C'EST LE JEU COMPLET, PAS LE RÉDUIT. Le site choisit entre les deux
    // sur le réglage « effets minimaux », que nous n'avons pas.
    CHECK(pattern.texture.find("mineffect") == std::string_view::npos);
  }
  CHECK(octets == std::set<std::uint8_t>{10, 11, 12});
  CHECK(images.size() == 3);
}

TEST_CASE("le nœud semé porte son style et rejoint la troupe", "[render][breathing_quad]") {
  const EffectBreathingQuadPattern& pattern = EffectBreathingQuadPatternsFor(671).front();
  std::uint32_t seed = 2U;
  ro::render::PooledEffectNode node = SowEffectBreathingQuadNode(
      pattern, glm::vec3{6.0F, 0.0F, 1.0F}, ro::render::kEffectNodeDefaultLifeTicks, seed);
  CHECK(node.shape == PooledNodeShape::kBreathingQuad);
  // ⚠ LE STYLE VIT DANS LE NŒUD, pas dans la piste.
  CHECK(node.trail_variant == 111);
  CHECK(node.tracks[0].active);
  for (std::size_t rang = 1; rang < kEffectTracksPerNode; ++rang) {
    CHECK_FALSE(node.tracks[rang].active);
  }

  EffectNodePool pool;
  pool.Add(node);
  for (int tick = 0; tick < 12; ++tick) {
    pool.Advance(16);
  }
  REQUIRE(pool.size() == 1);
  // L'octet trois ne tourne pas, mais son inclinaison avance de trois par tic.
  CHECK(pool.nodes().front().tracks[0].azimuth_degrees == 225);
}
