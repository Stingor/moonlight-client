#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_cloud_ring_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_figures.h"

using Catch::Approx;
using ro::render::AdvanceCloudRingTracks;
using ro::render::ComposeCloudRingQuads;
using ro::render::ComposeEffectTrackSkirt;
using ro::render::EffectCloudRingPattern;
using ro::render::EffectCloudRingPatternsFor;
using ro::render::EffectCloudRingPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kCloudRingDegreesPerTrack;
using ro::render::kCloudRingRadiusPerTick;
using ro::render::kCloudRingRiseUntilTick;
using ro::render::kCloudRingSlowRise;
using ro::render::kCloudRingSlowStyle;
using ro::render::kCloudRingSlowSweepCap;
using ro::render::kCloudRingSpinPerTick;
using ro::render::kCloudRingStartSweepDegrees;
using ro::render::kCloudRingSweepPerTick;
using ro::render::kCloudRingTiltFloor;
using ro::render::kCloudRingTiltSpread;
using ro::render::kCloudRingWideStyle;
using ro::render::kCloudRingWideSweepCap;
using ro::render::kEffectTracksPerNode;
using ro::render::SeedEffectCloudRingTracks;
using ro::render::SowEffectCloudRingNode;

namespace {

[[nodiscard]] EffectCloudRingPattern TestPattern(std::uint8_t octet) {
  EffectCloudRingPattern pattern;
  pattern.effect_id = 1;
  pattern.client_function = "test";
  pattern.texture = "essai.tga";
  pattern.style_byte = octet;
  pattern.trail_variant = 5;
  return pattern;
}

[[nodiscard]] EffectTrack LiveTrack(std::int16_t sweep_degrees, float height) {
  EffectTrack track;
  track.active = true;
  track.alpha = 200;
  track.degrees = sweep_degrees;
  track.radius_at_anchor = 10.0F;
  track.lift_key = height;
  track.tilt_degrees = 90;
  track.value.fill(1.0F);
  return track;
}

}  // namespace

TEST_CASE("l'anneau de nuage ne se dessine pas sans balayage", "[render][cloud_ring]") {
  EffectTrack track = LiveTrack(0, 0.0F);
  REQUIRE(ComposeCloudRingQuads(track, 5, glm::vec3{0.0F, 0.0F, 0.0F}).empty());
  track.degrees = -30;
  REQUIRE(ComposeCloudRingQuads(track, 5, glm::vec3{0.0F, 0.0F, 0.0F}).empty());
  track.active = false;
  track.degrees = 120;
  REQUIRE(ComposeCloudRingQuads(track, 5, glm::vec3{0.0F, 0.0F, 0.0F}).empty());
}

TEST_CASE("un balayage sous vingt et un ne rend RIEN plutot que de tourner sans fin",
          "[render][cloud_ring]") {
  // ⚠ LE CLIENT, LUI, SE FIGERAIT : son pas entier tomberait à zéro et son angle
  // cumulé n'avancerait jamais. Le bâtisseur part de trente, si bien que le cas
  // ne se présente pas — mais le rendu ne doit pas en dépendre.
  const EffectTrack track = LiveTrack(20, 0.0F);
  REQUIRE(ComposeCloudRingQuads(track, 5, glm::vec3{0.0F, 0.0F, 0.0F}).empty());
}

TEST_CASE("sa boucle tourne PLUS que ses segments n'en comptent", "[render][cloud_ring]") {
  // À trente degrés le pas vaut un : trente tours pour vingt et un segments, et
  // donc vingt-neuf quads. Le client borne alors son index à vingt.
  const std::vector<EffectWorldQuad> quads =
      ComposeCloudRingQuads(LiveTrack(30, 0.0F), 5, glm::vec3{0.0F, 0.0F, 0.0F});
  REQUIRE(quads.size() == 29);
}

