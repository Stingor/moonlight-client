#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/act.h"
#include "ro/formats/bmp.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_sprite.h"

namespace ro::render {

// Ce qui se dessine PAR-DESSUS la scène, en pixels d'écran.
//
// Le monde se dessine en panneaux face à la caméra, à des positions du monde ;
// un curseur, un nom, une barre de vie ne sont pas dans le monde. Ils vivent
// dans le repère de la FENÊTRE — origine en haut à gauche, l'ordonnée vers le
// bas, comme un sprite — et rien de leur position ne dépend de la caméra.
//
// Le format est celui d'une DESCRIPTION : `app` la construit, le renderer la
// consomme. C'est ce qui laisse composer et vérifier une superposition sans
// carte graphique.

// Un quad d'écran, en pixels.
//
// ⚠ IL EST ALIGNÉ AUX AXES, et c'est un choix : rien de l'interface ne tourne.
// La voie d'écran des EFFETS, elle, tourne — voir `effect_screen_quad.h`, qui
// porte quatre coins libres pour cette raison. Ramener l'un à l'autre ferait
// perdre la rotation à l'un, ou alourdirait chaque glyphe de texte de l'autre.
struct ScreenQuad {
  // Le coin haut-gauche et le coin bas-droit.
  glm::vec2 min{0.0F, 0.0F};
  glm::vec2 max{0.0F, 0.0F};
  // TROIS provenances possibles, et une seule à la fois :
  //
  //   une image de SPRITE, désignée par sa clé — un curseur, une émoticône ;
  //   un ATLAS et la portion qu'on y découpe — un glyphe de texte ;
  //   RIEN, et le quad est alors d'une seule couleur, celle de sa teinte —
  //   une jauge, un cadre.
  //
  // L'atlas n'est pas possédé : il doit vivre aussi longtemps que le quad.
  SpriteImageKey source;
  const formats::Image* atlas = nullptr;
  glm::vec2 uv_min{0.0F, 0.0F};
  glm::vec2 uv_max{1.0F, 1.0F};
  std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
  // Le miroir échange les abscisses de texture, il ne déplace pas le quad.
  bool mirrored = false;
};

// UN RECTANGLE PLEIN AUX COINS ARRONDIS, en quads.
//
// 🔴 LA COUCHE D'ÉCRAN NE SAIT PEINDRE QUE DES RECTANGLES : il n'y a
// ni triangle ni tracé courbe, et il n'en faut pas pour si peu. L'arrondi se
// fait donc en BANDES HORIZONTALES d'un pixel dans les deux bandes de coin,
// chacune retraitée de ce que le cercle demande à cette hauteur-là ; le milieu
// reste un seul quad. À quatre ou cinq pixels de rayon — la valeur du client —
// l'œil n'y voit qu'une courbe, et cela coûte une dizaine de quads.
//
// Un rayon nul, négatif, ou plus grand que la moitié du plus petit côté se
// ramène à ce qui a un sens : un coin ne peut pas mordre le coin d'en face.
[[nodiscard]] std::vector<ScreenQuad> ComposeRoundedRect(glm::vec2 top_left, glm::vec2 bottom_right,
                                                         float corner_radius_px,
                                                         const std::array<std::uint8_t, 4>& tint);

// Le CADRE d'un rectangle d'écran : quatre quads unis, un par bord.
//
// Les bords se posent À L'INTÉRIEUR du rectangle. Un cadre qui déborderait
// mentirait sur ce qu'il entoure, et deux cadres emboîtés — une emprise et la
// boîte élargie qui la contient — ne se liraient plus l'un dans l'autre.
//
// Les quatre quads ne se RECOUVRENT PAS aux coins : deux quads translucides
// superposés y doubleraient l'opacité, et les coins paraîtraient soulignés.
[[nodiscard]] std::vector<ScreenQuad> ComposeScreenOutline(glm::vec2 min, glm::vec2 max,
                                                           float thickness_px,
                                                           const std::array<std::uint8_t, 4>& tint);

// Les quads d'une image d'un sprite, posée sur un point de l'écran.
//
// `anchor_px` est le point où va l'ORIGINE du sprite — pour un curseur, la
// pointe : c'est le fichier qui décide, par le décalage de ses calques. Les
// coordonnées d'un sprite descendent déjà comme celles d'un écran, il n'y a
// donc rien à retourner ici.
//
// Vide si l'action ou l'image n'existe pas.
[[nodiscard]] std::vector<ScreenQuad> ComposeScreenSprite(const formats::SprSprite& sprite,
                                                          const formats::ActAnimation& animation,
                                                          std::size_t action, std::size_t frame,
                                                          glm::vec2 anchor_px, float scale = 1.0F);

}  // namespace ro::render
