#pragma once

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/world_fwd.h"

namespace moonclient {

// LES MARQUES DE QUÊTE au-dessus des PNJ dessinés — voir
// `ro/render/quest_marker.h` pour l'image et la tenue, `ro/sim/quest_markers.h`
// pour ce que le serveur en dit.
//
// Chaque marque est un sprite-enfant de son PNJ : elle naît quand le PNJ paraît
// avec une marque, et son animation repart de zéro quand l'effet ou la couleur
// change, ou quand le PNJ revient après être sorti de vue. Le même contrat que
// `SleepBubbles` pour le reste.
class QuestMarkerSprites {
 public:
  QuestMarkerSprites(const ro::formats::SprSprite* sprite,
                     const ro::formats::ActAnimation* animation)
      : sprite_(sprite), animation_(animation) {}

  // Ce qui se voit à cet instant, prêt à dessiner APRÈS la scène, sans épreuve
  // de profondeur. `translucent` suit le réglage des invocations translucides.
  [[nodiscard]] std::span<const ro::render::DrawnActor> Drawn(
      std::uint32_t now_ms, std::span<const ro::render::DrawnActor> drawn,
      std::span<const std::uint32_t> ids, const ro::sim::World& world, bool translucent);

 private:
  // La marque qu'un PNJ porte, et depuis quand.
  struct Held {
    std::uint16_t effect = 0;
    std::uint16_t color = 0;
    std::uint32_t since_ms = 0;
  };

  const ro::formats::SprSprite* sprite_ = nullptr;
  const ro::formats::ActAnimation* animation_ = nullptr;
  std::unordered_map<std::uint32_t, Held> held_;
  // Les compositions vivent ici, réservées d'avance : ce que l'on rend en
  // désigne les éléments.
  std::vector<ro::render::ComposedActor> frames_;
  std::vector<ro::render::DrawnActor> shown_;
};

}  // namespace moonclient
