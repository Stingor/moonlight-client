#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/camera.h"

namespace ro::render {

// LA QUATRIÈME VOIE DE SORTIE : un quad dont les coins sont des PIXELS.
//
// Voir l'ADR 0022. Les trois voies du lot 8 rendent des points du MONDE, que la
// caméra projette ensuite ; celle-ci rend des points déjà projetés — le client
// leur pose une profondeur d'un millionième et un `rhw` de 0,999999, si bien que
// rien ne les transforme plus.
//
// ⚠ CE N'EST PAS UNE VARIANTE D'`EffectWorldQuad`, et les mélanger serait le
// porter faux : un quad du monde projeté deux fois disparaît. Le type est donc
// distinct, et c'est le compilateur qui tient la frontière.
//
// ⚠ ET CE N'EST PAS NON PLUS LE `ScreenQuad` DE `screen_overlay.h`, bien que les
// deux soient en pixels. Celui-là est un rectangle ALIGNÉ AUX AXES — un coin
// haut-gauche, un coin bas-droit —, parce que rien de l'interface ne tourne :
// une barre de vie, un glyphe, un curseur se posent droit. Celui-ci porte
// QUATRE COINS LIBRES parce que sa figure TOURNE, et c'est précisément ce que
// deux points ne peuvent pas décrire.
//
// Les fusionner coûterait l'un ou l'autre : donner quatre coins au quad
// d'interface alourdirait chaque glyphe de texte, et ramener celui-ci à deux
// points lui ferait perdre sa rotation — la seule chose que son dessinateur
// calcule.

// LE QUAD D'ÉCRAN QU'UNE FIGURE REND.
struct EffectScreenQuad {
  // LES QUATRE COINS, EN PIXELS, EN CYCLE : haut-gauche, bas-gauche, bas-droit,
  // haut-droit de l'image.
  //
  // ⚠ CE N'EST PAS L'ORDRE DU CLIENT, qui soumet deux triangles dont le second
  // reprend deux sommets du premier. Le renderer referme un quad en tournant
  // autour ; recopier la suite du client y ferait un papillon.
  //
  // ⚠ L'ORDONNÉE DESCEND. C'est le repère de l'écran, pas celui du monde, et
  // les deux se croisent : ce qui doit s'élever a une ordonnée qui DIMINUE.
  std::array<glm::vec2, 4> corners = {};
  std::array<glm::vec2, 4> texture_uv = {};
  std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
  // ⚠ LE MODE DE MÉLANGE EST UN SEUL NOMBRE, pas une paire de facteurs. Le
  // client le passe TEL QUEL à sa file de rendu, qui le décompose — et cette
  // décomposition-là n'est pas connue. L'écrire en deux facteurs maintenant
  // serait en inventer la moitié.
  std::int32_t blend_mode = 0;
  // Le numéro de la texture dans la liste du nœud : c'est l'image courante de
  // l'animation, que le mouvement avance.
  std::size_t texture = 0;
};

// ⚠ LE DEMI-PIXEL QUE LE CLIENT RETIRE À CHAQUE COIN. C'est la correction de
// centre de texel de Direct3D 9, pas une marge : l'oublier décale l'image d'un
// demi-pixel, ce qui se voit sur une icône de seize.
inline constexpr float kEffectScreenTexelOffset = 0.5F;

// CE QU'UN QUAD D'ÉCRAN DEMANDE POUR SE DESSINER.
//
// Ce que la figure lit dans son nœud : ni miroir, ni billboard, ni troupe —
// c'est un quad plat, tourné d'un angle.
struct EffectScreenQuadRequest {
  // OÙ LE QUAD SE POSE, en pixels. L'appelant l'a déjà composée : le décalage du
  // nœud, plus la position d'écran de son porteur quand le nœud est relatif.
  glm::vec2 screen_position = {0.0F, 0.0F};
  float half_width = 0.0F;
  float half_height = 0.0F;
  // L'ANGLE EN DEGRÉS D'ÉCRAN. Le client indexe deux tables de sinus par un
  // entier ; nous calculons la même valeur.
  std::int32_t turn_degrees = 0;
  std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
  // Voir `EffectScreenQuad::blend_mode` : UN nombre, pas deux facteurs.
  std::int32_t blend_mode = 0;
  std::size_t texture = 0;
  // ⚠ VRAI QUAND LE QUAD PEND SOUS SON POINT au lieu d'être centré dessus. Le
  // drapeau du client ne change pas la TAILLE — la hauteur totale reste deux
  // demi-hauteurs — mais l'ORIGINE : centré, il va de moins une demi-hauteur à
  // plus une ; pendant, il va de moins DEUX à zéro. Le confondre avec une
  // échelle poserait la moitié de ces effets à mi-hauteur.
  bool hangs_below_anchor = false;
};

// LE QUAD, TOURNÉ ET POSÉ.
//
// ⚠ LA ROTATION ÉCHANGE LE SINUS ET LE COSINUS par rapport à la convention
// usuelle : un point local (u, v) devient (u·sin − v·cos, u·cos + v·sin).
// Recopier la formule habituelle tournerait la figure d'un quart de tour.
//
// Le client soumet ce quad en DEUX triangles séparés, dont le second reprend
// deux sommets du premier ; nous rendons le quad, et c'est la sortie qui le
// découpe — un lot de rendu au lieu de deux, pour la même image.
//
// Rend un lot VIDE quand une des deux demi-tailles est nulle : un quad sans aire
// ne se distingue pas d'une absence, et en soumettre paierait un lot pour rien.
[[nodiscard]] std::vector<EffectScreenQuad> ComposeScreenQuad(
    const EffectScreenQuadRequest& request);

// OÙ UN POINT DU MONDE TOMBE SUR L'ÉCRAN, en pixels.
//
// Un nœud de cette voie se pose par rapport à son porteur, dont le client tient
// la position d'écran DÉJÀ PROJETÉE. Nous la recalculons à chaque image : la
// projeter une fois pour toutes au semis figerait le nœud dès que la caméra
// bouge.
//
// L'origine est le coin HAUT-GAUCHE et l'ordonnée DESCEND — le repère de la
// souris et celui d'un sprite.
//
// ⚠ REND UN VIDE QUAND LE POINT EST DERRIÈRE LA CAMÉRA. Sa coordonnée homogène
// est alors nulle ou négative, et diviser quand même enverrait le point à
// l'OPPOSÉ de l'écran, où il se dessinerait retourné. Un porteur hors champ n'a
// pas de position d'écran ; dire « nulle part » est la seule réponse juste.
[[nodiscard]] std::optional<glm::vec2> EffectScreenPositionOf(const Camera& camera,
                                                              glm::vec3 world_position,
                                                              float viewport_width,
                                                              float viewport_height);

}  // namespace ro::render
