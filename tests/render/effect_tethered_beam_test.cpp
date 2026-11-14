#include "ro/render/effect_tethered_beam.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_tethered_beam_seed.h"
#include "ro/render/effect_track.h"

#include "support/effect_bearers.h"

using Catch::Approx;
using ro::render::AdvanceTetheredBeamTracks;
using ro::render::ComposeTetheredBeamQuads;
using ro::render::EffectTetheredBeamAlpha;
using ro::render::EffectTetheredBeamBlendMode;
using ro::render::EffectTetheredBeamColor;
using ro::render::EffectTetheredBeamFarEnd;
using ro::render::EffectTetheredBeamPattern;
using ro::render::EffectTetheredBeamPatternsFor;
using ro::render::EffectTetheredBeamPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kTetheredBeamAddedBlend;
using ro::render::kTetheredBeamGreyVariant;
using ro::render::kTetheredBeamGrowthTicks;
using ro::render::kTetheredBeamLift;
using ro::render::kTetheredBeamOrdinaryBlend;
using ro::render::kTetheredBeamPasses;
using ro::render::kTetheredBeamPinkVariant;
using ro::render::kTetheredBeamSides;
using ro::render::kTetheredBeamSlotOfVariant;
using ro::render::kTetheredBeamTailOfFarEnd;
using ro::render::kTetheredBeamTailOfNearEnd;
using ro::render::kTetheredBeamTealVariant;
using ro::render::kTetheredBeamYellowVariant;
using ro::render::SeedEffectTetheredBeamTracks;
using ro::render::SowEffectTetheredBeamNode;

namespace {

// LES CINQ IDENTIFIANTS du sous-type 38.
constexpr std::array<std::uint32_t, 5> kBeamEffectIds = {232, 384, 395, 819, 1210};

[[nodiscard]] const EffectTetheredBeamPattern& PatternOf(std::uint32_t effect_id) {
  const std::span<const EffectTetheredBeamPattern> patterns =
      EffectTetheredBeamPatternsFor(effect_id);
  return patterns.front();
}

[[nodiscard]] EffectTrack StretchedTrack(float variant = kTetheredBeamYellowVariant) {
  EffectTrack track;
  track.active = true;
  track.alpha = 60;
  track.lift_key = 0.4F;
  track.ticks = kTetheredBeamGrowthTicks;
  track.value[kTetheredBeamSlotOfVariant] = variant;
  track.tail[kTetheredBeamTailOfNearEnd] = 0.0F;
  track.tail[kTetheredBeamTailOfNearEnd + 1] = 8.0F;
  track.tail[kTetheredBeamTailOfNearEnd + 2] = 0.0F;
  track.tail[kTetheredBeamTailOfFarEnd] = 30.0F;
  track.tail[kTetheredBeamTailOfFarEnd + 1] = 8.0F;
  track.tail[kTetheredBeamTailOfFarEnd + 2] = 0.0F;
  return track;
}

}  // namespace

TEST_CASE("le tube rend dix faces, deux fois", "[render][tethered_beam]") {
  const std::vector<EffectWorldQuad> faces = ComposeTetheredBeamQuads(StretchedTrack());
  CHECK(faces.size() == kTetheredBeamSides * kTetheredBeamPasses);
}

TEST_CASE("la gaine est au double du rayon de l'ame", "[render][tethered_beam]") {
  const EffectTrack track = StretchedTrack();
  const std::vector<EffectWorldQuad> faces = ComposeTetheredBeamQuads(track);
  REQUIRE(faces.size() == kTetheredBeamSides * kTetheredBeamPasses);
  const glm::vec3 axe{track.tail[kTetheredBeamTailOfNearEnd],
                      track.tail[kTetheredBeamTailOfNearEnd + 1],
                      track.tail[kTetheredBeamTailOfNearEnd + 2]};
  const auto offset = [&axe](const glm::vec3& corner) {
    return std::sqrt((corner.y - axe.y) * (corner.y - axe.y) +
                     (corner.z - axe.z) * (corner.z - axe.z));
  };
  // L'AXE VA EN X : les coins s'écartent en y et en z du rayon exact.
  const float soul = offset(faces.front().corners[0]);
  const float gaine = offset(faces[kTetheredBeamSides].corners[0]);
  CHECK(soul == Approx(track.lift_key).margin(0.001F));
  CHECK(gaine == Approx(track.lift_key + track.lift_key).margin(0.001F));
}

