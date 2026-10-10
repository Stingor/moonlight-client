#include "ro/ui/sound_zones.h"

#include "imgui.h"

namespace ro::ui {

namespace {

// L'emprise en bleu, le cercle d'écoute en vert pâle, le trait vers le joueur
// en blanc ; tout vire au jaune à l'instant où la source sonne.
constexpr ImU32 kFootprintColor = IM_COL32(90, 170, 255, 230);
constexpr ImU32 kHearingColor = IM_COL32(120, 230, 150, 150);
constexpr ImU32 kLinkColor = IM_COL32(255, 255, 255, 120);
constexpr ImU32 kSoundingColor = IM_COL32(255, 220, 60, 255);
constexpr ImU32 kLabelColor = IM_COL32(255, 255, 255, 235);
constexpr ImU32 kLabelShadow = IM_COL32(0, 0, 0, 200);
constexpr float kFootprintThickness = 2.0F;
constexpr float kHearingThickness = 1.0F;
constexpr float kEmitterRadius = 4.0F;
constexpr float kShadowOffset = 1.0F;

void DrawClosed(ImDrawList* into, const std::vector<glm::vec2>& outline, ImU32 color,
                float thickness) {
  if (outline.size() < 2) {
    return;
  }
  std::vector<ImVec2> points;
  points.reserve(outline.size());
  for (const glm::vec2 point : outline) {
    points.emplace_back(point.x, point.y);
  }
  into->AddPolyline(points.data(), static_cast<int>(points.size()), color, ImDrawFlags_Closed,
                    thickness);
}

}  // namespace

void DrawSoundZones(std::span<const SoundZoneView> zones) {
  // ⚠ LA LISTE DE FOND : par-dessus le monde, SOUS toutes les fenêtres — comme
  // la grille d'alignement. Au premier plan, les zones couvriraient l'établi
  // même qui les allume.
  ImDrawList* into = ImGui::GetBackgroundDrawList();
  for (const SoundZoneView& zone : zones) {
    DrawClosed(into, zone.hearing, zone.just_played ? kSoundingColor : kHearingColor,
               kHearingThickness);
    DrawClosed(into, zone.footprint, zone.just_played ? kSoundingColor : kFootprintColor,
               kFootprintThickness);
    if (zone.emitter.has_value()) {
      const ImVec2 point(zone.emitter->x, zone.emitter->y);
      if (zone.listener.has_value()) {
        into->AddLine(point, ImVec2(zone.listener->x, zone.listener->y), kLinkColor);
      }
      into->AddCircleFilled(point, kEmitterRadius,
                            zone.just_played ? kSoundingColor : kFootprintColor);
    }
    if (zone.label_at.has_value() && !zone.label.empty()) {
      const ImVec2 at(zone.label_at->x, zone.label_at->y);
      into->AddText(ImVec2(at.x + kShadowOffset, at.y + kShadowOffset), kLabelShadow,
                    zone.label.c_str());
      into->AddText(at, kLabelColor, zone.label.c_str());
    }
  }
}

}  // namespace ro::ui
