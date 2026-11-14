#include "ro/render/effect_flying_shard.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <set>
#include <span>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_flying_shard_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_burst.h"

using Catch::Approx;
using ro::render::AdvanceFlyingShardTracks;
using ro::render::ComposeFlyingShardQuad;
using ro::render::EffectFlyingShardBlendMode;
using ro::render::EffectFlyingShardBuilder;
using ro::render::EffectFlyingShardColor;
using ro::render::EffectFlyingShardPattern;
using ro::render::EffectFlyingShardPatternsFor;
using ro::render::EffectFlyingShardPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kFlyingShardAlphaFall;
using ro::render::kFlyingShardAlphaRise;
using ro::render::kFlyingShardBirthHeight;
using ro::render::kFlyingShardDimsAfterTick;
using ro::render::kFlyingShardGold;
using ro::render::kFlyingShardLightsAtTick;
using ro::render::kFlyingShardMarkOfColor;
using ro::render::kFlyingShardMarkOfLength;
using ro::render::kFlyingShardPale;
using ro::render::kFlyingShardSky;
using ro::render::kFlyingShardSlenderGrowth;
using ro::render::kFlyingShardSlenderHalfWidth;
using ro::render::kFlyingShardSlenderLength;
using ro::render::kFlyingShardSlotOfGrowth;
using ro::render::kFlyingShardSlotOfHalfWidth;
using ro::render::kFlyingShardStartRadius;
using ro::render::kFlyingShardStoutGrowth;
using ro::render::kFlyingShardStoutHalfWidth;
using ro::render::kFlyingShardStoutLength;
using ro::render::kFlyingShardWhite;
using ro::render::SeedEffectFlyingShardTracks;
using ro::render::SowEffectFlyingShardNode;

namespace {

[[nodiscard]] EffectTrack Living(std::int16_t in_plane_degrees = 0,
                                 std::int16_t plane_azimuth_degrees = 0) {
  EffectTrack track;
  track.active = true;
  track.alpha = 100;
  track.ticks = 5;
  track.radius_at_anchor = 20.0F;
  track.degrees = in_plane_degrees;
  track.tilt_degrees = plane_azimuth_degrees;
  track.value[kFlyingShardSlotOfHalfWidth] = 1.0F;
  track.mark[kFlyingShardMarkOfLength] = 10;
  return track;
}

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Sown(EffectFlyingShardBuilder batisseur,
                                                                 std::uint8_t octet = 0,
                                                                 std::uint32_t seed = 9U) {
  EffectFlyingShardPattern pattern;
  pattern.builder = batisseur;
  pattern.mark = octet;
  return SeedEffectFlyingShardTracks(pattern, seed);
}

}  // namespace

TEST_CASE("l'eclat est un BARREAU dans un plan vertical", "[render][effect][shard]") {
  // ⚠ CE N'EST PAS UN BILLBOARD, malgré le nom du client : rien de son
  // orientation ne vient de la caméra. Deux angles le posent, et ils ne sont
  // pas de même nature.
  const std::optional<EffectWorldQuad> quad = ComposeFlyingShardQuad(Living(0, 0), {});
  REQUIRE(quad.has_value());
  // Avec les deux angles à zéro, le plan est celui des x, le barreau est
  // horizontal, et sa largeur est VERTICALE.
  CHECK(quad->corners[0].z == Approx(0.0F));
  CHECK(quad->corners[0].x == Approx(20.0F + 10.0F));
  CHECK(quad->corners[3].x == Approx(20.0F - 10.0F));
  // ⚠ SA LONGUEUR VIENT DE LA SECONDE MARQUE : dix demi-largeurs de part et
  // d'autre du rayon. La lire comme un numéro rendrait tous les éclats carrés.
  CHECK(quad->corners[0].x - quad->corners[3].x == Approx(20.0F));
  // Et sa largeur est bien de deux demi-largeurs.
  CHECK(std::abs(quad->corners[0].y - quad->corners[1].y) == Approx(2.0F));
}

TEST_CASE("le second angle dresse le PLAN", "[render][effect][shard]") {
  // ⚠ L'INCLINAISON DONNE L'AZIMUT DU PLAN, les degrés l'orientation DANS ce
  // plan. Les confondre poserait tous les éclats à plat.
  const std::optional<EffectWorldQuad> vers_le_sud = ComposeFlyingShardQuad(Living(0, 90), {});
  REQUIRE(vers_le_sud.has_value());
  CHECK(vers_le_sud->corners[0].x == Approx(0.0F).margin(1e-4F));
  CHECK(vers_le_sud->corners[0].z == Approx(30.0F));

  // Et dans le plan, un quart de tour met le barreau DEBOUT.
  const std::optional<EffectWorldQuad> debout = ComposeFlyingShardQuad(Living(90, 0), {});
  REQUIRE(debout.has_value());
  CHECK(debout->corners[0].x == Approx(1.0F).margin(1e-4F));
  // ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : un barreau dressé à
  // quatre-vingt-dix degrés part vers le BAS chez nous.
  CHECK(debout->corners[0].y == Approx(-30.0F));
}

