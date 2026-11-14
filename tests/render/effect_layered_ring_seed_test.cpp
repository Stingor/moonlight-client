#include "ro/render/effect_layered_ring_seed.h"

#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"

using ro::render::EffectLayeredRingNodePattern;
using ro::render::EffectLayeredRingNodePatternsFor;
using ro::render::EffectLayeredRingNodePatternTable;
using ro::render::EffectLayeredRingTrackSeed;
using ro::render::EffectLayeredRingTrackSeedTable;
using ro::render::kEffectTracksPerNode;
using ro::render::SowEffectLayeredRingNode;

namespace {
constexpr std::uint32_t kFirstEffect = 54;
}  // namespace
using ro::render::SeedEffectLayeredRingTracks;

// ⚠ CE SOUS-TYPE NE SE MONTRE PAS ENCORE, ET C'EST VOULU. Sa figure est portée,
// son semis est porté et vérifié ci-dessous — mais son MOUVEMENT ne l'est pas.
// La forme `kRingTracks` lie la figure et le mouvement, et celui qu'elle porte
// transcrit le cas 30 du répartiteur de mouvement, qui donne à chaque
// sous-type sa propre cible. Le prêter au 23 ferait
// une figure juste qui bouge faux, et l'on croirait l'effet traité.
//
// Ce que ces tests gardent, c'est donc la MESURE — et le silence tant qu'elle
// n'est pas complète.

TEST_CASE("Anneau en couches : les pistes d'un bâtisseur sont TOUTES DIFFÉRENTES",
          "[render][table]") {
  // C'est ce qui sépare cette famille de celle du sous-type 30, dont les quatre
  // pistes partagent rayon et inclinaison. Les semer identiques donnerait un
  // seul anneau épais au lieu de couronnes emboîtées.
  //
  // ⚠ LA PROPRIÉTÉ VAUT PAR TRANCHE, ET NON SUR LA TABLE ENTIÈRE. Deux
  // bâtisseurs y rangent leurs pistes bout à bout ; exiger que les sept soient
  // distinctes serait exiger des deux qu'ils ne se croisent jamais, ce que rien
  // ne promet.
  std::set<std::size_t> seen_slices;
  for (const EffectLayeredRingNodePattern& pattern : EffectLayeredRingNodePatternTable()) {
    if (!seen_slices.insert(pattern.first_track).second) {
      continue;
    }
    const std::span<const EffectLayeredRingTrackSeed> seeds = EffectLayeredRingTracksFor(pattern);
    INFO(pattern.client_function);
    REQUIRE(!seeds.empty());
    REQUIRE(seeds.size() <= kEffectTracksPerNode);
    std::set<float> radii;
    std::set<std::int16_t> inclinaisons;
    for (const EffectLayeredRingTrackSeed& seed : seeds) {
      radii.insert(seed.radius_at_anchor);
      inclinaisons.insert(seed.tilt_degrees);
      CHECK(seed.radius_at_anchor > 0.0F);
    }
    CHECK(radii.size() == seeds.size());
    CHECK(inclinaisons.size() == seeds.size());
  }
  CHECK(seen_slices.size() == 2);
}

TEST_CASE("Anneau en couches : la QUATRIÈME couronne est à part", "[render][table]") {
  // Les trois premières tournent depuis 315 degrés et brillent à 180 ; la
  // dernière part de 360, se couche presque à plat et ne monte qu'à 70. C'est
  // elle le halo, et l'aligner sur les autres le ferait disparaître dedans.
  //
  // ⚠ ET LE SECOND BÂTISSEUR N'EN A PAS. Il éteint sa quatrième piste : ses
  // trois couronnes sont les sœurs des trois premières de l'autre, sans le halo.
  // C'est pourquoi ce test cherche la tranche de QUATRE et ne juge qu'elle.
  const EffectLayeredRingNodePattern* with_four = nullptr;
  for (const EffectLayeredRingNodePattern& pattern : EffectLayeredRingNodePatternTable()) {
    if (pattern.track_count == kEffectTracksPerNode) {
      with_four = &pattern;
      break;
    }
  }
  REQUIRE(with_four != nullptr);
  const std::span<const EffectLayeredRingTrackSeed> seeds = EffectLayeredRingTracksFor(*with_four);
  REQUIRE(seeds.size() == kEffectTracksPerNode);
  for (std::size_t rang = 0; rang + 1 < seeds.size(); ++rang) {
    INFO("piste " << rang);
    CHECK(seeds[rang].alpha == seeds.front().alpha);
    CHECK(seeds[rang].degrees == seeds.front().degrees);
  }
  CHECK(seeds.back().alpha < seeds.front().alpha);
  CHECK(seeds.back().degrees != seeds.front().degrees);
}

TEST_CASE("Anneau en couches : les pistes se sèment VIVES et sans aire", "[render][pool]") {
  for (const EffectLayeredRingNodePattern& pattern : EffectLayeredRingNodePatternTable()) {
    const std::span<const EffectLayeredRingTrackSeed> seeds = EffectLayeredRingTracksFor(pattern);
    const std::array<ro::render::EffectTrack, kEffectTracksPerNode> tracks =
        SeedEffectLayeredRingTracks(seeds);
    for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
      INFO("effet " << pattern.effect_id << ", piste " << rang);
      // ⚠ AU-DELÀ DE LA TRANCHE, LA PISTE RESTE ÉTEINTE. Le second bâtisseur
      // éteint sa quatrième explicitement ; l'allumer sans valeurs donnerait une
      // couronne de rayon nul de plus.
      if (rang >= seeds.size()) {
        CHECK_FALSE(tracks[rang].active);
        continue;
      }
      CHECK(tracks[rang].active);
      CHECK(tracks[rang].radius_at_anchor == Catch::Approx(seeds[rang].radius_at_anchor));
      CHECK(tracks[rang].tilt_degrees == seeds[rang].tilt_degrees);
      CHECK(tracks[rang].value[0] == Catch::Approx(0.0F));
    }
  }
}