TEST_CASE("une piste eteinte ou muette ne rend rien", "[render][tethered_beam]") {
  EffectTrack faded = StretchedTrack();
  faded.active = false;
  CHECK(ComposeTetheredBeamQuads(faded).empty());
  EffectTrack silent = StretchedTrack();
  silent.alpha = 0;
  CHECK(ComposeTetheredBeamQuads(silent).empty());
}

TEST_CASE("le trait pousse en quinze tics, et pas lineairement", "[render][tethered_beam]") {
  EffectTrack track = StretchedTrack();
  track.ticks = 0;
  // AU PREMIER TIC IL N'A PAS DE LONGUEUR.
  CHECK(EffectTetheredBeamFarEnd(track).x == Approx(0.0F).margin(0.001F));
  track.ticks = kTetheredBeamGrowthTicks;
  CHECK(EffectTetheredBeamFarEnd(track).x == Approx(30.0F));
  track.ticks = 200;
  CHECK(EffectTetheredBeamFarEnd(track).x == Approx(30.0F));
  // ⚠ AU MILIEU DE SA POUSSE IL A DÉJÀ PLUS DE LA MOITIÉ DU CHEMIN : le sinus
  // part vite et arrive doucement. Une interpolation droite le mettrait à
  // quinze.
  track.ticks = kTetheredBeamGrowthTicks / 2;
  const float middle = EffectTetheredBeamFarEnd(track).x;
  CHECK(middle > 15.0F);
  CHECK(middle < 30.0F);
}

TEST_CASE("chaque variante a ses deux teintes et son melange", "[render][tethered_beam]") {
  CHECK(EffectTetheredBeamColor(kTetheredBeamYellowVariant, 0)[0] == 255);
  CHECK(EffectTetheredBeamColor(kTetheredBeamYellowVariant, 0)[2] == 0);
  CHECK(EffectTetheredBeamColor(kTetheredBeamYellowVariant, 1)[2] == 255);
  CHECK(EffectTetheredBeamColor(kTetheredBeamTealVariant, 0)[1] == 100);
  CHECK(EffectTetheredBeamColor(kTetheredBeamTealVariant, 1)[0] == 255);
  // LE ROSE EST LE MÊME AUX DEUX PASSES.
  CHECK(EffectTetheredBeamColor(kTetheredBeamPinkVariant, 0) ==
        EffectTetheredBeamColor(kTetheredBeamPinkVariant, 1));
  CHECK(EffectTetheredBeamBlendMode(kTetheredBeamYellowVariant) == kTetheredBeamAddedBlend);
  CHECK(EffectTetheredBeamBlendMode(kTetheredBeamGreyVariant) == kTetheredBeamOrdinaryBlend);
  // ⚠ ET L'AIGUILLAGE DU CLIENT A BIEN UN CAS PAR DÉFAUT, au contraire de la
  // bourrasque : une variante inconnue retombe sur le jaune.
  CHECK(EffectTetheredBeamColor(7.0F, 0) == EffectTetheredBeamColor(kTetheredBeamYellowVariant, 0));
}

TEST_CASE("la gaine se peint a l'alpha double, la variante grise a la moitie",
          "[render][tethered_beam]") {
  CHECK(EffectTetheredBeamAlpha(60, kTetheredBeamYellowVariant, 0) == 60);
  CHECK(EffectTetheredBeamAlpha(60, kTetheredBeamYellowVariant, 1) == 120);
  CHECK(EffectTetheredBeamAlpha(60, kTetheredBeamGreyVariant, 0) == 30);
  CHECK(EffectTetheredBeamAlpha(60, kTetheredBeamGreyVariant, 1) == 30);
  // ⚠ TRONQUÉ, PAS PLAFONNÉ : au-delà de cent vingt-sept l'alpha doublé
  // reboucle. Aucun des cinq semis n'y arrive, mais la règle est celle-là.
  CHECK(EffectTetheredBeamAlpha(200, kTetheredBeamYellowVariant, 1) == 144);
}

