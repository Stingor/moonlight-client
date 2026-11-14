// LA CROIX D'UN IMPACT — EF_HIT5 et EF_HIT6.
//
// 🔴 CES DEUX EFFETS NE MONTRAIENT QU'UNE BARRE. Leur bâtisseur pose DEUX quads
// d'écran, le second tourné de quatre-vingt-dix degrés par rapport au premier ;
// le relevé, qui cherche des constantes, n'en voyait qu'un — la boucle qui les
// compte n'est pas une valeur à lire. Un effet à moitié semé ne se lit pas comme
// une absence : il paraît, et de travers.

#include <cmath>
#include <cstdint>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_hit_cross_seed.h"
#include "ro/render/effect_lens_flare_seed.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_screen_node.h"

using ro::render::EffectHitCrossPattern;
using ro::render::EffectHitCrossPatternsFor;
using ro::render::EffectHitCrossPatternTable;
using ro::render::EffectScreenNode;
using ro::render::kHitCrossArmDegrees;
using ro::render::kHitCrossArms;
using ro::render::SowEffectHitCrossNodes;

namespace {

constexpr std::uint32_t kHit5 = 4;
constexpr std::uint32_t kHit6 = 5;

[[nodiscard]] const EffectHitCrossPattern& PatternOf(std::uint32_t effect_id) {
  const std::span<const EffectHitCrossPattern> found = EffectHitCrossPatternsFor(effect_id);
  REQUIRE(found.size() == 1);
  return found.front();
}

}  // namespace

TEST_CASE("Croix : les deux branches sont perpendiculaires", "[render][effet][croix]") {
  // 🔴 LE CAS QUI PORTE TOUT LE DÉFAUT. Une branche seule, c'est l'effet vu en
  // jeu ; deux branches au hasard l'une de l'autre, ce serait une paire et non
  // une croix. Seul l'écart de quatre-vingt-dix degrés fait la figure.
  const EffectHitCrossPattern& pattern = PatternOf(kHit5);
  for (std::uint32_t seed = 1; seed < 40; ++seed) {
    std::uint32_t generator = seed;
    const std::vector<EffectScreenNode> arms = SowEffectHitCrossNodes(pattern, generator);
    REQUIRE(arms.size() == static_cast<std::size_t>(kHitCrossArms));
    CHECK(arms[1].turn_degrees - arms[0].turn_degrees == kHitCrossArmDegrees);
    // ⚠ ET LES DEUX NE SE SUPERPOSENT JAMAIS : c'est l'autre bout de la même
    // vérification, celle qui échouerait si l'écart tombait à zéro.
    CHECK(arms[0].turn_degrees != arms[1].turn_degrees);
  }
}

TEST_CASE("Croix : l'angle de départ change d'une croix à l'autre", "[render][effet][croix]") {
  // Le client le tire au sort. Une croix toujours posée du même côté se
  // remarquerait au bout de trois coups portés.
  const EffectHitCrossPattern& pattern = PatternOf(kHit5);
  std::uint32_t first = 7;
  std::uint32_t second = 91;
  const std::vector<EffectScreenNode> one = SowEffectHitCrossNodes(pattern, first);
  const std::vector<EffectScreenNode> other = SowEffectHitCrossNodes(pattern, second);
  CHECK(one.front().turn_degrees != other.front().turn_degrees);
}

TEST_CASE("Croix : chaque branche s'amincit jusqu'à disparaître", "[render][effet][croix]") {
  // 🔴 UN CHAMP CALCULÉ, ET NON POSÉ : le pas de demi-largeur vaut moins la
  // largeur TIRÉE divisée par la vie. Un relevé qui cherche des constantes ne
  // peut rien en rendre, et la croix restait épaisse jusqu'à sa dernière image.
  const EffectHitCrossPattern& pattern = PatternOf(kHit5);
  std::uint32_t generator = 3;
  for (const EffectScreenNode& arm : SowEffectHitCrossNodes(pattern, generator)) {
    CHECK(arm.half_width_step < 0.0F);
    const float left = arm.half_width + arm.half_width_step * static_cast<float>(arm.life_ticks);
    CHECK(std::abs(left) < 0.01F);
  }
}