TEST_CASE("son pas se divise par VINGT ET UN, pas par vingt", "[render][cloud_ring]") {
  // ⚠ UN CRAN D'ÉCART AVEC LA JUPE, et la boucle ne fait pas le même nombre de
  // tours. À cent vingt degrés, son pas vaut cinq — celui de la jupe en vaudrait
  // six.
  const std::vector<EffectWorldQuad> nuage =
      ComposeCloudRingQuads(LiveTrack(120, 0.0F), 5, glm::vec3{0.0F, 0.0F, 0.0F});
  const std::vector<EffectWorldQuad> skirt =
      ComposeEffectTrackSkirt(LiveTrack(120, 0.0F), 5, glm::vec3{0.0F, 0.0F, 0.0F});
  REQUIRE(nuage.size() != skirt.size());
  // 120 / 21 = 5 : les angles 0, 5, ..., 115 font vingt-quatre tours.
  REQUIRE(nuage.size() == 23);
}

TEST_CASE("sa cle est une HAUTEUR, pas une cle", "[render][cloud_ring]") {
  const std::vector<EffectWorldQuad> bottom =
      ComposeCloudRingQuads(LiveTrack(120, 0.0F), 5, glm::vec3{0.0F, 0.0F, 0.0F});
  const std::vector<EffectWorldQuad> top =
      ComposeCloudRingQuads(LiveTrack(120, 12.0F), 5, glm::vec3{0.0F, 0.0F, 0.0F});
  // ⚠ LA JUPE COMPARE SA CLÉ À DEUX FLOTTANTS EXACTS et ne bouge pas pour
  // douze ; celui-ci la prend telle quelle. Lui prêter la règle de sa sœur
  // collerait l'anneau au sol.
  REQUIRE(top[0].corners[0].y - bottom[0].corners[0].y == Approx(12.0F));
  const std::vector<EffectWorldQuad> skirt =
      ComposeEffectTrackSkirt(LiveTrack(120, 12.0F), 5, glm::vec3{0.0F, 0.0F, 0.0F});
  REQUIRE(skirt[0].corners[0].y == Approx(0.0F));
}

TEST_CASE("il peint une piste a alpha NUL", "[render][cloud_ring]") {
  EffectTrack track = LiveTrack(120, 0.0F);
  track.alpha = 0;
  // ⚠ LA JUPE ABANDONNE UNE TELLE PISTE ; celui-ci peint quand même, et c'est
  // nécessaire : son mouvement fait naître ses pistes à zéro et monte l'alpha
  // sur douze tics.
  REQUIRE_FALSE(ComposeCloudRingQuads(track, 5, glm::vec3{0.0F, 0.0F, 0.0F}).empty());
  REQUIRE(ComposeEffectTrackSkirt(track, 5, glm::vec3{0.0F, 0.0F, 0.0F}).empty());
}

TEST_CASE("le balayage S'OUVRE de trois par tic", "[render][cloud_ring]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = LiveTrack(kCloudRingStartSweepDegrees, 0.0F);
  tracks[0].style = kCloudRingSlowStyle;
  AdvanceCloudRingTracks(tracks);
  REQUIRE(tracks[0].degrees == kCloudRingStartSweepDegrees + kCloudRingSweepPerTick);
}

TEST_CASE("le style decide du plafond du balayage", "[render][cloud_ring]") {
  for (const std::int16_t style : {kCloudRingSlowStyle, kCloudRingWideStyle}) {
    std::array<EffectTrack, kEffectTracksPerNode> tracks;
    tracks[0] = LiveTrack(kCloudRingStartSweepDegrees, 0.0F);
    tracks[0].style = style;
    for (int tick = 0; tick < 100; ++tick) {
      AdvanceCloudRingTracks(tracks);
    }
    // ⚠ CENT VINGT OU CENT QUATRE-VINGTS : les confondre figerait l'anneau
    // ouvert de soixante degrés de trop ou de trop peu.
    REQUIRE(tracks[0].degrees ==
            (style == kCloudRingWideStyle ? kCloudRingWideSweepCap : kCloudRingSlowSweepCap));
  }
}

TEST_CASE("l'azimut tourne de cinq par tic et reboucle", "[render][cloud_ring]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = LiveTrack(30, 0.0F);
  tracks[0].azimuth_degrees = 358;
  AdvanceCloudRingTracks(tracks);
  REQUIRE(tracks[0].azimuth_degrees == 358 + kCloudRingSpinPerTick - 360);
}

