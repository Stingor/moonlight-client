#pragma once

#include <cstddef>
#include <span>

#include <glm/glm.hpp>

#include "ro/render/actor_layers.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/sprite_canvas.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/sprite_vignette.h"

namespace ro::render {

// LE CADRAGE D'UN PORTRAIT : un personnage composé ENTIER, vu de près.
//
// 🔴 « TÊTE SEULE » EST UN CADRAGE, PAS UNE COMPOSITION. La tête s'ancre au
// corps et les coiffes à la tête : retirer le corps les ferait tomber. On
// compose donc tout, et c'est le cadre qui coupe ce qui sort de lui.
//
// 🔴 UN CADRAGE PAR POSE, ET NON PAR IMAGE : les boîtes se mesurent sur la
// BOUCLE ENTIÈRE — chaque image du corps dans la direction montrée, et chaque
// instant des pièces à horloge propre. Mesurées sur l'image courante, elles
// bougeraient avec une coiffe ou une cape animée, et le personnage sauterait
// dans son cadre ; sur la boucle, il s'anime DANS un cadre fixe, comme en jeu.
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

// Les deux boîtes de ce qu'une composition PEINT : les calques sans image ne
// comptent pas.
[[nodiscard]] PortraitBoxes MeasurePaintedPortraitBoxes(const ComposedActor& composed);

// LA POSE DONT ON MESURE LA BOUCLE.
struct PortraitLoop {
  // Le RANG d'animation et la direction du .act — voir `ActorVignette::Pose`.
  std::size_t animation = kAnimationIdle;
  std::size_t direction = 0;
  // Le corps joue-t-il ses images ? Faux, il tient `rest_frame` ; les pièces à
  // horloge propre bougent dans les deux cas.
  bool body_plays = false;
  std::size_t rest_frame = 0;
};

// Les deux boîtes de TOUTE la boucle de cette pose : l'union de ce que peint
// chacun de ses échantillons (`DollEnvelopeProbes`) — chaque image de chaque
// pièce, sur chaque image du corps.
//
// ⚠ UNE COMPOSITION PAR ÉCHANTILLON : à faire une fois par pose, jamais à
// chaque image — c'est le rôle de `PortraitFramer`.
[[nodiscard]] PortraitBoxes MeasureLoopPortraitBoxes(ActorOutfit& outfit, const PortraitLoop& loop);

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

// LE CADRAGE STABLE D'UN PORTRAIT : les boîtes de la boucle, mesurées une fois
// par pose, puis la règle de `FramePortrait` appliquée à chaque appel.
//
// La mesure se refait quand la tenue (l'apparence, ou la tenue que la
// garde-robe rend pour elle), l'animation, la direction ou le jeu du corps
// changent. Le choix du joueur — tête seule, zoom, décalages, taille du cadre
// — ne la refait pas : il ne change que la règle appliquée aux mêmes boîtes.
// La cape, elle, change l'apparence.
class PortraitFramer {
 public:
  [[nodiscard]] VignetteFraming Frame(ActorOutfit& outfit, const ActorLook& look,
                                      const PortraitLoop& loop, const PortraitAim& aim,
                                      glm::vec2 box_size);

  // Combien de boucles ont été mesurées depuis la naissance : ce qui prouve
  // que la mesure ne tourne pas à chaque image.
  [[nodiscard]] std::size_t measure_count() const noexcept { return measure_count_; }

 private:
  // ⚠ L'ADRESSE DE LA TENUE NE SUFFIT PAS : une tenue oubliée par la
  // garde-robe peut rendre la sienne à une autre apparence. L'apparence seule
  // non plus : oubliée puis refaite, sa tenue n'est plus la même.
  const ActorOutfit* outfit_ = nullptr;
  ActorLook look_;
  PortraitLoop loop_;
  PortraitBoxes boxes_;
  bool measured_ = false;
  std::size_t measure_count_ = 0;
};

}  // namespace ro::render
