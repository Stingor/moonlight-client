#include "ro/ui/survivors_album_window.h"

#include <algorithm>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/survivors_hud.h"
#include "ro/ui/window_bounds.h"
#include "ro/ui/window_name.h"

#include "survivors_meta_text.h"
#include "survivors_text.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

inline constexpr const char* kAlbumTitle = core::TrText("Album des Survivants");

constexpr float kFirstWidth = 480.0F;
constexpr float kFirstHeight = 600.0F;
// Une case de la grille : une carte debout, au rapport des illustrations.
constexpr float kCellWidth = 60.0F;
constexpr float kCellHeight = 80.0F;
constexpr float kCellGap = 8.0F;
constexpr float kDetailHeight = 150.0F;
constexpr float kFeatIconSide = 24.0F;
constexpr ImU32 kCursorEdge = IM_COL32(255, 215, 90, 220);
constexpr std::size_t kLineSize = 160;

// Une image ramenée dans une boîte, son rapport gardé ; la boîte entière
// quand sa taille est inconnue.
[[nodiscard]] ImVec2 Fitted(ImVec2 size, ImVec2 box) {
  if (size.x <= 0.0F || size.y <= 0.0F) return box;
  const float scale = std::min(box.x / size.x, box.y / size.y);
  return ImVec2(size.x * scale, size.y * scale);
}

// L'image d'une carte : son illustration trouvée, le dos sinon.
void DrawCardFace(const SurvivorsAlbumEntry& card, const SurvivorsAlbumView& view, ImVec2 box) {
  const bool found = card.found > 0;
  const ImTextureID texture = found ? card.illustration : view.card_back;
  const ImVec2 size = found ? card.illustration_size : view.card_back_size;
  if (texture != 0) {
    ImGui::Image(texture, Fitted(size, box));
  } else {
    ImGui::Dummy(box);
  }
}

[[nodiscard]] std::size_t ColumnsFor(float width) {
  const float columns = (width + kCellGap) / (kCellWidth + kCellGap);
  return std::max<std::size_t>(1, static_cast<std::size_t>(columns));
}

void DrawGrid(SurvivorsAlbumState& state, const SurvivorsAlbumView& view, std::size_t columns) {
  std::array<char, kLineSize> line = {};
  for (std::size_t index = 0; index < view.cards.size(); ++index) {
    const SurvivorsAlbumEntry& card = view.cards[index];
    if (index % columns != 0) ImGui::SameLine(0.0F, kCellGap);
    ImGui::PushID(static_cast<int>(index));
    ImGui::BeginGroup();
    DrawCardFace(card, view, ImVec2(kCellWidth, kCellHeight));
    if (card.found > 0) {
      std::snprintf(line.data(), line.size(), "x%u", static_cast<unsigned>(card.found));
      ImGui::TextUnformatted(line.data());
    } else {
      FaintText("?");
    }
    ImGui::EndGroup();
    if (index == state.cursor) {
      ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                          kCursorEdge);
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      state.cursor = index;
      state.detail = index;
    }
    if (card.found > 0) {
      SkinTooltip(card.name);
    } else {
      std::snprintf(line.data(), line.size(), core::Tr("Tombe de : %s"), card.dropped_by);
      SkinTooltip(line.data());
    }
    ImGui::PopID();
  }
}

void DrawDetail(const SurvivorsAlbumEntry& card, const SurvivorsAlbumView& view) {
  ImGui::Separator();
  DrawCardFace(card, view, ImVec2(kDetailHeight * kCellWidth / kCellHeight, kDetailHeight));
  ImGui::SameLine();
  ImGui::BeginGroup();
  std::array<char, kLineSize> line = {};
  if (card.found == 0) {
    FaintText(core::Tr("Carte non trouvée"));
    std::snprintf(line.data(), line.size(), core::Tr("Tombe de : %s"), card.dropped_by);
    ImGui::TextUnformatted(line.data());
  } else {
    ImGui::TextUnformatted(card.name);
    ImGui::TextWrapped("%s", SurvivorsText(card.description));
    std::snprintf(line.data(), line.size(), core::Tr("Trouvée %u fois"),
                  static_cast<unsigned>(card.found));
    ImGui::TextUnformatted(line.data());
    std::snprintf(line.data(), line.size(), core::Tr("Première trouvaille : %s"),
                  SurvivorsDateOfDay(card.first_found_day).data());
    ImGui::TextUnformatted(line.data());
  }
  ImGui::EndGroup();
}