TEST_CASE("l'alpha monte sous douze tics", "[render][cloud_ring]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = LiveTrack(30, 0.0F);
  tracks[0].alpha = 0;
  tracks[0].style = kCloudRingSlowStyle;
  AdvanceCloudRingTracks(tracks);
  REQUIRE(tracks[0].alpha == kCloudRingSlowRise);
  // Passé le douzième tic, il ne monte plus.
  for (int tick = 1; tick < kCloudRingRiseUntilTick + 3; ++tick) {
    AdvanceCloudRingTracks(tracks);
  }
  const std::int16_t ceiling = tracks[0].alpha;
  AdvanceCloudRingTracks(tracks);
  REQUIRE(tracks[0].alpha == ceiling);
}

TEST_CASE("le rayon ne croit QUE pour deux styles", "[render][cloud_ring]") {
  // ⚠ LE CLIENT SAUTE SON INCRÉMENT pour tout style qui n'est ni le large après
  // douze tics, ni le lent après vingt. Aux autres, l'anneau garde son rayon.
  for (const std::int16_t style : {std::int16_t{0}, std::int16_t{3}}) {
    std::array<EffectTrack, kEffectTracksPerNode> tracks;
    tracks[0] = LiveTrack(30, 0.0F);
    tracks[0].style = style;
    const float start = tracks[0].radius_at_anchor;
    for (int tick = 0; tick < 60; ++tick) {
      AdvanceCloudRingTracks(tracks);
    }
    REQUIRE(tracks[0].radius_at_anchor == Approx(start));
  }

  std::array<EffectTrack, kEffectTracksPerNode> larges;
  larges[0] = LiveTrack(30, 0.0F);
  larges[0].style = kCloudRingWideStyle;
  const float start = larges[0].radius_at_anchor;
  for (int tick = 0; tick < 20; ++tick) {
    AdvanceCloudRingTracks(larges);
  }
  // Huit tics de croissance après le douzième.
  REQUIRE(larges[0].radius_at_anchor ==
          Approx(start + 8.0F * kCloudRingRadiusPerTick).epsilon(1e-3));
}

TEST_CASE("une piste eteinte ne bouge pas", "[render][cloud_ring]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = LiveTrack(30, 0.0F);
  tracks[0].active = false;
  AdvanceCloudRingTracks(tracks);
  REQUIRE(tracks[0].degrees == 30);
  REQUIRE(tracks[0].ticks == 0);
}

TEST_CASE("le semis allume les quatre pistes a quatre-vingt-dix degres d'ecart",
          "[render][cloud_ring]") {
  std::uint32_t generator = 7U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectCloudRingTracks(TestPattern(0), generator);
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    REQUIRE(tracks[rang].active);
    REQUIRE(tracks[rang].alpha == 0);
    REQUIRE(tracks[rang].degrees == kCloudRingStartSweepDegrees);
    REQUIRE(tracks[rang].azimuth_degrees ==
            kCloudRingDegreesPerTrack * static_cast<std::int16_t>(rang));
    REQUIRE(tracks[rang].value[0] == Approx(1.0F));
    REQUIRE(tracks[rang].value[20] == Approx(1.0F));
    REQUIRE(tracks[rang].tilt_degrees >= kCloudRingTiltFloor);
    REQUIRE(tracks[rang].tilt_degrees < kCloudRingTiltFloor + kCloudRingTiltSpread);
  }
}

TEST_CASE("l'octet choisit le style et la cle", "[render][cloud_ring]") {
  std::uint32_t generator = 3U;
  for (const std::uint8_t octet :
       {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{3}}) {
    std::uint32_t own = generator;
    const std::array<EffectTrack, kEffectTracksPerNode> tracks =
        SeedEffectCloudRingTracks(TestPattern(octet), own);
    REQUIRE(tracks[0].style == (octet < 2 ? kCloudRingSlowStyle : kCloudRingWideStyle));
  }
  std::uint32_t a = 3U;
  std::uint32_t b = 3U;
  const std::array<EffectTrack, kEffectTracksPerNode> rise =
      SeedEffectCloudRingTracks(TestPattern(0), a);
  const std::array<EffectTrack, kEffectTracksPerNode> descend =
      SeedEffectCloudRingTracks(TestPattern(2), b);
  // ⚠ DEUX COMPTENT VERS LE HAUT, DEUX VERS LE BAS : les quatre formules sont
  // des mesures, pas un motif.
  REQUIRE(rise[3].lift_key > rise[0].lift_key);
  REQUIRE(descend[3].lift_key < descend[0].lift_key);
}

