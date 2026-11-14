#include "ro/render/effect_fading_sprite.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_fading_sprite_seed.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_render_word.h"

using Catch::Approx;
using ro::render::AdvanceFadingSpriteNode;
using ro::render::EffectFadingSpritePattern;
using ro::render::EffectFadingSpritePatternsFor;
using ro::render::EffectFadingSpritePatternTable;
using ro::render::EffectNodeScheduleSowsAt;
using ro::render::kFadingSpriteBody;
using ro::render::kFadingSpriteBodyCeiling;
using ro::render::kFadingSpriteFrameFirstTick;
using ro::render::kFadingSpriteLastFrame;
using ro::render::kFadingSpriteMarkOfBirthTick;
using ro::render::kFadingSpriteMarkOfStyle;
using ro::render::kFadingSpriteRebornCeiling;
using ro::render::kFadingSpriteRisePerTick;
using ro::render::kFadingSpriteWings;
using ro::render::PooledEffectNode;
using ro::render::SowEffectFadingSpriteNode;

namespace {

constexpr std::uint32_t kAngel2 = 338;
constexpr std::uint32_t kAngel3 = 582;
constexpr std::int32_t kLife = 300;

[[nodiscard]] const EffectFadingSpritePattern& PatternOfStyle(std::uint32_t effect_id,
                                                              std::uint8_t style) {
  const std::span<const EffectFadingSpritePattern> patterns =
      EffectFadingSpritePatternsFor(effect_id);
  const auto found = std::ranges::find(patterns, style, &EffectFadingSpritePattern::style);
  REQUIRE(found != patterns.end());
  return *found;
}

[[nodiscard]] PooledEffectNode AfterTicks(const EffectFadingSpritePattern& pattern,
                                          std::int32_t birth, std::int32_t count) {
  PooledEffectNode node = SowEffectFadingSpriteNode(pattern, {0.0F, 10.0F, 0.0F}, kLife, birth);
  for (std::int32_t tick = 0; tick < count; ++tick) {
    AdvanceFadingSpriteNode(node);
  }
  return node;
}

}  // namespace

TEST_CASE("la table porte un appel par sprite, pas un par effet", "[render][fading_sprite]") {
  const std::span<const EffectFadingSpritePattern> table = EffectFadingSpritePatternTable();
  CHECK(table.size() == 6);
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id <= table[rang].effect_id);
  }
  // ⚠ QUATRE APPELS POUR UN SEUL EFFET : l'ange, ses ailes et deux plumes.
  CHECK(EffectFadingSpritePatternsFor(kAngel2).size() == 4);
  // ET LES QUATRE NE PORTENT PAS LE MÊME SPRITE.
  std::array<std::string_view, 4> sprites{};
  std::size_t rang = 0;
  for (const EffectFadingSpritePattern& pattern : EffectFadingSpritePatternsFor(kAngel2)) {
    sprites[rang++] = pattern.sprite_base;
  }
  std::ranges::sort(sprites);
  CHECK(std::ranges::adjacent_find(sprites) == sprites.end());
  CHECK(EffectFadingSpritePatternsFor(1).empty());
}

TEST_CASE("seules les ailes se resement", "[render][fading_sprite]") {
  const EffectFadingSpritePattern& wings = PatternOfStyle(kAngel2, kFadingSpriteWings);
  const EffectFadingSpritePattern& body = PatternOfStyle(kAngel2, kFadingSpriteBody);
  // ⚠ TOUS LES TROIS TICS JUSQU'AU DIX-HUITIÈME : sept nœuds d'ailes en tout.
  CHECK(EffectNodeScheduleSowsAt(wings.schedule, 0));
  CHECK(EffectNodeScheduleSowsAt(wings.schedule, 3));
  CHECK(EffectNodeScheduleSowsAt(wings.schedule, 18));
  CHECK_FALSE(EffectNodeScheduleSowsAt(wings.schedule, 4));
  CHECK_FALSE(EffectNodeScheduleSowsAt(wings.schedule, 21));
  // Le corps, lui, ne naît qu'une fois.
  CHECK(EffectNodeScheduleSowsAt(body.schedule, 0));
  CHECK_FALSE(EffectNodeScheduleSowsAt(body.schedule, 3));
}

