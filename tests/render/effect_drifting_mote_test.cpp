#include "ro/render/effect_drifting_mote.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <set>
#include <span>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_drifting_mote_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_spread.h"

using Catch::Approx;
using ro::render::AdvanceDriftingMoteTracks;
using ro::render::ComposeDriftingMoteQuad;
using ro::render::EffectDriftingMoteColor;
using ro::render::EffectDriftingMotePattern;
using ro::render::EffectDriftingMotePatternsFor;
using ro::render::EffectDriftingMotePatternTable;
using ro::render::EffectNodeScheduleSowsAt;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kDriftingMoteAlphaFall;
using ro::render::kDriftingMoteAlphaRise;
using ro::render::kDriftingMoteBlue;
using ro::render::kDriftingMoteDim;
using ro::render::kDriftingMoteDrift;
using ro::render::kDriftingMoteFadesAfterTick;
using ro::render::kDriftingMoteLoneMark;
using ro::render::kDriftingMoteLoneRing;
using ro::render::kDriftingMoteSlotOfDepthBelow;
using ro::render::kDriftingMoteSlotOfForward;
using ro::render::kDriftingMoteSlotOfSideways;
using ro::render::kDriftingMoteSwarmMark;
using ro::render::kDriftingMoteSwarmRing;
using ro::render::kDriftingMoteWhite;
using ro::render::kEffectTracksPerNode;
using ro::render::SeedEffectDriftingMoteTracks;
using ro::render::SowEffectDriftingMoteNode;

namespace {

[[nodiscard]] EffectTrack Living(std::uint8_t mark = 1, float height = -5.0F) {
  EffectTrack track;
  track.active = true;
  track.alpha = 120;
  track.ticks = 10;
  track.radius_at_anchor = 3.0F;
  track.lift_key = 0.1F;
  track.tail[kDriftingMoteSlotOfSideways] = 2.0F;
  track.tail[kDriftingMoteSlotOfDepthBelow] = height;
  track.tail[kDriftingMoteSlotOfForward] = 4.0F;
  track.mark[0] = mark;
  return track;
}

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Sown(std::int16_t integer,
                                                                 std::uint32_t seed = 12U) {
  EffectDriftingMotePattern pattern;
  pattern.variant = integer;
  return SeedEffectDriftingMoteTracks(pattern, seed);
}

}  // namespace

TEST_CASE("le grain rend UN carre par piste", "[render][effect][mote]") {
  // ⚠ SON NOM DIT « QUAD4 » ET IL N'Y EN A QU'UN : le quatre est le nombre de
  // PISTES. Compter quatre quads par piste en peindrait seize.
  const std::optional<EffectWorldQuad> quad = ComposeDriftingMoteQuad(Living(), 0, 0, {});
  REQUIRE(quad.has_value());
  // ⚠ NOTRE REPÈRE MONTE : une hauteur de moins cinq chez le client est CINQ
  // unités au-dessus du nœud chez nous.
  for (const glm::vec3& corner : quad->corners) {
    CHECK(corner.y == Approx(5.0F));
  }
  // Le décalage se retrouve dans le plan du sol, sa profondeur — celle du
  // client — retournée dans la nôtre.
  const glm::vec3 middle =
      (quad->corners[0] + quad->corners[1] + quad->corners[2] + quad->corners[3]) / 4.0F;
  CHECK(middle.x == Approx(2.0F));
  CHECK(middle.z == Approx(-4.0F));
}

TEST_CASE("le grain ne se montre que SOUS le noeud du client", "[render][effect][mote]") {
  // ⚠ ET LE CLIENT TESTE LE PREMIER COIN, pas le centre : tester le centre le
  // ferait paraître un demi-carré trop tôt.
  CHECK_FALSE(ComposeDriftingMoteQuad(Living(1, 50.0F), 0, 0, {}).has_value());
  CHECK(ComposeDriftingMoteQuad(Living(1, -50.0F), 0, 0, {}).has_value());
  EffectTrack eteint = Living();
  eteint.active = false;
  CHECK_FALSE(ComposeDriftingMoteQuad(eteint, 0, 0, {}).has_value());
}

TEST_CASE("la marque choisit la teinte", "[render][effect][mote]") {
  CHECK(EffectDriftingMoteColor(2) == kDriftingMoteBlue);
  CHECK(EffectDriftingMoteColor(3) == kDriftingMoteDim);
  CHECK(EffectDriftingMoteColor(1) == kDriftingMoteWhite);
  CHECK(EffectDriftingMoteColor(0) == kDriftingMoteWhite);
  const std::optional<EffectWorldQuad> blue = ComposeDriftingMoteQuad(Living(2), 0, 0, {});
  REQUIRE(blue.has_value());
  CHECK(blue->tint[0] == kDriftingMoteBlue[0]);
  CHECK(blue->tint[3] == 120);
}