TEST_CASE("l'image est retournee", "[render][tethered_beam]") {
  const std::vector<EffectWorldQuad> faces = ComposeTetheredBeamQuads(StretchedTrack());
  REQUIRE_FALSE(faces.empty());
  CHECK(faces.front().texture_uv[0].y == Approx(1.0F));
  CHECK(faces.front().texture_uv[2].y == Approx(0.0F));
}

TEST_CASE("les deux bouts suivent leurs acteurs, eleves de huit", "[render][tethered_beam]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectTetheredBeamTracks(
      PatternOf(232), {0.0F, 0.0F, 0.0F}, glm::vec3{10.0F, 1.0F, 0.0F});
  AdvanceTetheredBeamTracks(tracks, {3.0F, 2.0F, 4.0F}, glm::vec3{20.0F, 5.0F, 6.0F});
  CHECK(tracks[0].tail[kTetheredBeamTailOfNearEnd] == Approx(3.0F));
  CHECK(tracks[0].tail[kTetheredBeamTailOfNearEnd + 1] == Approx(2.0F + kTetheredBeamLift));
  CHECK(tracks[0].tail[kTetheredBeamTailOfFarEnd] == Approx(20.0F));
  CHECK(tracks[0].tail[kTetheredBeamTailOfFarEnd + 1] == Approx(5.0F + kTetheredBeamLift));
  CHECK(tracks[0].ticks == 1);
}

TEST_CASE("pas de cible, pas de trait", "[render][tethered_beam]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectTetheredBeamTracks(
      PatternOf(232), {0.0F, 0.0F, 0.0F}, glm::vec3{10.0F, 0.0F, 0.0F});
  REQUIRE(tracks[0].active);
  AdvanceTetheredBeamTracks(tracks, {0.0F, 0.0F, 0.0F}, std::nullopt);
  CHECK_FALSE(tracks[0].active);
  CHECK(tracks[0].alpha == 0);
  CHECK(ComposeTetheredBeamQuads(tracks[0]).empty());
}

TEST_CASE("l'alpha du centre alpha monte, tient, puis tombe", "[render][tethered_beam]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectTetheredBeamTracks(
      PatternOf(384), {0.0F, 0.0F, 0.0F}, glm::vec3{10.0F, 0.0F, 0.0F});
  const glm::vec3 target{10.0F, 0.0F, 0.0F};
  REQUIRE(tracks[0].alpha == 0);
  for (std::int32_t tick = 0; tick < 19; ++tick) {
    AdvanceTetheredBeamTracks(tracks, {0.0F, 0.0F, 0.0F}, target);
  }
  REQUIRE(tracks[0].ticks == 19);
  CHECK(tracks[0].alpha == 114);
  // ⚠ LE PLATEAU : entre le vingtième et le quarantième tic, rien ne bouge.
  for (std::int32_t tick = 0; tick < 21; ++tick) {
    AdvanceTetheredBeamTracks(tracks, {0.0F, 0.0F, 0.0F}, target);
  }
  REQUIRE(tracks[0].ticks == 40);
  CHECK(tracks[0].alpha == 114);
  AdvanceTetheredBeamTracks(tracks, {0.0F, 0.0F, 0.0F}, target);
  CHECK(tracks[0].alpha == 111);
}

TEST_CASE("le rayon du rose RESPIRE, et lui seul", "[render][tethered_beam]") {
  const glm::vec3 target{10.0F, 0.0F, 0.0F};
  std::array<EffectTrack, kEffectTracksPerNode> rose =
      SeedEffectTetheredBeamTracks(PatternOf(395), {0.0F, 0.0F, 0.0F}, target);
  const float start = rose[0].lift_key;
  for (std::int32_t tick = 0; tick < 10; ++tick) {
    AdvanceTetheredBeamTracks(rose, {0.0F, 0.0F, 0.0F}, target);
  }
  CHECK(rose[0].lift_key > start);
  CHECK(rose[0].alpha == 10);
  std::array<EffectTrack, kEffectTracksPerNode> jaune =
      SeedEffectTetheredBeamTracks(PatternOf(232), {0.0F, 0.0F, 0.0F}, target);
  const float fixed = jaune[0].lift_key;
  for (std::int32_t tick = 0; tick < 10; ++tick) {
    AdvanceTetheredBeamTracks(jaune, {0.0F, 0.0F, 0.0F}, target);
  }
  CHECK(jaune[0].lift_key == Approx(fixed));
}

