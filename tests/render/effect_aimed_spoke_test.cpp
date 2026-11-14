// LES DEUX RAYONS POINTÉS VERS LA CIBLE, sous-type 154 : la table, le semis,
// le mouvement et la figure.

#include "ro/render/effect_aimed_spoke.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/effect_aimed_spoke_seed.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_pooled_node_shape.h"

using Catch::Approx;
using ro::render::AdvanceAimedSpokeTracks;
using ro::render::AimedSpokeClientHeading;
using ro::render::AimedSpokeColorsOf;
using ro::render::ComposeAimedSpokeQuads;
using ro::render::EffectAimedSpokePattern;
using ro::render::EffectAimedSpokePatternsFor;
using ro::render::EffectAimedSpokePatternTable;
using ro::render::EffectParticleBlend;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kAimedSpokeMarkOfTint;
using ro::render::kAimedSpokeSlotOfLength;
using ro::render::kEffectOwnerTintAbsent;
using ro::render::kEffectTracksPerNode;
using ro::render::PooledEffectNode;
using ro::render::PooledNodeShape;
using ro::render::SeedAimedSpokeTracks;
using ro::render::SowAimedSpokeNode;

namespace {

using Tracks = std::array<EffectTrack, kEffectTracksPerNode>;

// L'EFFET QUI NE POSAIT RIEN, et ses trois voisins de la même famille.
constexpr std::uint32_t kLoneSpokesEffectId = 863;
constexpr std::uint32_t kFirstMelodyEffectId = 854;
constexpr std::uint32_t kSecondMelodyEffectId = 858;
constexpr std::uint32_t kMetallicSoundEffectId = 1584;

constexpr std::uint8_t kBlueOctet = 0;
constexpr std::uint8_t kRedOctet = 1;
constexpr std::uint8_t kWhiteOctet = 2;

// Le nombre de tics où la piste est encore vive : quinze de montée jusqu'à
// cent quatre-vingt-quinze, cinq de palier, soixante-cinq de descente jusqu'à
// zéro — la piste meurt au tic suivant.
constexpr std::int32_t kLastLivingTick = 85;

void Advance(Tracks& tracks, std::int32_t ticks) {
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceAimedSpokeTracks(tracks);
  }
}

}  // namespace

TEST_CASE("Rayons pointés : cinq sites, et 863 à l'octet deux sur line3",
          "[render][effectnode][aimedspoke]") {
  CHECK(EffectAimedSpokePatternTable().size() == 5);
  const auto seuls = EffectAimedSpokePatternsFor(kLoneSpokesEffectId);
  REQUIRE(seuls.size() == 1);
  CHECK(seuls.front().client_function == "Effect_InitDirectedSprite");
  CHECK(seuls.front().texture == "data\\texture\\effect\\line3.tga");
  CHECK(seuls.front().octet == kWhiteOctet);
  CHECK(ro::render::KnowsProceduralEffect(kLoneSpokesEffectId));
  REQUIRE(EffectAimedSpokePatternsFor(kFirstMelodyEffectId).size() == 1);
  CHECK(EffectAimedSpokePatternsFor(kFirstMelodyEffectId).front().octet == kBlueOctet);
  REQUIRE(EffectAimedSpokePatternsFor(kSecondMelodyEffectId).size() == 1);
  CHECK(EffectAimedSpokePatternsFor(kSecondMelodyEffectId).front().octet == kRedOctet);
  // ⚠ DEUX LIGNES POUR 1584 : une par branche du réglage des effets allégés.
  CHECK(EffectAimedSpokePatternsFor(kMetallicSoundEffectId).size() == 2);
  CHECK(EffectAimedSpokePatternsFor(862).empty());
}

TEST_CASE("Rayons pointés : une seule piste allumée, les mesures écrites en dur",
          "[render][effectnode][aimedspoke]") {
  const Tracks tracks = SeedAimedSpokeTracks(kRedOctet, 123);
  CHECK(tracks[0].active);
  CHECK_FALSE(tracks[1].active);
  CHECK_FALSE(tracks[2].active);
  CHECK_FALSE(tracks[3].active);
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[0].ticks == 0);
  CHECK(tracks[0].value[kAimedSpokeSlotOfLength] == 50.0F);
  CHECK(tracks[0].lift_key == 5.0F);
  CHECK(tracks[0].tilt_degrees == 123);
  CHECK(tracks[0].mark[kAimedSpokeMarkOfTint] == kRedOctet);
}