TEST_CASE("le grain MONTE et derive de cote", "[render][effect][mote]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Living();
  // Un angle dont le sinus vaut un, pour les deux paires.
  tracks[0].value[0] = 90.0F;
  tracks[0].value[2] = 90.0F;
  std::uint32_t seed = 4U;
  AdvanceDriftingMoteTracks(tracks, 0, 0, seed);
  // Dans le repère du client, monter c'est retrancher la clé.
  CHECK(tracks[0].tail[kDriftingMoteSlotOfDepthBelow] == Approx(-5.1F));
  CHECK(tracks[0].tail[kDriftingMoteSlotOfSideways] == Approx(2.0F + kDriftingMoteDrift));
  CHECK(tracks[0].tail[kDriftingMoteSlotOfForward] == Approx(4.0F + kDriftingMoteDrift));
}

TEST_CASE("il cesse de deriver une fois passe SOUS le noeud", "[render][effect][mote]") {
  // ⚠ IL NE RENAÎT PAS, contrairement à la rafale : il continue de monter tout
  // droit et s'éteint par son alpha. Le laisser dériver l'éloignerait sans fin.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Living(1, 5.0F);
  tracks[0].value[0] = 90.0F;
  tracks[0].value[2] = 90.0F;
  std::uint32_t seed = 4U;
  AdvanceDriftingMoteTracks(tracks, 0, 0, seed);
  CHECK(tracks[0].tail[kDriftingMoteSlotOfSideways] == Approx(2.0F));
  CHECK(tracks[0].tail[kDriftingMoteSlotOfDepthBelow] == Approx(4.9F));
}

TEST_CASE("l'alpha monte de cinq puis tombe de deux", "[render][effect][mote]") {
  // ⚠ LA MONTÉE EST DEUX FOIS ET DEMIE PLUS RAIDE QUE LA CHUTE : le grain
  // paraît vite et s'efface longtemps.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].ticks = 0;
  tracks[0].alpha = 0;
  std::uint32_t seed = 4U;
  AdvanceDriftingMoteTracks(tracks, 0, 0, seed);
  CHECK(tracks[0].alpha == kDriftingMoteAlphaRise);
  tracks[0].ticks = kDriftingMoteFadesAfterTick - 1;
  AdvanceDriftingMoteTracks(tracks, 0, 0, seed);
  CHECK(tracks[0].ticks == kDriftingMoteFadesAfterTick);
  // Un seul tic de montee a eu lieu avant qu'on avance le compteur : cinq
  // moins deux.
  CHECK(tracks[0].alpha == kDriftingMoteAlphaRise - kDriftingMoteAlphaFall);
  // Et il ne passe pas sous zéro.
  for (std::int32_t tick = 0; tick < 200; ++tick) {
    AdvanceDriftingMoteTracks(tracks, 0, 0, seed);
  }
  CHECK(tracks[0].alpha == 0);
}

TEST_CASE("le retard court sans rien faire", "[render][effect][mote]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Living();
  tracks[0].ticks = -3;
  tracks[0].alpha = 0;
  std::uint32_t seed = 4U;
  AdvanceDriftingMoteTracks(tracks, 0, 0, seed);
  CHECK(tracks[0].ticks == -2);
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[0].tail[kDriftingMoteSlotOfDepthBelow] == Approx(-5.0F));
}

TEST_CASE("les deux angles de camera se rangent dans les styles", "[render][effect][mote]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  std::uint32_t seed = 1U;
  // 🔴 TELS QU'ILS ARRIVENT : ils sortent déjà de `EffectBillboard*Degrees`, qui
  // fait le calcul du client. Le refaire tournait la figure à l'envers de la
  // caméra. Voir `StoreEffectBillboardAngles`.
  AdvanceDriftingMoteTracks(tracks, 90, 100, seed);
  CHECK(tracks[0].style == 90);
  CHECK(tracks[1].style == 100);
}

TEST_CASE("le solitaire n'allume qu'UNE piste, la volee QUATRE", "[render][effect][mote]") {
  const std::array<EffectTrack, kEffectTracksPerNode> single = Sown(0);
  CHECK(single[0].active);
  CHECK_FALSE(single[1].active);
  CHECK_FALSE(single[3].active);
  CHECK(single[0].mark[0] == kDriftingMoteLoneMark);
  for (const EffectTrack& track : Sown(1)) {
    CHECK(track.active);
    CHECK(track.mark[0] == kDriftingMoteSwarmMark);
  }
  // ⚠ ET TOUT AUTRE ENTIER N'ALLUME RIEN : le client éteint les quatre puis
  // n'entre dans aucune branche.
  for (const EffectTrack& track : Sown(7)) {
    CHECK_FALSE(track.active);
  }
}

