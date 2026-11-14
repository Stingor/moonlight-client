#include "ro/render/effect_rain_particle.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_rain_particle_seed.h"
#include "ro/render/effect_track_motion.h"

using Catch::Approx;
using ro::render::AdvanceCameraFacingRadialTracks;
using ro::render::ComposeRainParticleQuads;
using ro::render::EffectRainParticlePattern;
using ro::render::EffectRainParticlePatternsFor;
using ro::render::EffectRainParticlePatternTable;
using ro::render::EffectRainParticleTintFor;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kMarkOfRainParticleTint;
using ro::render::kRainParticleDepthFloor;
using ro::render::kRainParticleHaloScale;
using ro::render::kRainParticleHaloTint;
using ro::render::kRainParticleLiftFloor;
using ro::render::kRainParticleRadiusFloor;
using ro::render::kSlotOfRainParticleOrbitRadius;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectRainParticleTracks;
using ro::render::SowEffectRainParticleNode;

namespace {

constexpr std::uint32_t kRainParticle2 = 944;
constexpr glm::vec3 kAtOrigin{0.0F, 0.0F, 0.0F};

// UNE PISTE POSEE A LA MAIN, pour n'eprouver qu'une regle a la fois.
[[nodiscard]] EffectTrack Grain(std::uint8_t mark, float depth) {
  EffectTrack track;
  track.active = true;
  track.alpha = 0;
  track.ticks = 0;
  track.degrees = 0;
  track.lift_key = kRainParticleLiftFloor;
  track.radius_at_anchor = kRainParticleRadiusFloor;
  track.value[kSlotOfRainParticleOrbitRadius] = 10.0F;
  track.anchor = {0.0F, depth, 0.0F};
  track.mark[kMarkOfRainParticleTint] = mark;
  return track;
}

}  // namespace

TEST_CASE("le grain nait AU-DESSUS de son noeud", "[render][rain_particle]") {
  // Le semis range sa hauteur comme le client, NEGATIVE dans un repere qui
  // descend : soixante-dix unites au-dessus du noeud. Recopiee, elle posait le
  // grain sous le sol.
  const std::vector<EffectWorldQuad> quads =
      ComposeRainParticleQuads(Grain(1, -70.0F), 0, 0, kAtOrigin);
  REQUIRE_FALSE(quads.empty());
  for (const EffectWorldQuad& quad : quads) {
    for (const glm::vec3& corner : quad.corners) {
      CHECK(corner.y > 50.0F);
    }
  }
}

TEST_CASE("le halo parait meme quand la marque n'est pas nommee", "[render][rain_particle]") {
  std::array<std::uint8_t, 3> tint{};
  CHECK(EffectRainParticleTintFor(0, tint));
  CHECK(tint == std::array<std::uint8_t, 3>{255, 255, 255});
  CHECK(EffectRainParticleTintFor(1, tint));
  CHECK(tint == std::array<std::uint8_t, 3>{255, 160, 160});
  CHECK(EffectRainParticleTintFor(2, tint));
  CHECK(tint == std::array<std::uint8_t, 3>{255, 90, 90});
  // ⚠ LE CLIENT N'A PAS DE CAS PAR DEFAUT : une quatrieme marque n'emet pas de
  // grain, et seul le halo reste.
  CHECK_FALSE(EffectRainParticleTintFor(3, tint));

  const std::vector<EffectWorldQuad> nomme =
      ComposeRainParticleQuads(Grain(1, -40.0F), 0, 0, kAtOrigin);
  CHECK(nomme.size() == 2);
  const std::vector<EffectWorldQuad> unknown =
      ComposeRainParticleQuads(Grain(7, -40.0F), 0, 0, kAtOrigin);
  REQUIRE(unknown.size() == 1);
  CHECK(unknown.front().tint[0] == kRainParticleHaloTint[0]);
  CHECK(unknown.front().tint[1] == kRainParticleHaloTint[1]);
  CHECK(unknown.front().tint[2] == kRainParticleHaloTint[2]);
}

TEST_CASE("le halo est le grain agrandi d'un dixieme", "[render][rain_particle]") {
  const EffectTrack track = Grain(0, -40.0F);
  const std::vector<EffectWorldQuad> quads = ComposeRainParticleQuads(track, 0, 0, kAtOrigin);
  REQUIRE(quads.size() == 2);

  // ⚠ L'AGRANDISSEMENT PORTE SUR LE COIN AVANT SON PORT A L'ANCRE : les deux
  // carres restent centres sur le meme point. Agrandir apres coup aurait
  // eloigne le halo de son grain.
  for (std::size_t corner = 0; corner < 4; ++corner) {
    const glm::vec3 grain = quads.front().corners[corner] - track.anchor;
    const glm::vec3 halo = quads.back().corners[corner] - track.anchor;
    CHECK(halo.x == Approx(grain.x * kRainParticleHaloScale));
    CHECK(halo.z == Approx(grain.z * kRainParticleHaloScale));
  }
}

