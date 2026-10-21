#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_fwd.h"

namespace ro::render {

// Ce qu'un effet DONNE À PEINDRE : des quads texturés, en pixels autour du
// point où l'effet est joué.
//
// Description PURE : aucune texture n'est chargée ici, aucun contexte
// graphique n'est touché. C'est ce qui permet de vérifier la géométrie sans
// carte graphique — et de la peindre à la main pour la regarder.

// Un quad d'effet, prêt à peindre.
struct EffectQuad {
  // Les coins, dans l'ordre haut-gauche, haut-droit, bas-droit, bas-gauche —
  // celui des quads de sprite. L'ordonnée DESCEND, comme à l'écran.
  std::array<glm::vec2, 4> corners = {};
  // La portion de texture, coin haut-gauche et coin bas-droit.
  glm::vec2 uv_min{0.0F, 0.0F};
  glm::vec2 uv_max{1.0F, 1.0F};
  std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
  // Les facteurs de mélange, dans la numérotation de Direct3D : c'est celle du
  // fichier, et la traduire ici perdrait ce qu'il dit.
  std::int32_t blend_source = 0;
  std::int32_t blend_destination = 0;
  // Le numéro de la texture dans la liste À PLAT de l'effet.
  std::size_t texture = 0;
};

// Les quads d'un effet à une image, dans l'ordre de ses couches — donc dans
// l'ordre où il faut les peindre : le fichier décide de ce qui passe devant.
//
// Une couche qui n'a pas commencé, qui n'a pas de texture ou que sa couleur
// rend invisible ne rend aucun quad.
//
// `frame_fraction` est la part de l'image suivante déjà écoulée, que seules les
// déformations lisent : voir `EffectLayerAt`. Zéro peint l'image entière.
[[nodiscard]] std::vector<EffectQuad> ComposeEffect(const PreparedEffect& effect,
                                                    std::int32_t frame,
                                                    float frame_fraction = 0.0F);

// La même composition, rangée dans `quads`, qu'elle vide d'abord : un appelant
// qui compose à chaque image y garde sa mémoire d'une image à l'autre.
void ComposeEffect(const PreparedEffect& effect, std::int32_t frame, std::vector<EffectQuad>& quads,
                   float frame_fraction = 0.0F);

// Les facteurs de mélange d'un nœud PROCÉDURAL, dans la numérotation de
// Direct3D : la source par son alpha, la destination TELLE QUELLE. C'est une
// ADDITION.
//
// ⚠ CE N'ÉTAIT PAS LE CAS, et la donnée a tranché. Le complément était posé
// ici faute de mieux — « seul le mode par défaut est relevé ». Or les textures
// de ces nœuds ont un FOND NOIR et non une couleur clé : `thunder_ball_a.bmp`
// et `thunder_plazma_blast_a.bmp` sont des BMP de 24 bits, sans alpha, dont le
// noir est la couleur la plus fréquente et dont pas un pixel n'est magenta. En
// recouvrement, ce fond se peint et l'éclair arrive dans un carré noir ; en
// addition, un noir n'ajoute rien et disparaît.
//
// Cela ne se voyait pas sur un fond noir — destination nulle, les deux mélanges
// y donnent le même pixel. Il a fallu un sol clair.
//
// Le client ne passe pas un couple mais un MOT, qui choisit un seau de sa file ;
// un nœud dont le mot est porté voit ce couple remplacé par celui que son seau
// impose — voir `effect_render_word.h`.
inline constexpr std::int32_t kEffectNodeBlendSource = 5;
inline constexpr std::int32_t kEffectNodeBlendDestination = 2;

// Le quad d'un nœud procédural dessiné en BILLBOARD, en pixels d'écran.
//
// ⚠ CE N'EST PAS LE SEUL DESSINATEUR. Le client en a environ cent soixante-dix,
// choisis par le même sous-type que le mouvement du nœud ; celui-ci est le leur
// à tous le plus simple, et celui des runes de buff. Les autres viendront quand
// un effet les réclamera, à côté de celui-ci et non à sa place.
//
// `anchor` est le point où l'effet est joué, DÉJÀ PROJETÉ : le nœud ne sait pas
// projeter, et le client non plus — il range dans ses sommets des coordonnées
// déjà à l'écran, avec un w unitaire. L'orbite du nœud s'y ajoute.
//
// ⚠ LE REPÈRE EST TOURNÉ D'UN QUART DE TOUR par rapport aux axes de la texture :
// à angle nul, la demi-largeur court le long de l'ordonnée et la demi-hauteur
// vers la gauche. C'est ce que le client calcule, et le corriger ferait tourner
// tous ces effets de quatre-vingt-dix degrés.
//
// Le client retranche un demi-pixel aux deux coordonnées : c'est la règle de
// Direct3D 9, qui aligne les centres de texels sur les centres de pixels. OpenGL
// les aligne déjà, et la reprendre décalerait le quad d'un demi-pixel.
[[nodiscard]] EffectQuad ComposeEffectNodeBillboard(const EffectNode& node, glm::vec2 anchor);

}  // namespace ro::render