TEST_CASE("le nuage SCINTILLE par cycles de dix tics", "[render][tethered_beam]") {
  const glm::vec3 target{10.0F, 0.0F, 0.0F};
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectTetheredBeamTracks(PatternOf(1210), {0.0F, 0.0F, 0.0F}, target);
  REQUIRE(tracks[0].alpha == 45);
  // Cinq tics de plus deux, cinq tics de moins deux : le cycle se referme.
  for (std::int32_t tick = 0; tick < 10; ++tick) {
    AdvanceTetheredBeamTracks(tracks, {0.0F, 0.0F, 0.0F}, target);
  }
  CHECK(tracks[0].alpha == 45);
  AdvanceTetheredBeamTracks(tracks, {0.0F, 0.0F, 0.0F}, target);
  CHECK(tracks[0].alpha == 47);
}

TEST_CASE("le semis n'allume qu'UNE piste sur quatre", "[render][tethered_beam]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectTetheredBeamTracks(PatternOf(384), {1.0F, 2.0F, 3.0F}, glm::vec3{9.0F, 2.0F, 3.0F});
  CHECK(tracks[0].active);
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    CHECK_FALSE(tracks[rang].active);
  }
  CHECK(tracks[0].lift_key == Approx(PatternOf(384).core_radius));
  CHECK(tracks[0].value[kTetheredBeamSlotOfVariant] == Approx(kTetheredBeamTealVariant));
  CHECK(tracks[0].tail[kTetheredBeamTailOfNearEnd + 1] == Approx(2.0F + kTetheredBeamLift));
}

TEST_CASE("une cible absente laisse le trait sans longueur", "[render][tethered_beam]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectTetheredBeamTracks(PatternOf(384), {1.0F, 2.0F, 3.0F}, std::nullopt);
  CHECK(tracks[0].tail[kTetheredBeamTailOfFarEnd] ==
        Approx(tracks[0].tail[kTetheredBeamTailOfNearEnd]));
  // ET ELLE NAÎT ÉTEINTE : le client ne garde la piste que s'il trouve la cible.
  CHECK_FALSE(tracks[0].active);
  CHECK(ComposeTetheredBeamQuads(tracks[0]).empty());
}

TEST_CASE("la table porte les cinq identifiants du sous-type 38", "[render][tethered_beam]") {
  const std::span<const EffectTetheredBeamPattern> table = EffectTetheredBeamPatternTable();
  REQUIRE(table.size() == kBeamEffectIds.size());
  for (std::size_t rang = 0; rang < kBeamEffectIds.size(); ++rang) {
    CHECK(table[rang].effect_id == kBeamEffectIds[rang]);
    CHECK_FALSE(table[rang].texture.empty());
    CHECK(table[rang].core_radius > 0.0F);
  }
  // ⚠ AUCUN DES CINQ NE TIRE : le générateur n'entre pas dans ce semis, et sa
  // signature le dit.
  CHECK(EffectTetheredBeamPatternsFor(1U).empty());
}

