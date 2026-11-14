#include "ro/render/effect_glow_sprite_seed.h"

#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_trail_history.h"

using ro::render::EffectGlowSpriteNodePattern;
using ro::render::EffectGlowSpriteNodePatternsFor;
using ro::render::EffectGlowSpriteNodePatternTable;
using ro::render::EffectGlowSpriteStyleSeed;
using ro::render::EffectGlowSpriteStyleSeedFor;
using ro::render::EffectGlowSpriteStyleSeedTable;
using ro::render::EffectNodePool;
using ro::render::PooledEffectNode;
using ro::render::PooledNodeShape;
using ro::render::SowEffectGlowSpriteNode;

namespace {

constexpr std::size_t kMarkOfDrawing = 0;
constexpr float kRadians = 3.14159265F / 180.0F;
// Le premier effet de la table, et un style qu'aucun site n'emploie.
constexpr std::uint32_t kFirstEffect = 853;
constexpr std::uint8_t kStyleNobodyCalls = 200;
// Trois styles qui partagent le MÊME monstre — le client les distingue par leur
// marque de dessin, pas par leur sprite.
constexpr std::uint8_t kFirstSharedStyle = 10;
constexpr std::uint8_t kLastSharedStyle = 12;
// La vie d'instance de l'effet 853.
constexpr std::int32_t kLifeOf853 = 400;

// UN SEMIS AU PREMIER TIC, sans monde : le nœud garde la hauteur de l'instance.
PooledEffectNode Sow(const EffectGlowSpriteNodePattern& pattern, glm::vec3 place,
                     std::int32_t tick = 0) {
  std::uint32_t generator = 1;
  return SowEffectGlowSpriteNode(pattern, place, tick, kLifeOf853, {}, generator);
}

}  // namespace

TEST_CASE("Monstres : un style sème un nœud PAR TIC jusqu'à son seuil", "[render][pool]") {
  CHECK(ro::render::EffectGlowSpriteSowingTicks(0) == 50);
  CHECK(ro::render::EffectGlowSpriteSowingTicks(1) == 50);
  CHECK(ro::render::EffectGlowSpriteSowingTicks(2) == 10);
  CHECK(ro::render::EffectGlowSpriteSowingTicks(8) == 10);
  CHECK(ro::render::EffectGlowSpriteSowingTicks(9) == 20);
  CHECK(ro::render::EffectGlowSpriteSowingTicks(12) == 20);
  CHECK(ro::render::EffectGlowSpriteSowingTicks(13) == 30);
  CHECK(ro::render::EffectGlowSpriteSowingTicks(14) == 30);
  CHECK(ro::render::EffectGlowSpriteSowingTicks(15) == 20);
}

TEST_CASE("Monstres : la ronde du style deux tourne de trente-six degrés par tic à quarante",
          "[render][pool]") {
  std::uint32_t generator = 7;
  const ro::render::EffectGlowSpriteDraws at_tick_one =
      ro::render::DrawEffectGlowSprite(2, 1, generator);
  CHECK(at_tick_one.radius == Catch::Approx(40.0F));
  CHECK(at_tick_one.heading_degrees == 36);
  CHECK(at_tick_one.sprite_scale == Catch::Approx(0.6F));
  CHECK(at_tick_one.frame_hold_ticks >= 6);
  CHECK(at_tick_one.frame_hold_ticks <= 8);
  CHECK(at_tick_one.action >= 0);
  CHECK(at_tick_one.action < 8);

  // Le nœud naît à l'écart `R·sin(cap)`, `R·cos(cap)` de l'instance dans le
  // repère du client, sur le sol que le monde lui donne : la profondeur se nie
  // chez nous, sans quoi la ronde tourne en miroir.
  const EffectGlowSpriteNodePattern pattern{866, "", 2};
  ro::render::EffectScatterWorld world;
  world.ground_height = [](float, float) { return 7.0F; };
  std::uint32_t other = 7;
  const PooledEffectNode node =
      SowEffectGlowSpriteNode(pattern, {100.0F, 0.0F, 100.0F}, 1, kLifeOf853, world, other);
  CHECK(node.ground_position.x == Catch::Approx(100.0F + (40.0F * std::sin(36.0F * kRadians))));
  CHECK(node.ground_position.z == Catch::Approx(100.0F - (40.0F * std::cos(36.0F * kRadians))));
  CHECK(node.ground_position.y == Catch::Approx(7.0F));
  CHECK(node.stays_where_sown);
  // IL VIT CE QUI RESTE À SON INSTANCE.
  CHECK(node.node.life_ticks == kLifeOf853 - 1);
  CHECK(node.node.sprite_scale == Catch::Approx(0.6F));
}

