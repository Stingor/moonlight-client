#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/formats/image.h"
#include "ro/render/ground_quad_renderer.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/companion_ai_window.h"

#include "game/companion_ai_stage.h"
#include "game/companion_ai_zones.h"
#include "world/map_stage.h"

namespace moonclient {

// CE QU'IL FAUT POUR POSER LES ZONES DE L'IA d'une image.
struct CompanionZonesFrame {
  const ro::sim::World& world;
  // Nul avant la première carte.
  const MapStage* stage = nullptr;
  const CompanionAiStage& companions;
  const ro::ui::CompanionAiWindowState& window;
  const ro::ui::CompanionAiView& view;
  // Ce que la fenêtre a rendu à cette image : le curseur survolé.
  const ro::ui::CompanionAiWindowAction& answer;
  // La case « Montrer les zones au sol ».
  bool kept_shown = false;
  std::uint32_t now_ms = 0;
};

// LES ZONES DE L'IA D'UN COMPAGNON, posées sur le relief autour du maître.
//
// La figure (`game/companion_ai_zones.h`) sait quelles cases peindre ; ici on
// sait où est le maître, ce que la fenêtre règle et sur quel sol poser les
// cases. ⚠ CHAQUE CASE ÉPOUSE SA CELLULE, comme le curseur au sol et la zone
// des sorts, par `ro::render::ComposeGroundCellQuad`.
class CompanionZonesView {
 public:
  // Refait le lot de cette image : vide quand les zones ne se montrent pas.
  void Compose(const CompanionZonesFrame& frame);
  // Le lot à peindre, jusqu'au prochain `Compose`.
  [[nodiscard]] std::span<const ro::render::DrawnGroundQuads> lots() const noexcept {
    return lots_;
  }

 private:
  std::vector<ZoneCellPaint> paints_;
  std::vector<ro::render::GroundQuad> quads_;
  std::vector<ro::render::DrawnGroundQuads> lots_;
  // LE CARREAU PLEIN DE LA ZONE DES SORTS, bâti une fois : la teinte du quad
  // donne la couleur, l'image la forme.
  std::optional<ro::formats::Image> tile_;
  // Le chemin que le lot désigne doit survivre à l'appel : le renderer indexe
  // son téléversement dessus.
  std::string texture_path_;
};

}  // namespace moonclient