TEST_CASE("le grain remonte puis s'eteint", "[render][rain_particle]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks.front() = Grain(0, -10.0F);

  // ⚠ L'ALPHA MONTE DE VINGT-CINQ PENDANT LES NEUF PREMIERS TICS, et le dixieme
  // ne le touche plus : la borne du client est stricte.
  AdvanceCameraFacingRadialTracks(tracks, 0, 0);
  CHECK(tracks.front().alpha == 25);
  CHECK(tracks.front().anchor.y == Approx(-10.0F + kRainParticleLiftFloor));
  // ⚠ ET L'ANCRE SE RECALCULE DEPUIS L'AZIMUT : a zero degre, le grain est sur
  // l'abscisse, a son rayon de disque.
  CHECK(tracks.front().anchor.x == Approx(10.0F));
  CHECK(tracks.front().anchor.z == Approx(0.0F));

  // Quatre tics de plus suffisent a franchir le zero : la profondeur part de
  // moins dix et remonte de deux et demie par tic.
  for (std::int32_t tick = 0; tick < 3; ++tick) {
    AdvanceCameraFacingRadialTracks(tracks, 0, 0);
  }
  CHECK(tracks.front().alpha == 100);
  CHECK(tracks.front().anchor.y == Approx(0.0F));

  // ⚠ UNE FOIS LE NŒUD REJOINT, LE GRAIN CESSE DE MONTER ET COMMENCE A PERDRE.
  // Le cinquieme tic monte encore de vingt-cinq, puis retranche quinze.
  AdvanceCameraFacingRadialTracks(tracks, 0, 0);
  CHECK(tracks.front().anchor.y == Approx(0.0F));
  CHECK(tracks.front().alpha == 110);

  // ⚠ LA PISTE MEURT PAR SON ALPHA, jamais par son age.
  for (std::int32_t tick = 0; tick < 40 && tracks.front().active; ++tick) {
    AdvanceCameraFacingRadialTracks(tracks, 0, 0);
  }
  CHECK_FALSE(tracks.front().active);
  CHECK(tracks.front().alpha == 0);
}

TEST_CASE("les deux angles de camera vont dans les styles", "[render][rain_particle]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  // ⚠ ILS ARRIVENT DÉJÀ COMPLÉTÉS : la troupe passe `EffectBillboardTurnDegrees`
  // et `…TiltDegrees`, et le mouvement les range tels quels. Cet essai attendait
  // qu'il les complète une seconde fois — la figure recevait alors l'angle brut
  // de la caméra. Voir `effect_billboard_facing_test.cpp`.
  AdvanceCameraFacingRadialTracks(tracks, 330, 50);
  // ⚠ ILS SE POSENT MEME QUAND LES QUATRE PISTES DORMENT.
  CHECK(tracks[0].style == 330);
  CHECK(tracks[1].style == 50);
}

TEST_CASE("le semis tire quatre grains dispersés", "[render][rain_particle]") {
  std::uint32_t generator = 1U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectRainParticleTracks(generator);
  for (const EffectTrack& track : tracks) {
    CHECK(track.active);
    CHECK(track.alpha == 0);
    CHECK(track.ticks == 0);
    // ⚠ LES GRAINS NAISSENT LOIN SOUS LEUR NŒUD : le repere du client descend.
    CHECK(track.anchor.y >= kRainParticleDepthFloor);
    CHECK(track.anchor.y <= kRainParticleDepthFloor + 20.0F);
    CHECK(track.lift_key >= kRainParticleLiftFloor);
    CHECK(track.lift_key <= 3.0F);
    CHECK(track.radius_at_anchor >= kRainParticleRadiusFloor);
    CHECK(track.radius_at_anchor <= 10.0F);
    CHECK(track.degrees >= 0);
    CHECK(track.degrees < 360);
    CHECK(track.mark[kMarkOfRainParticleTint] < 3);
    // ⚠ L'ABSCISSE ET LA COTE RESTENT NULLES : c'est le mouvement qui les pose.
    CHECK(track.anchor.x == Approx(0.0F));
    CHECK(track.anchor.z == Approx(0.0F));
  }
  // ⚠ CHAQUE PISTE TIRE POUR ELLE SEULE : quatre grains identiques feraient une
  // colonne, pas une bouffee.
  const bool dispersees = tracks[0].degrees != tracks[1].degrees ||
                          tracks[0].anchor.y != tracks[1].anchor.y ||
                          tracks[0].radius_at_anchor != tracks[1].radius_at_anchor;
  CHECK(dispersees);
}

TEST_CASE("la table des pluies porte les deux effets", "[render][rain_particle]") {
  const std::span<const EffectRainParticlePattern> table = EffectRainParticlePatternTable();
  CHECK(table.size() == 2);
  CHECK(EffectRainParticlePatternsFor(kRainParticle2).size() == 1);
  CHECK(EffectRainParticlePatternsFor(1U).empty());

  std::uint32_t generator = 3U;
  const ro::render::PooledEffectNode node =
      SowEffectRainParticleNode(table.front(), glm::vec3{1.0F, 2.0F, 3.0F}, 60, generator);
  CHECK(node.shape == PooledNodeShape::kRainParticle);
  CHECK(node.node.life_ticks == 60);
  CHECK(node.resource_paths.size() == 1);
  CHECK(node.tracks.front().active);
}
