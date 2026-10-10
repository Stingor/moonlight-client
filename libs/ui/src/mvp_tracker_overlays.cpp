// Le carnet de chasse MVP : ce qui se dessine hors de sa fenêtre — le bandeau
// d'alerte, les lignes détachées, le fantôme du glisser, l'aperçu d'un
// `<MVPL>`.

#include <algorithm>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <string>
#include <vector>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/hud_frame.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/window_bounds.h"

#include "mvp_tracker_parts.h"

namespace ro::ui {

namespace mvp_tracker {

namespace {

// Une ligne détachée est une fenêtre à elle : son fond, son opacité, son
// rectangle de survol. Sa place est posée à CHAQUE image depuis le livre, et le
// déplacement se joue à la main : laisser ImGui la bouger perdrait l'aimantage,
// qui doit corriger la place AVANT qu'elle ne s'applique.
constexpr ImGuiWindowFlags kLineWindowFlags =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
    ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

// Le bandeau : il prend le clic — une ligne ouvre le carnet sur son créneau —,
// mais pas le focus à son apparition, et ne se retient nulle part.
constexpr ImGuiWindowFlags kAlertWindowFlags =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
    ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings;

// L'arrondi au pixel d'une place posée.
constexpr float kHalfPixel = 0.5F;

[[nodiscard]] int RoundedPixel(float at) { return static_cast<int>(at + kHalfPixel); }

// Un rectangle est-il entièrement dans l'écran ?
[[nodiscard]] bool OnScreen(ImVec2 at, ImVec2 size) {
  const ImVec2 inside = ClampWindowPosToScreen(at, size);
  return inside.x == at.x && inside.y == at.y;
}

}  // namespace

void DrawAlerts(const MvpTrackerView& view, MvpTrackerAction& action) {
  if (view.alerts.empty()) {
    return;
  }
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(ImVec2(screen->WorkPos.x + screen->WorkSize.x * 0.5F,
                                 screen->WorkPos.y + Px(kMvpAlertTopPixels)),
                          ImGuiCond_Always, ImVec2(0.5F, 0.0F));
  ImGui::SetNextWindowBgAlpha(kMvpAlertBackAlpha);
  if (ImGui::Begin("##mvp_alert", nullptr, kAlertWindowFlags)) {
    ImGui::PushStyleColor(ImGuiCol_Text, kMvpAlertTextColor);
    for (const MvpAlertView& alert : view.alerts) {
      const std::string label(alert.label);
      const std::string map(alert.map);
      // Un instant exact dit l'HEURE du retour ; une fenêtre dit qu'elle s'ouvre.
      const std::string text =
          alert.exact
              ? Printed(core::Tr("%s (%s) : de retour à %s."), label.c_str(), map.c_str(),
                        std::string(alert.at_clock).c_str())
              : Printed(core::Tr("%s (%s) entre dans sa fenêtre."), label.c_str(), map.c_str());
      ImGui::PushID(static_cast<int>(alert.slot_id));
      if (ImGui::Selectable(text.c_str())) {
        action.alert_slot = alert.slot_id;
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        SkinTooltipFormat("%s", core::Tr("Clic : ouvrir le carnet sur la ligne"));
      }
      ImGui::PopID();
    }
    ImGui::PopStyleColor();
  }
  ImGui::End();
}

void DrawPinnedLines(MvpTrackerWindowState& state, const MvpTrackerView& view,
                     MvpTrackerAction& action) {
  if (view.lines.empty()) {
    return;
  }
  // Seules les lignes dont le créneau est au catalogue se dessinent, et seules
  // elles aimantent : une ligne qu'on ne voit pas n'attire rien.
  struct DrawnLine {
    const MvpPinnedLineView* line = nullptr;
    const MvpSlotView* slot = nullptr;
  };
  std::vector<DrawnLine> drawn;
  drawn.reserve(view.lines.size());
  int last_line = -1;
  for (const MvpPinnedLineView& line : view.lines) {
    const MvpSlotView* slot = FindSlotView(view.slots, line.slot_id);
    if (slot != nullptr && line.line >= 0) {
      drawn.push_back(DrawnLine{.line = &line, .slot = slot});
      last_line = std::max(last_line, line.line);
    }
  }
  // La taille MESURÉE de chaque ligne, par rang du livre : c'est sur elle que
  // les AUTRES s'aimantent.
  state.line_sizes.resize(static_cast<std::size_t>(last_line + 1), ImVec2(0.0F, 0.0F));
  std::vector<HudRect> frames;
  frames.reserve(drawn.size());
  for (const DrawnLine& each : drawn) {
    const ImVec2 size = state.line_sizes[static_cast<std::size_t>(each.line->line)];
    frames.push_back(HudRect{.x = static_cast<float>(each.line->x),
                             .y = static_cast<float>(each.line->y),
                             .width = size.x,
                             .height = size.y});
  }

  const ImVec2 mouse = ImGui::GetIO().MousePos;
  int removed = -1;
  // L'action ne porte qu'UN déplacement par image. Une borne à la pose le prend
  // au tirage, qui se recalcule de la souris à l'image suivante ; une borne qui
  // le trouve déjà pris par une autre borne attend son tour.
  bool moved_by_drag = false;
  for (std::size_t rank = 0; rank < drawn.size(); ++rank) {
    const MvpPinnedLineView& line = *drawn[rank].line;
    const MvpSlotView* slot = drawn[rank].slot;
    const std::string name = MvpLineName(*slot) + " — ";
    const std::string returns = MvpReturnText(*slot, view.server_now_s);
    const bool possible = MvpReturnToneOf(*slot, view.server_now_s) == MvpReturnTone::kPossible;
    const std::string id = std::format("##mvpline_{}_{}", line.mob_id, line.map);
    const ImVec2 at(static_cast<float>(line.x), static_cast<float>(line.y));
    ImGui::SetNextWindowPos(at, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,
                          ImGui::ColorConvertU32ToFloat4(state.look.line_background_color));
    ImGui::PushStyleColor(ImGuiCol_Text,
                          ImGui::ColorConvertU32ToFloat4(state.look.line_text_color));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, kMvpLinePadding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
    if (ImGui::Begin(id.c_str(), nullptr, kLineWindowFlags)) {
      if (state.look.line_show_sprite && slot->sprite_mob_id != 0) {
        DrawRowSprite(state, view, slot->sprite_mob_id);
        ImGui::SameLine();
      }
      // Le texte de la table, mot pour mot ; seul le vert s'éclaircit, sur ce
      // fond sombre.
      ImGui::TextUnformatted(name.c_str());
      ImGui::SameLine(0.0F, 0.0F);
      if (possible) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMvpPossibleLineColor);
      }
      ImGui::TextUnformatted(returns.c_str());
      if (possible) {
        ImGui::PopStyleColor();
      }

      const ImVec2 size = ImGui::GetWindowSize();
      ImVec2& measured = state.line_sizes[static_cast<std::size_t>(line.line)];
      // À sa première image, une fenêtre qui s'ajuste à son contenu n'a pas
      // encore sa taille : on attend la suivante pour la relever.
      if (!ImGui::IsWindowAppearing()) {
        // 🔴 BORNÉE À L'ÉCRAN À LA POSE : une ligne lâchée près d'un bord, ou
        // relue d'un autre écran, sortirait sinon du cadre sans qu'on puisse la
        // rattraper. Une ligne qui attend son tour n'est pas mesurée : sa
        // mesure nulle la désigne encore à l'image suivante.
        const bool first_measure = measured.x <= 0.0F && measured.y <= 0.0F;
        bool waits_its_turn = false;
        if (first_measure && !OnScreen(at, size)) {
          if (action.moved_line < 0 || moved_by_drag) {
            const ImVec2 inside = ClampWindowPosToScreen(at, size);
            action.moved_line = line.line;
            action.moved_x = RoundedPixel(inside.x);
            action.moved_y = RoundedPixel(inside.y);
            action.lines_released = true;
            moved_by_drag = false;
          } else {
            waits_its_turn = true;
          }
        }
        if (!waits_its_turn) {
          measured = size;
          frames[rank].width = size.x;
          frames[rank].height = size.y;
        }
      }
      const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
      if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        state.line_drag = line.line;
        state.line_drag_offset = ImVec2(mouse.x - at.x, mouse.y - at.y);
      }
      if (state.line_drag == line.line && ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
          action.moved_line < 0) {
        const HudSiblings siblings{.frames = frames, .mine = static_cast<int>(rank)};
        const float wanted_x =
            SnappedToSiblings(mouse.x - state.line_drag_offset.x, size.x, siblings, false);
        const float wanted_y =
            SnappedToSiblings(mouse.y - state.line_drag_offset.y, size.y, siblings, true);
        const ImVec2 inside = ClampWindowPosToScreen(ImVec2(wanted_x, wanted_y), size);
        action.moved_line = line.line;
        action.moved_x = RoundedPixel(inside.x);
        action.moved_y = RoundedPixel(inside.y);
        moved_by_drag = true;
      }
      if (state.line_drag == line.line && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        state.line_drag = -1;
        action.lines_released = true;
      }
      // Clic droit : la remettre au carnet. Pas de croix : elle mangerait la
      // moitié d'une ligne qu'on veut justement minuscule. ⚠ Retirée APRÈS la
      // boucle : les suivantes se dessinent encore à cette image.
      if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        removed = line.line;
      }
      if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        // Les deux gestes ne se devinent pas : aucune croix, aucune poignée.
        if (state.line_drag < 0) {
          SkinTooltipFormat("%s",
                            core::Tr("Glisser pour déplacer · Clic droit : remettre au carnet"));
        }
      }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
  }
  action.removed_line = removed;
}

