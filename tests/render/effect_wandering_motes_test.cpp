#include "ro/render/effect_wandering_motes.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_four_particle_slots.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_particle_figures.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_wandering_motes_seed.h"

using Catch::Approx;
using ro::render::AdvanceWanderingMoteTracks;
using ro::render::ComposeWanderingMoteQuads;
using ro::render::EffectFourParticleOffsetOf;
using ro::render::EffectTrack;
using ro::render::EffectWanderingMoteAlpha;
using ro::render::EffectWanderingMoteColor;
using ro::render::EffectWanderingMotePattern;
using ro::render::EffectWanderingMotePatternsFor;
using ro::render::EffectWanderingMotePatternTable;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kWanderingMoteGlowCap;
using ro::render::kWanderingMoteGreen;
using ro::render::kWanderingMoteLoneStyle;
using ro::render::kWanderingMoteMarkOfColor;
using ro::render::kWanderingMoteMarkOfFirstGlow;
using ro::render::kWanderingMoteMarkOfStyle;
using ro::render::kWanderingMoteParticles;
using ro::render::kWanderingMotePink;
using ro::render::kWanderingMotePinkColor;
using ro::render::kWanderingMoteSpawnFloor;
using ro::render::kWanderingMoteSpawnSpread;
using ro::render::kWanderingMoteSquareRadius;
using ro::render::kWanderingMoteWhite;
using ro::render::kWanderingMoteWhiteColor;
using ro::render::kWanderingMoteYellow;
using ro::render::kWanderingMoteYellowColor;
using ro::render::SeedEffectWanderingMoteTracks;
using ro::render::SetEffectFourParticleOffset;
using ro::render::SowEffectWanderingMoteNode;

namespace {

constexpr std::array<std::uint32_t, 3> kMoteEffectIds = {713, 714, 946};

[[nodiscard]] const EffectWanderingMotePattern& PatternOf(std::uint32_t effect_id) {
  return EffectWanderingMotePatternsFor(effect_id).front();
}

}  // namespace

TEST_CASE("l'essaim rend quatre carres par piste", "[render][wandering_mote]") {
  std::uint32_t seed = 5U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectWanderingMoteTracks(PatternOf(713), seed);
  // ⚠ AUCUNE GARDE DE HAUTEUR : les quatre se peignent quoi qu'il arrive, même
  // à éclat nul — c'est l'alpha qui les efface, pas une absence de quad.
  tracks[0].mark[kWanderingMoteMarkOfFirstGlow] = 60;
  const std::vector<EffectWorldQuad> carres =
      ComposeWanderingMoteQuads(tracks[0], 0, 0, {0.0F, 0.0F, 0.0F});
  CHECK(carres.size() == kWanderingMoteParticles);
  CHECK(carres.front().tint[3] == 120);
}

TEST_CASE("une piste eteinte ne rend rien", "[render][wandering_mote]") {
  EffectTrack faded;
  CHECK(ComposeWanderingMoteQuads(faded, 0, 0, {0.0F, 0.0F, 0.0F}).empty());
}

TEST_CASE("un eclat NEGATIF ne peint rien", "[render][wandering_mote]") {
  // ⚠ L'OCTET EST SIGNÉ : le client teste le bit de signe de l'octet ÉTENDU.
  // Le lire non signé peindrait à plein ce qui doit être invisible.
  CHECK(EffectWanderingMoteAlpha(static_cast<std::int8_t>(-3)) == 0);
  CHECK(EffectWanderingMoteAlpha(0) == 0);
  CHECK(EffectWanderingMoteAlpha(50) == 100);
  CHECK(EffectWanderingMoteAlpha(static_cast<std::int8_t>(kWanderingMoteGlowCap)) == 240);
}

TEST_CASE("chaque teinte a son octet, et le defaut existe", "[render][wandering_mote]") {
  CHECK(EffectWanderingMoteColor(kWanderingMotePinkColor) == kWanderingMotePink);
  CHECK(EffectWanderingMoteColor(kWanderingMoteYellowColor) == kWanderingMoteYellow);
  CHECK(EffectWanderingMoteColor(kWanderingMoteWhiteColor) == kWanderingMoteWhite);
  // ⚠ LE VERT N'A AUCUN SITE, et il est transcrit quand même : c'est le cas par
  // défaut de l'aiguillage du client.
  CHECK(EffectWanderingMoteColor(0) == kWanderingMoteGreen);
}

TEST_CASE("l'eclat monte d'un par tic jusqu'a son plafond", "[render][wandering_mote]") {
  std::uint32_t seed = 5U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectWanderingMoteTracks(PatternOf(713), seed);
  CHECK(tracks[0].mark[kWanderingMoteMarkOfFirstGlow] == 0);
  std::uint32_t rng = 11U;
  AdvanceWanderingMoteTracks(tracks, 0, 0, rng);
  CHECK(tracks[0].mark[kWanderingMoteMarkOfFirstGlow] >= 1);
  CHECK(tracks[0].ticks == 1);
}

