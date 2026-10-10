#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_screen_node.h"

namespace ro::render {

// LE SEMIS DE LA VOIE D'ÉCRAN, sous-type 0.
//
// Voir l'ADR 0022 pour la voie, et `effect_screen_node.h` pour ce qu'un tic en
// fait. Douze bâtisseurs y mènent, et ils ne partagent rien : chacun pose son
// propre jeu de champs, dont la plupart restent au défaut du constructeur.
//
// ⚠ DEUX IDENTIFIANTS SONT ÉCARTÉS, ET LE GÉNÉRATEUR LE DIT PLUTÔT QUE DE LES
// DEVINER. Le 32 ne résout pas son image — le relevé rend un vide, et inventer
// un chemin serait pire que l'absence. Le 164 est du GENRE 98 : ce n'est pas une
// variante de figure mais la barre d'icônes d'état du joueur, qui lit l'état de
// jeu et n'a donc pas sa place dans `render`.

// UN NŒUD D'ÉCRAN QU'UN EFFET POSE.
//
// Les plages disent un TIRAGE quand leurs deux bornes diffèrent, et une valeur
// fixe sinon — c'est la même convention que `EffectNodeSeed`.
struct EffectScreenPattern {
  std::uint32_t effect_id = 0;
  // LA FONCTION DU CLIENT qui le pose, par son nom.
  std::string_view client_function;
  std::string_view texture;
  // LE FORMAT D'UNE IMAGE NUMEROTEE, quand le client en compose le nom.
  //
  // ⚠ CERTAINS BATISSEURS NE NOMMENT PAS LEUR IMAGE, ILS LA COMPOSENT.
  // Celui de Napalm Beat compose le nom depuis un format et l'index de sa boucle
  // PLUS UN, puis charge le nom obtenu : huit images, de un a huit. Le
  // releve ne rend alors AUCUNE texture — et quinze identifiants sortaient
  // « image non resolue au releve », dont EF_NAPALMBEAT et ses sept sites.
  //
  // Vide quand le batisseur nomme son image en clair, ce qui est le cas ordinaire.
  // Non vide, c'est LUI qui vaut, et texture_count dit combien d'images en
  // sortent — numerotees a partir de UN, comme le client les compose.
  std::string_view texture_format;

  EffectNodeRange half_width;
  EffectNodeRange half_height;
  EffectNodeRange step;
  EffectNodeRange heading_degrees;
  // LE RAYON DU CERCLE OÙ LE NŒUD NAÎT, en pixels d'écran.
  //
  // ⚠ IL SE POSE SUR LE CAP, et non sur un angle à lui. Quatre bâtisseurs
  // écrivent le même couple — `916 = cos(cap) · rayon`, `920 = −sin(cap) ·
  // rayon` — puis ajoutent leur décalage fixe. Chez EF_ENDURE le cap sert AUSSI
  // au déplacement, et son pas est négatif : la poussière naît sur le cercle et
  // revient vers le porteur. Tirer un angle à part romprait cette convergence.
  //
  // Nul quand le bâtisseur n'en pose pas : le nœud naît alors à son décalage.
  EffectNodeRange birth_radius;
  // L'ANGLE DU QUAD à la naissance, en degrés d'écran. Voir
  // `turn_follows_heading`, qui passe devant quand il est vrai.
  EffectNodeRange turn_degrees;

  // JUSQU'À QUEL ÂGE DE L'EFFET CE BÂTISSEUR SÈME, et tous les combien.
  //
  // 🔴 LA MOITIÉ DE CES BÂTISSEURS SÈME À RÉPÉTITION, et un semis unique ne
  // montre alors qu'un grain là où le client en fait une gerbe : la poussière
  // d'EF_ENDURE naît à CHACUN des cinquante premiers tics, le sable d'EF_SAND
  // un tic sur deux pendant soixante. Le relevé ne rend pas cette cadence — elle
  // vit dans la GARDE du bâtisseur, pas dans un champ du nœud.
  //
  // Le défaut est le cas ordinaire : une fois, au premier tic.
  std::int32_t birth_last_tick = 0;
  std::int32_t birth_period = 1;