TEST_CASE("un trait dont la cible est partie s'eteint, et ne se rallume pas",
          "[render][tethered_beam]") {
  constexpr std::uint32_t kDevoter = 2'000'001;
  constexpr std::uint32_t kGone = 2'000'002;
  constexpr std::uint32_t kStaying = 2'000'003;
  ro::render::EffectNodePool pool;
  pool.AnchorNextTo(kDevoter);
  pool.SowFor(232);
  for (const std::uint32_t target_id : {kGone, kStaying}) {
    ro::render::PooledEffectNode node = SowEffectTetheredBeamNode(
        PatternOf(232), {0.0F, 0.0F, 0.0F}, glm::vec3{10.0F, 0.0F, 0.0F}, 600);
    node.target_actor_id = target_id;
    pool.Add(std::move(node));
  }
  pool.AnchorNextTo(0);
  pool.SowFor(0);

  pool.LoseTargetsUnless([](std::uint32_t id) { return id != kGone; });
  REQUIRE(pool.nodes().size() == 2);
  CHECK_FALSE(pool.nodes()[0].target_position.has_value());
  CHECK(pool.nodes()[1].target_position.has_value());

  // AU TIC SUIVANT, LE TRAIT VERS L'ABSENT EST ÉTEINT…
  const auto tick_ms = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  static_cast<void>(pool.Advance(tick_ms));
  REQUIRE(pool.nodes().size() == 2);
  CHECK_FALSE(pool.nodes()[0].tracks[0].active);
  CHECK(pool.nodes()[1].tracks[0].active);
  // …ET SON RETOUR NE LE RALLUME PAS : seule une annonce nouvelle le retend.
  ro::test_support::MoveBearer(pool, kGone, glm::vec3{12.0F, 0.0F, 0.0F});
  static_cast<void>(pool.Advance(tick_ms));
  CHECK_FALSE(pool.nodes()[0].tracks[0].active);
  CHECK(ComposeTetheredBeamQuads(pool.nodes()[0].tracks[0]).empty());
}

TEST_CASE("defaire un trait retire les plus anciennes instances, et pas au-dela",
          "[render][tethered_beam]") {
  constexpr std::uint32_t kDevoter = 2'000'001;
  constexpr std::uint32_t kOtherBearer = 2'000'009;
  constexpr std::size_t kUndone = 5;
  ro::render::EffectNodePool pool;
  // SEPT INSTANCES DU TRAIT SUR LE CROISÉ, dans l'ordre des naissances, et une
  // sur un autre porteur : le rang de naissance se lit dans la cible.
  constexpr std::uint32_t kInstances = 7;
  pool.SowFor(232);
  for (std::uint32_t rank = 0; rank < kInstances; ++rank) {
    pool.AnchorNextTo(kDevoter);
    ro::render::PooledEffectNode node = SowEffectTetheredBeamNode(
        PatternOf(232), {0.0F, 0.0F, 0.0F}, glm::vec3{10.0F, 0.0F, 0.0F}, 600);
    node.target_actor_id = rank + 1;
    pool.Add(std::move(node));
  }
  pool.AnchorNextTo(kOtherBearer);
  pool.Add(SowEffectTetheredBeamNode(PatternOf(232), {0.0F, 0.0F, 0.0F}, std::nullopt, 600));
  pool.AnchorNextTo(0);
  pool.SowFor(0);

  CHECK(pool.DropOldestFromEffect(232, kDevoter, kUndone) == kUndone);
  // LES DEUX PLUS JEUNES RESTENT, dans leur ordre, et le trait de l'autre aussi.
  REQUIRE(pool.nodes().size() == 3);
  CHECK(pool.nodes()[0].target_actor_id == 6);
  CHECK(pool.nodes()[1].target_actor_id == 7);
  CHECK(pool.nodes()[2].anchor_id == kOtherBearer);
  // Ce qui n'est pas là ne se compte pas.
  CHECK(pool.DropOldestFromEffect(232, kDevoter, kUndone) == 2);
  CHECK(pool.DropOldestFromEffect(384, kOtherBearer, kUndone) == 0);
  CHECK(pool.nodes().size() == 1);
}

TEST_CASE("le noeud seme porte sa forme, son image et sa cible", "[render][tethered_beam]") {
  const ro::render::PooledEffectNode node = SowEffectTetheredBeamNode(
      PatternOf(819), {2.0F, 0.0F, 7.0F}, glm::vec3{12.0F, 0.0F, 7.0F}, 600);
  CHECK(node.shape == ro::render::PooledNodeShape::kTetheredBeam);
  CHECK(node.node.life_ticks == 600);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == std::string(PatternOf(819).texture));
  REQUIRE(node.target_position.has_value());
  CHECK(node.target_position->x == Approx(12.0F));
}
