#include "ro/ui/warp_list_window.h"

#include <algorithm>
#include <cmath>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/bitmap_button.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// Le nom ImGui : la fenêtre n'a pas de barre, donc pas de titre à traduire.
constexpr const char* kWindowName = "###destinations";
constexpr const char* kRowsName = "##destinations_rows";

// Une molette déroule d'une ligne par cran.
constexpr int kWheelRows = 1;
// Le plus petit diviseur d'échelle : une échelle nulle ne divise pas par zéro.
constexpr float kLeastScale = 1e-6F;

// LA LIGNE CHOISIE RESTE VISIBLE : une flèche qui la pousse hors de la liste
// déroule d'une ligne.
void KeepRowVisible(WarpListWindowState& state, int row_count, int visible_rows) {
  if (state.selected < 0) {
    return;
  }
  if (state.selected < state.first_row) {
    state.first_row = WarpListScrolled(state.first_row, -1, row_count, visible_rows);
  } else if (state.selected > state.first_row + visible_rows - 1) {
    state.first_row = WarpListScrolled(state.first_row, 1, row_count, visible_rows);
  }
}

// Une pièce de la barre à sa taille, son coin haut-gauche à `y_pixels` sous le
// haut de la barre.
void PaintBarPiece(ImDrawList* into, const SkinArt& art, render::SkinPiece piece, ImVec2 bar,
                   int y_pixels) {
  const SkinImage& image = art.Get(piece);
  if (!image.usable()) {
    return;
  }
  const float scale = Scale();
  const ImVec2 at = ImVec2(bar.x, bar.y + static_cast<float>(y_pixels) * scale);
  into->AddImage(image.id, at, ImVec2(at.x + image.width * scale, at.y + image.height * scale));
}

// LA BARRE, dans l'ordre où le client la peint : la flèche du haut, la piste en
// carreaux — le premier recouvre la dernière rangée de la flèche —, un dernier
// carreau collé à la flèche du bas, cette flèche, puis la poignée. La rangée du
// bord haut et celle du bord bas restent vides.
void PaintBar(ImDrawList* into, const SkinArt& art, ImVec2 bar, int bar_height,
              const WarpListGrab& grab) {
  PaintBarPiece(into, art, render::SkinPiece::kScrollUp, bar, kWarpListArrowInsetPixels);
  const int track_span =
      bar_height - kWarpListArrowPixels - kWarpListArrowPixels - kWarpListArrowInsetPixels;
  const int tiles =
      track_span > 0 ? (track_span + kWarpListTrackTilePixels - 1) / kWarpListTrackTilePixels : 0;
  for (int tile = 0; tile < tiles; ++tile) {
    PaintBarPiece(into, art, render::SkinPiece::kScrollMiddle, bar,
                  kWarpListArrowPixels + tile * kWarpListTrackTilePixels);
  }
  PaintBarPiece(into, art, render::SkinPiece::kScrollMiddle, bar,
                bar_height - kWarpListArrowPixels - kWarpListTrackTilePixels);
  PaintBarPiece(into, art, render::SkinPiece::kScrollDown, bar,
                bar_height - kWarpListArrowPixels - kWarpListArrowInsetPixels);

  PaintBarPiece(into, art, render::SkinPiece::kScrollGrabTop, bar, grab.top);
  const int grab_end = grab.top + grab.length - kWarpListGrabTilePixels;
  for (int y = grab.top + kWarpListGrabTilePixels; y < grab_end; y += kWarpListGrabTilePixels) {
    PaintBarPiece(into, art, render::SkinPiece::kScrollGrabMiddle, bar, y);
  }
  PaintBarPiece(into, art, render::SkinPiece::kScrollGrabBottom, bar, grab_end);
}

