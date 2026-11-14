// LA LOI DE L'AMPOULE : les guirlandes de Lutie.
//
// 🔴 UN SEUL TIRAGE DÉCIDE DE DEUX CHOSES, et c'est ce qui se vérifie ici. Le
// cas de l'effet 165 tire `rand() % 10`, puis en sort l'ACTION par la moitié du
// tirage — l'une des cinq couleurs d'ampoule du sprite `크리스마스` — et une ÉCHELLE
// d'un centième quand le reste par trois vaut un, ce qui éteint l'ampoule.
//
// Signalé en jeu : les lumières de Lutie s'allumaient mais restaient TOUTES
// rouges, faute de tirer leur action. Ce sont les semeurs de la carte qui les
// font changer — ils relancent l'effet toutes les deux secondes, et chaque
// relance retire.

#include <cstdint>
#include <set>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_node_sprite_seed.h"

namespace {

// L'effet des guirlandes, et le seul de la table qui porte cette loi.
constexpr std::uint32_t kLutieLights = 165;
// Les cinq couleurs d'ampoule du sprite.
constexpr std::int32_t kBulbColours = 5;
// L'échelle d'une ampoule éteinte.
constexpr float kDark = 0.01F;

}  // namespace

TEST_CASE("Un seul patron de la table porte la loi de l'ampoule", "[render][table]") {
  // ⚠ LE COMPTE EST LE CONTRÔLE : la loi est écrite dans le CAS d'un effet et
  // non dans un bâtisseur partagé. La donner à un second patron voudrait dire
  // qu'on l'a déduite au lieu de la lire.
  std::size_t porteurs = 0;
  for (const ro::render::EffectNodePattern& pattern : ro::render::EffectNodePatternTable()) {
    if (pattern.draws_bulb) {
      ++porteurs;
      CHECK(pattern.effect_id == kLutieLights);
    }
  }
  CHECK(porteurs == 1);
}

TEST_CASE("L'ampoule tire sa couleur parmi cinq, et son allumage", "[render][effets]") {
  const std::span<const ro::render::EffectNodePattern> patterns =
      ro::render::EffectNodePatternsFor(kLutieLights);
  REQUIRE_FALSE(patterns.empty());
  // La vie restante, le tic du semis et le paramètre d'instance n'entrent pas
  // dans cette loi-là : ce sont les valeurs d'une entrée en scène ordinaire.
  const ro::render::EffectNodeSeed seed =
      ro::render::EffectNodeSeedFromPattern(patterns.front(), patterns.front().life_ticks, 0, 1);
  REQUIRE(seed.draws_bulb);

  std::set<std::int32_t> couleurs;
  std::size_t eteintes = 0;
  constexpr int kSowingCount = 400;
  std::uint32_t generator = 1;
  for (int sowing = 0; sowing < kSowingCount; ++sowing) {
    const ro::render::EffectNode node = ro::render::SowEffectNode(seed, generator);
    couleurs.insert(node.action);
    CHECK(node.action >= 0);
    CHECK(node.action < kBulbColours);
    if (node.sprite_scale == kDark) {
      ++eteintes;
    }
  }
  // LES CINQ COULEURS SORTENT, et c'est ce qui manquait : une guirlande d'une
  // seule couleur est exactement ce que le joueur a vu.
  CHECK(couleurs.size() == static_cast<std::size_t>(kBulbColours));
  // ⚠ ET TROIS TIRAGES SUR DIX ÉTEIGNENT. Le compte se voit sur une guirlande :
  // toutes allumées, ce n'est pas la même chose. On laisse du jeu au tirage,
  // mais pas assez pour que zéro ou tout passe.
  CHECK(eteintes > kSowingCount / 10);
  CHECK(eteintes < kSowingCount / 2);
}
