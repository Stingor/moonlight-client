#pragma once

// LA SONDE DES ESSAIS DU CARNET MVP DESSINÉ : une interface hors écran, l'art
// uni, une vue de créneaux et de membres, et les gestes de la souris qui la
// parcourent — survol, clic, balayage jusqu'à un élément.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/mvp_tracker_window.h"
#include "ro/ui/ui_context.h"

namespace mvp_tracker_probe {

using ro::render::SkinPiece;
using ro::ui::ChatLinkGesture;
using ro::ui::kMvpNoSlotView;
using ro::ui::MvpMemberView;
using ro::ui::MvpPinnedLineView;
using ro::ui::MvpSlotView;
using ro::ui::MvpSpriteArt;
using ro::ui::MvpTrackerAction;
using ro::ui::MvpTrackerView;
using ro::ui::MvpTrackerWindowState;

inline constexpr ro::ui::Viewport kViewport{.width_px = 1600.0F, .height_px = 900.0F};
inline constexpr float kFrameSeconds = 1.0F / 60.0F;
inline constexpr float kPlainPiecePixels = 8.0F;
inline constexpr float kStarPixels = 20.0F;
inline constexpr float kScanStepY = 3.0F;
inline constexpr float kScanStepX = 6.0F;
inline constexpr int kSettleFrames = 4;
inline constexpr std::int64_t kNow = 1'800'000'000;
inline constexpr std::uint32_t kNowMs = 100'000;
inline constexpr std::uint32_t kGroup = 7;
inline constexpr const char* kTracker = "###mvp_tracker";

[[nodiscard]] inline ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

[[nodiscard]] inline ImGuiWindow* Window(const char* name) { return ImGui::FindWindowByName(name); }

// La fenêtre a-t-elle été soumise à la dernière image ?
[[nodiscard]] inline bool Shown(const char* name) {
  const ImGuiWindow* window = Window(name);
  return window != nullptr && window->Active;
}

struct TrackerProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  MvpTrackerWindowState state;
  std::vector<MvpSlotView> slots;
  std::vector<MvpMemberView> members;
  std::vector<MvpPinnedLineView> lines;
  std::vector<MvpSpriteArt> sprites;
  MvpTrackerView view;

  explicit TrackerProbe(int slot_count = 3) {
    static constexpr std::array<std::string_view, 4> kMaps = {"prt_maze03", "gef_fild10",
                                                              "pay_fild11", "moc_pryd06"};
    for (int rank = 0; rank < slot_count; ++rank) {
      const auto slot_id = static_cast<std::uint16_t>(rank);
      const auto mob = static_cast<std::uint16_t>(1000 + rank);
      labels.push_back("Mvp " + std::string(1, static_cast<char>('A' + rank % 26)) +
                       std::to_string(rank));
      slots.push_back(MvpSlotView{.slot_id = slot_id,
                                  .mob_id = mob,
                                  .sprite_mob_id = mob,
                                  .link_mob_id = mob,
                                  .delay1_ms = 3'600'000,
                                  .delay2_ms = 600'000,
                                  .map_cells_x = 200,
                                  .map_cells_y = 200,
                                  .map = kMaps[static_cast<std::size_t>(rank) % kMaps.size()]});
    }
    for (std::size_t rank = 0; rank < slots.size(); ++rank) {
      slots[rank].label = labels[rank];
    }
    view.server_now_s = kNow;
    view.now_ms = kNowMs;
    view.enabled = true;
    view.link_ready = true;
    view.catalog_known = true;
    view.group_id = kGroup;
    view.owner_user_id = 1;
    view.group_name = "Chasseurs";
    view.favorite_off = ro::ui::SkinImage{.id = 2, .width = kStarPixels, .height = kStarPixels};
    view.favorite_on = ro::ui::SkinImage{.id = 3, .width = kStarPixels, .height = kStarPixels};
    state.open = true;
    state.look.show_sprites = false;
    Bind();
  }

  void Bind() {
    view.slots = slots;
    view.members = members;
    view.lines = lines;
    view.sprites = sprites;
  }

  MvpTrackerAction Frame() {
    Bind();
    ui.BeginFrame(kViewport, kFrameSeconds);
    MvpTrackerAction action = ro::ui::DrawMvpTrackerWindow(state, view, art);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }

  // Une mesure qui demande une image en cours : la police est celle de l'image.
  float RowHeight(bool show_sprites) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const float height = ro::ui::MvpTableRowHeight(show_sprites, view, art);
    ui.EndFrame();
    return height;
  }

  std::vector<std::string> labels;
};

[[nodiscard]] inline ImGuiTable* TableOf(const char* window_name, const char* table_id) {
  ImGuiWindow* window = Window(window_name);
  return window != nullptr ? ImGui::TableFindByID(window->GetID(table_id)) : nullptr;
}