TEST_CASE("le noeud seme porte son sprite et sa forme", "[render][fading_sprite]") {
  const PooledEffectNode node = SowEffectFadingSpriteNode(
      PatternOfStyle(kAngel2, kFadingSpriteBody), {0.0F, 10.0F, 0.0F}, kLife, 0);
  CHECK(node.shape == ro::render::PooledNodeShape::kFadingSprite);
  // ⚠ SA FORME DOIT SE DÉCLARER COMME UNE FORME DE SPRITE, sans quoi la fenêtre
  // chercherait son `.spr` parmi les textures et il ne paraîtrait jamais.
  CHECK(ro::render::EffectNodeShapeDrawsASprite(node.shape));
  REQUIRE(node.resource_paths.size() == 1);
  // ⚠ UNE SEULE TRACE : le sprite se peint une fois, au point du nœud. En peindre
  // plusieurs les additionne, et l'ange sort en tache blanche.
  CHECK(node.trail.empty());
  // ⚠ L'ALPHA NAÎT NUL : le sprite s'allume sous le mouvement.
  CHECK(node.node.alpha == Approx(0.0F));
  // ⚠ ET IL NAÎT AU-DESSUS DE L'EFFET : notre axe vertical monte. Plus bas, il
  // serait sous le sol, et l'épreuve de profondeur le cacherait tout entier.
  CHECK(node.ground_position.y == Approx(10.0F + ro::render::kFadingSpriteSpawnHeight));
  CHECK(ro::render::kFadingSpriteSpawnHeight > 0.0F);
  CHECK(SowEffectFadingSpriteNode(PatternOfStyle(kAngel3, ro::render::kFadingSpriteHanbokBody),
                                  {0.0F, 10.0F, 0.0F}, kLife, 0)
            .ground_position.y == Approx(10.0F + ro::render::kFadingSpriteHanbokSpawnHeight));
}

TEST_CASE("chaque style porte son mot de rendu", "[render][fading_sprite]") {
  const auto word_of = [](std::uint32_t effect_id, std::uint8_t style) {
    return SowEffectFadingSpriteNode(PatternOfStyle(effect_id, style), {0.0F, 0.0F, 0.0F}, kLife, 0)
        .node.render_word;
  };
  // Le corps du hanbok se mélange à l'ordinaire et passe par-dessus tout.
  const auto hanbok_body = word_of(kAngel3, ro::render::kFadingSpriteHanbokBody);
  REQUIRE(hanbok_body.has_value());
  CHECK_FALSE(ro::render::EffectSpriteIsAdditive(*hanbok_body));
  CHECK_FALSE(ro::render::EffectLotIsDepthTested(hanbok_body));
  // Les autres s'ajoutent, sous l'épreuve de profondeur.
  for (const auto word : {word_of(kAngel3, ro::render::kFadingSpriteHanbokWings),
                          word_of(kAngel2, kFadingSpriteBody), word_of(kAngel2, kFadingSpriteWings),
                          word_of(kAngel2, ro::render::kFadingSpriteFirstFeather)}) {
    REQUIRE(word.has_value());
    CHECK(ro::render::EffectSpriteIsAdditive(*word));
    CHECK(ro::render::EffectLotIsDepthTested(word));
  }
}

TEST_CASE("l'ange en hanbok monte, tient, puis s'eteint", "[render][fading_sprite]") {
  const EffectFadingSpritePattern& body =
      PatternOfStyle(kAngel3, ro::render::kFadingSpriteHanbokBody);
  // Dix par tic, borné à son plafond exact.
  CHECK(AfterTicks(body, 0, 10).node.alpha == Approx(100.0F));
  CHECK(AfterTicks(body, 0, 26).node.alpha == Approx(ro::render::kFadingSpriteHanbokCeiling));
  CHECK(AfterTicks(body, 0, 100).node.alpha == Approx(ro::render::kFadingSpriteHanbokCeiling));
  // Passé le centième tic, il perd six par tic.
  CHECK(AfterTicks(body, 0, 101).node.alpha ==
        Approx(ro::render::kFadingSpriteHanbokCeiling - 6.0F));
  CHECK(AfterTicks(body, 0, 143).node.alpha == Approx(0.0F));
  // Une image tous les quatre tics, jusqu'à la quinzième.
  CHECK(AfterTicks(body, 0, 32).node.frame == 1);
  CHECK(AfterTicks(body, 0, 400).node.frame == ro::render::kFadingSpriteHanbokLastFrame);
}