TEST_CASE("Anneau en couches : le second bâtisseur en sème TROIS, plus serrées",
          "[render][table]") {
  // ⚠ TROIS IDENTIFIANTS PASSENT PAR UN SECOND BÂTISSEUR — 200, 397 et 679. Ses
  // pistes n'ont aucune valeur commune avec celles du premier : des rayons de
  // 3,9 à 4,3 là où l'autre va de 4 à 5,5, et des clés de 11 à 15 là où l'autre
  // va de 19 à 250. Les confondre aurait donné à ces trois-là une figure qui
  // n'est pas la leur.
  for (const std::uint32_t effect_id : {200U, 397U, 679U}) {
    const std::span<const EffectLayeredRingNodePattern> own_entries =
        EffectLayeredRingNodePatternsFor(effect_id);
    INFO("effet " << effect_id);
    REQUIRE(own_entries.size() == 1);
    CHECK(own_entries.front().track_count == 3);
    const std::span<const EffectLayeredRingTrackSeed> seeds =
        EffectLayeredRingTracksFor(own_entries.front());
    REQUIRE(seeds.size() == 3);
    for (const EffectLayeredRingTrackSeed& seed : seeds) {
      CHECK(seed.alpha == 120);
      CHECK(seed.degrees == 315);
      CHECK(seed.radius_at_anchor < 5.0F);
    }
    // Les azimuts s'écartent d'un quart puis d'un demi-tour : trois couronnes
    // qui ne se recouvrent pas.
    CHECK(seeds[0].azimuth_degrees == 0);
    CHECK(seeds[1].azimuth_degrees == 90);
    CHECK(seeds[2].azimuth_degrees == 180);
  }

  // ⚠ ET LEUR VIE N'EST PAS MESURÉE, à la différence de celle du premier
  // bâtisseur : celui-ci n'en pose aucune, et le nœud garde celle du
  // constructeur du client. Le dire dans un test empêche qu'on la prenne un jour
  // pour une valeur du bâtisseur.
  CHECK(EffectLayeredRingNodePatternsFor(200).front().life_ticks ==
        ro::render::kEffectNodeDefaultLifeTicks);
  CHECK(EffectLayeredRingNodePatternsFor(54).front().life_ticks ==
        ro::render::kEffectLayeredRingLifeTicks);
}

TEST_CASE("Anneau en couches : les nœuds REVIENNENT, le mouvement étant porté", "[render][table]") {
  // ⚠ CE TEST A REMPLACÉ UN GARDE-SILENCE, et c'est ainsi que le silence devait
  // finir. La table est restée vide tant que le mouvement du sous-type 23
  // n'était pas transcrit ; elle se remplit maintenant que
  // `AdvanceParticleConeSpawnTracks` le porte.
  const std::span<const EffectLayeredRingNodePattern> table = EffectLayeredRingNodePatternTable();
  REQUIRE(!table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id <= table[rang].effect_id);
  }
  for (const EffectLayeredRingNodePattern& pattern : table) {
    INFO("effet " << pattern.effect_id);
    CHECK(pattern.texture.starts_with("data\\texture\\"));
  }
}

TEST_CASE("Anneau en couches : le nœud prend la forme du CÔNE, pas celle de l'anneau",
          "[render][pool]") {
  // ⚠ MÊME FIGURE, AUTRE MOUVEMENT. Lui donner `kRingTracks` le ferait onduler
  // comme un anneau du sous-type 30 : une figure juste qui bouge faux. C'est la
  // faute que cette forme-ci répare.
  const std::span<const EffectLayeredRingNodePattern> patterns =
      EffectLayeredRingNodePatternsFor(kFirstEffect);
  REQUIRE(patterns.size() == 1);
  const ro::render::PooledEffectNode node =
      SowEffectLayeredRingNode(patterns.front(), {3.0F, 0.0F, 4.0F});
  CHECK(node.shape == ro::render::PooledNodeShape::kParticleConeRings);
  CHECK(node.node.life_ticks == ro::render::kEffectLayeredRingLifeTicks);
  CHECK(node.trail_variant == patterns.front().trail_variant);
}

TEST_CASE("Anneau en couches : la troupe OUVRE le cône", "[render][pool]") {
  const std::span<const EffectLayeredRingNodePattern> patterns =
      EffectLayeredRingNodePatternsFor(kFirstEffect);
  REQUIRE_FALSE(patterns.empty());
  ro::render::EffectNodePool pool;
  pool.Add(SowEffectLayeredRingNode(patterns.front(), {}));
  float plus_grande = 0.0F;
  for (std::int32_t tick = 0; tick < ro::render::kEffectLayeredRingLifeTicks; ++tick) {
    pool.Advance(1000 / ro::render::kEffectTicksPerSecond);
    if (pool.nodes().empty()) {
      break;
    }
    for (const ro::render::EffectTrack& track : pool.nodes().front().tracks) {
      plus_grande = std::max(plus_grande, track.value[10]);
    }
  }
  // ⚠ SANS LE MOUVEMENT, LE CÔNE N'A PAS D'AIRE : ses segments naissent nuls.
  CHECK(plus_grande > 0.0F);
}