TEST_CASE("Croix : le fondu commence à la moitié de la vie", "[render][effet][croix]") {
  // ⚠ EN DIVISION ENTIÈRE, comme le client l'écrit : dix-sept donne neuf, et non
  // huit ni huit et demi.
  const EffectHitCrossPattern& pattern = PatternOf(kHit5);
  std::uint32_t generator = 5;
  const std::vector<EffectScreenNode> arms = SowEffectHitCrossNodes(pattern, generator);
  CHECK(arms.front().life_ticks == 17);
  CHECK(arms.front().fade_out_tick == 9);
}

TEST_CASE("Croix : la rotation freine sans s'inverser", "[render][effet][croix]") {
  // L'accélération est de signe OPPOSÉ à la vitesse, et assez faible pour ne pas
  // la retourner avant la fin de la vie.
  const EffectHitCrossPattern& pattern = PatternOf(kHit5);
  std::uint32_t generator = 11;
  const EffectScreenNode arm = SowEffectHitCrossNodes(pattern, generator).front();
  REQUIRE(arm.turn_speed < 0.0F);
  CHECK(arm.turn_acceleration > 0.0F);
  const float last = arm.turn_speed + arm.turn_acceleration * static_cast<float>(arm.life_ticks);
  CHECK(last < 0.0F);
}

TEST_CASE("Croix : les deux impacts ne se confondent pas", "[render][effet][croix]") {
  // ⚠ LE MÊME CODE À QUATRE NOMBRES PRÈS, et ces nombres sont la différence
  // entre les deux coups : EF_HIT6 est la moitié d'EF_HIT5.
  const EffectHitCrossPattern& five = PatternOf(kHit5);
  const EffectHitCrossPattern& six = PatternOf(kHit6);
  CHECK(five.half_width.low > six.half_width.low);
  CHECK(five.half_height.low > six.half_height.low);
  CHECK(five.half_height_step != six.half_height_step);
  CHECK(five.texture == six.texture);
}

TEST_CASE("Croix : une famille portée est une famille ADMISE", "[render][effet][croix]") {
  // 🔴 LE DÉFAUT QUE CE CAS EMPÊCHE DE REVENIR, ET IL EST DÉJÀ ARRIVÉ DEUX FOIS.
  // `KnowsProceduralEffect` est la SEULE liste qui décide si un effet est joué.
  // Une famille qu'on porte sans l'y inscrire est MUETTE — et le silence ne se
  // distingue pas d'un effet que le client ne montre pas non plus.
  //
  // ⚠ LA GERBE DE REFLETS Y EST AUSSI. Elle n'y a jamais été inscrite : elle ne
  // passait que parce que son identifiant traînait encore dans la table des
  // nœuds d'écran, et le jour où il en est sorti l'effet a cessé de paraître.
  // Les deux tables portées à la main sont donc éprouvées ensemble.
  for (const EffectHitCrossPattern& node_pattern : EffectHitCrossPatternTable()) {
    CHECK(ro::render::KnowsProceduralEffect(node_pattern.effect_id));
  }
  for (const ro::render::EffectLensFlarePattern& node_pattern :
       ro::render::EffectLensFlarePatternTable()) {
    CHECK(ro::render::KnowsProceduralEffect(node_pattern.effect_id));
  }
}

TEST_CASE("Croix : la table ne porte que ses deux impacts", "[render][effet][croix]") {
  // ⚠ ET RIEN D'AUTRE : les identifiants qu'elle porte sont refusés à la table
  // des nœuds d'écran, sans quoi le même bâtisseur serait semé deux fois.
  CHECK(EffectHitCrossPatternTable().size() == 2);
  CHECK(EffectHitCrossPatternsFor(3).empty());
  CHECK(EffectHitCrossPatternsFor(6).empty());
}
