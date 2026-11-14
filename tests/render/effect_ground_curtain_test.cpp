#include "ro/render/effect_ground_curtain.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <set>
#include <span>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_ground_curtain_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_burst.h"

using Catch::Approx;
using ro::render::AdvanceGroundCurtainTracks;
using ro::render::ComposeGroundCurtainQuad;
using ro::render::EffectGroundCurtainColor;
using ro::render::EffectGroundCurtainPattern;
using ro::render::EffectGroundCurtainPatternsFor;
using ro::render::EffectGroundCurtainPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kGroundCurtainCrossAlpha;
using ro::render::kGroundCurtainCrossHalf;
using ro::render::kGroundCurtainCrossLiveTracks;
using ro::render::kGroundCurtainFastStepDegrees;
using ro::render::kGroundCurtainGreen;
using ro::render::kGroundCurtainHeightAmplitude;
using ro::render::kGroundCurtainHeightMiddle;
using ro::render::kGroundCurtainLatticeAlpha;
using ro::render::kGroundCurtainLatticeHalf;
using ro::render::kGroundCurtainPink;
using ro::render::kGroundCurtainPlain;
using ro::render::kGroundCurtainSalmon;
using ro::render::kGroundCurtainScatterAlpha;
using ro::render::kGroundCurtainStepDegrees;
using ro::render::kGroundCurtainYellow;
using ro::render::SeedEffectGroundCurtainTracks;
using ro::render::SowEffectGroundCurtainNode;

namespace {

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Sown(std::uint8_t octet,
                                                                 std::uint32_t seed = 11U) {
  EffectGroundCurtainPattern pattern;
  pattern.arrangement = octet;
  return SeedEffectGroundCurtainTracks(pattern, seed);
}

// La longueur au sol d'un rectangle debout : la distance entre ses deux pieds.
[[nodiscard]] float GroundLength(const EffectWorldQuad& quad) {
  const glm::vec3 offset = quad.corners[1] - quad.corners[0];
  return std::sqrt(offset.x * offset.x + offset.z * offset.z);
}

}  // namespace

TEST_CASE("la tenture est un rectangle DEBOUT", "[render][effect][curtain]") {
  // ⚠ CE N'EST PAS UN BILLBOARD, malgré le nom du client : rien de son
  // orientation ne vient de la caméra. Ses deux pieds sont au sol, ses deux
  // têtes à la hauteur que la clé donne.
  EffectTrack track;
  track.active = true;
  track.alpha = 100;
  track.lift_key = 20.0F;
  track.anchor = {-6.0F, 0.0F, 0.0F};
  track.tail[3] = 6.0F;
  track.tail[5] = 0.0F;

  const std::optional<EffectWorldQuad> quad = ComposeGroundCurtainQuad(track, {});
  REQUIRE(quad.has_value());
  CHECK(quad->corners[0].y == Approx(0.0F));
  CHECK(quad->corners[1].y == Approx(0.0F));
  // ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : recopier son signe
  // planterait la tenture sous le sol.
  CHECK(quad->corners[2].y == Approx(20.0F));
  CHECK(quad->corners[3].y == Approx(20.0F));
  // Les deux pieds portent bien l'ancre et les deux cases de queue.
  CHECK(quad->corners[0].x == Approx(-6.0F));
  CHECK(quad->corners[1].x == Approx(6.0F));
  CHECK(GroundLength(*quad) == Approx(12.0F));
  // Et les coins font le TOUR : la tête est au-dessus du pied du MÊME côté.
  CHECK(quad->corners[3].x == Approx(quad->corners[0].x));
  CHECK(quad->corners[2].x == Approx(quad->corners[1].x));
}

TEST_CASE("une piste eteinte ou muette ne rend rien", "[render][effect][curtain]") {
  EffectTrack track;
  track.alpha = 100;
  track.lift_key = 20.0F;
  CHECK_FALSE(ComposeGroundCurtainQuad(track, {}).has_value());
  track.active = true;
  track.alpha = 0;
  CHECK_FALSE(ComposeGroundCurtainQuad(track, {}).has_value());
}

TEST_CASE("la case zero choisit la teinte ET le melange", "[render][effect][curtain]") {
  // ⚠ C'EST UNE CASE DE PISTE, PAS UN CHAMP DE NŒUD : la chercher dans le nœud
  // rendrait les cinq marques au sol blanches.
  CHECK(EffectGroundCurtainColor(1.0F) == kGroundCurtainPink);
  CHECK(EffectGroundCurtainColor(2.0F) == kGroundCurtainGreen);
  CHECK(EffectGroundCurtainColor(3.0F) == kGroundCurtainYellow);
  CHECK(EffectGroundCurtainColor(4.0F) == kGroundCurtainSalmon);
  CHECK(EffectGroundCurtainColor(0.0F) == kGroundCurtainPlain);
  CHECK(EffectGroundCurtainColor(9.0F) == kGroundCurtainPlain);

  // ⚠ LES DEUX PREMIERS STYLES SE MÉLANGENT, LES TROIS AUTRES S'AJOUTENT.
  const auto blend_of = [](float style) {
    EffectTrack track;
    track.active = true;
    track.alpha = 100;
    track.lift_key = 20.0F;
    track.tail[3] = 6.0F;
    track.value[0] = style;
    return ComposeGroundCurtainQuad(track, {})->blend_destination;
  };
  CHECK(blend_of(1.0F) == blend_of(2.0F));
  CHECK(blend_of(3.0F) == blend_of(4.0F));
  CHECK(blend_of(0.0F) == blend_of(3.0F));
  CHECK(blend_of(1.0F) != blend_of(3.0F));
}