TEST_CASE("deux octets TIRENT leur rayon, les deux autres non", "[render][cloud_ring]") {
  // ⚠ DEUX TIRAGES PAR PISTE CONTRE UN : les uniformiser décalerait la suite du
  // générateur.
  std::uint32_t with = 11U;
  std::uint32_t sans = 11U;
  static_cast<void>(SeedEffectCloudRingTracks(TestPattern(0), with));
  static_cast<void>(SeedEffectCloudRingTracks(TestPattern(2), sans));
  REQUIRE(with != sans);
}

TEST_CASE("le noeud seme porte sa variante de palette", "[render][cloud_ring]") {
  std::uint32_t generator = 5U;
  EffectCloudRingPattern pattern = TestPattern(1);
  pattern.trail_variant = 8;
  const ro::render::PooledEffectNode node =
      SowEffectCloudRingNode(pattern, glm::vec3{0.0F, 0.0F, 0.0F}, 100, generator);
  REQUIRE(node.shape == ro::render::PooledNodeShape::kCloudRing);
  REQUIRE(node.trail_variant == 8);
}

TEST_CASE("la table des anneaux de nuage porte neuf identifiants sur treize lignes",
          "[render][cloud_ring]") {
  // ⚠ NEUF, PAS SIX : le frere effectcls_InitHealCloud est entre avec EF_WIND,
  // et le nuage violet et rouge avec les deux murs — 317 et 341.
  const std::span<const EffectCloudRingPattern> table = EffectCloudRingPatternTable();
  REQUIRE(table.size() == 13);
  REQUIRE(std::ranges::is_sorted(table, {}, &EffectCloudRingPattern::effect_id));
  // ⚠ QUATRE DES SIX APPELLENT LEUR BÂTISSEUR DEUX FOIS.
  for (const std::uint32_t effect_id : {435U, 459U, 460U, 461U}) {
    REQUIRE(EffectCloudRingPatternsFor(effect_id).size() == 2);
  }
  for (const std::uint32_t effect_id : {224U, 317U, 341U, 389U, 402U}) {
    REQUIRE(EffectCloudRingPatternsFor(effect_id).size() == 1);
  }
  REQUIRE(EffectCloudRingPatternsFor(1U).empty());
}

TEST_CASE("seul l'octet UN porte la variante HUIT", "[render][cloud_ring]") {
  for (const EffectCloudRingPattern& pattern : EffectCloudRingPatternTable()) {
    // ⚠ ET LES DEUX AUTRES BÂTISSEURS NE SUIVENT PAS CETTE REGLE. Le frere ne
    // recoit aucun octet et sa variante vaut QUATRE ; le nuage violet et rouge
    // recoit un STYLE, et c'est lui qui la choisit — quatre pour le mur bleu,
    // dix pour le rouge. Leur appliquer la regle de l'octet leur donnerait cinq,
    // c'est-a-dire la teinte d'un autre nuage.
    if (pattern.builder == ro::render::EffectCloudRingBuilder::kHealCloud) {
      REQUIRE(pattern.trail_variant == 4);
      continue;
    }
    if (pattern.builder == ro::render::EffectCloudRingBuilder::kVioletRedCloud) {
      REQUIRE(pattern.trail_variant == (pattern.style_byte == 3 ? 10 : 4));
      continue;
    }
    REQUIRE(pattern.trail_variant == (pattern.style_byte == 1 ? 8 : 5));
  }
}

