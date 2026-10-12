#pragma once

#include <span>

#include <glm/glm.hpp>

#include "ro/render/actor_layers.h"
#include "ro/render/sprite_canvas.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/sprite_vignette.h"

namespace ro::render {

// LE CADRAGE D'UN PORTRAIT : un personnage composé ENTIER, vu de près.
//
// 🔴 « TÊTE SEULE » EST UN CADRAGE, PAS UNE COMPOSITION. La tête s'ancre au
// corps et les coiffes à la tête : retirer le corps les ferait tomber. On
// compose donc tout, et c'est le cadre qui coupe ce qui sort de lui.
//
// ⚠ IL SE REFAIT À CHAQUE IMAGE, sur ce que l'image peint : la tête qui
// respire ou se baisse en attaquant reste au même endroit du cadre. C'est
// l'inverse de la vignette en pied, dont l'échelle se fixe sur l'enveloppe de
// toute l'animation.
//
// ⚠ AUCUNE DÉPENDANCE À OPENGL : tout se vérifie par des nombres.

// CE QUE L'IMAGE PEINT de la tête et du corps, en pixels de sprite autour des
// pieds.
struct PortraitBoxes {
  // La tête ET ses trois coiffes, ancres comprises.
  SpriteBounds head;
  // Le corps seul : ni arme, ni bouclier, ni cape.
  SpriteBounds body;
  // ⚠ DES DRAPEAUX, ET NON DES BORNES NULLES : une boîte jamais touchée est
  // indiscernable d'une boîte vide.
  bool has_head = false;
  bool has_body = false;
};

// Relève les deux boîtes sur les quads PEINTS d'une image, et sur leurs
// parties dans le même ordre. Un quad sans partie compte pour le corps, comme
// `PartOfQuad` le veut.
//
// ⚠ À L'APPELANT DE RETIRER LES QUADS SANS IMAGE : un calque vide porte quand
// même le décalage de son .act, et étirerait la boîte jusque-là.
[[nodiscard]] PortraitBoxes MeasurePortraitBoxes(std::span<const SpriteQuad> quads,
                                                 std::span<const ActorPart> parts);

// CE QUE LE JOUEUR A CHOISI DE VOIR.
struct PortraitAim {
  // Vrai : la tête et ses coiffes remplissent le cadre. Faux : le personnage
  // entier, tête et corps.
  bool head_only = true;
  // × l'échelle qui fait tenir la boîte cadrée dans la LARGEUR du cadre.
  float zoom = 1.0F;
  // Où tombe le point visé, en tailles du cadre, depuis son centre : zéro le
  // laisse au milieu. ⚠ LE ZOOM PIVOTE TOUJOURS AUTOUR DE CE POINT, d'où un
  // décalage qui ne bouge pas quand on zoome.
  glm::vec2 offset{0.0F, 0.0F};
  // L'écart entre la boîte DESSINÉE et le cadre, de chaque côté, en pixels.
  // La vignette ne couvre que l'intérieur du cadre ; le calcul, lui, se fait
  // sur le cadre entier.
  float inset_px = 0.0F;
};

// Ce que la LARGEUR du cadre perd avant l'ajustement : deux pixels de chaque
// côté, que la silhouette ne touche pas.
inline constexpr float kPortraitFitMarginPx = 4.0F;

// Une boîte plus étroite ou plus basse que ce seuil, en pixels de sprite, ne
// se cadre pas : l'échelle exploserait sur un calque d'un pixel.
inline constexpr float kPortraitLeastBoxPx = 1.0F;

// Le cadrage d'une image dans une boîte dessinée de `box_size` pixels, posée à
// `aim.inset_px` du bord du cadre.
//
// LA RÈGLE :
// - la boîte cadrée est la tête en « tête seule », quand l'image en peint
//   une ; sinon l'union de la tête et du corps ;
// - l'échelle fait tenir sa LARGEUR dans celle du cadre, moins la marge, puis
//   se multiplie par le zoom — la hauteur déborde si elle veut ;
// - le point visé est le centre de la tête en « tête seule », la moyenne des
//   centres de la tête et du corps pour le personnage entier, et le centre de
//   la boîte quand l'un des deux manque ;
// - ce point tombe au centre du cadre, décalé de `aim.offset` tailles.
//
// Inutilisable (`usable()` faux) quand l'image ne peint ni tête ni corps, ou
// sur une boîte dégénérée.
[[nodiscard]] VignetteFraming FramePortrait(const PortraitBoxes& boxes, const PortraitAim& aim,
                                            glm::vec2 box_size);

}  // namespace ro::render