TEST_CASE("Rayons pointés : le cap du client vers la cible, profondeur retournée",
          "[render][effectnode][aimedspoke]") {
  const glm::vec3 node{10.0F, 0.0F, 10.0F};
  // À NOTRE EST : le x est partagé, cap zéro.
  CHECK(AimedSpokeClientHeading(node, glm::vec3{60.0F, 0.0F, 10.0F}) == 0);
  // VERS NOTRE Z NÉGATIF : c'est le z positif du client, un quart de tour.
  CHECK(AimedSpokeClientHeading(node, glm::vec3{10.0F, 0.0F, -40.0F}) == 90);
  // SANS CIBLE : zéro.
  CHECK(AimedSpokeClientHeading(node, std::nullopt) == 0);
}

TEST_CASE("Rayons pointés : le nœud porte le mot cinq, l'image et la vie de l'effet",
          "[render][effectnode][aimedspoke]") {
  const EffectAimedSpokePattern& pattern = EffectAimedSpokePatternsFor(kLoneSpokesEffectId).front();
  const PooledEffectNode node =
      SowAimedSpokeNode(pattern, glm::vec3{1.0F, 2.0F, 3.0F}, glm::vec3{1.0F, 2.0F, -97.0F}, 300);
  CHECK(node.shape == PooledNodeShape::kAimedSpokes);
  REQUIRE(node.node.render_word.has_value());
  CHECK(*node.node.render_word == 5U);
  CHECK(node.node.life_ticks == 300);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == "data\\texture\\effect\\line3.tga");
  CHECK(node.ground_position == glm::vec3{1.0F, 2.0F, 3.0F});
  CHECK(node.tracks[0].tilt_degrees == 90);
  CHECK(node.tracks[0].mark[kAimedSpokeMarkOfTint] == kWhiteOctet);
}

TEST_CASE("Rayons pointés : treize par tic jusqu'au quinzième, palier, puis moins trois",
          "[render][effectnode][aimedspoke]") {
  Tracks tracks = SeedAimedSpokeTracks(kBlueOctet, 0);
  Advance(tracks, 1);
  CHECK(tracks[0].alpha == 13);
  Advance(tracks, 14);
  CHECK(tracks[0].alpha == 195);
  Advance(tracks, 5);
  CHECK(tracks[0].alpha == 195);
  Advance(tracks, 1);
  CHECK(tracks[0].alpha == 192);
  // ⚠ LES PISTES ÉTEINTES NE BOUGENT PAS.
  CHECK(tracks[1].ticks == 0);
  CHECK(tracks[1].alpha == 0);
}

TEST_CASE("Rayons pointés : la piste meurt quand l'alpha passerait sous zéro",
          "[render][effectnode][aimedspoke]") {
  Tracks tracks = SeedAimedSpokeTracks(kBlueOctet, 0);
  Advance(tracks, kLastLivingTick);
  CHECK(tracks[0].active);
  CHECK(tracks[0].alpha == 0);
  Advance(tracks, 1);
  CHECK_FALSE(tracks[0].active);
  CHECK(tracks[0].alpha == 0);
}

TEST_CASE("Rayons pointés : l'octet choisit les teintes des deux passages",
          "[render][effectnode][aimedspoke]") {
  using Tint = std::array<std::uint8_t, 3>;
  CHECK(AimedSpokeColorsOf(0).dim_pass == Tint{255, 255, 255});
  CHECK(AimedSpokeColorsOf(0).full_pass == Tint{25, 25, 255});
  CHECK(AimedSpokeColorsOf(1).dim_pass == Tint{255, 89, 182});
  CHECK(AimedSpokeColorsOf(1).full_pass == Tint{255, 25, 25});
  CHECK(AimedSpokeColorsOf(2).dim_pass == Tint{255, 255, 255});
  CHECK(AimedSpokeColorsOf(2).full_pass == Tint{255, 255, 255});
  // ⚠ TOUT AUTRE OCTET PREND LA BRANCHE DU ZÉRO.
  CHECK(AimedSpokeColorsOf(7).full_pass == Tint{25, 25, 255});
}