TEST_CASE("la nappe du portail attend le tic DEUX et ne tire rien", "[render][cloud_ring]") {
  // 🔴 SON BÂTISSEUR POSE TROIS NŒUDS DE TROIS SOUS-TYPES aux trois premiers
  // tics — la colonne au zero, les anneaux qui pulsent au UN, cette fumee au
  // DEUX —, et sa garde est une EGALITE sur le compteur de l'instance. Les six
  // autres nuages naissent au tic zero, et c'est ce qui avait fait croire que
  // cette famille n'avait pas besoin d'horloge.
  const std::span<const EffectCloudRingPattern> smoke = EffectCloudRingPatternsFor(317);
  REQUIRE(smoke.size() == 1);
  REQUIRE(smoke.front().builder == ro::render::EffectCloudRingBuilder::kVioletRedCloud);
  CHECK(smoke.front().schedule.first_tick == 2);
  CHECK(ro::render::EffectNodeScheduleLastSowTick(smoke.front().schedule) == 2);
  CHECK(!ro::render::EffectNodeScheduleSowsAt(smoke.front().schedule, 0));
  CHECK(ro::render::EffectNodeScheduleSowsAt(smoke.front().schedule, 2));
  CHECK(!ro::render::EffectNodeScheduleSowsAt(smoke.front().schedule, 3));
  // Et les six qui naissent d'emblee ne changent pas d'heure.
  CHECK(ro::render::EffectNodeScheduleSowsAt(EffectCloudRingPatternsFor(389).front().schedule, 0));

  std::uint32_t generator = 4242U;
  const std::uint32_t before = generator;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectCloudRingTracks(smoke.front(), generator);
  CHECK(generator == before);

  // Quatre anneaux emboites : la cle monte de deux par rang, le rayon retombe
  // d'un et demi. Lus un a un dans le troisieme bloc du batisseur.
  CHECK(tracks[0].lift_key == Approx(1.5F));
  CHECK(tracks[3].lift_key == Approx(7.5F));
  CHECK(tracks[0].radius_at_anchor == Approx(9.0F));
  CHECK(tracks[3].radius_at_anchor == Approx(4.5F));
  CHECK(tracks[0].azimuth_degrees == 0);
  CHECK(tracks[3].azimuth_degrees == 270);
  for (const EffectTrack& track : tracks) {
    CHECK(track.active);
    CHECK(track.alpha == 0);
    CHECK(track.degrees == kCloudRingStartSweepDegrees);
    CHECK(track.tilt_degrees == 90);
    CHECK(track.value.front() == Approx(1.5F));
    CHECK(track.mark.front() == 0);
    // 🔴 UN TROISIÈME STYLE, ni le lent ni le large : le mouvement ne le traite
    // que par exclusion, et il ne lui reste que la descente du mille quatre
    // centieme tic. C'est elle qui eteint la nappe du portail.
    CHECK(track.style == 0);
    CHECK(track.style != kCloudRingSlowStyle);
    CHECK(track.style != kCloudRingWideStyle);
  }
}

TEST_CASE("le nuage de soin ne TIRE rien, et ses quatre pistes se resserrent",
          "[render][cloud_ring]") {
  // ⚠ SON FRERE TIRE DEUX DE SES QUATRE RAYONS ; celui-ci n'en tire aucun. Le
  // faire passer par les formules de l'octet lui donnerait des rayons au hasard
  // ET decalerait la suite du generateur pour tout ce qui vient apres lui.
  const std::span<const EffectCloudRingPattern> nuage = EffectCloudRingPatternsFor(224);
  REQUIRE(nuage.size() == 1);
  REQUIRE(nuage.front().builder == ro::render::EffectCloudRingBuilder::kHealCloud);

  std::uint32_t generator = 12345;
  const std::uint32_t before = generator;
  const std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode> tracks =
      ro::render::SeedEffectCloudRingTracks(nuage.front(), generator);
  CHECK(generator == before);

  // Quatre suites regulieres, lues une a une dans effectcls_InitHealCloud.
  CHECK(tracks[0].degrees == 90);
  CHECK(tracks[3].degrees == 30);
  CHECK(tracks[0].azimuth_degrees == 0);
  CHECK(tracks[3].azimuth_degrees == 30);
  CHECK(tracks[0].radius_at_anchor == Catch::Approx(3.0F));
  CHECK(tracks[3].radius_at_anchor == Catch::Approx(3.6F));
  CHECK(tracks[0].value.front() == Catch::Approx(3.0F));
  CHECK(tracks[3].value.front() == Catch::Approx(1.5F));
  for (const ro::render::EffectTrack& track : tracks) {
    CHECK(track.active);
    CHECK(track.lift_key == Catch::Approx(8.0F));
    CHECK(track.tilt_degrees == 90);
    CHECK(track.alpha == 0);
  }
}