TEST_CASE("la hauteur RESPIRE au lieu de croitre", "[render][effect][curtain]") {
  // ⚠ CE N'EST PAS UN ÉTAT QUI CROÎT : la clé est RECALCULÉE à chaque tic
  // depuis un sinus, entre dix et trente. L'écrire comme une croissance la
  // ferait monter sans jamais redescendre, et le battement disparaîtrait.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].degrees = 0;
  float bottom = 1e9F;
  float top = -1e9F;
  for (std::int32_t tick = 0; tick < 200; ++tick) {
    AdvanceGroundCurtainTracks(tracks);
    bottom = std::min(bottom, tracks[0].lift_key);
    top = std::max(top, tracks[0].lift_key);
  }
  CHECK(bottom == Approx(kGroundCurtainHeightMiddle - kGroundCurtainHeightAmplitude).margin(0.02F));
  CHECK(top == Approx(kGroundCurtainHeightMiddle + kGroundCurtainHeightAmplitude).margin(0.02F));
  // L'angle reste dans le tour, et le compteur avance.
  CHECK(tracks[0].degrees >= 0);
  CHECK(tracks[0].degrees < 360);
  CHECK(tracks[0].ticks == 200);
}

TEST_CASE("un seul style respire deux fois plus vite", "[render][effect][curtain]") {
  // ⚠ QUATRE DEGRÉS PAR TIC POUR LE STYLE UN, DEUX POUR LES AUTRES. Leur donner
  // la même cadence ferait respirer ensemble des marques que le client
  // distingue.
  const auto after_one_tick = [](float style) {
    std::array<EffectTrack, kEffectTracksPerNode> tracks;
    tracks[0].active = true;
    tracks[0].value[0] = style;
    AdvanceGroundCurtainTracks(tracks);
    return tracks[0].degrees;
  };
  CHECK(after_one_tick(1.0F) == kGroundCurtainFastStepDegrees);
  CHECK(after_one_tick(2.0F) == kGroundCurtainStepDegrees);
  CHECK(after_one_tick(0.0F) == kGroundCurtainStepDegrees);
}

TEST_CASE("une piste eteinte n'avance pas", "[render][effect][curtain]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  AdvanceGroundCurtainTracks(tracks);
  for (const EffectTrack& track : tracks) {
    CHECK(track.ticks == 0);
    CHECK(track.lift_key == Approx(0.0F));
  }
}

TEST_CASE("la gerbe part du centre et reste blanche", "[render][effect][curtain]") {
  // ⚠ LA GERBE NE POSE PAS DE STYLE : sa case zéro garde zéro, et sa tenture
  // est donc BLANCHE. Lui donner l'octet du site la teindrait du premier style.
  const std::array<EffectTrack, kEffectTracksPerNode> spray = Sown(0);
  for (const EffectTrack& track : spray) {
    CHECK(track.active);
    CHECK(track.alpha == kGroundCurtainScatterAlpha);
    CHECK(track.value[0] == Approx(0.0F));
    // Le pied proche est AU CENTRE ; la gerbe part du personnage.
    CHECK(track.anchor.x == Approx(0.0F));
    CHECK(track.anchor.z == Approx(0.0F));
    CHECK(track.tail[3] >= -20.0F);
    CHECK(track.tail[3] < 20.0F);
    CHECK(track.tail[5] >= -20.0F);
    CHECK(track.tail[5] < 20.0F);
  }
  // Et les quatre traits ne partent pas au même endroit.
  std::set<float> bouts;
  for (const EffectTrack& track : spray) {
    bouts.insert(track.tail[3]);
  }
  CHECK(bouts.size() > 1);
}