void FinishRowDrag(MvpTrackerWindowState& state, const MvpTrackerView& view,
                   MvpTrackerAction& action) {
  if (state.drag_slot == kMvpNoSlotView) {
    return;
  }
  // 🔴 UN BOUTON RETOMBÉ SANS RELÂCHEMENT VU : ImGui a vidé la souris, l'appli
  // ayant perdu le focus en plein geste. Le geste s'annule, sans quoi le
  // prochain relâchement, n'importe où, le prendrait pour son lâcher.
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
      !ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    state.drag_slot = kMvpNoSlotView;
    return;
  }
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const MvpSlotView* slot = FindSlotView(view.slots, state.drag_slot);
  // Le fantôme qui suit la souris : sans lui, rien ne dit que le geste a pris.
  if (slot != nullptr) {
    const std::string text = MvpLineText(*slot, view.server_now_s);
    ImDrawList* into = ImGui::GetForegroundDrawList();
    const ImVec2 size = ImGui::CalcTextSize(text.c_str());
    const ImVec2 origin(mouse.x + kMvpGhostOffset.x, mouse.y + kMvpGhostOffset.y);
    into->AddRectFilled(
        ImVec2(origin.x - kMvpGhostMargin.x, origin.y - kMvpGhostMargin.y),
        ImVec2(origin.x + size.x + kMvpGhostMargin.x, origin.y + size.y + kMvpGhostMargin.y),
        kMvpGhostBackColor, kMvpGhostRounding);
    into->AddText(origin, kMvpGhostTextColor, text.c_str());
  }
  if (!ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    return;
  }
  // Lâché DANS le carnet, rien : un glissement maladroit dans la table ne doit
  // pas semer une ligne sous la fenêtre, où elle serait invisible.
  const bool inside =
      mouse.x >= state.window_pos.x && mouse.x <= state.window_pos.x + state.window_size.x &&
      mouse.y >= state.window_pos.y && mouse.y <= state.window_pos.y + state.window_size.y;
  if (!inside && slot != nullptr) {
    const ImVec2 at = ClampWindowPosToScreen(mouse, ImVec2(0.0F, 0.0F));
    action.pin_slot = state.drag_slot;
    action.pin_x = static_cast<int>(at.x);
    action.pin_y = static_cast<int>(at.y);
  }
  state.drag_slot = kMvpNoSlotView;
}

}  // namespace mvp_tracker

void DrawMvpLinkTip(const MvpLinkTip& tip) {
  if (!BeginSkinTooltip()) {
    return;
  }
  // Le nom est le SEUL texte ordinaire ; le reste est en retrait.
  ImGui::TextUnformatted(tip.name.c_str());
  ImGui::Separator();
  const ImVec4 dim = ImGui::ColorConvertU32ToFloat4(mvp_tracker::kMvpTipDimColor);
  ImGui::TextColored(dim, "%s", tip.map.c_str());
  // L'échéance passée vient en dernier : elle juge les lignes qui précèdent.
  for (const std::string* line :
       {&tip.respawn_line, &tip.death_line, &tip.tomb_line, &tip.passed_line}) {
    if (!line->empty()) {
      ImGui::TextColored(dim, "%s", line->c_str());
    }
  }
  ImGui::Spacing();
  ImGui::TextColored(dim, "%s", core::Tr("Clic : ouvrir le carnet de chasse"));
  EndSkinTooltip();
}

}  // namespace ro::ui
