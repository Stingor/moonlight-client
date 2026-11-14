#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_growing_sprite_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_track_motion.h"

using Catch::Approx;
using ro::render::AdvanceGrowingSpriteTracks;
using ro::render::EffectGrowingSpritePattern;
using ro::render::EffectGrowingSpritePatternsFor;
using ro::render::EffectGrowingSpritePatternTable;
using ro::render::EffectGrowingSpriteVariantFor;
using ro::render::EffectTrack;
using ro::render::kEffectTracksPerNode;
using ro::render::kSlotOfGrowingSpriteHeight;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectGrowingSpriteTracks;
using ro::render::SowEffectGrowingSpriteNode;

namespace {

constexpr std::uint32_t kStasis = 799;
// LA VARIANTE QUI MONTE VITE, et le style qui la pose.
constexpr std::uint8_t kFastVariant = 113;
constexpr std::uint8_t kPullingStyle = 3;

[[nodiscard]] EffectGrowingSpritePattern Pattern(std::int32_t delay, std::uint8_t style) {
  return {kStasis, "Effect_Build_SpriteFromParam58", "data\\texture\\test.bmp", delay, style};
}

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Sow(std::int32_t delay,
                                                                std::uint8_t style) {
  std::uint32_t seed = 1U;
  return SeedEffectGrowingSpriteTracks(Pattern(delay, style), seed);
}

}  // namespace

TEST_CASE("la table echelonne six boules par leur retard", "[render][growing_sprite]") {
  const std::span<const EffectGrowingSpritePattern> table = EffectGrowingSpritePatternTable();
  REQUIRE_FALSE(table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id <= table[rang].effect_id);
  }

  const std::span<const EffectGrowingSpritePattern> foudre =
      EffectGrowingSpritePatternsFor(kStasis);
  REQUIRE(foudre.size() == 6);
  // ⚠ LES RETARDS SONT NÉGATIFS ET RÉGULIERS : de moins dix à moins trente, par
  // pas de quatre. Un relevé lu sans signe les rendrait tous immenses et positifs,
  // donc jamais visibles.
  for (std::size_t rang = 0; rang < foudre.size(); ++rang) {
    CHECK(foudre[rang].delay_ticks == -10 - 4 * static_cast<std::int32_t>(rang));
    CHECK(foudre[rang].builder_argument == kPullingStyle);
  }
  // ET SIX IMAGES DISTINCTES : c'est une animation, pas six copies.
  CHECK(foudre[0].texture != foudre[1].texture);
}

TEST_CASE("les cinq variantes ne se deduisent pas du style", "[render][growing_sprite]") {
  CHECK(EffectGrowingSpriteVariantFor(1) == 101);
  CHECK(EffectGrowingSpriteVariantFor(2) == 102);
  CHECK(EffectGrowingSpriteVariantFor(3) == 111);
  CHECK(EffectGrowingSpriteVariantFor(4) == kFastVariant);
  CHECK(EffectGrowingSpriteVariantFor(10) == 114);
  // ⚠ UN STYLE QUE LE CLIENT NE NOMME PAS NE POSE RIEN : son `switch` le laisse
  // passer, et la variante reste celle du constructeur — UN, pas zéro.
  CHECK(EffectGrowingSpriteVariantFor(5) == 1);
  CHECK(EffectGrowingSpriteVariantFor(0) == 1);
}

TEST_CASE("le semis n'allume qu'une piste, et elle part eteinte", "[render][growing_sprite]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = Sow(-10, kPullingStyle);
  CHECK(tracks[0].active);
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[0].ticks == -10);
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    CHECK_FALSE(tracks[rang].active);
  }
  // LE STYLE QUI TIRE PART D'UN RAYON PLUS PETIT que ses voisins.
  CHECK(tracks[0].radius_at_anchor == Approx(13.0F));
  CHECK(Sow(0, 4)[0].radius_at_anchor == Approx(15.0F));
}

TEST_CASE("le style trois tire sa hauteur, les autres non", "[render][growing_sprite]") {
  // ⚠ CINQ UNITÉS PLUS UN TIRAGE SUR SIX : la hauteur reste dans la fourchette,
  // et c'est ce qui disperse les six boules au lieu de les empiler.
  for (std::uint32_t seed = 1U; seed < 40U; ++seed) {
    std::uint32_t courante = seed;
    const float height = SeedEffectGrowingSpriteTracks(Pattern(-10, kPullingStyle), courante)
                             .front()
                             .value[kSlotOfGrowingSpriteHeight];
    CHECK(height <= -5.0F);
    CHECK(height >= -10.0F);
  }
  // LES AUTRES STYLES LA POSENT, et elle ne bouge pas d'une graine à l'autre.
  std::uint32_t seed = 7U;
  CHECK(SeedEffectGrowingSpriteTracks(Pattern(0, 4), seed)
            .front()
            .value[kSlotOfGrowingSpriteHeight] == Approx(-5.0F));
}

