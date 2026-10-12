#pragma once

#include <cstddef>
#include <cstdint>

#include <imgui.h>

#include "ro/render/actor_vignette.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/ui/skin_art.h"

namespace moonclient {

// L'IMAGE DE SON ATTENTE À UN INSTANT : le tour dure quatre secondes, partagées
// également entre les images, quelle que soit la durée que le .act leur donne.
[[nodiscard]] std::size_t WorldMapWarpAgentFrame(std::size_t frames, std::uint32_t now_ms);

// L'ÉCHELLE DU WARP AGENT à un palier de taille : la moitié au premier, deux
// dixièmes de plus à chacun des suivants.
[[nodiscard]] float WorldMapWarpAgentScale(std::size_t size_tier);

// Compose la figure d'un Warp Agent, de face, dans une image qui dure d'une
// image de jeu à l'autre.
class WorldMapWarpAgent {
 public:
  explicit WorldMapWarpAgent(ro::render::ActorWardrobe& wardrobe)
      : wardrobe_(&wardrobe), vignette_(wardrobe) {}

  struct Shown {
    ro::ui::SkinImage image;
    // Ses pieds dans l'image, et sa hauteur au-dessus d'eux, en pixels d'écran.
    ImVec2 feet{0.0F, 0.0F};
    float height = 0.0F;
  };
  // `job` : la classe de PNJ de sa figure, celle que le serveur annonce.
  [[nodiscard]] Shown Compose(std::uint32_t job, float scale, std::uint32_t now_ms);

 private:
  ro::render::ActorWardrobe* wardrobe_;
  ro::render::ActorVignette vignette_;
};

}  // namespace moonclient