TEST_CASE("une piste eteinte ou muette ne rend rien", "[render][effect][shard]") {
  EffectTrack track = Living();
  track.active = false;
  CHECK_FALSE(ComposeFlyingShardQuad(track, {}).has_value());
  track = Living();
  track.alpha = 0;
  CHECK_FALSE(ComposeFlyingShardQuad(track, {}).has_value());
}

TEST_CASE("la PREMIERE marque choisit la teinte et le melange", "[render][effect][shard]") {
  CHECK(EffectFlyingShardColor(0) == kFlyingShardGold);
  CHECK(EffectFlyingShardColor(1) == kFlyingShardWhite);
  CHECK(EffectFlyingShardColor(2) == kFlyingShardSky);
  CHECK(EffectFlyingShardColor(9) == kFlyingShardPale);
  // ⚠ SEULE LA QUATRIÈME TEINTE SE MÉLANGE PAR L'ALPHA ; les trois autres
  // s'ajoutent. Et aucun de nos quatre sites ne la demande.
  CHECK(EffectFlyingShardBlendMode(0) == EffectFlyingShardBlendMode(2));
  CHECK(EffectFlyingShardBlendMode(9) != EffectFlyingShardBlendMode(0));
}

TEST_CASE("l'eclat FUIT, et traverse le centre", "[render][effect][shard]") {
  // ⚠ IL PART DE L'AUTRE CÔTÉ — rayon moins dix — et le traverse. Le semer à
  // zéro lui ferait manquer la moitié de sa course.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].ticks = 0;
  tracks[0].radius_at_anchor = kFlyingShardStartRadius;
  tracks[0].value[kFlyingShardSlotOfGrowth] = kFlyingShardSlenderGrowth;
  AdvanceFlyingShardTracks(tracks);
  CHECK(tracks[0].radius_at_anchor == Approx(kFlyingShardStartRadius + kFlyingShardSlenderGrowth));
  // ⚠ ET LE RAYON NE GRANDIT PAS PENDANT LE RETARD.
  tracks[0].ticks = -5;
  tracks[0].radius_at_anchor = kFlyingShardStartRadius;
  AdvanceFlyingShardTracks(tracks);
  CHECK(tracks[0].radius_at_anchor == Approx(kFlyingShardStartRadius));
  CHECK(tracks[0].ticks == -4);
}

TEST_CASE("l'alpha TOMBE avant de monter", "[render][effect][shard]") {
  // ⚠ DU PREMIER AU DIXIÈME TIC le client RETRANCHE cinq à un alpha déjà nul,
  // puis il AJOUTE quinze du onzième au vingtième, puis il retranche de
  // nouveau. Une montée franche dès le premier tic allumerait l'éclat dix tics
  // trop tôt.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].ticks = 0;
  tracks[0].alpha = 0;
  for (std::int32_t tick = 0; tick < kFlyingShardLightsAtTick - 1; ++tick) {
    AdvanceFlyingShardTracks(tracks);
    CHECK(tracks[0].alpha == 0);
  }
  AdvanceFlyingShardTracks(tracks);
  CHECK(tracks[0].ticks == kFlyingShardLightsAtTick);
  CHECK(tracks[0].alpha == kFlyingShardAlphaRise);

  // Il monte jusqu'au vingtième, puis retombe.
  while (tracks[0].ticks < kFlyingShardDimsAfterTick) {
    AdvanceFlyingShardTracks(tracks);
  }
  const std::int16_t full = tracks[0].alpha;
  CHECK(full == kFlyingShardAlphaRise * (kFlyingShardDimsAfterTick - kFlyingShardLightsAtTick + 1));
  AdvanceFlyingShardTracks(tracks);
  CHECK(tracks[0].alpha == full - kFlyingShardAlphaFall);

  // Et il ne passe pas sous zéro.
  for (std::int32_t tick = 0; tick < 200; ++tick) {
    AdvanceFlyingShardTracks(tracks);
  }
  CHECK(tracks[0].alpha == 0);
}

TEST_CASE("une piste eteinte n'avance pas", "[render][effect][shard]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  AdvanceFlyingShardTracks(tracks);
  for (const EffectTrack& track : tracks) {
    CHECK(track.ticks == 0);
  }
}