TEST_CASE("Rayons pointés : rien tant que l'alpha n'est pas positif",
          "[render][effectnode][aimedspoke]") {
  const Tracks tracks = SeedAimedSpokeTracks(kWhiteOctet, 0);
  std::vector<EffectWorldQuad> quads;
  ComposeAimedSpokeQuads(tracks[0], glm::vec3{0.0F}, quads);
  ComposeAimedSpokeQuads(tracks[1], glm::vec3{0.0F}, quads);
  CHECK(quads.empty());
}

TEST_CASE("Rayons pointés : deux triangles à vingt-cinq degrés du cap, deux passages chacun",
          "[render][effectnode][aimedspoke]") {
  Tracks tracks = SeedAimedSpokeTracks(kBlueOctet, 0);
  tracks[0].alpha = 101;
  const glm::vec3 origin{100.0F, 10.0F, 200.0F};
  std::vector<EffectWorldQuad> quads;
  ComposeAimedSpokeQuads(tracks[0], origin, quads);
  REQUIRE(quads.size() == 4);

  // LES PASSAGES : pâle au quart ENTIER de l'alpha en mélange ordinaire, puis
  // plein en addition.
  const auto ordinary = EffectParticleBlend(1, kEffectOwnerTintAbsent);
  const auto addition = EffectParticleBlend(0, kEffectOwnerTintAbsent);
  for (std::size_t radius = 0; radius < 2; ++radius) {
    const EffectWorldQuad& pale = quads[radius * 2];
    const EffectWorldQuad& full = quads[(radius * 2) + 1];
    CHECK(pale.tint[3] == 25);
    CHECK(full.tint[3] == 101);
    CHECK(pale.blend_destination == ordinary.destination);
    CHECK(full.blend_destination == addition.destination);
    CHECK(full.tint[2] == 255);
    CHECK(full.tint[0] == 25);
    // LE CENTRE, deux fois, six unités au-dessus du nœud.
    CHECK(pale.corners[0] == origin + glm::vec3{0.0F, 6.0F, 0.0F});
    CHECK(pale.corners[3] == pale.corners[0]);
  }

  // LA POINTE DU PREMIER RAYON : au cap zéro moins vingt-cinq degrés dans le
  // repère du client, dont le z est opposé au nôtre.
  const float tip_x = 50.0F * std::cos(glm::radians(335.0F));
  const float tip_client_z = 50.0F * std::sin(glm::radians(335.0F));
  CHECK(quads[0].corners[2].x == Approx(origin.x + tip_x).margin(1e-3));
  CHECK(quads[0].corners[2].y == Approx(origin.y + 6.0F));
  CHECK(quads[0].corners[2].z == Approx(origin.z - tip_client_z).margin(1e-3));
  // ⚠ LA LARGEUR SE DÉPORTE D'UN QUART DE TOUR EN ARRIÈRE : 335 − 90 = 245.
  const float width_x = 5.0F * std::cos(glm::radians(245.0F));
  const float width_client_z = 5.0F * std::sin(glm::radians(245.0F));
  CHECK(quads[0].corners[1].x == Approx(origin.x + tip_x + width_x).margin(1e-3));
  CHECK(quads[0].corners[1].z == Approx(origin.z - tip_client_z - width_client_z).margin(1e-3));
  // LE SECOND RAYON, à plus vingt-cinq : symétrique du premier par rapport au
  // cap, sa pointe de l'autre côté.
  CHECK(quads[2].corners[2].x == Approx(quads[0].corners[2].x).margin(1e-3));
  CHECK(quads[2].corners[2].z - origin.z ==
        Approx(-(quads[0].corners[2].z - origin.z)).margin(1e-3));
}