  std::int32_t life_ticks = 100;
  std::int32_t fade_out_tick = -1;
  std::int32_t second_step_tick = -1;
  std::int32_t growth_stop_tick = 0;
  std::int32_t ticks_per_frame = 4;
  std::int32_t texture_count = 1;
  // VRAI par défaut, comme le constructeur du client le pose : voir
  // `EffectScreenNode::frame_loops`.
  bool frame_loops = true;

  float alpha = 254.0F;
  float alpha_max = 254.0F;
  // DE COMBIEN L'ALPHA MONTE À CHAQUE TIC.
  //
  // 🔴 UN NŒUD QUI NAÎT TRANSPARENT NE SE VOIT QUE PAR LUI. Six bâtisseurs sur
  // huit posent l'alpha à ZÉRO puis cette montée dans la foulée ; sans elle le
  // nœud reste à zéro toute sa vie et l'effet est MUET — c'était le cas
  // d'EF_ENDURE, d'EF_NAPALMBEAT et des trois effets d'agilité.
  //
  // ⚠ ET LE CLIENT LA CALCULE, il ne la pose pas : `808 = 724 / n`, une fraction
  // de l'alpha maximal. Un relevé qui cherche des constantes n'en rend donc
  // rien, et son absence est indiscernable d'un bâtisseur qui n'y touche pas.
  float alpha_step = 0.0F;
  float step_change = 0.0F;
  float half_width_step = 0.0F;
  float half_width_acceleration = 0.0F;
  float half_height_step = 0.0F;
  float half_height_acceleration = 0.0F;
  float half_width_step_after = kEffectScreenUnchanged;
  float half_height_step_after = kEffectScreenUnchanged;
  float turn_speed = 0.0F;

  // LE DÉCALAGE D'ÉCRAN, en pixels, par rapport au porteur.
  float screen_offset_x = 0.0F;
  float screen_offset_y = 0.0F;

  std::int32_t red = 255;
  std::int32_t green = 255;
  std::int32_t blue = 255;

  std::uint32_t flags = 0;
  std::int32_t blend_mode = 0;
  bool uses_blend_mode = false;

  // VRAI QUAND L'ANGLE DU QUAD EST LE CAP, du même tirage, et non un angle à
  // part.
  //
  // 🔴 LA POUSSIÈRE D'EF_ENDURE TIRE UN SEUL ANGLE POUR LES DEUX : `788 = 780 =
  // rand() % 360`. Sa traînée — un quad de soixante à cent pixels de haut pour
  // deux de large — est donc COUCHÉE SUR SA COURSE, et c'est ce qui fait des
  // rayons qui convergent vers le porteur. Un angle laissé à zéro couche toutes
  // les traînées à l'horizontale, et c'est ce qu'on voyait. Un relevé de
  // constantes ne rend pas « le même tirage », d'où la lecture à la main.
  bool turn_follows_heading = false;
  // LA DEMI-TAILLE TIRÉE DE L'IMAGE ELLE-MÊME : demi-largeur et demi-hauteur
  // valent la largeur et la hauteur de la texture, en pixels, fois ce facteur.
  // Le bâtisseur des buffs génériques les lit ainsi sur l'icône que son
  // répartiteur lui passe. Zéro : les demi-tailles du patron valent.
  float half_size_per_texture_pixel = 0.0F;
};

// Tous les nœuds d'écran connus, par identifiant d'effet.
[[nodiscard]] std::span<const EffectScreenPattern> EffectScreenPatternTable();

// TOUS les nœuds d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectScreenPattern> EffectScreenPatternsFor(std::uint32_t effect_id);

// LE NŒUD qu'un patron décrit, tirages faits, prêt à vivre.
//
// Le générateur est passé PAR RÉFÉRENCE et avance : deux nœuds semés à la suite
// ne tirent pas la même taille, et repartir de la même graine rejoue la même
// scène.
// LES IMAGES D'UN NOEUD D'ECRAN, dans l'ordre ou son index de vue les prend.
//
// Une seule quand le batisseur la nomme en clair ; autant que texture_count
// quand il la COMPOSE, numerotees a partir de UN. Rendre ici plutot qu'au semis
// garde la composition eprouvable sans fenetre ni archive.
[[nodiscard]] std::vector<std::string> EffectScreenTexturePaths(const EffectScreenPattern& pattern);

[[nodiscard]] EffectScreenNode SowEffectScreenNode(const EffectScreenPattern& pattern,
                                                   std::uint32_t& generator);

}  // namespace ro::render