TEST_CASE("Monstres : les échelles et les rayons suivent l'âge pour les styles 9, 13, 14 et 15",
          "[render][pool]") {
  std::uint32_t generator = 3;
  CHECK(ro::render::DrawEffectGlowSprite(9, 1, generator).radius == Catch::Approx(24.0F));
  CHECK(ro::render::DrawEffectGlowSprite(9, 2, generator).sprite_scale == Catch::Approx(0.85F));
  CHECK(ro::render::DrawEffectGlowSprite(13, 5, generator).radius == Catch::Approx(22.0F));
  CHECK(ro::render::DrawEffectGlowSprite(14, 3, generator).sprite_scale == Catch::Approx(0.5F));
  // LE DIABLOTIN DU STYLE QUINZE est trois à quatre fois plus grand que les
  // autres : de deux à deux et demi aux tics impairs.
  const ro::render::EffectGlowSpriteDraws diablotin =
      ro::render::DrawEffectGlowSprite(15, 1, generator);
  CHECK(diablotin.sprite_scale >= 2.0F);
  CHECK(diablotin.sprite_scale <= 2.5F + 1e-4F);
  CHECK(diablotin.frame_hold_ticks == 4);
  CHECK(ro::render::EffectGlowSpriteRenderWord(15) == 5);
  CHECK(ro::render::EffectGlowSpriteRenderWord(2) != 5);
}

TEST_CASE("Monstres : la succube tire sa base d'action parmi trois", "[render][pool]") {
  for (std::uint32_t seed = 1; seed < 40; ++seed) {
    std::uint32_t generator = seed;
    const std::int32_t action = ro::render::DrawEffectGlowSprite(4, 0, generator).action;
    INFO("graine " << seed);
    CHECK(action >= 40);
    CHECK(action < 64);
  }
  std::uint32_t generator = 1;
  CHECK(ro::render::DrawEffectGlowSprite(0, 0, generator).action == 0);
  CHECK(ro::render::DrawEffectGlowSprite(14, 0, generator).action == 0);
}

TEST_CASE("Monstres : la table des nœuds est TRIÉE par identifiant", "[render][table]") {
  const std::span<const EffectGlowSpriteNodePattern> table = EffectGlowSpriteNodePatternTable();
  REQUIRE(!table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id <= table[rang].effect_id);
  }
}

TEST_CASE("Monstres : tout nœud nomme un style porté, et tout style un sprite", "[render][table]") {
  for (const EffectGlowSpriteNodePattern& pattern : EffectGlowSpriteNodePatternTable()) {
    INFO("effet " << pattern.effect_id);
    CHECK(EffectGlowSpriteStyleSeedFor(pattern.style) != nullptr);
  }
  CHECK(EffectGlowSpriteStyleSeedFor(kStyleNobodyCalls) == nullptr);
  for (const EffectGlowSpriteStyleSeed& seed : EffectGlowSpriteStyleSeedTable()) {
    INFO("style " << static_cast<int>(seed.style));
    // ⚠ LE NOM EST SANS EXTENSION : le couple .spr/.act la porte, pas la table.
    // Une extension ici voudrait dire qu'on a séparé les deux fichiers.
    CHECK_FALSE(seed.sprite_base.empty());
    CHECK(seed.sprite_base.find(".spr") == std::string_view::npos);
    CHECK(seed.sprite_base.find(".act") == std::string_view::npos);
  }
}