TEST_CASE("les ailes du hanbok ne se montrent que resemees", "[render][fading_sprite]") {
  const EffectFadingSpritePattern& wings =
      PatternOfStyle(kAngel3, ro::render::kFadingSpriteHanbokWings);
  CHECK(AfterTicks(wings, 0, 30).node.alpha == Approx(0.0F));
  // Un de plus par tic, jusqu'à quarante moins son tic de naissance.
  CHECK(AfterTicks(wings, 3, 30).node.alpha == Approx(30.0F));
  CHECK(AfterTicks(wings, 3, 40).node.alpha == Approx(kFadingSpriteRebornCeiling - 3.0F));
}

TEST_CASE("il s'eleve d'un dixieme par tic", "[render][fading_sprite]") {
  const PooledEffectNode start = SowEffectFadingSpriteNode(
      PatternOfStyle(kAngel2, kFadingSpriteBody), {0.0F, 10.0F, 0.0F}, kLife, 0);
  const PooledEffectNode after = AfterTicks(PatternOfStyle(kAngel2, kFadingSpriteBody), 0, 10);
  // ⚠ IL MONTE : notre axe vertical monte, l'ordonnée croît.
  CHECK(after.ground_position.y ==
        Approx(start.ground_position.y + 10.0F * kFadingSpriteRisePerTick));
}

TEST_CASE("le corps monte a son plafond, tient, puis s'eteint", "[render][fading_sprite]") {
  const EffectFadingSpritePattern& body = PatternOfStyle(kAngel2, kFadingSpriteBody);
  // Cinq par tic : le plafond est atteint au quarante-neuvième.
  CHECK(AfterTicks(body, 0, 49).node.alpha == Approx(kFadingSpriteBodyCeiling));
  CHECK(AfterTicks(body, 0, 100).node.alpha == Approx(kFadingSpriteBodyCeiling));
  // ⚠ PLUS RIEN NE MONTE APRÈS LE CINQUANTIÈME TIC : la chute de quatre, passé le
  // centième, l'emporte donc et l'ange s'éteint bien avant la mort du nœud.
  CHECK(AfterTicks(body, 0, 101).node.alpha == Approx(kFadingSpriteBodyCeiling - 4.0F));
  CHECK(AfterTicks(body, 0, 162).node.alpha == Approx(0.0F));
}

TEST_CASE("une plume, elle, s'eteint pour de bon", "[render][fading_sprite]") {
  // ⚠ CE QUI LA SÉPARE DU CORPS N'EST PAS SA CHUTE MAIS SA MONTÉE : elle cesse
  // de monter au trentième tic, si bien que plus rien ne compense sa chute.
  const EffectFadingSpritePattern& plume =
      PatternOfStyle(kAngel2, ro::render::kFadingSpriteFirstFeather);
  CHECK(AfterTicks(plume, 0, 30).node.alpha > 0.0F);
  CHECK(AfterTicks(plume, 0, 120).node.alpha == Approx(0.0F));
}

TEST_CASE("une aile resemee est plus pale que la premiere", "[render][fading_sprite]") {
  const EffectFadingSpritePattern& wings = PatternOfStyle(kAngel2, kFadingSpriteWings);
  // ⚠ SON PLAFOND VAUT QUARANTE MOINS SON TIC DE NAISSANCE, et c'est ce qui
  // fait la traînée du battement. La dernière aile monte à vingt-deux.
  const PooledEffectNode first = AfterTicks(wings, 0, 60);
  const PooledEffectNode last = AfterTicks(wings, 18, 60);
  CHECK(last.node.alpha < first.node.alpha);
  CHECK(last.node.alpha == Approx(kFadingSpriteRebornCeiling - 18.0F));
  CHECK(last.tracks.front().mark[kFadingSpriteMarkOfBirthTick] == 18);
  CHECK(last.tracks.front().mark[kFadingSpriteMarkOfStyle] == kFadingSpriteWings);
}

TEST_CASE("l'image n'avance qu'apres le trentieme tic, et se borne", "[render][fading_sprite]") {
  const EffectFadingSpritePattern& body = PatternOfStyle(kAngel2, kFadingSpriteBody);
  CHECK(AfterTicks(body, 0, kFadingSpriteFrameFirstTick).node.frame == 0);
  // Un cran tous les trois tics : au quarante-deuxième, quatre crans.
  CHECK(AfterTicks(body, 0, 42).node.frame == 4);
  // ⚠ ET LE PLAFOND L'EMPÊCHE DE REBOUCLER : sans lui, l'ange battrait des ailes
  // indéfiniment.
  CHECK(AfterTicks(body, 0, 400).node.frame == kFadingSpriteLastFrame);
}