// LE GESTE SUR LA BARRE : une flèche déroule d'une ligne, un clic dans la piste
// d'une page moins une ligne, du côté du clic, et la poignée se glisse. Rien ne
// se répète tant que le bouton reste enfoncé.
void DriveBar(WarpListWindowState& state, int bar_height, int row_count, int visible_rows,
              const WarpListGrab& grab, float bar_top) {
  const float scale = Scale();
  const int y_pixels = static_cast<int>(
      std::floor((ImGui::GetIO().MousePos.y - bar_top) / std::max(scale, kLeastScale)));
  if (ImGui::IsItemHovered()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  }
  if (ImGui::IsItemActivated()) {
    const int page = visible_rows - 1;
    switch (WarpListBarZoneAt(y_pixels, bar_height, grab)) {
      case WarpListBarZone::kUpArrow:
        state.first_row = WarpListScrolled(state.first_row, -1, row_count, visible_rows);
        break;
      case WarpListBarZone::kPageUp:
        state.first_row = WarpListScrolled(state.first_row, -page, row_count, visible_rows);
        break;
      case WarpListBarZone::kGrab:
        state.dragging_grab = true;
        state.drag_start_pixels = y_pixels;
        state.drag_start_first_row = state.first_row;
        break;
      case WarpListBarZone::kPageDown:
        state.first_row = WarpListScrolled(state.first_row, page, row_count, visible_rows);
        break;
      case WarpListBarZone::kDownArrow:
        state.first_row = WarpListScrolled(state.first_row, 1, row_count, visible_rows);
        break;
    }
  }
  if (!ImGui::IsItemActive()) {
    state.dragging_grab = false;
    return;
  }
  if (state.dragging_grab) {
    state.first_row =
        WarpListDraggedFirstRow(state.drag_start_first_row, y_pixels - state.drag_start_pixels,
                                bar_height, row_count, visible_rows);
  }
}

// LES LIGNES MONTRÉES, peintes sur la liste de dessin : la bande de la ligne
// choisie, puis le texte, puis la barre quand des lignes sont cachées. Un clic
// choisit la ligne sous le pointeur — ou aucune, sous la dernière ; un
// double-clic sur une ligne vaut « OK ».
void DrawRows(WarpListWindowState& state, const WarpListView& view, const SkinArt& art,
              WarpListWindowAction& action, bool keys_moved) {
  const float scale = Scale();
  const float row_height = kWarpListRowPixels * scale;
  const int count = static_cast<int>(view.rows.size());
  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImVec2 area = ImGui::GetContentRegionAvail();
  const int list_height = static_cast<int>(std::lround(area.y / std::max(scale, kLeastScale)));
  const int visible_rows = WarpListVisibleRows(list_height);

  if (keys_moved) {
    KeepRowVisible(state, count, visible_rows);
  }
  state.first_row = WarpListScrolled(state.first_row, 0, count, visible_rows);

  const int shown_end = std::min(count, state.first_row + visible_rows);
  for (int row = state.first_row; row < shown_end; ++row) {
    const ImVec2 top =
        ImVec2(origin.x, origin.y + static_cast<float>(row - state.first_row) * row_height);
    if (row == state.selected) {
      into->AddRectFilled(top, ImVec2(top.x + area.x, top.y + kWarpListSelectionPixels * scale),
                          kWarpListSelectionColor);
    }
    const std::string& text = view.rows[static_cast<std::size_t>(row)];
    into->AddText(ImVec2(top.x + kWarpListTextInsetPixels * scale, top.y), kWarpListTextColor,
                  text.data(), text.data() + text.size());
  }

  // LA ZONE CLIQUABLE COUVRE TOUTE LA LISTE, lignes et vide dessous — sauf la
  // barre quand elle est là : un clic dans le vide désélectionne, et ce n'est
  // pas un détail — « OK » ne part plus.
  const bool has_bar = count > visible_rows;
  const float bar_width = has_bar ? static_cast<float>(kWarpListBarPixels) * scale : 0.0F;
  const float rows_width = std::max(area.x - bar_width, 1.0F);
  ImGui::InvisibleButton("##rows", ImVec2(rows_width, std::max(area.y, 1.0F)));
  if (ImGui::IsItemHovered()) {
    const float y_in_rows =
        ImGui::GetIO().MousePos.y - origin.y + static_cast<float>(state.first_row) * row_height;
    const int under = WarpListRowAt(y_in_rows, row_height, count);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      state.selected = under;
    }
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && under != kWarpListNoRow) {
      action.verb = WarpListVerb::kChoose;
      action.row = under;
    }
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0F) {
      const int delta = wheel > 0.0F ? -kWheelRows : kWheelRows;
      state.first_row = WarpListScrolled(state.first_row, delta, count, visible_rows);
    }
  }
  if (!has_bar) {
    state.dragging_grab = false;
    return;
  }
  const ImVec2 bar = ImVec2(origin.x + rows_width, origin.y);
  ImGui::SetCursorScreenPos(bar);
  ImGui::InvisibleButton("##bar", ImVec2(bar_width, std::max(area.y, 1.0F)));
  DriveBar(state, list_height, count, visible_rows,
           WarpListGrabOf(list_height, count, visible_rows, state.first_row), bar.y);
  PaintBar(into, art, bar, list_height,
           WarpListGrabOf(list_height, count, visible_rows, state.first_row));
}

}  // namespace

