#pragma once

#include <cstdint>
#include <set>
#include <span>
#include <vector>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/emotion.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/world_fwd.h"

namespace moonclient {

// Les émoticônes des acteurs dessinés, chacune au-dessus du sien.
//
// `drawn` et `ids` sont les deux moitiés de ce que la scène vient de composer :
// le quoi et le qui. La bulle se pose au SOMMET de ce que son porteur occupe —
// l'écart au-dessus de la tête est déjà dans le fichier — et elle suit donc la
// taille de qui l'émet, du poring au seigneur des ténèbres, sans rien savoir de
// son sprite.
//
// Une émoticône dont le sprite n'a pas l'action est SIGNALÉE, une fois par
// numéro : sans cela, un serveur qui en annonce une hors table ne produirait
// rien du tout, et rien ne dirait pourquoi.
//
// Sans sprite d'émoticônes, rien ne paraît et le reste fonctionne.
class EmotionBubbles {
 public:
  EmotionBubbles(const ro::formats::SprSprite* sprite, const ro::formats::ActAnimation* animation)
      : sprite_(sprite), animation_(animation) {}

  // Ce qui se voit à cet instant, prêt à dessiner APRÈS les acteurs. Une bulle
  // dont l'animation est finie ne rend aucun calque et disparaît d'elle-même.
  [[nodiscard]] std::span<const ro::render::DrawnActor> Drawn(
      std::uint32_t now_ms, std::span<const ro::render::DrawnActor> drawn,
      std::span<const std::uint32_t> ids, const ro::sim::World& world);

 private:
  const ro::formats::SprSprite* sprite_ = nullptr;
  const ro::formats::ActAnimation* animation_ = nullptr;
  // Les compositions vivent ici : ce que l'on rend en désigne les éléments, et
  // le tableau est réservé d'avance pour qu'aucune ne se déplace en mémoire.
  std::vector<ro::render::ComposedActor> frames_;
  std::vector<ro::render::DrawnActor> shown_;
  // Les numéros et lots déjà signalés : le serveur répète, le journal ne doit
  // pas.
  std::set<std::uint16_t> reported_;
};

}  // namespace moonclient
