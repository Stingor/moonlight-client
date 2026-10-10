#pragma once

#include <vector>

#include <imgui.h>

#include "ro/render/camera.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/overlay_renderer.h"
#include "ro/render/screen_overlay.h"

#include "game/over_world_paint.h"

namespace moonclient {

class EffectNodesInPlay;

// CE QUI SE PEINT PAR-DESSUS TOUT LE MONDE ET SOUS LES FENÊTRES.
//
// 🔴 LE VOILE DE TÉNÈBRES CACHE TOUT CE QUI APPARTIENT AU MONDE, et ce client
// en fait une règle d'écran : acteurs, effets, jauges et noms au-dessus des
// têtes, bulles de parole, chiffres de dégâts passent DESSOUS ; les fenêtres
// d'interface restent au-dessus. Or une partie de ce monde — les barres
// d'entité, les bulles — se peint dans la liste de fond d'ImGui, donc pendant
// que l'interface se dessine. Le voile se peint donc là aussi, par un rappel
// glissé juste après elles, avant toute fenêtre.
//
// ⚠ ET LE HUD DE JEU PASSE AVEC LUI, JUSTE APRÈS : le classement PvP, l'étiquette
// de ce qui est armé, le grimoire et le banc d'atelier sont peints en pixels
// avant l'interface ; les laisser là les mettrait sous le voile. Ils sont
// récoltés pendant le dessin du monde, et peints ici, par-dessus le voile.
//
// Les deux passes, dans l'ordre : `kOverWorldPasses`.
class OverWorldLayer {
 public:
  // Ce que la partie prête à chaque image, avant l'interface. Les dimensions
  // sont celles dans lesquelles chaque passe a été composée.
  struct Painters {
    ro::render::EffectRenderer* effects = nullptr;
    ro::render::OverlayRenderer* overlay = nullptr;
    EffectNodesInPlay* nodes = nullptr;
    const ro::render::Camera* camera = nullptr;
    float scene_width = 0.0F;
    float scene_height = 0.0F;
    float hud_width = 0.0F;
    float hud_height = 0.0F;
  };

  // Ouvre l'image : les peintres de cette image, et un HUD vide.
  void Prepare(const Painters& painters);

  // Où le dessin du monde dépose le HUD de jeu, au lieu de le peindre.
  [[nodiscard]] std::vector<ro::render::ScreenQuad>& game_hud() noexcept { return game_hud_; }

  // Le rappel à glisser dans l'interface.
  [[nodiscard]] OverWorldPaint Paint() noexcept { return {&OverWorldLayer::PaintCallback, this}; }

 private:
  static void PaintCallback(const ImDrawList* list, const ImDrawCmd* command);
  void PaintNow();

  Painters painters_;
  std::vector<ro::render::ScreenQuad> game_hud_;
};

}  // namespace moonclient