TEST_CASE("le compteur avance MEME sur une piste eteinte", "[render][wandering_mote]") {
  std::uint32_t seed = 5U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectWanderingMoteTracks(PatternOf(714), seed);
  REQUIRE_FALSE(tracks[1].active);
  std::uint32_t rng = 11U;
  AdvanceWanderingMoteTracks(tracks, 0, 0, rng);
  // ⚠ LE CLIENT INCRÉMENTE AVANT DE TESTER. C'est sans effet aujourd'hui, et le
  // déplacer serait faux.
  CHECK(tracks[1].ticks == 1);
}

TEST_CASE("une luciole echappee se replace au hasard", "[render][wandering_mote]") {
  std::uint32_t seed = 5U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectWanderingMoteTracks(PatternOf(713), seed);
  // Loin, et l'éclat presque éteint : le prochain tic la ramène.
  SetEffectFourParticleOffset(tracks[0], 0, {100.0F, 0.0F, 0.0F});
  tracks[0].mark[kWanderingMoteMarkOfFirstGlow] = 1;
  std::uint32_t rng = 11U;
  AdvanceWanderingMoteTracks(tracks, 0, 0, rng);
  const glm::vec3 place = EffectFourParticleOffsetOf(tracks[0], 0);
  CHECK(place.x >= static_cast<float>(kWanderingMoteSpawnFloor));
  CHECK(place.x <= static_cast<float>(kWanderingMoteSpawnFloor + kWanderingMoteSpawnSpread));
}

TEST_CASE("une luciole echappee mais brillante ne fait que palir", "[render][wandering_mote]") {
  std::uint32_t seed = 5U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectWanderingMoteTracks(PatternOf(713), seed);
  SetEffectFourParticleOffset(tracks[0], 0, {100.0F, 0.0F, 0.0F});
  tracks[0].mark[kWanderingMoteMarkOfFirstGlow] = 60;
  std::uint32_t rng = 11U;
  AdvanceWanderingMoteTracks(tracks, 0, 0, rng);
  // Plus un de montée, moins deux de fuite.
  CHECK(tracks[0].mark[kWanderingMoteMarkOfFirstGlow] == 59);
  CHECK(EffectFourParticleOffsetOf(tracks[0], 0).x > 90.0F);
}

TEST_CASE("la luciole solitaire MONTE et repart de son noeud", "[render][wandering_mote]") {
  std::uint32_t seed = 5U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectWanderingMoteTracks(PatternOf(714), seed);
  REQUIRE(tracks[0].mark[kWanderingMoteMarkOfStyle] == kWanderingMoteLoneStyle);
  tracks[0].value[0] = 90.0F;
  std::uint32_t rng = 11U;
  AdvanceWanderingMoteTracks(tracks, 0, 0, rng);
  const glm::vec3 place = EffectFourParticleOffsetOf(tracks[0], 0);
  // ⚠ ELLE BOUGE EN ORDONNÉE, pas en abscisse ni en profondeur.
  CHECK(place.x == Approx(0.0F));
  CHECK(place.z == Approx(0.0F));
  CHECK(place.y != Approx(0.0F));
  // ⚠ ET C'EST SON COMPTEUR QUI LA RAMÈNE, pas sa distance.
  tracks[0].ticks = 4000;
  tracks[0].mark[kWanderingMoteMarkOfFirstGlow] = 1;
  AdvanceWanderingMoteTracks(tracks, 0, 0, rng);
  CHECK(tracks[0].ticks == 0);
  CHECK(EffectFourParticleOffsetOf(tracks[0], 0).y == Approx(0.0F));
}

TEST_CASE("les deux angles de camera se rangent dans les styles", "[render][wandering_mote]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  std::uint32_t rng = 9U;
  // 🔴 TELS QU'ILS ARRIVENT : ils sortent déjà de `EffectBillboard*Degrees`, qui
  // fait le calcul du client. Le refaire tournait la figure à l'envers de la
  // caméra. Voir `StoreEffectBillboardAngles`.
  AdvanceWanderingMoteTracks(tracks, 30, 100, rng);
  CHECK(tracks[0].style == 30);
  CHECK(tracks[1].style == 100);
}

