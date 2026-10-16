// Le clavier d'une liste. Voir l'en-tête.

#include "ro/ui/list_keys.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include <imgui_internal.h>

namespace ro::ui {

int ListKeyTarget(int current, int count, int page_rows, ListKeyMove move) {
  if (count <= 0) {
    return kListNoEntry;
  }
  const int last = count - 1;
  const int page = std::max(1, page_rows);
  const bool none = current < 0 || current > last;
  switch (move) {
    case ListKeyMove::kNone:
      return none ? kListNoEntry : current;
    case ListKeyMove::kPrevious:
      return none ? last : std::max(0, current - 1);
    case ListKeyMove::kNext:
      return none ? 0 : std::min(last, current + 1);
    case ListKeyMove::kPageUp:
      return none ? last : std::max(0, current - page);
    case ListKeyMove::kPageDown:
      return none ? 0 : std::min(last, current + page);
    case ListKeyMove::kFirst:
      return 0;
    case ListKeyMove::kLast:
      return last;
  }
  return current;
}

int ListKeyTarget(int current, int count, int page_rows, ListKeyMove move,
                  const std::function<bool(int)>& selectable) {
  const int aimed = ListKeyTarget(current, count, page_rows, move);
  if (move == ListKeyMove::kNone || aimed == kListNoEntry || !selectable || selectable(aimed)) {
    return aimed;
  }
  const bool downward =
      move == ListKeyMove::kNext || move == ListKeyMove::kPageDown || move == ListKeyMove::kFirst;
  const int toward = downward ? 1 : -1;
  for (const int direction : {toward, -toward}) {
    for (int rank = aimed + direction; rank >= 0 && rank < count; rank += direction) {
      if (selectable(rank)) {
        return rank;
      }
    }
  }
  return kListNoEntry;
}

ListKeyMove PressedListKey(bool arrows_only) {
  constexpr std::array<std::pair<ImGuiKey, ListKeyMove>, 2> kArrows = {{
      {ImGuiKey_UpArrow, ListKeyMove::kPrevious},
      {ImGuiKey_DownArrow, ListKeyMove::kNext},
  }};
  constexpr std::array<std::pair<ImGuiKey, ListKeyMove>, 4> kJumps = {{
      {ImGuiKey_PageUp, ListKeyMove::kPageUp},
      {ImGuiKey_PageDown, ListKeyMove::kPageDown},
      {ImGuiKey_Home, ListKeyMove::kFirst},
      {ImGuiKey_End, ListKeyMove::kLast},
  }};
  for (const auto& [key, move] : kArrows) {
    if (ImGui::IsKeyPressed(key, /*repeat=*/true)) {
      return move;
    }
  }
  if (arrows_only) {
    return ListKeyMove::kNone;
  }
  for (const auto& [key, move] : kJumps) {
    if (ImGui::IsKeyPressed(key, /*repeat=*/true)) {
      return move;
    }
  }
  return ListKeyMove::kNone;
}

int VisibleListRows(float row_pitch) {
  const ImGuiWindow* const window = ImGui::GetCurrentWindow();
  if (window == nullptr || row_pitch <= 0.0F) {
    return 1;
  }
  return std::max(1, static_cast<int>(std::floor(window->InnerRect.GetHeight() / row_pitch)));
}

void KeepLastItemInView() { ImGui::ScrollToItem(ImGuiScrollFlags_KeepVisibleEdgeY); }

namespace {

// Où la fenêtre de la liste retient le curseur de `ComboKeyChoice::kOnEnter`.
constexpr const char* kCursorKey = "##curseur_clavier";

}  // namespace

ComboKeys::ComboKeys(int chosen, int count, const std::function<bool(int)>& selectable,
                     ComboKeyChoice choice) {
  marked_ = chosen >= 0 && chosen < count ? chosen : kListNoEntry;
  ImGuiStorage* const storage = ImGui::GetStateStorage();
  const ImGuiID cursor_key = ImGui::GetID(kCursorKey);
  const bool on_enter = choice == ComboKeyChoice::kOnEnter;
  if (on_enter) {
    // Le curseur part de l'entrée choisie à chaque ouverture.
    if (ImGui::IsWindowAppearing()) {
      storage->SetInt(cursor_key, marked_);
    }
    marked_ = std::clamp(storage->GetInt(cursor_key, marked_), kListNoEntry, count - 1);
  }
  // ⚠ LA LISTE ET SES FILLES, PAS LA FENÊTRE D'OÙ ELLE S'EST OUVERTE : un clic
  // dans la fille qui porte les entrées lui donne la main.
  if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows |
                              ImGuiFocusedFlags_NoPopupHierarchy)) {
    return;
  }
  const ListKeyMove move = PressedListKey(/*arrows_only=*/ImGui::GetIO().WantTextInput);
  const int target = ListKeyTarget(
      marked_, count, VisibleListRows(ImGui::GetTextLineHeightWithSpacing()), move, selectable);
  if (move != ListKeyMove::kNone && target != kListNoEntry && target != marked_) {
    keyed_ = target;
    marked_ = target;
    if (on_enter) {
      storage->SetInt(cursor_key, target);
    } else {
      taken_ = target;
    }
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
    if (on_enter && marked_ != kListNoEntry && (!selectable || selectable(marked_))) {
      taken_ = marked_;
    }
    ImGui::CloseCurrentPopup();
  }
}

bool ComboKeys::Took(int rank, bool clicked) {
  if (rank == marked_) {
    ImGui::SetItemDefaultFocus();
    if (rank == keyed_) {
      KeepLastItemInView();
    }
  }
  return clicked || rank == taken_;
}

}  // namespace ro::ui