TEST_CASE("le treillis traverse dans les DEUX sens", "[render][effect][curtain]") {
  // ⚠ LA MOITIÉ DES PISTES TRAVERSE DANS L'AUTRE SENS. Les poser toutes dans le
  // même donnerait quatre traits parallèles au lieu d'un treillis.
  const std::array<EffectTrack, kEffectTracksPerNode> treillis = Sown(2);
  for (const EffectTrack& track : treillis) {
    CHECK(track.active);
    CHECK(track.alpha == kGroundCurtainLatticeAlpha);
    CHECK(track.value[0] == Approx(2.0F));
  }
  // Les deux premières vont d'un bord en z à l'autre, les deux dernières en x.
  CHECK(treillis[0].anchor.z == Approx(kGroundCurtainLatticeHalf));
  CHECK(treillis[0].tail[5] == Approx(-kGroundCurtainLatticeHalf));
  CHECK(treillis[3].anchor.x == Approx(kGroundCurtainLatticeHalf));
  CHECK(treillis[3].tail[3] == Approx(-kGroundCurtainLatticeHalf));
  // ⚠ ET CHAQUE TRAIT EST DÉCALÉ AU SORT, symétriquement : le décalage d'un
  // bout est l'OPPOSÉ de celui de l'autre, si bien que le trait passe par le
  // centre en biais.
  CHECK(treillis[0].anchor.x == Approx(-treillis[0].tail[3]));
  CHECK(treillis[3].anchor.z == Approx(-treillis[3].tail[5]));
}

TEST_CASE("la croix n'allume que deux pistes", "[render][effect][curtain]") {
  // ⚠ DEUX PISTES VIVES SUR QUATRE, et le client écrit quand même la géométrie
  // des quatre. En allumer quatre doublerait la croix.
  const std::array<EffectTrack, kEffectTracksPerNode> cross = Sown(4);
  CHECK(cross[0].active);
  CHECK(cross[1].active);
  CHECK_FALSE(cross[2].active);
  CHECK_FALSE(cross[3].active);
  CHECK(cross[0].alpha == kGroundCurtainCrossAlpha);
  CHECK(cross[0].value[0] == Approx(4.0F));
  // Une branche sur l'axe des x, l'autre sur celui des z.
  CHECK(cross[0].anchor.x == Approx(-kGroundCurtainCrossHalf));
  CHECK(cross[0].tail[3] == Approx(kGroundCurtainCrossHalf));
  CHECK(cross[0].anchor.z == Approx(0.0F));
  CHECK(cross[1].anchor.z == Approx(kGroundCurtainCrossHalf));
  CHECK(cross[1].tail[5] == Approx(-kGroundCurtainCrossHalf));
  CHECK(cross[1].anchor.x == Approx(0.0F));
  CHECK(kGroundCurtainCrossLiveTracks == 2);
}

TEST_CASE("un octet inconnu ne seme RIEN", "[render][effect][curtain]") {
  // ⚠ LE CLIENT SAUTE LA PISTE ENTIÈRE — pas même son drapeau. Retomber sur la
  // gerbe inventerait une figure qu'aucun site ne demande.
  for (const EffectTrack& track : Sown(7)) {
    CHECK_FALSE(track.active);
    CHECK(track.alpha == 0);
  }
}

TEST_CASE("les angles de depart sont tires au sort", "[render][effect][curtain]") {
  // C'est ce qui décale la respiration des quatre traits ; les semer égaux les
  // ferait battre ensemble.
  std::set<std::int16_t> angles;
  for (const EffectTrack& track : Sown(2)) {
    angles.insert(track.degrees);
    CHECK(track.degrees >= 0);
    CHECK(track.degrees < 360);
  }
  CHECK(angles.size() > 1);
}

TEST_CASE("la table des tentures est triee et porte cinq marques", "[render][effect][curtain]") {
  const std::span<const EffectGroundCurtainPattern> table = EffectGroundCurtainPatternTable();
  REQUIRE(table.size() == 5);
  CHECK(std::ranges::is_sorted(table, {}, &EffectGroundCurtainPattern::effect_id));
  // ⚠ DEUX IDENTIFIANTS PARTAGENT UNE IMAGE SANS PARTAGER LEUR OCTET : ranger
  // la famille par image les confondrait.
  REQUIRE_FALSE(EffectGroundCurtainPatternsFor(287).empty());
  REQUIRE_FALSE(EffectGroundCurtainPatternsFor(290).empty());
  CHECK(EffectGroundCurtainPatternsFor(287).front().texture ==
        EffectGroundCurtainPatternsFor(290).front().texture);
  CHECK(EffectGroundCurtainPatternsFor(287).front().arrangement == 1);
  CHECK(EffectGroundCurtainPatternsFor(290).front().arrangement == 0);
  CHECK(EffectGroundCurtainPatternsFor(1).empty());
}

TEST_CASE("un noeud de tenture porte une image et sa forme", "[render][effect][curtain]") {
  EffectGroundCurtainPattern pattern;
  pattern.texture = "data\\texture\\effect\\ring_red.tga";
  pattern.arrangement = 1;
  std::uint32_t seed = 4U;
  const ro::render::PooledEffectNode node =
      SowEffectGroundCurtainNode(pattern, {1.0F, 0.0F, 2.0F}, 100, seed);
  CHECK(node.shape == ro::render::PooledNodeShape::kGroundCurtain);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths[0] == "data\\texture\\effect\\ring_red.tga");
  CHECK(node.owner_position.z == Approx(2.0F));
}
