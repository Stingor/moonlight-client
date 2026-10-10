#include "ro/ui/zone_capture.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <span>

#include <imgui.h>

#include "ro/ui/skin_palette.h"

namespace ro::ui {
namespace {

// Ce qui assombrit le reste de l'écran pendant le tracé : assez pour que le
// rectangle clair se détache, pas assez pour qu'on ne voie plus ce qu'on cadre.
constexpr ImU32 kOutsideVeil = IM_COL32(0, 0, 0, 110);
// Le trait du rectangle en cours et celui de la zone mémorisée.
constexpr ImU32 kZoneLineColor = IM_COL32(255, 216, 96, 255);
constexpr ImU32 kRecordingLineColor = IM_COL32(236, 84, 84, 255);
constexpr float kZoneLineThickness = 2.0F;
// Le fond du témoin, et sa marge intérieure.
constexpr ImU32 kBadgeColor = IM_COL32(16, 18, 24, 205);
constexpr float kBadgePadding = 6.0F;
constexpr float kBadgeRounding = 4.0F;
// La marge qu'il faut trouver autour de la zone pour poser le témoin sans
// empiéter dessus.
constexpr float kBadgeGap = 4.0F;

[[nodiscard]] ImVec2 Corner(const ZoneRect& zone) {
  return ImVec2(static_cast<float>(zone.left), static_cast<float>(zone.top));
}

[[nodiscard]] ImVec2 FarCorner(const ZoneRect& zone) {
  return ImVec2(static_cast<float>(zone.left + zone.width),
                static_cast<float>(zone.top + zone.height));
}

// Le rectangle entre deux coins, quel que soit le sens du glissement : on trace
// aussi bien de bas à droite vers haut à gauche.
[[nodiscard]] ZoneRect Between(ImVec2 one, ImVec2 other) {
  const int left = static_cast<int>(std::min(one.x, other.x));
  const int top = static_cast<int>(std::min(one.y, other.y));
  return ZoneRect{.left = left,
                  .top = top,
                  .width = static_cast<int>(std::max(one.x, other.x)) - left,
                  .height = static_cast<int>(std::max(one.y, other.y)) - top};
}

// ⚠ LE CADRE BORDE LA ZONE PAR L'EXTÉRIEUR. Tracé dessus, il mangerait une
// rangée de pixels de chaque côté de la capture.
void OutlineOutside(ImDrawList* into, const ZoneRect& zone, ImU32 color) {
  const ImVec2 near_corner(Corner(zone).x - kZoneLineThickness,
                           Corner(zone).y - kZoneLineThickness);
  const ImVec2 far_corner(FarCorner(zone).x + kZoneLineThickness,
                          FarCorner(zone).y + kZoneLineThickness);
  into->AddRect(near_corner, far_corner, color, 0.0F, 0, kZoneLineThickness);
}

// Où poser le témoin sans toucher à la zone : au-dessus si la place y est, sinon
// dessous, sinon à droite, sinon à gauche.
//
// 🔴 SINON NULLE PART. Un témoin qu'on pousserait dans la zone faute de marge se
// retrouverait dans toutes les images de la prise, ce qui est précisément ce que
// l'on tient à éviter. Mieux vaut ne rien montrer.
[[nodiscard]] bool PlaceBadge(const ZoneRect& zone, ImVec2 size, ImVec2* at) {
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  const float needed_y = size.y + kBadgeGap * 2.0F;
  const float needed_x = size.x + kBadgeGap * 2.0F;
  const auto left = static_cast<float>(zone.left);
  const auto top = static_cast<float>(zone.top);
  const auto right = static_cast<float>(zone.left + zone.width);
  const auto bottom = static_cast<float>(zone.top + zone.height);

  if (top >= needed_y) {
    *at = ImVec2(left, top - size.y - kBadgeGap);
    return true;
  }
  if (screen.y - bottom >= needed_y) {
    *at = ImVec2(left, bottom + kBadgeGap);
    return true;
  }
  if (screen.x - right >= needed_x) {
    *at = ImVec2(right + kBadgeGap, top);
    return true;
  }
  if (left >= needed_x) {
    *at = ImVec2(left - size.x - kBadgeGap, top);
    return true;
  }
  return false;
}

void PaintBadge(ImDrawList* into, ImVec2 at, const char* text, ImU32 tint) {
  const ImVec2 size = ImGui::CalcTextSize(text);
  into->AddRectFilled(ImVec2(at.x - kBadgePadding, at.y - kBadgePadding),
                      ImVec2(at.x + size.x + kBadgePadding, at.y + size.y + kBadgePadding),
                      kBadgeColor, kBadgeRounding);
  into->AddText(at, tint, text);
}

// Le texte du témoin, selon l'étape. Rend faux quand il n'y a rien à dire.
[[nodiscard]] bool BadgeText(const ZoneCaptureView& view, std::span<char> into, ImU32* tint) {
  *tint = IM_COL32(255, 255, 255, 255);
  switch (view.phase) {
    case ZoneCapturePhase::kCountdown:
      std::snprintf(into.data(), into.size(), "%s %d", core::Tr("Départ dans"), view.seconds_left);
      return true;
    case ZoneCapturePhase::kRecording:
      *tint = kRecordingLineColor;
      std::snprintf(into.data(), into.size(), "%s  %d / %d", core::Tr("Enregistrement"),
                    view.frames_taken, view.frames_wanted);
      return true;
    case ZoneCapturePhase::kEncoding:
      std::snprintf(into.data(), into.size(), "%s", core::Tr("Préparation du GIF…"));
      return true;
    case ZoneCapturePhase::kIdle:
    case ZoneCapturePhase::kSelecting:
      return false;
  }
  return false;
}

}  // namespace

void DrawZoneSelection(const ZoneCaptureView& view, const SkinArt& art, ZoneTraceState& trace,
                       ZoneCaptureAsk* asked) {
  static_cast<void>(art);
  if (view.phase != ZoneCapturePhase::kSelecting || asked == nullptr) {
    trace = ZoneTraceState();
    return;
  }
  bool& dragging = trace.dragging;
  ImVec2& anchor = trace.anchor;

  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
  ImGui::SetNextWindowSize(screen);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
  // 🔴 LA TOILE PASSE DEVANT TOUTES LES FENÊTRES, à chaque image du tracé. On
  // la demande depuis les options : la fenêtre qui a le focus resterait sinon
  // au-dessus et prendrait le clic qui commence le rectangle.
  ImGui::SetNextWindowFocus();
  const bool open = ImGui::Begin("##trace_de_zone", nullptr,
                                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
  if (open) {
    ImGui::InvisibleButton("##toile_de_trace", screen, ImGuiMouseButton_Left);
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    if (ImGui::IsItemActive() && !dragging) {
      dragging = true;
      anchor = mouse;
    }
    ZoneRect drawn = dragging ? Between(anchor, mouse) : ZoneRect{};

    ImDrawList* into = ImGui::GetWindowDrawList();
    // Le voile, en quatre bandes autour du rectangle : un voile plein recouvert
    // d'un trou demanderait un masque que ce dessin n'a pas.
    if (drawn.empty()) {
      into->AddRectFilled(ImVec2(0.0F, 0.0F), screen, kOutsideVeil);
    } else {
      const ImVec2 near_corner = Corner(drawn);
      const ImVec2 far_corner = FarCorner(drawn);
      into->AddRectFilled(ImVec2(0.0F, 0.0F), ImVec2(screen.x, near_corner.y), kOutsideVeil);
      into->AddRectFilled(ImVec2(0.0F, far_corner.y), screen, kOutsideVeil);
      into->AddRectFilled(ImVec2(0.0F, near_corner.y), ImVec2(near_corner.x, far_corner.y),
                          kOutsideVeil);
      into->AddRectFilled(ImVec2(far_corner.x, near_corner.y), ImVec2(screen.x, far_corner.y),
                          kOutsideVeil);
      OutlineOutside(into, drawn, kZoneLineColor);

      std::array<char, 64> size_text = {};
      std::snprintf(size_text.data(), size_text.size(), "%d x %d", drawn.width, drawn.height);
      ImVec2 at;
      if (PlaceBadge(drawn, ImGui::CalcTextSize(size_text.data()), &at)) {
        PaintBadge(into, at, size_text.data(), IM_COL32(255, 255, 255, 255));
      }
    }

    // La consigne, en haut au centre : c'est le seul moment où l'outil demande
    // quelque chose au joueur.
    const char* hint = core::Tr("Trace un rectangle à la souris.  Échap annule.");
    const ImVec2 hint_size = ImGui::CalcTextSize(hint);
    PaintBadge(into, ImVec2((screen.x - hint_size.x) * 0.5F, hint_size.y), hint,
               IM_COL32(255, 255, 255, 255));

    if (dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      dragging = false;
      asked->drawn = true;
      asked->zone = drawn;
    }
    // ⚠ ÉCHAP ANNULE MÊME EN PLEIN GLISSEMENT : un tracé commencé par erreur ne
    // doit pas obliger à le finir pour s'en défaire.
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
      dragging = false;
      asked->cancelled = true;
    }
  }
  ImGui::End();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
}

void DrawCaptureBadge(const ZoneCaptureView& view, const SkinArt& art) {
  static_cast<void>(art);
  if (view.zone.empty()) {
    return;
  }
  // 🔴 LA LISTE DU PREMIER PLAN : le témoin doit rester visible par-dessus les
  // fenêtres, sans quoi celle qu'on est en train de montrer le cacherait.
  ImDrawList* into = ImGui::GetForegroundDrawList();

  const bool live =
      view.phase == ZoneCapturePhase::kRecording || view.phase == ZoneCapturePhase::kCountdown;
  if (view.show_zone || live) {
    OutlineOutside(into, view.zone, live ? kRecordingLineColor : kZoneLineColor);
  }

  std::array<char, 96> text = {};
  ImU32 tint = 0;
  if (!BadgeText(view, text, &tint)) {
    return;
  }
  ImVec2 at;
  if (PlaceBadge(view.zone, ImGui::CalcTextSize(text.data()), &at)) {
    PaintBadge(into, at, text.data(), tint);
  }
}

}  // namespace ro::ui