TEST_CASE("les deux batisseurs ne different que par QUATRE nombres", "[render][effect][shard]") {
  const std::array<EffectTrack, kEffectTracksPerNode> elance =
      Sown(EffectFlyingShardBuilder::kSlender);
  const std::array<EffectTrack, kEffectTracksPerNode> trapu =
      Sown(EffectFlyingShardBuilder::kStout);
  CHECK(elance[0].value[kFlyingShardSlotOfGrowth] == Approx(kFlyingShardSlenderGrowth));
  CHECK(trapu[0].value[kFlyingShardSlotOfGrowth] == Approx(kFlyingShardStoutGrowth));
  CHECK(elance[0].value[kFlyingShardSlotOfHalfWidth] == Approx(kFlyingShardSlenderHalfWidth));
  CHECK(trapu[0].value[kFlyingShardSlotOfHalfWidth] == Approx(kFlyingShardStoutHalfWidth));
  CHECK(elance[0].mark[kFlyingShardMarkOfLength] == kFlyingShardSlenderLength);
  CHECK(trapu[0].mark[kFlyingShardMarkOfLength] == kFlyingShardStoutLength);
  // Et tout le reste est identique.
  for (const std::array<EffectTrack, kEffectTracksPerNode>* lot : {&elance, &trapu}) {
    for (const EffectTrack& track : *lot) {
      CHECK(track.active);
      CHECK(track.alpha == 0);
      CHECK(track.radius_at_anchor == Approx(kFlyingShardStartRadius));
      CHECK(track.degrees >= 0);
      CHECK(track.degrees < 360);
      CHECK(track.tilt_degrees >= 0);
      CHECK(track.tilt_degrees < 360);
    }
  }
}

TEST_CASE("les quatre eclats partent en decale", "[render][effect][shard]") {
  // ⚠ LE RETARD EST TIRÉ POUR CHAQUE PISTE. Les semer au même tic en ferait un
  // seul trait épais.
  std::set<std::int32_t> retards;
  std::set<std::int16_t> plans;
  for (const EffectTrack& track : Sown(EffectFlyingShardBuilder::kSlender)) {
    CHECK(track.ticks <= -10);
    CHECK(track.ticks >= -34);
    retards.insert(track.ticks);
    plans.insert(track.tilt_degrees);
  }
  CHECK(retards.size() > 1);
  CHECK(plans.size() > 1);
  for (const EffectTrack& track : Sown(EffectFlyingShardBuilder::kStout)) {
    CHECK(track.ticks <= -20);
    CHECK(track.ticks >= -29);
  }
}

TEST_CASE("le noeud monte de NEUF au-dessus du point de l'effet", "[render][effect][shard]") {
  // ⚠ LES DEUX BÂTISSEURS LE FONT, et en MUTANT la position de l'instance. Le
  // poser au sol enterrerait les éclats à moitié.
  EffectFlyingShardPattern pattern;
  pattern.texture = "data\\texture\\effect\\lens1.tga";
  std::uint32_t seed = 3U;
  const ro::render::PooledEffectNode node =
      SowEffectFlyingShardNode(pattern, {2.0F, 1.0F, 3.0F}, 100, seed);
  CHECK(node.shape == ro::render::PooledNodeShape::kFlyingShard);
  CHECK(node.ground_position.y == Approx(1.0F + kFlyingShardBirthHeight));
  CHECK(node.ground_position.x == Approx(2.0F));
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths[0] == "data\\texture\\effect\\lens1.tga");
}

TEST_CASE("la table des eclats est triee et porte deux batisseurs", "[render][effect][shard]") {
  const std::span<const EffectFlyingShardPattern> table = EffectFlyingShardPatternTable();
  REQUIRE(table.size() == 4);
  CHECK(std::ranges::is_sorted(table, {}, &EffectFlyingShardPattern::effect_id));
  CHECK(EffectFlyingShardPatternsFor(262).front().mark == 0);
  CHECK(EffectFlyingShardPatternsFor(266).front().mark == 1);
  CHECK(EffectFlyingShardPatternsFor(276).front().builder == EffectFlyingShardBuilder::kStout);
  CHECK(EffectFlyingShardPatternsFor(889).front().builder == EffectFlyingShardBuilder::kSlender);
  // ⚠ DEUX IDENTIFIANTS PARTAGENT UNE IMAGE SANS PARTAGER LEUR OCTET.
  CHECK(EffectFlyingShardPatternsFor(262).front().texture ==
        EffectFlyingShardPatternsFor(889).front().texture);
  CHECK(EffectFlyingShardPatternsFor(262).front().mark !=
        EffectFlyingShardPatternsFor(889).front().mark);
  CHECK(EffectFlyingShardPatternsFor(1).empty());
}

TEST_CASE("un site seme VINGT noeuds d'un coup, pas un", "[render][effect][shard]") {
  // EF_TEIHIT3 de Raid : quatre-vingts eclats, vingt noeuds de quatre.
  CHECK(EffectFlyingShardPatternsFor(276).front().nodes_per_spawn == 20);
  CHECK(EffectFlyingShardPatternsFor(262).front().nodes_per_spawn == 20);
  CHECK(EffectFlyingShardPatternsFor(266).front().nodes_per_spawn == 24);
  CHECK(EffectFlyingShardPatternsFor(889).front().nodes_per_spawn == 20);
  // ⚠ SEUL EF_TEIHIT1T LIT LES EFFETS SIMPLIFIES, et tombe a six.
  CHECK(EffectFlyingShardPatternsFor(889).front().nodes_when_minimal == 6);
  CHECK(EffectFlyingShardPatternsFor(276).front().nodes_when_minimal == 20);
}
