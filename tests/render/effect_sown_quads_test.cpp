// LES CARRÉS SEMÉS DES SOUS-TYPES 143, 156 ET 172 : le carré qui tourne à plat,
// la note qui flotte et le voile teinté.

#include <algorithm>
#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_bobbing_note.h"
#include "ro/render/effect_colored_veil.h"
#include "ro/render/effect_floor_spinner.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_track_motion.h"

using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::PooledEffectNode;
using ro::render::PooledNodeShape;

namespace {

constexpr std::uint32_t kDust = 827;
constexpr std::uint32_t kTorches = 829;
constexpr std::uint32_t kWater = 838;
constexpr std::uint32_t kWhiteNote = 856;
constexpr std::uint32_t kBlueNote = 860;
constexpr std::uint32_t kReverberation = 1569;
constexpr std::uint32_t kRedVeil = 669;
constexpr std::uint32_t kBlueVeil = 673;

// LES DEUX MÉLANGES DE LA VOIE DES QUADS, en facteurs Direct3D.
constexpr std::int32_t kBlendOne = 2;
constexpr std::int32_t kBlendInverseSourceAlpha = 6;

}  // namespace

TEST_CASE("Carré à plat : 829 pose trois torches aux caps 0, 120 et 240", "[render][table]") {
  const auto torches = ro::render::EffectFloorSpinnerPatternsFor(kTorches);
  REQUIRE(torches.size() == 3);
  CHECK(torches[0].azimuth_degrees == 0);
  CHECK(torches[1].azimuth_degrees == 120);
  CHECK(torches[2].azimuth_degrees == 240);
  CHECK(torches[0].byte == ro::render::kFloorSpinnerByteOfTorch);
  CHECK(torches[0].life_ticks == 29999);
  CHECK(ro::render::KnowsProceduralEffect(kTorches));
  CHECK(ro::render::KnowsProceduralEffect(kDust));
  CHECK(ro::render::KnowsProceduralEffect(kWater));
}

TEST_CASE("Carré à plat : l'octet choisit les images, et leur numérotation", "[render][seed]") {
  const std::vector<std::string> torches = ro::render::EffectFloorSpinnerTextures(1);
  REQUIRE(torches.size() == 13);
  CHECK(torches.front() == "data\\texture\\effect\\torch_red01.bmp");
  CHECK(torches.back() == "data\\texture\\effect\\torch_red13.bmp");
  const std::vector<std::string> eaux = ro::render::EffectFloorSpinnerTextures(2);
  REQUIRE(eaux.size() == 51);
  CHECK(eaux.front() == "data\\texture\\effect\\W_c_00.tga");
  const std::vector<std::string> poussieres = ro::render::EffectFloorSpinnerTextures(0);
  REQUIRE(poussieres.size() == 9);
  CHECK(poussieres.front() == "data\\texture\\effect\\dust01.bmp");
}

TEST_CASE("Carré à plat : la torche tourne, respire et fait défiler ses images",
          "[render][motion]") {
  const auto torches = ro::render::EffectFloorSpinnerPatternsFor(kTorches);
  REQUIRE(!torches.empty());
  std::uint32_t generator = 5;
  PooledEffectNode node =
      ro::render::SowFloorSpinnerNode(torches[1], {0.0F, 0.0F, 0.0F}, 0.0F, 0.0F, generator);
  CHECK(node.shape == PooledNodeShape::kFloorSpinner);
  CHECK(node.node.render_word == ro::render::kFloorSpinnerRenderWord);
  EffectTrack& track = node.tracks.front();
  CHECK(track.alpha == 200);
  CHECK(track.radius_at_anchor == Catch::Approx(18.0F));
  const std::int16_t breath = track.tilt_degrees;
  for (int tick = 0; tick < 4; ++tick) {
    ro::render::AdvanceFloorSpinnerTracks(node.tracks, node.resource_paths.size());
  }
  CHECK(track.azimuth_degrees == 120 + (4 * 6));
  CHECK(track.tilt_degrees == (breath + (4 * 3)) % 360);
  CHECK(ro::render::FloorSpinnerImageOf(track) == 1);

  // UN CARRÉ À PLAT, deux unités au-dessus du nœud, ajouté.
  const std::optional<EffectWorldQuad> quad =
      ro::render::ComposeFloorSpinnerQuad(track, {10.0F, 0.0F, 0.0F});
  REQUIRE(quad.has_value());
  for (const glm::vec3& corner : quad->corners) {
    CHECK(corner.y == Catch::Approx(2.0F));
  }
  CHECK(quad->blend_destination == kBlendOne);
}