void DrawFeats(const SurvivorsAlbumView& view) {
  if (view.feats.empty()) return;
  ImGui::SeparatorText(core::Tr("Skills à débloquer"));
  for (const SurvivorsFeatEntry& feat : view.feats) {
    ImGui::PushID(feat.key);
    if (feat.soon) ImGui::BeginDisabled();
    if (feat.icon != 0) {
      ImGui::Image(feat.icon, ImVec2(kFeatIconSide, kFeatIconSide));
    } else {
      ImGui::Dummy(ImVec2(kFeatIconSide, kFeatIconSide));
    }
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextUnformatted(feat.name);
    if (feat.soon) {
      FaintText(core::Tr("bientôt"));
    } else if (feat.reached) {
      ImGui::TextUnformatted(core::Tr("Débloqué"));
    } else {
      FaintText(SurvivorsFeatGoal(feat.key));
      const float share = feat.threshold == 0
                              ? 0.0F
                              : static_cast<float>(static_cast<double>(feat.progress) /
                                                   static_cast<double>(feat.threshold));
      ImGui::ProgressBar(std::min(share, 1.0F), ImVec2(-1.0F, 0.0F),
                         SurvivorsFeatProgressText(feat).data());
    }
    ImGui::EndGroup();
    if (feat.soon) ImGui::EndDisabled();
    ImGui::PopID();
  }
}

void ReadKeys(SurvivorsAlbumState& state, std::size_t count, std::size_t columns) {
  if (count == 0 || !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;
  const std::size_t last = count - 1;
  if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && state.cursor > 0) --state.cursor;
  if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && state.cursor < last) ++state.cursor;
  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && state.cursor >= columns) state.cursor -= columns;
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && state.cursor + columns <= last) {
    state.cursor += columns;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
      ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
    state.detail = state.cursor;
  }
}

}  // namespace

std::array<char, kSurvivorsFeatProgressTextSize> SurvivorsFeatProgressText(
    const SurvivorsFeatEntry& feat) {
  std::array<char, kSurvivorsFeatProgressTextSize> text = {};
  if (feat.measure == SurvivorsFeatMeasure::kSurvivedMs) {
    std::snprintf(text.data(), text.size(), "%s / %s", SurvivorsClockText(feat.progress).data(),
                  SurvivorsClockText(feat.threshold).data());
  } else {
    std::snprintf(text.data(), text.size(), "%llu / %llu",
                  static_cast<unsigned long long>(feat.progress),
                  static_cast<unsigned long long>(feat.threshold));
  }
  return text;
}

void DrawSurvivorsAlbumBody(SurvivorsAlbumState& state, const SurvivorsAlbumView& view) {
  if (!view.cards.empty()) state.cursor = std::min(state.cursor, view.cards.size() - 1);
  if (state.detail.has_value() && *state.detail >= view.cards.size()) state.detail.reset();
  const std::size_t columns = ColumnsFor(ImGui::GetContentRegionAvail().x);
  DrawGrid(state, view, columns);
  ReadKeys(state, view.cards.size(), columns);
  if (state.detail.has_value()) DrawDetail(view.cards[*state.detail], view);
  DrawFeats(view);
}

void DrawSurvivorsAlbumWindow(SurvivorsAlbumState& state, const SurvivorsAlbumView& view,
                              const SkinArt& art) {
  if (!state.open) return;
  static WindowName name(kAlbumTitle, kSurvivorsAlbumId);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidth, kFirstHeight), ImGuiCond_FirstUseEver);
  ConstrainNextWindowToScreen(name.Get());
  if (!BeginSkinnedWindow(name.Get(), &state.open, art)) {
    EndSkinnedWindow();
    return;
  }
  DrawSurvivorsAlbumBody(state, view);
  EndSkinnedWindow();
}

}  // namespace ro::ui