TEST_CASE("le grain nait sur un CERCLE, pas au centre", "[render][effect][mote]") {
  // ⚠ LE POSER AU CENTRE ferait partir les quatre du même point.
  for (const EffectTrack& track : Sown(1)) {
    const float radius = std::sqrt(
        track.tail[kDriftingMoteSlotOfSideways] * track.tail[kDriftingMoteSlotOfSideways] +
        track.tail[kDriftingMoteSlotOfForward] * track.tail[kDriftingMoteSlotOfForward]);
    CHECK(radius == Approx(kDriftingMoteSwarmRing));
  }
  const EffectTrack single = Sown(0)[0];
  const float radius = std::sqrt(
      single.tail[kDriftingMoteSlotOfSideways] * single.tail[kDriftingMoteSlotOfSideways] +
      single.tail[kDriftingMoteSlotOfForward] * single.tail[kDriftingMoteSlotOfForward]);
  CHECK(radius == Approx(kDriftingMoteLoneRing));
}

TEST_CASE("le solitaire est plus gros et plus rapide", "[render][effect][mote]") {
  // ⚠ TROIS ÉCARTS, ET ILS VONT DANS DEUX SENS CONTRAIRES : plus gros, plus
  // rapide, mais un cercle de départ plus petit.
  for (std::uint32_t seed = 1U; seed < 30U; ++seed) {
    const EffectTrack single = Sown(0, seed)[0];
    const EffectTrack volee = Sown(1, seed)[0];
    CHECK(single.radius_at_anchor >= 3.0F);
    CHECK(single.radius_at_anchor <= Approx(4.0F));
    CHECK(volee.radius_at_anchor >= 2.0F);
    CHECK(volee.radius_at_anchor <= Approx(2.5F));
    CHECK(single.lift_key >= 0.1F);
    CHECK(single.lift_key <= Approx(0.25F));
    CHECK(volee.lift_key >= 0.08F);
    CHECK(volee.lift_key <= Approx(0.13F));
  }
  CHECK(kDriftingMoteLoneRing < kDriftingMoteSwarmRing);
}

TEST_CASE("les grains naissent AU-DESSUS du noeud et en decale", "[render][effect][mote]") {
  std::set<float> hauteurs;
  std::set<std::int32_t> retards;
  for (const EffectTrack& track : Sown(1)) {
    // ⚠ NÉGATIVE DANS LE REPÈRE DU CLIENT : de zéro à sept unités au-dessus.
    CHECK(track.tail[kDriftingMoteSlotOfDepthBelow] <= 0.0F);
    CHECK(track.tail[kDriftingMoteSlotOfDepthBelow] > -8.0F);
    CHECK(track.ticks <= 0);
    CHECK(track.ticks > -26);
    CHECK(track.alpha == 0);
    hauteurs.insert(track.tail[kDriftingMoteSlotOfDepthBelow]);
    retards.insert(track.ticks);
  }
  CHECK(hauteurs.size() > 1);
  CHECK(retards.size() > 1);
}

TEST_CASE("l'entier du site est aussi le TIC du semis", "[render][effect][mote]") {
  // ⚠ DEUX EXCEPTIONS SUR UNE RÈGLE DE DEUX CAS : la valeur UN sème au tic
  // ZÉRO, la valeur ZÉRO au tic TRENTE-CINQ. Le semer au premier tic
  // montrerait l'un des quatre une seconde et demie trop tôt.
  const std::span<const EffectDriftingMotePattern> table = EffectDriftingMotePatternTable();
  REQUIRE(table.size() == 4);
  CHECK(std::ranges::is_sorted(table, {}, &EffectDriftingMotePattern::effect_id));

  REQUIRE_FALSE(EffectDriftingMotePatternsFor(411).empty());
  const EffectDriftingMotePattern& tardif = EffectDriftingMotePatternsFor(411).front();
  CHECK(tardif.variant == 0);
  CHECK_FALSE(EffectNodeScheduleSowsAt(tardif.schedule, 0));
  CHECK(EffectNodeScheduleSowsAt(tardif.schedule, 35));
  CHECK_FALSE(EffectNodeScheduleSowsAt(tardif.schedule, 36));

  REQUIRE_FALSE(EffectDriftingMotePatternsFor(462).empty());
  const EffectDriftingMotePattern& tot = EffectDriftingMotePatternsFor(462).front();
  CHECK(tot.variant == 1);
  CHECK(EffectNodeScheduleSowsAt(tot.schedule, 0));
  CHECK_FALSE(EffectNodeScheduleSowsAt(tot.schedule, 1));
  CHECK(EffectDriftingMotePatternsFor(1).empty());
}

TEST_CASE("un noeud de grain porte une image et sa forme", "[render][effect][mote]") {
  EffectDriftingMotePattern pattern;
  pattern.texture = "data\\texture\\effect\\whitelight.tga";
  pattern.variant = 1;
  std::uint32_t seed = 6U;
  const ro::render::PooledEffectNode node =
      SowEffectDriftingMoteNode(pattern, {5.0F, 0.0F, 6.0F}, 100, seed);
  CHECK(node.shape == ro::render::PooledNodeShape::kDriftingMote);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths[0] == "data\\texture\\effect\\whitelight.tga");
  CHECK(node.ground_position.z == Approx(6.0F));
}
