#include "ro/ui/path_canvas.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "ro/core/i18n.h"
#include "ro/render/number_look.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"

namespace ro::ui {
namespace {

// Les couleurs du cadre : le fond et le sol de celui des entités.
constexpr ImU32 kCanvasBackground = IM_COL32(24, 26, 32, 220);
constexpr ImU32 kCanvasGround = IM_COL32(255, 255, 255, 40);
constexpr float kCanvasRounding = 4.0F;
constexpr float kGroundInsetPixels = 12.0F;

// La trajectoire, en tirets, et l'ellipse de la dispersion.
constexpr ImU32 kTraceColor = IM_COL32(255, 255, 255, 150);
constexpr ImU32 kScatterColor = IM_COL32(120, 200, 255, 110);
constexpr float kTraceThickness = 1.5F;
// Le nombre de pas qui découpent le chemin : un sur deux se trace, et ce sont
// les tirets.
constexpr int kTraceSteps = 32;
// Le trait qui relie une poignée au point qu'elle tire.
constexpr ImU32 kHandleLineColor = IM_COL32(90, 210, 255, 140);
constexpr float kHandleLineThickness = 1.0F;

// Le point où le client fait naître le nombre, en cercle pâle : l'origine qu'on
// quitte en tirant le départ.
constexpr ImU32 kBirthColor = IM_COL32(255, 255, 255, 90);
constexpr float kBirthRadius = 4.0F;

// Les ancres : une couleur par sorte, un liseré sombre qui les détache des
// nombres, et un anneau pour celle qu'on survole ou qu'on tient.
constexpr ImU32 kStartColor = IM_COL32(110, 230, 120, 255);
constexpr ImU32 kArrivalColor = IM_COL32(255, 170, 60, 255);
constexpr ImU32 kBendColor = IM_COL32(90, 210, 255, 255);
constexpr ImU32 kRadiusColor = IM_COL32(120, 200, 255, 255);
constexpr ImU32 kAnchorEdge = IM_COL32(0, 0, 0, 220);
constexpr ImU32 kAnchorRing = IM_COL32(255, 255, 255, 230);
constexpr float kAnchorRadius = 5.0F;
constexpr float kAnchorEdgeThickness = 1.5F;
constexpr float kAnchorRingRadius = 9.0F;

// Aucune ancre tenue. L'ancre tenue se range dans le stockage d'ImGui, sous
// l'identité du bouton invisible du cadre : elle survit d'une image à l'autre
// tant que le bouton reste enfoncé.
constexpr int kNoAnchor = -1;

// La grille : ses lignes, et les deux axes qui passent par la naissance.
constexpr ImU32 kGridColor = IM_COL32(255, 255, 255, 18);
constexpr ImU32 kGridAxisColor = IM_COL32(255, 255, 255, 45);
// ⚠ PLUS SERRÉES, les lignes deviennent un voile : le pas se double à l'écran
// jusqu'à cet écart, sans changer l'aimant.
constexpr float kGridSmallestSpacingPixels = 6.0F;

// Le menu du clic droit, et ce que la souris a saisi entre deux images :
// l'ancre que vise le menu, la place NON AIMANTÉE de celle qu'on tire.
constexpr const char* kMenuId = "##menu_trajectoire";
constexpr const char* kMenuAnchorKey = "##ancre_du_menu";
constexpr const char* kRawXKey = "##ancre_brute_x";
constexpr const char* kRawYKey = "##ancre_brute_y";

[[nodiscard]] ImU32 ColorOf(PathAnchorKind kind) noexcept {
  switch (kind) {
    case PathAnchorKind::kStart:
      return kStartColor;
    case PathAnchorKind::kArrival:
      return kArrivalColor;
    case PathAnchorKind::kBend:
      return kBendColor;
    case PathAnchorKind::kRadius:
      return kRadiusColor;
  }
  return kStartColor;
}

void PaintAnchor(ImDrawList& into, ImVec2 at, PathAnchorKind kind, bool lit) {
  const ImU32 color = ColorOf(kind);
  const float radius = kAnchorRadius;
  switch (kind) {
    case PathAnchorKind::kStart:
      into.AddCircleFilled(at, radius, color);
      into.AddCircle(at, radius, kAnchorEdge, 0, kAnchorEdgeThickness);
      break;
    case PathAnchorKind::kArrival:
      into.AddLine(ImVec2(at.x - radius, at.y - radius), ImVec2(at.x + radius, at.y + radius),
                   kAnchorEdge, kAnchorEdgeThickness * 2.0F);
      into.AddLine(ImVec2(at.x - radius, at.y + radius), ImVec2(at.x + radius, at.y - radius),
                   kAnchorEdge, kAnchorEdgeThickness * 2.0F);
      into.AddLine(ImVec2(at.x - radius, at.y - radius), ImVec2(at.x + radius, at.y + radius),
                   color, kAnchorEdgeThickness);
      into.AddLine(ImVec2(at.x - radius, at.y + radius), ImVec2(at.x + radius, at.y - radius),
                   color, kAnchorEdgeThickness);
      break;
    case PathAnchorKind::kBend:
      into.AddQuadFilled(ImVec2(at.x, at.y - radius), ImVec2(at.x + radius, at.y),
                         ImVec2(at.x, at.y + radius), ImVec2(at.x - radius, at.y), color);
      into.AddQuad(ImVec2(at.x, at.y - radius), ImVec2(at.x + radius, at.y),
                   ImVec2(at.x, at.y + radius), ImVec2(at.x - radius, at.y), kAnchorEdge,
                   kAnchorEdgeThickness);
      break;
    case PathAnchorKind::kRadius: {
      const float half = radius * 0.7F;
      into.AddRectFilled(ImVec2(at.x - half, at.y - half), ImVec2(at.x + half, at.y + half), color);
      into.AddRect(ImVec2(at.x - half, at.y - half), ImVec2(at.x + half, at.y + half), kAnchorEdge,
                   0.0F, 0, kAnchorEdgeThickness);
      break;
    }
  }
  if (lit) {
    into.AddCircle(at, kAnchorRingRadius, kAnchorRing, 0, kAnchorEdgeThickness);
  }
}

// Le chemin en tirets, du départ à l'arrivée, au pas du nombre qui le suit.
void PaintTrace(ImDrawList& into, ImVec2 feet, const PathSample& sample, const PathTrace& trace) {
  // 🔴 LE CHEMIN DE L'ALLURE PLUS CELUI DE LA TRAJECTOIRE, à la même part de la
  // vie : c'est ce que le nombre parcourt, et le tracé passe donc par lui.
  const auto point_at = [&](float progress) {
    const ImVec2 allure = AllureAt(sample, progress);
    const float x = render::BezierAt(
        progress, trace.arrival.x * render::kFirstHandleShare + trace.bend_one.x,
        trace.arrival.x * render::kSecondHandleShare + trace.bend_two.x, trace.arrival.x);
    const float y = render::BezierAt(
        progress, trace.arrival.y * render::kFirstHandleShare + trace.bend_one.y,
        trace.arrival.y * render::kSecondHandleShare + trace.bend_two.y, trace.arrival.y);
    return PathCanvasPoint(feet, sample,
                           ImVec2(trace.start.x + allure.x + x, trace.start.y + allure.y + y));
  };
  for (int step = 0; step < kTraceSteps; step += 2) {
    const float from = static_cast<float>(step) / static_cast<float>(kTraceSteps);
    const float to = static_cast<float>(step + 1) / static_cast<float>(kTraceSteps);
    into.AddLine(point_at(from), point_at(to), kTraceColor, kTraceThickness);
  }
  if (trace.scatter && (trace.scatter_radii.x > 0.0F || trace.scatter_radii.y > 0.0F)) {
    const ImVec2 center = point_at(1.0F);
    into.AddEllipse(center,
                    ImVec2(std::max(trace.scatter_radii.x * sample.sprite_scale, 1.0F),
                           std::max(trace.scatter_radii.y * sample.sprite_scale, 1.0F)),
                    kScatterColor, 0.0F, 0, kTraceThickness);
  }
}

// Les nombres : les quads du sprite, puis ceux écrits en police.
void PaintNumbers(ImDrawList& into, ImVec2 feet, const PathSample& sample,
                  std::vector<DamageTextView>& placed_texts) {
  const float scale = sample.sprite_scale;
  const auto on_screen = [&](ImVec2 pixels) {
    return ImVec2(feet.x + pixels.x * scale, feet.y + pixels.y * scale);
  };
  for (const PathSampleQuad& quad : sample.quads) {
    const float left = quad.mirrored ? 1.0F : 0.0F;
    const float right = quad.mirrored ? 0.0F : 1.0F;
    into.AddImageQuad(quad.texture, on_screen(quad.corners[0]), on_screen(quad.corners[1]),
                      on_screen(quad.corners[2]), on_screen(quad.corners[3]), ImVec2(left, 0.0F),
                      ImVec2(right, 0.0F), ImVec2(right, 1.0F), ImVec2(left, 1.0F), quad.tint);
  }
  placed_texts.clear();
  for (const DamageTextView& text : sample.texts) {
    DamageTextView placed = text;
    placed.middle = on_screen(text.middle);
    placed.digit_height = text.digit_height * scale;
    placed.pitch = text.pitch * scale;
    placed.outline = text.outline * scale;
    placed_texts.push_back(placed);
  }
  PaintDamageTexts(into, placed_texts, sample.font);
}

// Les lignes de la grille, à pas égaux de part et d'autre de la naissance.
void PaintGrid(ImDrawList& into, ImVec2 min, ImVec2 max, ImVec2 origin, float spacing) {
  const auto lines = [&](float from, float to, float zero, auto&& paint) {
    const auto first = static_cast<int>(std::ceil((from - zero) / spacing));
    const auto last = static_cast<int>(std::floor((to - zero) / spacing));
    for (int rank = first; rank <= last; ++rank) {
      paint(zero + static_cast<float>(rank) * spacing, rank == 0 ? kGridAxisColor : kGridColor);
    }
  };
  lines(min.x, max.x, origin.x,
        [&](float x, ImU32 color) { into.AddLine(ImVec2(x, min.y), ImVec2(x, max.y), color); });
  lines(min.y, max.y, origin.y,
        [&](float y, ImU32 color) { into.AddLine(ImVec2(min.x, y), ImVec2(max.x, y), color); });
}

// Les cibles où copier `what`, sous leur titre.
void CopyItems(const PathCanvasView& view, const char* heading, int what, bool separated,
               std::optional<OptionCanvasMove>& chosen) {
  if (view.copy_targets.empty()) {
    return;
  }
  if (separated) {
    ImGui::Separator();
  }
  ImGui::TextDisabled("%s", heading);
  // ⚠ LES MÊMES NOMS DANS LES DEUX LISTES : chacune sous son identifiant.
  ImGui::PushID(what);
  for (std::size_t rank = 0; rank < view.copy_targets.size(); ++rank) {
    if (static_cast<int>(rank) == view.copy_skipped) {
      continue;
    }
    if (ImGui::MenuItem(view.copy_targets[rank])) {
      chosen = OptionCanvasMove{.anchor = what, .copy_to = static_cast<int>(rank)};
    }
  }
  ImGui::PopID();
}

// LE MENU DU CLIC DROIT : remettre l'ancre visée, la copier, ou copier toute la
// trajectoire.
[[nodiscard]] std::optional<OptionCanvasMove> PathMenu(const PathCanvasView& view,
                                                       const PathAnchor* aimed) {
  std::optional<OptionCanvasMove> chosen;
  if (aimed != nullptr) {
    ImGui::TextDisabled("%s", aimed->label);
    if (ImGui::MenuItem(core::Tr("Remettre comme le client"))) {
      chosen = OptionCanvasMove{.anchor = aimed->id, .point = aimed->rest};
    }
    CopyItems(view, core::Tr("Copier ce point vers"), aimed->id, true, chosen);
  }
  if (view.whole_path >= 0) {
    CopyItems(view, core::Tr("Copier toute la trajectoire vers"), view.whole_path, aimed != nullptr,
              chosen);
  }
  return chosen;
}

[[nodiscard]] const PathAnchor* AnchorById(std::span<const PathAnchor> anchors, int id) {
  const auto found =
      std::ranges::find_if(anchors, [id](const PathAnchor& anchor) { return anchor.id == id; });
  return found != anchors.end() ? &*found : nullptr;
}

}  // namespace

ImVec2 AllureAt(const PathSample& sample, float progress) noexcept {
  if (sample.allure.empty()) {
    return ImVec2(0.0F, 0.0F);
  }
  const auto last = static_cast<float>(sample.allure.size() - 1);
  const float at = std::clamp(progress, 0.0F, 1.0F) * last;
  const auto below = static_cast<std::size_t>(at);
  const std::size_t above = std::min(below + 1, sample.allure.size() - 1);
  const float share = at - static_cast<float>(below);
  const ImVec2 low = sample.allure[below];
  const ImVec2 high = sample.allure[above];
  return ImVec2(low.x + (high.x - low.x) * share, low.y + (high.y - low.y) * share);
}

ImVec2 SnappedToGrid(ImVec2 at, float grid_pixels) noexcept {
  if (grid_pixels <= 0.0F) {
    return at;
  }
  return ImVec2(std::round(at.x / grid_pixels) * grid_pixels,
                std::round(at.y / grid_pixels) * grid_pixels);
}

ImVec2 PathCanvasPoint(ImVec2 feet, const PathSample& sample, ImVec2 at) noexcept {
  return ImVec2(feet.x + (sample.birth.x + at.x) * sample.sprite_scale,
                feet.y - (sample.birth.y + at.y) * sample.sprite_scale);
}

ImVec2 DraggedAnchor(const PathAnchor& anchor, ImVec2 screen_delta, float sprite_scale,
                     bool fine) noexcept {
  const float share = fine ? kFineDragShare : 1.0F;
  const float per_pixel = sprite_scale > 0.0F ? share / sprite_scale : share;
  // L'écran descend, la trajectoire monte.
  const ImVec2 moved(anchor.at.x + screen_delta.x * per_pixel,
                     anchor.at.y - screen_delta.y * per_pixel);
  return ImVec2(std::clamp(moved.x, anchor.lowest.x, anchor.highest.x),
                std::clamp(moved.y, anchor.lowest.y, anchor.highest.y));
}

std::optional<std::size_t> AnchorUnder(ImVec2 feet, const PathSample& sample,
                                       std::span<const PathAnchor> anchors, ImVec2 mouse) noexcept {
  std::optional<std::size_t> nearest;
  float nearest_squared = kAnchorGrabPixels * kAnchorGrabPixels;
  for (std::size_t rank = 0; rank < anchors.size(); ++rank) {
    const ImVec2 at = PathCanvasPoint(feet, sample, anchors[rank].at);
    const float dx = at.x - mouse.x;
    const float dy = at.y - mouse.y;
    const float squared = dx * dx + dy * dy;
    // ⚠ À ÉGALITÉ, LA DERNIÈRE : elle se peint par-dessus, c'est elle qu'on voit.
    if (squared <= nearest_squared) {
      nearest_squared = squared;
      nearest = rank;
    }
  }
  return nearest;
}

std::optional<OptionCanvasMove> DrawPathCanvas(ImDrawList& into, ImVec2 min, ImVec2 max,
                                               const PathCanvasView& view) {
  into.AddRectFilled(min, max, kCanvasBackground, kCanvasRounding);
  if (view.sample == nullptr) {
    return std::nullopt;
  }
  const PathSample& sample = *view.sample;
  const EntitySamplePlace place = PlaceEntitySample(min, max, sample.target, view.floor_pixels);
  const ImVec2 feet = place.feet;
  if (view.traced && view.grid_pixels > 0.0F && sample.sprite_scale > 0.0F) {
    float spacing = view.grid_pixels * sample.sprite_scale;
    while (spacing < kGridSmallestSpacingPixels) {
      spacing *= 2.0F;
    }
    PaintGrid(into, min, max, PathCanvasPoint(feet, sample, ImVec2(0.0F, 0.0F)), spacing);
  }
  into.AddLine(ImVec2(min.x + kGroundInsetPixels, feet.y),
               ImVec2(max.x - kGroundInsetPixels, feet.y), kCanvasGround);
  if (sample.target.sprite.usable()) {
    into.AddImage(sample.target.sprite.id, place.image_min, place.image_max);
  }
  std::vector<DamageTextView> placed_texts;
  PaintNumbers(into, feet, sample, placed_texts);
  if (view.traced) {
    into.AddCircle(PathCanvasPoint(feet, sample, ImVec2(0.0F, 0.0F)), kBirthRadius, kBirthColor);
    PaintTrace(into, feet, sample, view.trace);
  }

  // ── LA SOURIS, par le bouton invisible qui couvre le cadre ──────────────────
  const ImGuiIO& io = ImGui::GetIO();
  ImGuiStorage* const storage = ImGui::GetStateStorage();
  // ⚠ LES CLÉS SE CALCULENT ICI, DANS LA FENÊTRE DU CADRE : ouvert, le menu
  // est une autre fenêtre, avec son propre stockage.
  const ImGuiID held_key = ImGui::GetItemID();
  const ImGuiID menu_key = ImGui::GetID(kMenuAnchorKey);
  const ImGuiID raw_x_key = ImGui::GetID(kRawXKey);
  const ImGuiID raw_y_key = ImGui::GetID(kRawYKey);
  int held = storage->GetInt(held_key, kNoAnchor);
  const bool hovered = ImGui::IsItemHovered();
  const std::optional<std::size_t> under =
      hovered ? AnchorUnder(feet, sample, view.anchors, io.MousePos) : std::nullopt;
  if (ImGui::IsItemActivated() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    held = under.has_value() ? view.anchors[*under].id : kNoAnchor;
    if (under.has_value()) {
      storage->SetFloat(raw_x_key, view.anchors[*under].at.x);
      storage->SetFloat(raw_y_key, view.anchors[*under].at.y);
    }
  }
  if (!ImGui::IsItemActive()) {
    held = kNoAnchor;
  }
  storage->SetInt(held_key, held);

  std::optional<OptionCanvasMove> moved;
  const PathAnchor* const holding = AnchorById(view.anchors, held);
  if (holding != nullptr) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    if (io.MouseDelta.x != 0.0F || io.MouseDelta.y != 0.0F) {
      // 🔴 LE GESTE S'ACCUMULE HORS DE LA GRILLE : repartir chaque image du
      // point aimanté ramènerait tout petit geste au même nœud, et l'ancre ne
      // le quitterait jamais.
      PathAnchor from = *holding;
      from.at = ImVec2(storage->GetFloat(raw_x_key, holding->at.x),
                       storage->GetFloat(raw_y_key, holding->at.y));
      const ImVec2 raw = DraggedAnchor(from, io.MouseDelta, sample.sprite_scale, io.KeyShift);
      storage->SetFloat(raw_x_key, raw.x);
      storage->SetFloat(raw_y_key, raw.y);
      const ImVec2 snapped = io.KeyShift ? raw : SnappedToGrid(raw, view.grid_pixels);
      moved = OptionCanvasMove{
          .anchor = holding->id,
          .point = ImVec2(std::clamp(snapped.x, holding->lowest.x, holding->highest.x),
                          std::clamp(snapped.y, holding->lowest.y, holding->highest.y))};
    }
  } else if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) &&
             (under.has_value() || view.whole_path >= 0)) {
    storage->SetInt(menu_key, under.has_value() ? view.anchors[*under].id : kNoAnchor);
    ImGui::OpenPopup(kMenuId);
  } else if (under.has_value()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  }
  const bool menu_open = ImGui::IsPopupOpen(kMenuId);
  const int menu_anchor = menu_open ? storage->GetInt(menu_key, kNoAnchor) : kNoAnchor;

  // LES TRAITS DES POIGNÉES, sous les ancres.
  for (const PathAnchor& anchor : view.anchors) {
    if (anchor.tied.has_value()) {
      into.AddLine(PathCanvasPoint(feet, sample, *anchor.tied),
                   PathCanvasPoint(feet, sample, anchor.at), kHandleLineColor,
                   kHandleLineThickness);
    }
  }
  // LES ANCRES PAR-DESSUS TOUT, celle qu'on tient ou survole cerclée.
  for (std::size_t rank = 0; rank < view.anchors.size(); ++rank) {
    const PathAnchor& anchor = view.anchors[rank];
    const bool lit =
        anchor.id == held || anchor.id == menu_anchor || (holding == nullptr && under == rank);
    PaintAnchor(into, PathCanvasPoint(feet, sample, anchor.at), anchor.kind, lit);
  }
  const PathAnchor* const told =
      holding != nullptr ? holding : (under.has_value() ? &view.anchors[*under] : nullptr);
  if (told != nullptr && !menu_open) {
    const char* const hint = core::Tr(
        "Maj : au pixel près, hors de la grille. Clic droit : remettre comme le client, ou copier "
        "vers un autre type.");
    if (told->single) {
      SkinTooltipFormat("%s — %.0f px\n%s", told->label, static_cast<double>(told->value.x), hint);
    } else {
      SkinTooltipFormat("%s — %.0f, %.0f px\n%s", told->label, static_cast<double>(told->value.x),
                        static_cast<double>(told->value.y), hint);
    }
  }
  if (BeginSkinPopup(kMenuId)) {
    if (std::optional<OptionCanvasMove> chosen =
            PathMenu(view, AnchorById(view.anchors, menu_anchor))) {
      moved = chosen;
    }
    EndSkinPopup();
  }
  return moved;
}

}  // namespace ro::ui