int WarpListRowAt(float y_in_rows, float row_height, int row_count) {
  if (y_in_rows < 0.0F || row_height <= 0.0F) {
    return kWarpListNoRow;
  }
  const int row = static_cast<int>(std::floor(y_in_rows / row_height));
  return row < row_count ? row : kWarpListNoRow;
}

int WarpListVisibleRows(int list_height_pixels) {
  return std::max(list_height_pixels, 0) / static_cast<int>(kWarpListRowPixels);
}

int WarpListScrolled(int first_row, int delta, int row_count, int visible_rows) {
  const int last_first_row = std::max(row_count - visible_rows, 0);
  return std::clamp(first_row + delta, 0, last_first_row);
}

WarpListGrab WarpListGrabOf(int bar_height_pixels, int row_count, int visible_rows, int first_row) {
  WarpListGrab grab;
  const int hidden = row_count - visible_rows;
  if (hidden <= 0) {
    return grab;
  }
  const int track = bar_height_pixels - 2 * kWarpListArrowPixels;
  int length = track * visible_rows / row_count;
  length = std::max(length, kWarpListGrabLeastPixels);
  length = std::max(length, kWarpListGrabFloorPixels);
  grab.length = length;
  grab.top = kWarpListArrowPixels + (track - length) * first_row / hidden;
  return grab;
}

WarpListBarZone WarpListBarZoneAt(int y_pixels, int bar_height_pixels, const WarpListGrab& grab) {
  if (y_pixels < kWarpListArrowPixels) {
    return WarpListBarZone::kUpArrow;
  }
  if (y_pixels < grab.top) {
    return WarpListBarZone::kPageUp;
  }
  if (y_pixels < grab.top + grab.length) {
    return WarpListBarZone::kGrab;
  }
  if (y_pixels >= bar_height_pixels - kWarpListArrowPixels) {
    return WarpListBarZone::kDownArrow;
  }
  return WarpListBarZone::kPageDown;
}

int WarpListDraggedFirstRow(int start_first_row, int moved_pixels, int bar_height_pixels,
                            int row_count, int visible_rows) {
  const int hidden = row_count - visible_rows;
  if (hidden <= 0) {
    return 0;
  }
  const WarpListGrab grab = WarpListGrabOf(bar_height_pixels, row_count, visible_rows, 0);
  const int travel = bar_height_pixels - 2 * kWarpListArrowPixels - grab.length;
  if (travel <= 0) {
    return WarpListScrolled(start_first_row, 0, row_count, visible_rows);
  }
  const int moved_rows = (hidden + 1) * moved_pixels / travel;
  return WarpListScrolled(start_first_row, moved_rows, row_count, visible_rows);
}

int WarpListStep(int selected, int delta, int row_count) {
  if (row_count <= 0) {
    return kWarpListNoRow;
  }
  return std::clamp(selected + delta, 0, row_count - 1);
}

ImVec2 WarpListSpawn(ImVec2 screen, float scale) {
  return ImVec2(screen.x * 0.5F - kWarpListSpawnLeftOfCenterPixels * scale,
                screen.y * kWarpListSpawnHeightShare);
}

