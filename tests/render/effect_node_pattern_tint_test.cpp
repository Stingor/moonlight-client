// LA TEINTE D'UN SPRITE de nœud : blanche au constructeur, et le dessin des
// particules de sprite l'applique tant que le bit 0x200 des drapeaux est
// éteint. Ces essais tiennent les effets où elle se voit.
#include <array>
#include <cstdint>
#include <span>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_sprite_seed.h"

using ro::render::EffectNodePattern;
using ro::render::EffectNodePatternsFor;

namespace {

using Tint = std::array<std::uint8_t, 3>;

[[nodiscard]] std::size_t TintCount(std::uint32_t effect_id, Tint tint) {
  std::size_t count = 0;
  for (const EffectNodePattern& pattern : EffectNodePatternsFor(effect_id)) {
    count += pattern.color == tint ? 1U : 0U;
  }
  return count;
}

}  // namespace

TEST_CASE("les trois boules de FireBall sont bleue, verte et rouge", "[render][table]") {
  // Fire Ball écrit 155 dans deux des trois canaux de chaque nœud. Blanches,
  // les trois boules se confondraient.
  const std::span<const EffectNodePattern> balls = EffectNodePatternsFor(751);
  REQUIRE(balls.size() == 3);
  CHECK(balls[0].color == Tint{155, 155, 255});
  CHECK(balls[1].color == Tint{155, 255, 155});
  CHECK(balls[2].color == Tint{255, 155, 155});
}

TEST_CASE("la gerbe de RingBlueSprayDark est presque noire", "[render][table]") {
  // Vingt dans les trois canaux, pour ses deux nœuds
  // de sprite : la gerbe assombrit au lieu d'éclairer.
  CHECK(TintCount(180, Tint{20, 20, 20}) == 2);
  CHECK(TintCount(184, Tint{20, 20, 20}) == 2);
}

TEST_CASE("Dark Breath est rouge", "[render][table]") {
  // Le cas du répartiteur bâtit son nœud lui-même et éteint le vert et le bleu.
  CHECK(TintCount(212, Tint{255, 0, 0}) >= 1);
}

TEST_CASE("les boules de Fire Ball s'effacent par l'arrière sans changer de teinte",
          "[render][table]") {
  // `(5 − âge) · 40` pour la première boule de Fire Ball, et de même
  // pour les deux autres : 200, 160, 120, 80 selon le rang. La teinte de chaque
  // boule — bleue, verte, rouge — reste la sienne.
  const std::span<const EffectNodePattern> balls = EffectNodePatternsFor(751);
  REQUIRE(balls.size() == 3);
  CHECK(balls[0].first_tick == 0);
  CHECK(balls[1].first_tick == 6);
  CHECK(balls[2].first_tick == 12);
  for (const EffectNodePattern& ball : balls) {
    CHECK(ball.last_tick == ball.first_tick + 3);
    CHECK(ball.launch_drop == Catch::Approx(-15.0F));
    // Au quatrième semis de la boule verte, alpha 80 et toujours verte.
    const ro::render::EffectNodeSeed quatrieme =
        ro::render::EffectNodeSeedFromPattern(ball, 20, ball.first_tick + 3, 0);
    CHECK(quatrieme.alpha_max == Catch::Approx(80.0F));
    CHECK(quatrieme.color[0] == Catch::Approx(static_cast<float>(ball.color[0])));
    CHECK(quatrieme.color[1] == Catch::Approx(static_cast<float>(ball.color[1])));
  }
  // L'effet 80 sème aux âges 0, 4, 8 et 12.
  const EffectNodePattern& projectile = EffectNodePatternsFor(80).front();
  CHECK(projectile.period_ticks == 4);
  CHECK(projectile.last_tick == 12);
}
