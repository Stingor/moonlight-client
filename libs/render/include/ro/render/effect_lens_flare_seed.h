#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "ro/render/effect_screen_node.h"

namespace ro::render {

// LA GERBE DE REFLETS D'UN IMPACT.
//
// Huit quads d'écran, UN TOUS LES QUARANTE-CINQ DEGRÉS, qui partent du point
// touché et s'éloignent en s'amincissant. C'est le second impact du jeu,
// `EF_HIT2`, celui qu'on voit à chaque coup porté.
//
// 🔴 LA BOUCLE EST UN TOUR, ET NON UN COMPTEUR. Le client accumule un angle par
// pas de quarante-cinq degrés et s'arrête à trois cent soixante. C'est la seule
// des quatre familles d'impact à ne porter aucun compteur entier : qui chercherait
// un compteur n'y verrait qu'un seul nœud.
//
// 🔴 ET SA TEXTURE ALTERNE. Les rangs pairs prennent `lens1.tga`, les impairs
// `lens2.tga` — selon la parité de l'index de la boucle. Une ressource « choisie
// par branche » se refuse d'ordinaire, et c'est la bonne règle pour une garde
// sur le MONDE : l'ordre du code n'en laisse qu'une, celle de la dernière
// branche, et la donner à tous montre la mauvaise image à tous sauf un. Ici la
// branche porte sur le RANG DANS LA BOUCLE — déterministe, et différente pour
// chaque exemplaire. Jeter ce nœud-là, c'est ne plus rien montrer de l'effet.

// LES SIX TIRAGES d'un reflet, dans l'ordre où le client les consomme. Un
// `rand() % etendue` auquel s'ajoute un plancher.
struct EffectLensFlareDraw {
  std::int32_t spread = 0;
  float floor = 0.0F;
  float divisor = 1.0F;
};

// Combien de tirages un reflet consomme : la vie, le cap, le pas, la
// demi-largeur, la demi-hauteur, et le rayon de son décalage.
inline constexpr std::size_t kLensFlareDrawsPerNode = 6;

struct EffectLensFlarePattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  // LES DEUX IMAGES, distribuées sur les rangs. La première va aux PAIRS.
  std::string_view even_texture;
  std::string_view odd_texture;

  // Le pas d'angle, et le tour qu'il parcourt : leur quotient donne le nombre
  // de reflets, et c'est ainsi que le client compte.
  float turn_step_degrees = 45.0F;
  float full_turn_degrees = 360.0F;

  EffectLensFlareDraw life;
  EffectLensFlareDraw heading;
  EffectLensFlareDraw step;
  EffectLensFlareDraw half_width;
  EffectLensFlareDraw half_height;
  EffectLensFlareDraw offset_radius;

  // 🔴 LE REFLET S'ÉTIRE, ET C'EST TOUTE SA PUISSANCE. Sa demi-hauteur gagne ce
  // pas à chaque tic, et le pas lui-même gagne son accélération : une aiguille
  // de vingt pixels en fait plus de soixante au dixième tic. Sans ces deux
  // nombres la gerbe garde la taille de sa naissance, ce qui ne se lit pas comme
  // une gerbe tronquée mais comme une gerbe FAIBLE — c'est le mot qui est
  // revenu du jeu.

  float half_height_step = 0.0F;
  float half_height_acceleration = 0.0F;

  // ⚠ CE QUE LE CAP RETRANCHE À L'ANGLE DU RANG. Le client tire trente degrés
  // et en ôte quinze : le reflet part donc DE PART ET D'AUTRE de sa branche, et
  // non toujours du même côté.
  float heading_bias_degrees = -15.0F;
  // Ce que l'ordonnée du décalage descend en plus, en pixels d'écran.
  float offset_lift = -20.0F;

  float alpha_start = 0.0F;
  // Le plafond de l'alpha : celui du constructeur du nœud, que le bâtisseur
  // ne réécrit pas. 254, et non 255.
  float alpha_max = 254.0F;
  // La part du plafond que l'alpha gagne à chaque tic : un huitième, donc huit
  // tics pour s'allumer entièrement.
  float alpha_rise_share = 0.125F;
  std::uint32_t flags = 1;

  // LA TEINTE, rouge, vert, bleu. Le même bâtisseur sert deux effets : le
  // coup ordinaire garde le blanc, l'autre rabat le rouge et le vert à cent
  // vingt — une gerbe BLEUTÉE.
  std::array<std::int32_t, 3> color = {255, 255, 255};

  // 🔴 SI LA GERBE SUIT SON PORTEUR À L'ÉCRAN. Le coup ordinaire reprojette la
  // place du porteur à chaque tic ; la gerbe bleutée ne le fait qu'au tic zéro,
  // et reste ensuite où elle est née.
  bool follows_bearer = true;
};

[[nodiscard]] std::span<const EffectLensFlarePattern> EffectLensFlarePatternTable();

[[nodiscard]] std::span<const EffectLensFlarePattern> EffectLensFlarePatternsFor(
    std::uint32_t effect_id);

// UN REFLET, ET L'IMAGE QU'IL PORTE. Les deux voyagent ensemble parce que le
// rang décide de la seconde, et que le rang ne se relit pas depuis le nœud.
struct EffectLensFlareNode {
  EffectScreenNode node;
  std::string_view texture;
};

// LA GERBE ENTIÈRE, tirages faits.
//
// Le générateur avance d'un reflet à l'autre : repartir de la même graine
// rejouerait huit fois le même.
[[nodiscard]] std::vector<EffectLensFlareNode> SowEffectLensFlareNodes(
    const EffectLensFlarePattern& pattern, std::uint32_t& generator);

}  // namespace ro::render
