#include "ro/render/pvp_rank_tint.h"

#include <array>
#include <cstdint>
#include <span>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pattern_table.h"

using ro::render::EffectReadsValueAsParam;
using ro::render::kPvpTopRankEffect;
using ro::render::PvpTopRankTint;

TEST_CASE("Teinte du classement : du presque blanc au rouge vif", "[render][pvp]") {
  // Comme le client : rouge constant à 250, vert et bleu par
  // `(10 − rank) × 25`. C'est la rampe ENTIÈRE qui se vérifie, et non ses deux
  // bouts : un pas inversé donnerait les mêmes extrémités.
  constexpr std::array<std::array<float, 3>, 10> kExpected = {{
      {250.0F, 225.0F, 225.0F},  // rang 1, presque blanc
      {250.0F, 200.0F, 200.0F},
      {250.0F, 175.0F, 175.0F},
      {250.0F, 150.0F, 150.0F},
      {250.0F, 125.0F, 125.0F},
      {250.0F, 100.0F, 100.0F},
      {250.0F, 75.0F, 75.0F},
      {250.0F, 50.0F, 50.0F},
      {250.0F, 25.0F, 25.0F},
      {250.0F, 0.0F, 0.0F},  // rang 10, rouge vif
  }};
  for (std::int32_t rank = 1; rank <= 10; ++rank) {
    const std::array<float, 3> tint = PvpTopRankTint(rank);
    INFO("rang " << rank);
    CHECK(tint[0] == Catch::Approx(kExpected[static_cast<std::size_t>(rank - 1)][0]));
    CHECK(tint[1] == Catch::Approx(kExpected[static_cast<std::size_t>(rank - 1)][1]));
    CHECK(tint[2] == Catch::Approx(kExpected[static_cast<std::size_t>(rank - 1)][2]));
  }
}

TEST_CASE("Teinte du classement : un rang absent ou caché reste PRESQUE BLANC", "[render][pvp]") {
  // ⚠ ET NON ROUGE, ce qui serait l'erreur naturelle. Le serveur envoie moins un
  // pour le rang d'un joueur caché : `(10 − (−1)) × 25` vaut 275, que la rampe
  // du client ÉCRÊTE à 255. Lu sans cet écrêtage, l'octet bas de 275 vaut 19 et
  // l'anneau sortirait rouge presque pur — l'inverse de ce qu'on voit.
  const std::array<float, 3> cache = PvpTopRankTint(-1);
  CHECK(cache[0] == Catch::Approx(250.0F));
  CHECK(cache[1] == Catch::Approx(255.0F));
  CHECK(cache[2] == Catch::Approx(255.0F));

  // Un rang ZÉRO est celui d'un effet allumé autrement que par le paquet — une
  // commande d'essai. Presque blanc lui aussi, et c'est ce que fait le client.
  const std::array<float, 3> without_rank = PvpTopRankTint(0);
  CHECK(without_rank[0] == Catch::Approx(250.0F));
  CHECK(without_rank[1] == Catch::Approx(250.0F));
}

TEST_CASE("Teinte du classement : le rang onze est VERT, et il dort", "[render][pvp]") {
  // ⚠ CETTE BRANCHE EST DANS LE CODE ET PAS DANS LE JEU : le récepteur de
  // `ZC_NOTIFY_RANKING` ne pose l'effet que si le rang est au plus dix. On la
  // porte parce qu'elle est LUE ; la taire ferait croire à un oubli.
  const std::array<float, 3> onze = PvpTopRankTint(11);
  CHECK(onze[0] == Catch::Approx(0.0F));
  CHECK(onze[1] == Catch::Approx(250.0F));
  CHECK(onze[2] == Catch::Approx(0.0F));
}

TEST_CASE("Anneau du classement : il ne s'éteint jamais de lui-même", "[render][pvp]") {
  // 🔴 LE BÂTISSEUR RAMÈNE L'ÂGE DE L'INSTANCE À ZÉRO À 9 998 TICS et rebâtit
  // aussitôt son anneau, de vie 9 999 : l'instance n'atteint jamais sa durée, et
  // l'ancien nœud meurt un tic après la naissance du nouveau. Semé une seule
  // fois, l'anneau disparaissait au bout de deux minutes quarante, et rien ne le
  // reposait — le serveur ne redit un rang que lorsqu'il change.
  const std::span<const ro::render::EffectNodePattern> patterns =
      ro::render::EffectNodePatternsFor(kPvpTopRankEffect);
  REQUIRE(patterns.size() == 1);
  const ro::render::EffectNodePattern& ring = patterns.front();
  constexpr std::int32_t kAgeWrapTicks = 9998;

  CHECK(ro::render::EffectNodePatternSowsAt(ring, 0));
  CHECK_FALSE(ro::render::EffectNodePatternSowsAt(ring, 1));
  CHECK_FALSE(ro::render::EffectNodePatternSowsAt(ring, kAgeWrapTicks - 1));
  CHECK(ro::render::EffectNodePatternSowsAt(ring, kAgeWrapTicks));
  CHECK(ro::render::EffectNodePatternSowsAt(ring, 2 * kAgeWrapTicks));
  CHECK(ro::render::EffectNodePatternLastSowTick(ring) > 100 * kAgeWrapTicks);
  // ⚠ CHAQUE NŒUD VIT PLUS LONGTEMPS QUE L'INTERVALLE : pas une image sans anneau.
  CHECK(ring.life_ticks > ring.period_ticks);
  // ⚠ ET LA VIE DE L'INSTANCE NE LE BORNE PAS : un nœud rebâti au tic 9 998
  // garde ses 9 999 tics.
  CHECK(ro::render::EffectNodePatternTicksLeft(ring, kAgeWrapTicks) >= ring.life_ticks);
}

TEST_CASE("Paramètre d'instance : seuls le classement et les sphères lisent leur valeur",
          "[render][pvp]") {
  // 🔴 LE MÊME CHAMP SERT À DEUX MÉTIERS chez le client, et verser la valeur d'un
  // effet dans le paramètre sans regarder lequel ferait semer des centaines de
  // nœuds pour une valeur de soin.
  CHECK(EffectReadsValueAsParam(kPvpTopRankEffect));
  CHECK_FALSE(EffectReadsValueAsParam(kPvpTopRankEffect + 1));
  CHECK_FALSE(EffectReadsValueAsParam(0));
  // Les quatre sphères élémentaires y lisent leur nombre, et elles seules.
  CHECK(EffectReadsValueAsParam(ro::render::kFirstSpiritCharmEffect));
  CHECK(EffectReadsValueAsParam(ro::render::kLastSpiritCharmEffect));
  CHECK_FALSE(EffectReadsValueAsParam(ro::render::kFirstSpiritCharmEffect - 1));
  CHECK_FALSE(EffectReadsValueAsParam(ro::render::kLastSpiritCharmEffect + 1));
}
