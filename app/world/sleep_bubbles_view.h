#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/world_fwd.h"

// LES SPRITES-ENFANTS QUI TIENNENT SUR UN ACTEUR TANT QU'UN ÉTAT DURE : les
// bulles du sommeil, les points de suspension du silence, les étoiles de
// l'étourdissement et le bloc de glace du gel. Tous suivent la même tenue — voir
// `ro::render::ComposeHeldChildSprite`.
namespace moonclient {

// LES BULLES DU SOMMEIL des acteurs dessinés, chacune au-dessus des pieds du
// sien — voir `ro/render/sleep_bubbles.h`.
//
// `drawn` et `ids` sont les deux moitiés de ce que la scène vient de composer :
// le quoi et le qui. Sans sprite de bulles, rien ne paraît et le reste
// fonctionne.
class SleepBubbles {
 public:
  SleepBubbles(const ro::formats::SprSprite* sprite, const ro::formats::ActAnimation* animation)
      : sprite_(sprite), animation_(animation) {}

  // Ce qui se voit à cet instant, prêt à dessiner APRÈS la scène, sans épreuve
  // de profondeur. `translucent` suit le réglage des invocations translucides.
  [[nodiscard]] std::span<const ro::render::DrawnActor> Drawn(
      std::uint32_t now_ms, std::span<const ro::render::DrawnActor> drawn,
      std::span<const std::uint32_t> ids, const ro::sim::World& world, bool translucent);

 private:
  const ro::formats::SprSprite* sprite_ = nullptr;
  const ro::formats::ActAnimation* animation_ = nullptr;
  // Les compositions vivent ici, réservées d'avance : ce que l'on rend en
  // désigne les éléments.
  std::vector<ro::render::ComposedActor> frames_;
  std::vector<ro::render::DrawnActor> shown_;
};

// LES POINTS DE SUSPENSION du silence, au-dessus des acteurs dessinés que le
// silence tient — voir `ro/render/silence_dots.h`. Même contrat que
// `SleepBubbles` ; le sprite est celui des émoticônes.
class SilenceDots {
 public:
  SilenceDots(const ro::formats::SprSprite* sprite, const ro::formats::ActAnimation* animation)
      : sprite_(sprite), animation_(animation) {}

  [[nodiscard]] std::span<const ro::render::DrawnActor> Drawn(
      std::uint32_t now_ms, std::span<const ro::render::DrawnActor> drawn,
      std::span<const std::uint32_t> ids, const ro::sim::World& world, bool translucent);

 private:
  const ro::formats::SprSprite* sprite_ = nullptr;
  const ro::formats::ActAnimation* animation_ = nullptr;
  std::vector<ro::render::ComposedActor> frames_;
  std::vector<ro::render::DrawnActor> shown_;
};

// LES ÉTOILES au-dessus des acteurs dessinés que l'étourdissement tient — voir
// `ro/render/ailment_sprites.h`. Même contrat que `SleepBubbles`.
class StunStars {
 public:
  StunStars(const ro::formats::SprSprite* sprite, const ro::formats::ActAnimation* animation)
      : sprite_(sprite), animation_(animation) {}

  [[nodiscard]] std::span<const ro::render::DrawnActor> Drawn(
      std::uint32_t now_ms, std::span<const ro::render::DrawnActor> drawn,
      std::span<const std::uint32_t> ids, const ro::sim::World& world, bool translucent);

 private:
  const ro::formats::SprSprite* sprite_ = nullptr;
  const ro::formats::ActAnimation* animation_ = nullptr;
  std::vector<ro::render::ComposedActor> frames_;
  std::vector<ro::render::DrawnActor> shown_;
};

// LA MARQUE DU MAUDIT au-dessus des pieds des acteurs dessinés que la
// malédiction tient — voir `ro/render/ailment_sprites.h`. Son action suit la
// face d'écran du corps, d'où l'angle de la caméra.
//
// ⚠ À PEINDRE DANS LA SCÈNE comme la glace : selon la face, elle passe devant ou
// derrière le corps.
class CurseMarks {
 public:
  CurseMarks(const ro::formats::SprSprite* sprite, const ro::formats::ActAnimation* animation)
      : sprite_(sprite), animation_(animation) {}

  [[nodiscard]] std::span<const ro::render::DrawnActor> Drawn(
      std::uint32_t now_ms, float camera_yaw_degrees, std::span<const ro::render::DrawnActor> drawn,
      std::span<const std::uint32_t> ids, const ro::sim::World& world, bool translucent);

 private:
  const ro::formats::SprSprite* sprite_ = nullptr;
  const ro::formats::ActAnimation* animation_ = nullptr;
  std::vector<ro::render::ComposedActor> frames_;
  std::vector<ro::render::DrawnActor> shown_;
};

// LE BLOC DE GLACE aux pieds des acteurs dessinés que le gel tient — voir
// `ro/render/ailment_sprites.h`.
//
// ⚠ À PEINDRE DANS LA SCÈNE, avec l'épreuve de profondeur et sans l'écrire,
// après les corps : chaque bloc porte son avance sur le corps qu'il enferme.
class FrozenIce {
 public:
  FrozenIce(const ro::formats::SprSprite* sprite, const ro::formats::ActAnimation* animation)
      : sprite_(sprite), animation_(animation) {}

  [[nodiscard]] std::span<const ro::render::DrawnActor> Drawn(
      std::uint32_t now_ms, std::span<const ro::render::DrawnActor> drawn,
      std::span<const std::uint32_t> ids, const ro::sim::World& world, bool translucent);

 private:
  const ro::formats::SprSprite* sprite_ = nullptr;
  const ro::formats::ActAnimation* animation_ = nullptr;
  std::vector<ro::render::ComposedActor> frames_;
  std::vector<ro::render::DrawnActor> shown_;
};

}  // namespace moonclient