TEST_CASE("une piste retardee ne fait rien, pas meme enfler", "[render][growing_sprite]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = Sow(-3, kPullingStyle);
  const float radius = tracks[0].radius_at_anchor;
  for (std::int32_t tick = 0; tick < 3; ++tick) {
    AdvanceGrowingSpriteTracks(tracks, 111);
    CHECK(tracks[0].alpha == 0);
    CHECK(tracks[0].radius_at_anchor == Approx(radius));
  }
  // AU QUATRIÈME TIC le compteur passe à un, et tout commence.
  AdvanceGrowingSpriteTracks(tracks, 111);
  CHECK(tracks[0].ticks == 1);
  CHECK(tracks[0].alpha == 2);
  CHECK(tracks[0].radius_at_anchor > radius);
}

TEST_CASE("la variante cent treize parait trois fois plus vite", "[render][growing_sprite]") {
  std::array<EffectTrack, kEffectTracksPerNode> slow = Sow(0, 1);
  std::array<EffectTrack, kEffectTracksPerNode> fast = Sow(0, 4);
  for (std::int32_t tick = 0; tick < 10; ++tick) {
    AdvanceGrowingSpriteTracks(slow, 101);
    AdvanceGrowingSpriteTracks(fast, kFastVariant);
  }
  CHECK(slow[0].alpha == 20);
  CHECK(fast[0].alpha == 60);
  // ⚠ ET LES DEUX ONT ENFLÉ PAREIL : le rayon ne dépend pas de la variante.
  CHECK(slow[0].radius_at_anchor == Approx(fast[0].radius_at_anchor));
}

TEST_CASE("le rayon est multiplie, non augmente", "[render][growing_sprite]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = Sow(0, 4);
  const float start = tracks[0].radius_at_anchor;
  AdvanceGrowingSpriteTracks(tracks, kFastVariant);
  const float after_one = tracks[0].radius_at_anchor;
  AdvanceGrowingSpriteTracks(tracks, kFastVariant);
  const float after_two = tracks[0].radius_at_anchor;
  // Le deuxième pas est PLUS GRAND que le premier : c'est la signature d'un
  // produit. Une addition les rendrait égaux.
  CHECK(after_two - after_one > after_one - start);
  CHECK(after_two == Approx(start * 1.07F * 1.07F));
}

TEST_CASE("passe dix tics, l'alpha redescend et le rayon enfle encore",
          "[render][growing_sprite]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = Sow(0, 4);
  for (std::int32_t tick = 0; tick < 10; ++tick) {
    AdvanceGrowingSpriteTracks(tracks, kFastVariant);
  }
  const std::int16_t vertex = tracks[0].alpha;
  const float radius = tracks[0].radius_at_anchor;
  AdvanceGrowingSpriteTracks(tracks, kFastVariant);
  CHECK(tracks[0].alpha == vertex - 1);
  // ⚠ LES DEUX RÈGLES NE SONT PAS EXCLUSIVES : le rayon enfle aussi après le
  // dixième tic. N'en garder qu'une par tic ferait grandir le sprite deux fois
  // moins vite.
  CHECK(tracks[0].radius_at_anchor > radius);

  // L'ALPHA S'ARRÊTE À ZÉRO, il ne passe pas en négatif.
  for (std::int32_t tick = 0; tick < 200; ++tick) {
    AdvanceGrowingSpriteTracks(tracks, kFastVariant);
  }
  CHECK(tracks[0].alpha == 0);
}

TEST_CASE("le noeud seme porte sa forme et sa variante", "[render][growing_sprite]") {
  std::uint32_t seed = 3U;
  const ro::render::PooledEffectNode node =
      SowEffectGrowingSpriteNode(Pattern(-10, 4), glm::vec3{1.0F, 2.0F, 3.0F}, 60, seed);
  CHECK(node.shape == PooledNodeShape::kGrowingSprite);
  CHECK(node.trail_variant == kFastVariant);
  CHECK(node.tracks.front().ticks == -10);
  REQUIRE(node.resource_paths.size() == 1);
}