WarpListWindowAction DrawWarpListWindow(WarpListWindowState& state, const WarpListView& view,
                                        const SkinArt& art) {
  WarpListWindowAction action;
  if (!state.open) {
    return action;
  }
  const float scale = Scale();
  const int count = static_cast<int>(view.rows.size());

  // UNE LISTE NOUVELLE RECRÉE LA FENÊTRE chez le client : la sélection revient
  // sur la première ligne, la liste revient en haut, et la fenêtre repasse au
  // premier plan.
  if (view.offer_seq != state.seen_offer_seq) {
    state.seen_offer_seq = view.offer_seq;
    state.selected = count > 0 ? 0 : kWarpListNoRow;
    state.first_row = 0;
    state.dragging_grab = false;
    ImGui::SetNextWindowFocus();
  }
  if (state.selected >= count) {
    state.selected = kWarpListNoRow;
  }

  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(WarpListSpawn(screen->Size, scale), ImGuiCond_FirstUseEver);
  const ImVec2 size = ImVec2(kWarpListSizePixels.x * scale, kWarpListSizePixels.y * scale);
  ImGui::SetNextWindowSize(size, ImGuiCond_Always);

  // Le fond et le filet d'ImGui s'effacent : c'est le cadre peint qui fait le
  // corps, et les mesures du client se comptent depuis le bord.
  ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  constexpr ImGuiWindowFlags kFlags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse;
  if (ImGui::Begin(kWindowName, nullptr, kFlags)) {
    DrawDescPanelFrameAroundWindow(ImGui::GetWindowDrawList(), art, /*fill_bg=*/true);

    // ── LE CLAVIER, fenêtre au premier plan ──────────────────────────────────
    // Les flèches déplacent la sélection ; Entrée et Espace valent « OK »,
    // Échap « Annuler ».
    bool keys_moved = false;
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
      if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        state.selected = WarpListStep(state.selected, -1, count);
        keys_moved = true;
      }
      if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        state.selected = WarpListStep(state.selected, 1, count);
        keys_moved = true;
      }
      if ((ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter) ||
           ImGui::IsKeyPressed(ImGuiKey_Space)) &&
          state.selected != kWarpListNoRow) {
        action.verb = WarpListVerb::kChoose;
        action.row = state.selected;
      }
      if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        action.verb = WarpListVerb::kCancel;
        action.escape_spent = true;
      }
    }

    // ── LA LIGNE DE TITRE ────────────────────────────────────────────────────
    ImGui::SetCursorPos(ImVec2(kWarpListTitlePixels.x * scale, kWarpListTitlePixels.y * scale));
    ColoredText(kWarpListTextColor, view.title);

    // ── LA LISTE ─────────────────────────────────────────────────────────────
    ImGui::SetCursorPos(ImVec2(kWarpListRowsPixels.x * scale, kWarpListRowsPixels.y * scale));
    const ImVec2 rows_size = ImVec2(size.x - kWarpListRowsMarginPixels.x * scale,
                                    size.y - kWarpListRowsMarginPixels.y * scale);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kWarpListRowsColor);
    // ⚠ LA LISTE NE DÉFILE PAS À LA MANIÈRE D'IMGUI : elle montre ses lignes à
    // partir de la première ligne montrée, et peint sa propre barre.
    constexpr ImGuiWindowFlags kRowsFlags =
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (ImGui::BeginChild(kRowsName, rows_size, ImGuiChildFlags_None, kRowsFlags)) {
      DrawRows(state, view, art, action, keys_moved);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ── LES DEUX BOUTONS ─────────────────────────────────────────────────────
    const ImVec2 button = ImVec2(kWarpListButtonPixels.x * scale, kWarpListButtonPixels.y * scale);
    const float buttons_y = size.y - kWarpListButtonsFromBottomPixels * scale;
    ImGui::SetCursorPos(ImVec2(size.x - kWarpListOkFromRightPixels * scale, buttons_y));
    // « OK » sans ligne choisie ne fait rien, et la fenêtre reste ouverte.
    if (BitmapButton("##ok", art, kOkFaces, core::Tr("OK"), button) &&
        state.selected != kWarpListNoRow) {
      action.verb = WarpListVerb::kChoose;
      action.row = state.selected;
    }
    ImGui::SetCursorPos(ImVec2(size.x - kWarpListCancelFromRightPixels * scale, buttons_y));
    if (BitmapButton("##cancel", art, kCancelFaces, core::Tr("Annuler"), button)) {
      action.verb = WarpListVerb::kCancel;
    }
  }
  ImGui::End();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(2);
  return action;
}

}  // namespace ro::ui