[[nodiscard]] inline ImGuiTable* SlotTable() { return TableOf(kTracker, "##mvp_slots"); }

// Un point près du haut d'une cellule de la table des créneaux, table au repos
// (sans défilement), pour un rang de hauteur donnée : le contenu d'une cellule
// s'aligne EN HAUT, et le plus petit — le petit bouton — n'en descend pas loin.
[[nodiscard]] inline ImVec2 CellAt(const ImGuiTable& table, int column, int row, float row_height,
                                   float into_x = 4.0F) {
  constexpr float kIntoCellY = 4.0F;
  const float top = table.OuterRect.Min.y + table.InstanceDataFirst.LastFrozenHeight;
  return ImVec2(
      table.Columns[column].WorkMinX + into_x,
      top + row_height * static_cast<float>(row) + ImGui::GetStyle().CellPadding.y + kIntoCellY);
}

[[nodiscard]] inline ImRect AreaOf(const ImGuiWindow& window) {
  return ImRect(window.Pos.x, window.Pos.y, window.Pos.x + window.Size.x,
                window.Pos.y + window.Size.y);
}

// Balaie un rectangle jusqu'à survoler l'élément `id`.
[[nodiscard]] inline std::optional<ImVec2> FindHovered(TrackerProbe& probe, ImRect area,
                                                       ImGuiID id) {
  ImGuiIO& io = ImGui::GetIO();
  for (float y = area.Min.y + 1.0F; y < area.Max.y; y += kScanStepY) {
    for (float x = area.Min.x + 1.0F; x < area.Max.x; x += kScanStepX) {
      io.AddMousePosEvent(x, y);
      static_cast<void>(probe.Frame());
      if (GImGui->HoveredId == id) {
        return ImVec2(x, y);
      }
    }
  }
  return std::nullopt;
}

[[nodiscard]] inline std::optional<ImVec2> FindButton(TrackerProbe& probe, const char* window_name,
                                                      const char* label) {
  ImGuiWindow* window = Window(window_name);
  REQUIRE(window != nullptr);
  const ImGuiID id = window->GetID(label);
  return FindHovered(probe, AreaOf(*window), id);
}

struct Click {
  MvpTrackerAction pressed;
  MvpTrackerAction released;
};

inline Click ClickAt(TrackerProbe& probe, ImVec2 at,
                     ImGuiMouseButton button = ImGuiMouseButton_Left, bool shift = false) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(at.x, at.y);
  io.AddKeyEvent(ImGuiMod_Shift, shift);
  static_cast<void>(probe.Frame());
  Click click;
  io.AddMouseButtonEvent(button, true);
  click.pressed = probe.Frame();
  io.AddMouseButtonEvent(button, false);
  click.released = probe.Frame();
  io.AddKeyEvent(ImGuiMod_Shift, false);
  static_cast<void>(probe.Frame());
  return click;
}

// Déplie l'en-tête du groupe, replié par défaut.
inline void OpenGroupHeader(TrackerProbe& probe) {
  const std::optional<ImVec2> header = FindButton(probe, kTracker, "###mvp_group");
  REQUIRE(header.has_value());
  static_cast<void>(ClickAt(probe, *header));
  probe.Settle();
}

// Une frappe d'Entrée, enfoncée puis relâchée ; l'action de chaque image.
inline Click PressEnter(TrackerProbe& probe) {
  ImGuiIO& io = ImGui::GetIO();
  Click press;
  io.AddKeyEvent(ImGuiKey_Enter, true);
  press.pressed = probe.Frame();
  io.AddKeyEvent(ImGuiKey_Enter, false);
  press.released = probe.Frame();
  return press;
}

[[nodiscard]] inline bool PopupOpenIn(const char* window_name, const char* popup_id) {
  const ImGuiWindow* window = Window(window_name);
  REQUIRE(window != nullptr);
  const ImGuiID wanted = ImHashStr(popup_id, 0, window->ID);
  return std::ranges::any_of(GImGui->OpenPopupStack,
                             [&](const ImGuiPopupData& popup) { return popup.PopupId == wanted; });
}

[[nodiscard]] inline std::vector<std::uint16_t> OrderedSlots(const TrackerProbe& probe) {
  std::vector<std::uint16_t> ordered;
  for (const std::size_t index : probe.state.order) {
    ordered.push_back(probe.slots[index].slot_id);
  }
  return ordered;
}

// Un clic sur l'en-tête d'une colonne de la table des créneaux.
inline void SortBy(TrackerProbe& probe, int column) {
  const ImGuiTable* table = SlotTable();
  REQUIRE(table != nullptr);
  const ImVec2 at(table->Columns[column].MinX + 4.0F,
                  table->OuterRect.Min.y + table->InstanceDataFirst.LastFrozenHeight * 0.5F);
  static_cast<void>(ClickAt(probe, at));
  probe.Settle();
}

}  // namespace mvp_tracker_probe