TEST_CASE("une luciole fait face a la camera, quel que soit son lacet",
          "[render][wandering_mote]") {
  // 🔴 L'INVARIANT QUE LES ANGLES SERVENT : un carré à plat, redressé par les
  // deux angles que le mouvement a rangés, doit se présenter de face. Refaire
  // le calcul du client sur des angles qui en sortent déjà le laissait juste à
  // lacet nul, et le couchait par la tranche dès qu'on tournait la caméra.
  for (const float yaw : {0.0F, 60.0F, 135.0F, 250.0F}) {
    ro::render::Camera camera;
    camera.pitch_degrees = ro::render::kGameElevationDegrees;
    camera.yaw_degrees = yaw;
    std::array<EffectTrack, kEffectTracksPerNode> tracks;
    std::uint32_t rng = 9U;
    AdvanceWanderingMoteTracks(tracks, ro::render::EffectBillboardTurnDegrees(camera),
                               ro::render::EffectBillboardTiltDegrees(camera), rng);
    std::array<glm::vec3, 4> corners = {glm::vec3{1.0F, 0.0F, 0.0F}, glm::vec3{0.0F, 0.0F, 1.0F},
                                        glm::vec3{-1.0F, 0.0F, 0.0F}, glm::vec3{0.0F, 0.0F, -1.0F}};
    ro::render::RotateEffectQuadCorners(corners, tracks[0].style, tracks[1].style);
    // Le carré est tourné par les angles de la caméra du client : il revient au
    // nôtre par `S` entier.
    for (glm::vec3& corner : corners) {
      corner = ro::render::EffectPointFromClient(corner);
    }
    const glm::vec3 normal =
        glm::normalize(glm::cross(corners[1] - corners[0], corners[3] - corners[0]));
    const glm::vec3 eye = glm::normalize(camera.Position() - camera.look_at);
    CHECK(std::abs(glm::dot(normal, eye)) == Approx(1.0F).margin(0.01F));
  }
}

TEST_CASE("le semis place seize lucioles et tire pour les eteintes", "[render][wandering_mote]") {
  std::uint32_t seed = 13U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectWanderingMoteTracks(PatternOf(946), seed);
  std::uint32_t witness = 13U;
  for (std::size_t draw = 0; draw < ro::render::kWanderingMoteDrawsPerMote *
                                        kWanderingMoteParticles * kEffectTracksPerNode;
       ++draw) {
    static_cast<void>(ro::render::NextEffectNodeDraw(witness));
  }
  CHECK(seed == witness);
  for (const EffectTrack& track : tracks) {
    CHECK(track.active);
    CHECK(track.radius_at_anchor == Approx(kWanderingMoteSquareRadius));
    CHECK(track.mark[kWanderingMoteMarkOfColor] == kWanderingMoteWhiteColor);
    for (std::size_t firefly = 0; firefly < kWanderingMoteParticles; ++firefly) {
      const glm::vec3 place = EffectFourParticleOffsetOf(track, firefly);
      CHECK(place.x >= static_cast<float>(kWanderingMoteSpawnFloor));
      CHECK(place.x <= static_cast<float>(kWanderingMoteSpawnFloor + kWanderingMoteSpawnSpread));
      // ⚠ LA HAUTEUR EST TIRÉE PUIS JETÉE : les quatre naissent à zéro.
      CHECK(place.y == Approx(0.0F));
      CHECK(track.mark[kWanderingMoteMarkOfFirstGlow + firefly] == 0);
    }
  }
}

TEST_CASE("l'octet UN n'allume qu'une piste et tire moins", "[render][wandering_mote]") {
  std::uint32_t seed = 13U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectWanderingMoteTracks(PatternOf(714), seed);
  CHECK(tracks[0].active);
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    CHECK_FALSE(tracks[rang].active);
  }
  std::uint32_t witness = 13U;
  for (std::size_t draw = 0; draw < ro::render::kWanderingMoteLoneDrawsPerMote *
                                        kWanderingMoteParticles * kEffectTracksPerNode;
       ++draw) {
    static_cast<void>(ro::render::NextEffectNodeDraw(witness));
  }
  // ⚠ LES TROIS PISTES ÉTEINTES TIRENT QUAND MÊME : le bâtisseur les éteint
  // puis continue. Les sauter décalerait toute la suite.
  CHECK(seed == witness);
  CHECK(EffectFourParticleOffsetOf(tracks[0], 0).x == Approx(0.0F));
}

TEST_CASE("la table porte les trois identifiants du sous-type 130", "[render][wandering_mote]") {
  const std::span<const EffectWanderingMotePattern> table = EffectWanderingMotePatternTable();
  REQUIRE(table.size() == kMoteEffectIds.size());
  for (std::size_t rang = 0; rang < kMoteEffectIds.size(); ++rang) {
    CHECK(table[rang].effect_id == kMoteEffectIds[rang]);
    CHECK(table[rang].style_byte == static_cast<std::uint8_t>(rang));
    CHECK_FALSE(table[rang].texture.empty());
  }
  CHECK(EffectWanderingMotePatternsFor(1U).empty());
}

TEST_CASE("le noeud seme porte sa forme et son image", "[render][wandering_mote]") {
  std::uint32_t seed = 3U;
  const ro::render::PooledEffectNode node =
      SowEffectWanderingMoteNode(PatternOf(946), {2.0F, 0.0F, 7.0F}, 600, seed);
  CHECK(node.shape == ro::render::PooledNodeShape::kWanderingMote);
  CHECK(node.node.life_ticks == 600);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == std::string(PatternOf(946).texture));
}