TEST_CASE("Carré à plat : l'eau ne tourne pas, lit son objet de carte et se mélange",
          "[render][motion]") {
  const auto eau = ro::render::EffectFloorSpinnerPatternsFor(kWater);
  REQUIRE(eau.size() == 1);
  std::uint32_t generator = 5;
  // SANS OBJET DE CARTE, vingt ; un rayon de moins de cinq compte pour rien.
  CHECK(ro::render::SeedFloorSpinnerTracks(eau[0], 0.0F, 0.0F, generator)[0].radius_at_anchor ==
        Catch::Approx(20.0F));
  CHECK(ro::render::SeedFloorSpinnerTracks(eau[0], 4.9F, 0.0F, generator)[0].radius_at_anchor ==
        Catch::Approx(20.0F));
  CHECK(ro::render::SeedFloorSpinnerTracks(eau[0], 33.0F, 400.0F, generator)[0].azimuth_degrees ==
        45);
  auto tracks = ro::render::SeedFloorSpinnerTracks(eau[0], 33.0F, 90.5F, generator);
  CHECK(tracks[0].radius_at_anchor == Catch::Approx(33.0F));
  CHECK(tracks[0].azimuth_degrees == 90);
  const std::int16_t breath = tracks[0].tilt_degrees;
  ro::render::AdvanceFloorSpinnerTracks(tracks, 51);
  CHECK(tracks[0].azimuth_degrees == 90);
  CHECK(tracks[0].tilt_degrees == breath);
  const std::optional<EffectWorldQuad> quad =
      ro::render::ComposeFloorSpinnerQuad(tracks[0], {0.0F, 0.0F, 0.0F});
  REQUIRE(quad.has_value());
  CHECK(quad->blend_destination == kBlendInverseSourceAlpha);
}

TEST_CASE("Note : 1569 porte l'image que /mineffect lui substitue", "[render][table]") {
  const auto notes = ro::render::EffectBobbingNotePatternsFor(kReverberation);
  REQUIRE(notes.size() == 1);
  CHECK(ro::render::EffectBobbingNoteTexture(notes[0], false) ==
        "data\\texture\\effect\\new_reverberation\\melody_a.bmp");
  CHECK(ro::render::EffectBobbingNoteTexture(notes[0], true) ==
        "data\\texture\\effect\\mineffect\\new_reverberation\\melody_a.bmp");
  CHECK(notes[0].byte == 2);
  CHECK(notes[0].life_ticks == 150);
  CHECK(ro::render::KnowsProceduralEffect(kWhiteNote));
}

TEST_CASE("Note : cinq carrés, un plein et quatre copies à demi-alpha", "[render][figure]") {
  const auto bleue = ro::render::EffectBobbingNotePatternsFor(kBlueNote);
  REQUIRE(bleue.size() == 1);
  std::uint32_t generator = 9;
  PooledEffectNode node =
      ro::render::SowBobbingNoteNode(bleue[0], {0.0F, 0.0F, 0.0F}, false, generator);
  ro::render::AdvanceBobbingNoteTracks(node.tracks, node.ground_position, 0, 0);
  const EffectTrack& track = node.tracks.front();
  // SIX UNITÉS AU-DESSUS DU PORTEUR, à trois près.
  CHECK(track.anchor.y >= 3.0F);
  CHECK(track.anchor.y <= 9.0F);
  const std::vector<EffectWorldQuad> quads =
      ro::render::ComposeBobbingNoteQuads(track, 0, 0, node.node.wander_seed);
  REQUIRE(quads.size() == 5);
  CHECK(quads[0].tint[3] == 120);
  CHECK(quads[0].blend_destination == kBlendInverseSourceAlpha);
  CHECK(quads[1].tint[3] == 60);
  // L'OCTET UN FAIT UN HALO BLEU, toujours ajouté.
  CHECK(quads[1].tint[0] == 5);
  CHECK(quads[1].tint[2] == 255);
  CHECK(quads[1].blend_destination == kBlendOne);
}

TEST_CASE("Voile : 669 rouge monte de dix et retombe, 673 bleu monte de cinq et reste",
          "[render][motion]") {
  const auto red = ro::render::EffectColoredVeilPatternsFor(kRedVeil);
  const auto blue = ro::render::EffectColoredVeilPatternsFor(kBlueVeil);
  REQUIRE(red.size() == 1);
  REQUIRE(blue.size() == 1);
  CHECK(red[0].life_ticks == 70);
  CHECK(red[0].color == std::array<std::uint8_t, 3>{255, 0, 0});
  CHECK(blue[0].color == std::array<std::uint8_t, 3>{0, 0, 255});

  PooledEffectNode voile = ro::render::SowColoredVeilNode(red[0], {0.0F, 0.0F, 0.0F});
  CHECK(voile.node.render_word == 0x201U);
  std::int16_t highest = 0;
  for (int tick = 0; tick < 20; ++tick) {
    ro::render::AdvanceParticleFrameTracks(voile.tracks, 0, 0);
    highest = std::max(highest, voile.tracks[0].alpha);
  }
  CHECK(highest == 50);
  for (int tick = 20; tick < 70; ++tick) {
    ro::render::AdvanceParticleFrameTracks(voile.tracks, 0, 0);
  }
  CHECK(voile.tracks[0].alpha == 0);

  const EffectTrack full = [] {
    EffectTrack track;
    track.active = true;
    track.alpha = 50;
    return track;
  }();
  const std::optional<EffectWorldQuad> quad =
      ro::render::ComposeColoredVeilQuad(full, 0, 0, voile.node.color, {0.0F, 0.0F, 0.0F});
  REQUIRE(quad.has_value());
  CHECK(quad->tint[0] == 255);
  CHECK(quad->tint[1] == 0);
  for (const glm::vec3& corner : quad->corners) {
    CHECK(std::abs(corner.x) == Catch::Approx(200.0F));
  }
}
