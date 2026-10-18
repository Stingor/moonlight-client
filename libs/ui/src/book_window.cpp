// LE LIVRE QU'ON LIT : ses pages, et la bulle de la lecture à voix haute.

#include "ro/ui/book_window.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "ro/core/i18n.h"
#include "ro/ui/rich_text.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "description_odds.h"
#include "ui_strings.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// La taille d'ouverture et le plancher : un livre se lit en lignes longues.
constexpr float kFirstWidthPixels = 620.0F;
constexpr float kFirstHeightPixels = 460.0F;
constexpr float kLeastWidthPixels = 360.0F;
constexpr float kLeastHeightPixels = 240.0F;
// Au-delà d'un demi-pixel, la largeur a vraiment changé : on replie.
constexpr float kWidthTolerancePixels = 0.5F;

// « ^RRGGBB » : la marque et six chiffres hexadécimaux. « ^000000 » rend
// l'encre par défaut.
constexpr char kColorMark = '^';
constexpr std::size_t kColorTokenLength = 7;
constexpr std::string_view kDefaultInk = "^000000";

// LA BULLE : en haut de l'écran, centrée, comme le client la pose. ⚠ SANS
// FOND : un liseré gris aux coins coupés sur le décor, et le texte rose pâle
// porte une ombre noire décalée d'un pixel.
constexpr float kRecitalTopPixels = 81.0F;
constexpr float kRecitalPaddingXPixels = 6.0F;
constexpr float kRecitalPaddingYPixels = 4.0F;
constexpr float kRecitalRounding = 3.0F;
constexpr float kRecitalShadowPixels = 1.0F;
constexpr ImU32 kRecitalBorder = IM_COL32(0x5C, 0x5C, 0x5C, 255);
constexpr ImU32 kRecitalInk = IM_COL32(0xFF, 0xE4, 0xE1, 255);
constexpr ImU32 kRecitalShadow = IM_COL32(0, 0, 0, 255);

// Une marque de couleur commence-t-elle ici ?
[[nodiscard]] bool ColorTokenAt(std::string_view text, std::size_t at) {
  if (at + kColorTokenLength > text.size() || text[at] != kColorMark) {
    return false;
  }
  return std::all_of(text.begin() + static_cast<std::ptrdiff_t>(at + 1),
                     text.begin() + static_cast<std::ptrdiff_t>(at + kColorTokenLength),
                     IsHexDigit);
}

// Le texte sans ses marques, tel qu'il se mesure et se lit.
[[nodiscard]] std::string Visible(std::string_view text) {
  std::string shown;
  shown.reserve(text.size());
  for (std::size_t at = 0; at < text.size();) {
    if (ColorTokenAt(text, at)) {
      at += kColorTokenLength;
      continue;
    }
    shown.push_back(text[at]);
    ++at;
  }
  return shown;
}

// La couleur active après ce texte, partant de `active`.
[[nodiscard]] std::string ColorAfter(std::string_view text, std::string active) {
  for (std::size_t at = 0; at < text.size(); ++at) {
    if (ColorTokenAt(text, at)) {
      const std::string_view token = text.substr(at, kColorTokenLength);
      active = token == kDefaultInk ? std::string() : std::string(token);
      at += kColorTokenLength - 1;
    }
  }
  return active;
}

[[nodiscard]] float VisibleWidth(std::string_view text) { return TextWidth(Visible(text)); }

}  // namespace

std::vector<std::string> WrapBookLines(std::span<const std::string> lines, float width) {
  std::vector<std::string> rows;
  std::string active;
  for (const std::string& line : lines) {
    std::string row = active;
    bool row_has_words = false;
    std::size_t from = 0;
    while (from <= line.size()) {
      const std::size_t space = line.find(' ', from);
      const std::size_t end = space == std::string::npos ? line.size() : space;
      const std::string_view word(line.data() + from, end - from);
      const std::string candidate =
          row_has_words ? row + " " + std::string(word) : row + std::string(word);
      // ⚠ UN MOT PLUS LONG QUE LA LIGNE RESTE ENTIER, sur la sienne : le couper
      // le rendrait illisible, et la fenêtre se redimensionne.
      if (row_has_words && VisibleWidth(candidate) > width) {
        rows.push_back(row);
        row = active + std::string(word);
      } else {
        row = candidate;
      }
      row_has_words = true;
      active = ColorAfter(word, active);
      if (space == std::string::npos) {
        break;
      }
      from = space + 1;
    }
    rows.push_back(row);
  }
  return rows;
}