TEST_CASE("Monstres : trois styles partagent un sprite et diffèrent par leur MARQUE",
          "[render][table]") {
  // C'est ce que le relevé montre, et c'est ce qui rend la marque nécessaire :
  // sans elle, ces trois-là seraient le même effet.
  const EffectGlowSpriteStyleSeed* first = EffectGlowSpriteStyleSeedFor(kFirstSharedStyle);
  const EffectGlowSpriteStyleSeed* last = EffectGlowSpriteStyleSeedFor(kLastSharedStyle);
  REQUIRE(first != nullptr);
  REQUIRE(last != nullptr);
  CHECK(first->sprite_base == last->sprite_base);
  CHECK(first->draw_mark != last->draw_mark);
}

TEST_CASE("Monstres : le nœud semé est un SPRITE, éteint, à UN cran de traînée", "[render][pool]") {
  const std::span<const EffectGlowSpriteNodePattern> patterns =
      EffectGlowSpriteNodePatternsFor(kFirstEffect);
  REQUIRE(patterns.size() == 1);
  const ro::render::PooledEffectNode node = Sow(patterns.front(), {3.0F, 0.0F, 4.0F});
  CHECK(node.shape == PooledNodeShape::kGlowSprite);
  CHECK(node.resource_paths.size() == 1);
  // ⚠ L'ALPHA NAÎT NUL : le monstre PARAÎT, il n'apparaît pas d'un coup.
  CHECK(node.node.alpha == Catch::Approx(0.0F));
  // ⚠ ET LA TRAÎNÉE NE FAIT QU'UN CRAN. Le constructeur de nœud du client la
  // pose à un, et ce bâtisseur ne la change pas : lui en donner quatre
  // montrerait une rémanence que le client n'a pas.
  CHECK(node.trail.size() == 1);
  CHECK(node.tracks.front().active);
  CHECK(node.tracks.front().mark[kMarkOfDrawing] ==
        EffectGlowSpriteStyleSeedFor(patterns.front().style)->draw_mark);
}

TEST_CASE("Monstres : la troupe fait PARAÎTRE puis DISPARAÎTRE le monstre", "[render][pool]") {
  const std::span<const EffectGlowSpriteNodePattern> patterns =
      EffectGlowSpriteNodePatternsFor(kFirstEffect);
  REQUIRE_FALSE(patterns.empty());
  EffectNodePool pool;
  pool.Add(Sow(patterns.front(), {}));

  float highest = 0.0F;
  for (std::int32_t tick = 0; tick < 60; ++tick) {
    pool.Advance(1000 / ro::render::kEffectTicksPerSecond);
    if (!pool.nodes().empty()) {
      highest = std::max(highest, pool.nodes().front().node.alpha);
    }
  }
  CHECK(highest > 0.0F);
  // Et le cran de traînée porte bien cet alpha : c'est lui que la fenêtre lit.
  REQUIRE_FALSE(pool.nodes().empty());
  CHECK(pool.nodes().front().trail.front().alpha == Catch::Approx(highest));
}

TEST_CASE("Monstres : la forme est DÉCLARÉE comme dessinant un sprite", "[render][pool]") {
  // ⚠ CE TEST GARDE UNE LISTE TENUE À LA MAIN, et il existe parce qu'elle a
  // menti. `EffectNodeShapeDrawsASprite` énumère les formes que la fenêtre
  // cherche parmi les SPRITES ; celle-ci en était absente, si bien que ses
  // quinze identifiants semaient leur nœud, cherchaient leur `.spr` dans le
  // cache des TEXTURES, s'y comptaient « sans texture » et ne paraissaient
  // jamais. Rien ne s'en plaignait : la troupe vivait, le mouvement tournait,
  // l'alpha montait — et l'écran restait vide.
  //
  // Le semis pose bien un chemin de sprite, ce qui suffit à le prouver : une
  // forme dont la ressource est un `.spr` et que cette liste ignore ne se
  // dessine pas.
  const std::span<const EffectGlowSpriteNodePattern> patterns =
      EffectGlowSpriteNodePatternsFor(kFirstEffect);
  REQUIRE_FALSE(patterns.empty());
  const PooledEffectNode node = Sow(patterns.front(), {});
  REQUIRE_FALSE(node.resource_paths.empty());
  CHECK(node.resource_paths.front().starts_with("data\\sprite\\"));
  CHECK(ro::render::EffectNodeShapeDrawsASprite(node.shape));
}
