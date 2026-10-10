#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_screen_node.h"

namespace ro::render {

// LA CROIX D'UN IMPACT — les impacts 5 et 6.
//
// 🔴 DEUX QUADS D'ÉCRAN À QUATRE-VINGT-DIX DEGRÉS L'UN DE L'AUTRE, et c'est
// tout l'effet : une croix qui tourne sur elle-même en s'amincissant. Semé une
// seule fois, il ne reste qu'une barre — l'effet paraît, mais dans une seule
// direction, et c'est ce qui se voyait en jeu.
//
// 🔴 L'ANGLE DE DÉPART EST TIRÉ UNE FOIS POUR LES DEUX. `rand() % 360` avant la
// boucle, puis le tableau `{base, base + 90}` que la boucle indexe. Deux
// tirages indépendants donneraient deux barres au hasard l'une de
// l'autre : ce serait une paire, pas une croix. C'est la raison pour laquelle ce
// semis rend TOUS ses nœuds d'un coup au lieu d'un seul que l'appelant
// répéterait.
//
// ⚠ ET TROIS DE SES CHAMPS SONT CALCULÉS, non posés. Le pas de demi-largeur
// vaut moins la demi-largeur TIRÉE divisée par la vie ; l'accélération de
// rotation, moins la vitesse divisée par la vie puis par une constante ; le tic
// de fondu, la vie moins sa moitié. Une table de constantes ne peut pas les
// donner — voir `scripts/lectures_a_la_main_des_patrons.py` —, et les laisser à
// zéro donnerait une croix qui ne rétrécit ni ne s'efface.
//
// ⚠ CE MODULE POSSÈDE SES DEUX IDENTIFIANTS. Le générateur de la table des
// nœuds d'écran les refuse nommément : sans quoi le même bâtisseur serait semé
// deux fois, une branche en trop au milieu de la croix.

// LES DEUX BRANCHES, et l'angle qui les sépare.
inline constexpr std::int32_t kHitCrossArms = 2;
inline constexpr float kHitCrossArmDegrees = 90.0F;

// LE TOUR ENTIER, dans lequel l'angle de départ est tiré.
inline constexpr float kHitCrossFullTurn = 360.0F;

struct EffectHitCrossPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;

  std::int32_t life_ticks = 17;
  // Les deux tirages, DANS L'ORDRE OÙ LE CLIENT LES CONSOMME : la demi-largeur,
  // puis la demi-hauteur. Les intervertir donnerait à chaque branche la taille
  // de l'autre — et le générateur n'avançant pas de la même façon, la croix
  // suivante différerait aussi.
  EffectNodeRange half_width;
  EffectNodeRange half_height;

  float half_height_step = 0.0F;
  float half_height_acceleration = 0.0F;
  // La vitesse de rotation. ⚠ NÉGATIVE : la croix tourne dans le sens des
  // aiguilles d'une montre à l'écran.
  float turn_speed = 0.0F;
  // Ce par quoi l'accélération de rotation divise, après la vie. Le client
  // écrit `-(vitesse / vie) / 1.5` : la rotation freine sans s'inverser.
  float turn_slowdown_divisor = 1.5F;
  // Le décalage vertical du point d'impact, en pixels d'écran.
  float screen_offset_y = 0.0F;
};

[[nodiscard]] std::span<const EffectHitCrossPattern> EffectHitCrossPatternTable();

[[nodiscard]] std::span<const EffectHitCrossPattern> EffectHitCrossPatternsFor(
    std::uint32_t effect_id);

// LA CROIX ENTIÈRE, tirages faits.
//
// Le générateur avance d'une branche à l'autre pour les TAILLES, mais l'angle
// de départ est tiré une seule fois — voir l'en-tête, c'est ce qui fait la
// croix.
[[nodiscard]] std::vector<EffectScreenNode> SowEffectHitCrossNodes(
    const EffectHitCrossPattern& pattern, std::uint32_t& generator);

}  // namespace ro::render