BookWindowAction DrawBookWindow(BookWindowState& state, const BookView& view, const SkinArt& art) {
  BookWindowAction action;
  if (!state.open || view.item_id == 0) {
    state.was_open = false;
    // FERMÉE, ELLE REPLIE QUAND MÊME le livre désigné, à la dernière largeur
    // mesurée : la lecture à voix haute dit ces lignes-là, fenêtre cachée.
    if (view.item_id != 0 && state.rows_width > 0.0F && state.rows_for != view.item_id) {
      state.rows = WrapBookLines(view.lines, state.rows_width);
      state.rows_for = view.item_id;
    }
    return action;
  }
  PlaceDescription(state.place_at_mouse, state.was_open, state.placement);
  if (state.raise) {
    ImGui::SetNextWindowFocus();
    state.raise = false;
  }
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels, kFirstHeightPixels), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(kLeastWidthPixels, kLeastHeightPixels),
                                      ImVec2(FLT_MAX, FLT_MAX));
  const std::string title =
      (view.title.empty() ? std::string(core::Tr("Livre")) : std::string(view.title)) + "###book";
  bool open = true;
  const bool shown = BeginSkinnedWindow(title.c_str(), &open, art, SkinnedTitle::kDescription,
                                        ImGuiWindowFlags_NoCollapse);
  KeepDescriptionOnScreen();
  if (!open) {
    // LE SIGNET : la page où l'on ferme, que `app` retient.
    action.closed = true;
    action.page = state.page;
    state.open = false;
  }
  if (!shown) {
    EndSkinnedWindow();
    return action;
  }

  const float width = ImGui::GetContentRegionAvail().x;
  if (state.rows_for != view.item_id ||
      std::fabs(state.rows_width - width) > kWidthTolerancePixels) {
    state.rows = WrapBookLines(view.lines, width);
    state.rows_for = view.item_id;
    state.rows_width = width;
  }
  const int pages = std::max(
      1, static_cast<int>((state.rows.size() + kBookLinesPerPage - 1) / kBookLinesPerPage));
  state.page = std::clamp(state.page, 1, pages);

  // ── LA NAVIGATION ─────────────────────────────────────────────────────────
  const bool first = state.page <= 1;
  const bool last = state.page >= pages;
  ImGui::BeginDisabled(first);
  if (SkinSmallButton(core::Tr("<< Début"), art)) {
    state.page = 1;
  }
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("< Précédente"), art)) {
    state.page -= 1;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::Text(core::Tr("Page %d / %d"), state.page, pages);
  ImGui::SameLine();
  ImGui::BeginDisabled(last);
  if (SkinSmallButton(core::Tr("Suivante >"), art)) {
    state.page += 1;
  }
  ImGui::EndDisabled();
  ImGui::Separator();

  // ── LA PAGE, sur le fond que le livre demande ──────────────────────────────
  const std::size_t first_row = static_cast<std::size_t>(state.page - 1) * kBookLinesPerPage;
  const std::size_t count = first_row < state.rows.size()
                                ? std::min(kBookLinesPerPage, state.rows.size() - first_row)
                                : 0;
  if (view.background != 0) {
    const ImVec2 corner = ImGui::GetCursorScreenPos();
    const ImVec2 extent = ImGui::GetContentRegionAvail();
    ImGui::GetWindowDrawList()->AddRectFilled(
        corner, ImVec2(corner.x + extent.x, corner.y + extent.y), view.background);
  }
  const RichText laid =
      LayOutRichTextHere(std::span<const std::string>(state.rows).subspan(first_row, count),
                         ImGui::GetColorU32(ImGuiCol_Text), width);
  static_cast<void>(DrawRichText("##page_livre", laid, ImGui::GetTextLineHeightWithSpacing()));

  // LA MOLETTE TOURNE LES PAGES : vers soi, la suivante.
  const float notches = WheelOver(state.wheel, ImGui::GetID("##pages_livre"),
                                  ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows));
  if (notches < 0.0F && !last) {
    state.page += 1;
  } else if (notches > 0.0F && !first) {
    state.page -= 1;
  }
  EndSkinnedWindow();
  return action;
}

std::string BookLinePlain(std::string_view line) { return Visible(line); }

void DrawBookRecital(std::string_view line) {
  const std::string shown = Visible(line);
  if (shown.empty()) {
    return;
  }
  ImDrawList* const into = ImGui::GetForegroundDrawList();
  ImFont* const font = ImGui::GetFont();
  const float size = ImGui::GetFontSize();
  const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0F, shown.c_str(), nullptr);
  const ImVec2 screen = ImGui::GetMainViewport()->Size;
  const float width = extent.x + 2.0F * kRecitalPaddingXPixels;
  const ImVec2 top_left(std::floor((screen.x - width) * 0.5F), kRecitalTopPixels);
  const ImVec2 bottom_right(top_left.x + width,
                            top_left.y + extent.y + 2.0F * kRecitalPaddingYPixels);
  into->AddRect(top_left, bottom_right, kRecitalBorder, kRecitalRounding);
  const ImVec2 ink(top_left.x + kRecitalPaddingXPixels, top_left.y + kRecitalPaddingYPixels);
  into->AddText(font, size, ImVec2(ink.x + kRecitalShadowPixels, ink.y + kRecitalShadowPixels),
                kRecitalShadow, shown.c_str());
  into->AddText(font, size, ink, kRecitalInk, shown.c_str());
}

}  // namespace ro::ui
