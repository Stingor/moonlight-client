// La colonne de gauche du journal des quêtes : les filtres, les groupes, les
// lignes, et l'historique. Voir l'en-tête.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "quest_window_parts.h"

namespace ro::ui::quest_window {
namespace {

namespace paint_kit = achievement;

// ── LA BARRE DE FILTRES ─────────────────────────────────────────────────────

// Un filtre de catégorie à image : sa case, l'écart entre deux, et le retrait
// de l'image dans sa case.
constexpr float kCategorySide = 24.0F;
constexpr float kCategoryGap = 3.0F;
constexpr float kCategoryInset = 2.0F;
constexpr float kCategoryRounding = 3.0F;
// Le champ de recherche ne rétrécit pas sous cette largeur : il passe alors à
// la ligne.
constexpr float kSearchLeast = 110.0F;
// La liste des ordres.
constexpr float kSortWidth = 120.0F;
// Dans l'ordre de `QuestSort`.
constexpr std::array<const char*, kQuestSortCount> kSortLabels = {
    core::TrText("Récentes"), core::TrText("Temps restant"), core::TrText("Nom"),
    core::TrText("Progression")};

// ── UNE LIGNE ───────────────────────────────────────────────────────────────

constexpr float kRowHeight = 46.0F;
// L'icône, centrée en hauteur.
constexpr float kRowIconLeft = 6.0F;
constexpr float kRowIconTop = 8.0F;
// Le titre, le résumé, et la barre d'avancée au pied.
constexpr float kRowTextLeft = 38.0F;
constexpr float kRowTitleTop = 6.0F;
constexpr float kRowSummaryTop = 24.0F;
constexpr float kRowBarTop = 40.0F;
constexpr float kRowBarHeight = 3.0F;
// La colonne de droite : la marge au bord, l'écart entre deux pièces.
constexpr float kRowRightPad = 8.0F;
constexpr float kRowColumnGap = 6.0F;
// Le cadenas et la case « suivie », sur la seconde ligne.
constexpr float kRowControlsTop = 22.0F;
constexpr float kLockWidth = 20.0F;
constexpr float kLockHeight = 17.0F;
constexpr float kTrackSide = 14.0F;
// Un trait d'un pixel : le filet sous une ligne sans art.
constexpr float kRulePixels = 1.0F;

// Le fond d'une ligne sans art : au repos, choisie, puis le voile du survol.
constexpr ImU32 kRowFill = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
constexpr ImU32 kRowSelectedFill = IM_COL32(0xD6, 0xE4, 0xFA, 0xFF);
constexpr ImU32 kRowHoverVeil = IM_COL32(0x9C, 0xB8, 0xEA, 0x2C);
// Le halo d'une pièce survolée de la colonne de droite.
constexpr ImU32 kControlHoverVeil = IM_COL32(0x9C, 0xB8, 0xEA, 0x70);
constexpr float kControlRounding = 3.0F;

// LE CADENAS DESSINÉ, quand l'installation n'en a pas l'art : un corps, et
// une anse fermée (suspendue) ou levée (active).
constexpr float kPadlockBodyLeft = 5.0F;
constexpr float kPadlockBodyTop = 8.0F;
constexpr float kPadlockBodyWidth = 10.0F;
constexpr float kPadlockBodyHeight = 8.0F;
constexpr float kPadlockShackleRadius = 3.5F;
constexpr float kPadlockShackleThickness = 1.6F;
constexpr float kPadlockOpenLift = 3.0F;
constexpr float kPadlockRounding = 1.5F;
constexpr int kPadlockSegments = 10;
constexpr ImU32 kPadlockOpenInk = IM_COL32(0x7A, 0x8C, 0xAE, 0xFF);
constexpr ImU32 kPadlockClosedInk = IM_COL32(0xD9, 0x8A, 0x1E, 0xFF);

// ── UN EN-TÊTE DE GROUPE ────────────────────────────────────────────────────

constexpr float kGroupHeight = 24.0F;
constexpr float kGroupArrowLeft = 6.0F;
constexpr float kGroupIconLeft = 20.0F;
constexpr float kGroupIconSide = 20.0F;
constexpr float kGroupTextGap = 6.0F;
constexpr ImU32 kGroupFill = IM_COL32(0xE4, 0xEA, 0xF5, 0xFF);
constexpr ImU32 kGroupHoverFill = IM_COL32(0xD5, 0xDF, 0xF1, 0xFF);
// Le triangle de pliage.
constexpr float kGroupMarkSide = 10.0F;

// ── L'HISTORIQUE ────────────────────────────────────────────────────────────

constexpr float kHistoryRowHeight = 40.0F;
constexpr float kHistoryIconTop = 5.0F;
constexpr float kHistoryTitleTop = 5.0F;
constexpr float kHistoryDateTop = 22.0F;
// L'écart entre la date et le lien vers l'étape suivante.
constexpr float kHistoryLinkGap = 8.0F;

// ── LE VIDE ET L'AVERTISSEMENT ──────────────────────────────────────────────

// Le texte d'une liste vide, sous le haut de la liste.
constexpr float kEmptyTop = 40.0F;
constexpr float kEmptySideMargin = 12.0F;
// Le bandeau du catalogue manquant : sa marge, son liseré.
constexpr float kBannerMargin = 6.0F;
constexpr float kBannerPadding = 6.0F;
constexpr ImU32 kBannerFill = IM_COL32(0xFF, 0xF4, 0xD6, 0xFF);
constexpr ImU32 kBannerBorder = IM_COL32(0xE0, 0xB0, 0x50, 0xFF);
constexpr ImU32 kBannerInk = IM_COL32(0x5A, 0x3A, 0x08, 0xFF);

// Assez pour « Sortie le 2026/10/06 16:40 » dans toute langue.
constexpr std::size_t kLineBytes = 160;

[[nodiscard]] bool Contains(const std::vector<std::string>& keys, std::string_view key) {
  return std::ranges::find(keys, key) != keys.end();
}

void Toggle(std::vector<std::string>& keys, std::string_view key) {
  const auto found = std::ranges::find(keys, key);
  if (found != keys.end()) {
    keys.erase(found);
  } else {
    keys.emplace_back(key);
  }
}

// La souris est-elle sur ce rectangle d'art, dans la fenêtre courante ?
[[nodiscard]] bool Pointed(const Painter& paint, float x, float y, float width, float height) {
  return ImGui::IsWindowHovered() &&
         ImGui::IsMouseHoveringRect(paint.At(x, y), paint.At(x + width, y + height));
}

// La largeur disponible, en pixels d'art.
[[nodiscard]] float ArtWidth() { return ImGui::GetContentRegionAvail().x / Scale(); }

void PaintPadlock(const Painter& paint, float x, float y, bool open) {
  const ImU32 ink = PaperColor(open ? kPadlockOpenInk : kPadlockClosedInk);
  const float scale = paint.scale();
  ImDrawList* const into = paint.into();
  into->AddRectFilled(
      paint.At(x + kPadlockBodyLeft, y + kPadlockBodyTop),
      paint.At(x + kPadlockBodyLeft + kPadlockBodyWidth, y + kPadlockBodyTop + kPadlockBodyHeight),
      ink, kPadlockRounding * scale);
  const float lift = open ? kPadlockOpenLift : 0.0F;
  const ImVec2 middle =
      paint.At(x + kPadlockBodyLeft + kPadlockBodyWidth * 0.5F, y + kPadlockBodyTop - lift);
  into->PathArcTo(middle, kPadlockShackleRadius * scale, std::numbers::pi_v<float>,
                  2.0F * std::numbers::pi_v<float>, kPadlockSegments);
  into->PathStroke(ink, ImDrawFlags_None, kPadlockShackleThickness * scale);
}

// ── LA BARRE DE FILTRES ─────────────────────────────────────────────────────

// UN FILTRE À IMAGE : une case, l'image dedans, enfoncée quand il est actif.
[[nodiscard]] bool CategoryImageToggle(const QuestCategoryView& category, bool active,
                                       ImU32 border) {
  const ImVec2 corner = ImGui::GetCursorScreenPos();
  const Painter paint(ImGui::GetWindowDrawList(), corner, Scale());
  const bool pressed =
      ImGui::InvisibleButton("##categorie", ImVec2(Px(kCategorySide), Px(kCategorySide)));
  const bool hovered = ImGui::IsItemHovered();
  // La sélection de l'habillage, que la palette de la fenêtre a posée.
  ImU32 fill = 0;
  if (active) {
    fill = ImGui::GetColorU32(ImGuiCol_HeaderActive);
  } else if (hovered) {
    fill = ImGui::GetColorU32(ImGuiCol_HeaderHovered);
  }
  if (fill != 0) {
    paint.into()->AddRectFilled(paint.At(0.0F, 0.0F), paint.At(kCategorySide, kCategorySide), fill,
                                kCategoryRounding * Scale());
  }
  paint.into()->AddRect(paint.At(0.0F, 0.0F), paint.At(kCategorySide, kCategorySide),
                        active ? OnPaper(kRowTitleText, PaintingLook().body) : border,
                        kCategoryRounding * Scale());
  PaintFitted(paint, category.image, kCategoryInset, kCategoryInset,
              kCategorySide - 2.0F * kCategoryInset, kCategorySide - 2.0F * kCategoryInset);
  SkinTooltip(Owned(category.label).c_str());
  return pressed;
}

// Les filtres de catégorie, puis la recherche qui prend le reste de la ligne.
void DrawCategoryRow(QuestWindowState& state, const QuestWindowView& view, const SkinArt& skin) {
  const float row_left = ImGui::GetCursorPosX();
  for (std::size_t rank = 0; rank < view.categories.size(); ++rank) {
    const QuestCategoryView& category = view.categories[rank];
    if (rank > 0) {
      ImGui::SameLine(0.0F, Px(kCategoryGap));
    }
    ImGui::PushID(static_cast<int>(rank));
    const bool active = state.category == category.key;
    const bool pressed = category.image.usable()
                             ? CategoryImageToggle(category, active, skin.look().border)
                             : SkinSmallToggleButton(Owned(category.label).c_str(), active, skin);
    // RECLIQUER LE FILTRE ACTIF L'ÔTE : la liste revient à « tout ».
    if (pressed) {
      state.category = std::string(active ? kAllCategory : category.key);
    }
    ImGui::PopID();
  }
  if (!view.categories.empty()) {
    ImGui::SameLine(0.0F, Px(kCategoryGap));
  }
  float search_width = row_left + Px(kListWidth) - ImGui::GetCursorPosX();
  if (search_width < Px(kSearchLeast)) {
    ImGui::NewLine();
    search_width = Px(kListWidth);
  }
  ImGui::SetNextItemWidth(search_width);
  static_cast<void>(ImGui::InputTextWithHint("##quetes_recherche", core::Tr("Rechercher…"),
                                             state.search.data(), state.search.size()));
}

// L'ordre et le regroupement : l'historique n'a ni l'un ni l'autre.
void DrawOrderRow(QuestWindowState& state, const SkinArt& skin) {
  ImGui::SetNextItemWidth(Px(kSortWidth));
  int sort = static_cast<int>(state.sort);
  if (SkinCombo("##quetes_ordre", &sort, kSortLabels, skin)) {
    state.sort = static_cast<QuestSort>(sort);
  }
  SkinTooltip(core::Tr("Ordre de la liste"));
  ImGui::SameLine();
  static_cast<void>(SkinCheckBox(core::Tr("Grouper"), &state.group_by_category, skin));
  SkinTooltip(core::Tr("Ranger les quêtes sous leur catégorie"));
}

// ── LE VIDE ET L'AVERTISSEMENT ──────────────────────────────────────────────

void DrawCatalogBanner(std::string_view problem) {
  const ImVec2 corner = ImGui::GetCursorScreenPos();
  const Painter paint(ImGui::GetWindowDrawList(), corner, Scale());
  const float width = ArtWidth() - 2.0F * kBannerMargin;
  const float wrap = (width - 2.0F * kBannerPadding) * Scale();
  const ImVec2 extent =
      ImGui::CalcTextSize(problem.data(), problem.data() + problem.size(), false, wrap);
  const float height = extent.y / Scale() + 2.0F * kBannerPadding;
  const ImU32 fill = PaperColor(kBannerFill);
  paint.Fill(kBannerMargin, kBannerMargin, width, height, fill);
  paint.Frame(kBannerMargin, kBannerMargin, width, height, PaperColor(kBannerBorder));
  const ImVec2 text_at = paint.At(kBannerMargin + kBannerPadding, kBannerMargin + kBannerPadding);
  ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), text_at,
                                      ReadableOn(PaperColor(kBannerInk), fill), problem.data(),
                                      problem.data() + problem.size(), wrap);
  ImGui::Dummy(ImVec2(Px(kBannerMargin * 2.0F + width), Px(height + 2.0F * kBannerMargin)));
}

// LE TEXTE D'UNE LISTE VIDE, gris, centré sous le haut de la liste.
void DrawEmptyText(const char* text) {
  const float margin = Px(kEmptySideMargin);
  const float room = ImGui::GetContentRegionAvail().x - 2.0F * margin;
  const float width = ImGui::CalcTextSize(text).x;
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + Px(kEmptyTop));
  ImGui::PushStyleColor(ImGuiCol_Text, FaintInk());
  if (width <= room) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + margin + (room - width) * 0.5F);
    ImGui::TextUnformatted(text);
  } else {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + margin);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + room);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
  }
  ImGui::PopStyleColor();
}

[[nodiscard]] bool Filtered(const QuestWindowState& state) {
  return state.search[0] != '\0' || state.category != kAllCategory;
}

[[nodiscard]] const char* EmptyText(const QuestWindowState& state, const QuestWindowView& view) {
  if (Filtered(state)) {
    return core::Tr("Aucune quête ne correspond à la recherche.");
  }
  switch (view.tab) {
    case QuestTab::kActive:
      return core::Tr("Aucune quête en cours.");
    case QuestTab::kSuspended:
      return core::Tr("Aucune quête suspendue.");
    case QuestTab::kHistory:
      return core::Tr("Aucune quête dans l'historique.");
    case QuestTab::kRecommended:
      return core::Tr("Aucune recommandation.");
    case QuestTab::kWaiting:
      return core::Tr("Aucune quête en attente.");
  }
  return "";
}

// ── UNE LIGNE ───────────────────────────────────────────────────────────────

// Le fond : l'art étiré à la largeur, sinon un aplat et son filet ; puis le
// voile du survol. Tous posés sur le papier.
void PaintRowBack(const Painter& paint, const QuestArt& art, float width, float height,
                  bool selected, bool hovered) {
  const SkinImage& back = selected ? art.row_selected : art.row;
  if (back.usable()) {
    PaintOnPaper(paint.into(), back, paint.At(0.0F, 0.0F), paint.At(width, height), Paper());
  } else {
    paint.Fill(0.0F, 0.0F, width, height, PaperColor(selected ? kRowSelectedFill : kRowFill));
    paint.Fill(0.0F, height - kRulePixels, width, kRulePixels, PaperColor(kRuleColor));
  }
  if (hovered && !selected) {
    paint.Fill(0.0F, 0.0F, width, height, PaperColor(kRowHoverVeil));
  }
}

[[nodiscard]] ImU32 TimeColor(const QuestRowView& row) {
  // UNE ATTENTE ÉCHUE EST UNE BONNE NOUVELLE : la quête est prête.
  if (row.waiting) {
    return row.expired ? ToneInk(SkinTone::kGood) : PaperInk(kTimeText);
  }
  if (row.expired) {
    return ToneInk(SkinTone::kBad);
  }
  return row.urgent ? ToneInk(SkinTone::kWarn) : PaperInk(kTimeText);
}

// Le cadenas : l'art de l'état, sinon le cadenas dessiné.
void PaintLock(const Painter& paint, const QuestArt& art, const QuestRowView& row, float left,
               bool hovered) {
  if (hovered) {
    paint.into()->AddRectFilled(paint.At(left, kRowControlsTop),
                                paint.At(left + kLockWidth, kRowControlsTop + kLockHeight),
                                PaperColor(kControlHoverVeil), kControlRounding * paint.scale());
  }
  const SkinImage& image = row.active ? art.lock_active : art.lock_suspended;
  if (image.usable()) {
    PaintFittedOnPaper(paint, image, left, kRowControlsTop, kLockWidth, kLockHeight);
  } else {
    PaintPadlock(paint, left, kRowControlsTop, row.active);
  }
}

// LE CONTENU : icône, titre, pastille, temps, résumé, barre, case et cadenas.
void PaintRowContent(const Painter& paint, const QuestArt& art, const QuestRowView& row,
                     float width, float track_left, float lock_left, bool lock_hovered,
                     bool track_hovered) {
  PaintFitted(paint, row.icon, kRowIconLeft, kRowIconTop, kIconWidth, kIconHeight);
  float title_right = width - kRowRightPad;
  if (!row.time_left.empty()) {
    const float time_width = paint.TextWidth(row.time_left, kRowTextSize);
    const float drop =
        std::trunc((paint.TextHeight(kRowTitleSize) - paint.TextHeight(kRowTextSize)) * 0.5F);
    paint.Text(title_right - time_width, kRowTitleTop + drop, row.time_left, kRowTextSize,
               TimeColor(row));
    title_right -= time_width + kRowColumnGap;
  }
  const char* const fresh = core::Tr("Nouveau");
  const float pill_room = row.fresh ? PillWidth(paint, fresh, kPillTextSize) + kRowColumnGap : 0.0F;
  const std::string title = quest_paint::FitToWidth(paint, row.title, kRowTitleSize, true,
                                                    title_right - kRowTextLeft - pill_room);
  paint.Text(kRowTextLeft, kRowTitleTop, title, kRowTitleSize, PaperInk(kRowTitleText), true);
  if (row.fresh) {
    const float pill_top =
        kRowTitleTop +
        std::trunc((paint.TextHeight(kRowTitleSize) - PillHeight(paint, kPillTextSize)) * 0.5F);
    static_cast<void>(
        PaintPill(paint, kRowTextLeft + paint.TextWidth(title, kRowTitleSize, true) + kRowColumnGap,
                  pill_top, fresh, kPillTextSize, kFreshFill, kFreshText));
  }
  const float lower_room = track_left - kRowColumnGap - kRowTextLeft;
  const std::string summary =
      quest_paint::FitToWidth(paint, row.summary, kRowTextSize, false, lower_room);
  paint.Text(kRowTextLeft, kRowSummaryTop, summary, kRowTextSize, BodyInk());
  quest_paint::PaintBar(paint, kRowTextLeft, kRowBarTop, lower_room, kRowBarHeight, row.progress,
                        PaperBar(row.objectives_met ? kProgressBarDone : kProgressBar));
  const float track_top = kRowControlsTop + (kLockHeight - kTrackSide) * 0.5F;
  if (art.track_on.usable() && art.track_off.usable()) {
    if (track_hovered) {
      paint.into()->AddRectFilled(paint.At(track_left, track_top),
                                  paint.At(track_left + kTrackSide, track_top + kTrackSide),
                                  PaperColor(kControlHoverVeil), kControlRounding * paint.scale());
    }
    PaintFittedOnPaper(paint, row.tracked ? art.track_on : art.track_off, track_left, track_top,
                       kTrackSide, kTrackSide);
  }
  PaintLock(paint, art, row, lock_left, lock_hovered);
}

void DrawRow(QuestWindowState& state, const QuestRowView& row, const QuestArt& art,
             const SkinArt& skin, QuestWindowAction& action) {
  const ImVec2 corner = ImGui::GetCursorScreenPos();
  const Painter paint(ImGui::GetWindowDrawList(), corner, Scale());
  const float width = ArtWidth();
  const bool selected = state.selected == row.quest_id;
  const bool visible = ImGui::IsRectVisible(paint.At(0.0F, 0.0F), paint.At(width, kRowHeight));
  // ⚠ LE FOND AVANT LES CONTRÔLES : la case « suivie » sans art est un widget,
  // peint à sa soumission, et un fond peint après la couvrirait.
  const bool row_pointed = Pointed(paint, 0.0F, 0.0F, width, kRowHeight);
  if (visible) {
    PaintRowBack(paint, art, width, kRowHeight, selected, row_pointed);
  }
  ImGui::PushID(static_cast<int>(row.quest_id));
  // ⚠ LES CONTRÔLES AVANT LA LIGNE : ImGui donne la souris au premier
  // rectangle soumis, et la ligne les couvre.
  const float lock_left = width - kRowRightPad - kLockWidth;
  const float track_left = lock_left - kRowColumnGap - kTrackSide;
  const paint_kit::HitResult lock = paint.Hit("##cadenas", lock_left, kRowControlsTop, kLockWidth,
                                              kLockHeight, paint_kit::kOnRelease);
  if (lock.pressed) {
    action.toggle_active = row.quest_id;
  }
  SkinTooltip(row.active ? core::Tr("Suspendre") : core::Tr("Reprendre"));
  bool track_hovered = false;
  if (art.track_on.usable() && art.track_off.usable()) {
    const float track_top = kRowControlsTop + (kLockHeight - kTrackSide) * 0.5F;
    const paint_kit::HitResult track =
        paint.Hit("##suivie", track_left, track_top, kTrackSide, kTrackSide, paint_kit::kOnRelease);
    if (track.pressed) {
      action.toggle_tracked = row.quest_id;
    }
    track_hovered = track.hovered;
  } else {
    ImGui::SetCursorScreenPos(paint.At(track_left, kRowControlsTop));
    bool tracked = row.tracked;
    if (SkinCheckBox("##suivie", &tracked, skin)) {
      action.toggle_tracked = row.quest_id;
    }
  }
  SkinTooltip(core::Tr("Suivre à l'écran"));
  ImGui::SetCursorScreenPos(corner);
  if (ImGui::InvisibleButton("##ligne", ImVec2(Px(width), Px(kRowHeight)))) {
    state.selected = row.quest_id;
    action.selected = row.quest_id;
  }
  if (state.scroll_to_selected && selected) {
    ImGui::ScrollToItem(ImGuiScrollFlags_KeepVisibleCenterY);
  }
  if (ImGui::IsItemHovered() && !row.icon_label.empty() &&
      Pointed(paint, kRowIconLeft, kRowIconTop, kIconWidth, kIconHeight)) {
    SkinTooltipFormat("%s", Owned(row.icon_label).c_str());
  }
  ImGui::PopID();
  if (visible) {
    PaintRowContent(paint, art, row, width, track_left, lock_left, lock.hovered, track_hovered);
  }
}

// L'EN-TÊTE D'UN GROUPE : la flèche, l'icône, le libellé et le compte. Un clic
// le replie ou le déplie.
void DrawGroupHeader(QuestWindowState& state, const QuestGroupView& group) {
  const ImVec2 corner = ImGui::GetCursorScreenPos();
  const Painter paint(ImGui::GetWindowDrawList(), corner, Scale());
  const float width = ArtWidth();
  const bool folded = Contains(state.folded_groups, group.key);
  ImGui::PushID(group.key.data(), group.key.data() + group.key.size());
  if (ImGui::InvisibleButton("##groupe", ImVec2(Px(width), Px(kGroupHeight)))) {
    Toggle(state.folded_groups, group.key);
  }
  const bool hovered = ImGui::IsItemHovered();
  ImGui::PopID();
  paint.Fill(0.0F, 0.0F, width, kGroupHeight, PaperColor(hovered ? kGroupHoverFill : kGroupFill));
  paint.Fill(0.0F, kGroupHeight - kRulePixels, width, kRulePixels, PaperColor(kRuleColor));
  const ImU32 title_ink = PaperInk(kRowTitleText);
  const float text_top = std::trunc((kGroupHeight - paint.TextHeight(kRowTitleSize)) * 0.5F);
  quest_paint::PaintFoldMark(paint, kGroupArrowLeft, (kGroupHeight - kGroupMarkSide) * 0.5F,
                             kGroupMarkSide, folded, title_ink, 0);
  float text_left = kGroupIconLeft;
  if (group.image.usable()) {
    PaintFitted(paint, group.image, kGroupIconLeft, (kGroupHeight - kGroupIconSide) * 0.5F,
                kGroupIconSide, kGroupIconSide);
    text_left += kGroupIconSide + kGroupTextGap;
  }
  paint.Text(text_left, text_top, group.label, kRowTitleSize, title_ink, true);
  std::array<char, kLineBytes> count{};
  std::snprintf(count.data(), count.size(), "(%zu)", group.rows.size());
  paint.Text(text_left + paint.TextWidth(group.label, kRowTitleSize, true) + kGroupTextGap,
             text_top, count.data(), kRowTitleSize, FaintInk());
}

void DrawQuestRows(QuestWindowState& state, const QuestWindowView& view, const QuestArt& art,
                   const SkinArt& skin, QuestWindowAction& action) {
  std::size_t total = 0;
  for (const QuestGroupView& group : view.groups) {
    total += group.rows.size();
  }
  if (total == 0) {
    DrawEmptyText(EmptyText(state, view));
    return;
  }
  for (const QuestGroupView& group : view.groups) {
    if (group.rows.empty()) {
      continue;
    }
    // UNE QUÊTE À AMENER EN VUE DÉPLIE SON GROUPE : repliée, elle n'aurait pas
    // de ligne où se montrer.
    if (state.scroll_to_selected && Contains(state.folded_groups, group.key) &&
        std::ranges::any_of(
            group.rows, [&](const QuestRowView& row) { return row.quest_id == state.selected; })) {
      Toggle(state.folded_groups, group.key);
    }
    if (!group.label.empty()) {
      DrawGroupHeader(state, group);
      if (Contains(state.folded_groups, group.key)) {
        continue;
      }
    }
    for (const QuestRowView& row : group.rows) {
      DrawRow(state, row, art, skin, action);
    }
  }
}

// ── L'HISTORIQUE ────────────────────────────────────────────────────────────

// « Achevée le 2026/10/06 16:40 », ou « Sortie le … » quand on ne sait pas
// comment ; la seule fin si la date manque.
[[nodiscard]] std::string LeftText(const QuestHistoryView& entry) {
  std::array<char, kLineBytes> line{};
  if (entry.left_at.empty()) {
    return std::string(entry.ending);
  }
  if (entry.ending.empty()) {
    std::snprintf(line.data(), line.size(), core::Tr("Sortie le %s"), Owned(entry.left_at).c_str());
  } else {
    std::snprintf(line.data(), line.size(), core::Tr("%s le %s"), Owned(entry.ending).c_str(),
                  Owned(entry.left_at).c_str());
  }
  return line.data();
}

void DrawHistoryRow(QuestWindowState& state, const QuestHistoryView& entry, const QuestArt& art,
                    const SkinArt& skin, QuestWindowAction& action) {
  const ImVec2 corner = ImGui::GetCursorScreenPos();
  const Painter paint(ImGui::GetWindowDrawList(), corner, Scale());
  const float width = ArtWidth();
  const bool selected = state.selected == entry.quest_id;
  PaintRowBack(paint, art, width, kHistoryRowHeight, selected,
               Pointed(paint, 0.0F, 0.0F, width, kHistoryRowHeight));
  ImGui::PushID(static_cast<int>(entry.quest_id));
  const char* const forget = core::Tr("Oublier");
  const float button_width = SkinSmallButtonWidth(forget, skin) / Scale();
  const float button_left = width - kRowRightPad - button_width;
  const float button_top =
      std::trunc((kHistoryRowHeight - SkinSmallButtonHeight(skin) / Scale()) * 0.5F);
  ImGui::SetCursorScreenPos(paint.At(button_left, button_top));
  if (SkinSmallButton(forget, skin)) {
    action.forget = entry.quest_id;
  }
  SkinTooltip(core::Tr("Retirer cette quête de l'historique"));
  // LA LIGNE DU BAS : comment et quand la quête est sortie, puis le lien vers
  // l'étape qui l'a remplacée, dans la place qui reste.
  const float text_room = button_left - kRowColumnGap - kRowTextLeft;
  const std::string left_text =
      quest_paint::FitToWidth(paint, LeftText(entry), kRowTextSize, false, text_room);
  const float left_width = left_text.empty() ? 0.0F : paint.TextWidth(left_text, kRowTextSize);
  const float link_left = kRowTextLeft + left_width + (left_text.empty() ? 0.0F : kHistoryLinkGap);
  const float link_room = kRowTextLeft + text_room - link_left;
  std::string link_text;
  if (entry.next_quest_id != 0 && link_room > 0.0F) {
    std::array<char, kLineBytes> next{};
    std::snprintf(next.data(), next.size(), core::Tr("Suite : %s"),
                  Owned(entry.next_title).c_str());
    link_text = quest_paint::FitToWidth(paint, next.data(), kRowTextSize, false, link_room);
  }
  // ⚠ LE LIEN AVANT LA LIGNE : de deux boutons qui se recouvrent, le premier
  // soumis prend le clic, comme « Oublier ».
  bool link_hovered = false;
  if (!link_text.empty()) {
    ImGui::SetCursorScreenPos(paint.At(link_left, kHistoryDateTop));
    if (ImGui::InvisibleButton("##suite", ImVec2(Px(paint.TextWidth(link_text, kRowTextSize)),
                                                 Px(paint.TextHeight(kRowTextSize))))) {
      action.open_quest = entry.next_quest_id;
    }
    link_hovered = ImGui::IsItemHovered();
    if (link_hovered) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
  }
  ImGui::SetCursorScreenPos(corner);
  if (ImGui::InvisibleButton("##ligne", ImVec2(Px(width), Px(kHistoryRowHeight)))) {
    state.selected = entry.quest_id;
    action.selected = entry.quest_id;
  }
  if (state.scroll_to_selected && selected) {
    ImGui::ScrollToItem(ImGuiScrollFlags_KeepVisibleCenterY);
  }
  ImGui::PopID();
  PaintFitted(paint, entry.icon, kRowIconLeft, kHistoryIconTop, kIconWidth, kIconHeight);
  paint.Text(kRowTextLeft, kHistoryTitleTop,
             quest_paint::FitToWidth(paint, entry.title, kRowTitleSize, true, text_room),
             kRowTitleSize, PaperInk(kRowTitleText), true);
  if (!left_text.empty()) {
    paint.Text(kRowTextLeft, kHistoryDateTop, left_text, kRowTextSize, FaintInk());
  }
  if (!link_text.empty()) {
    const ImU32 link_ink = PaperInk(kRowTitleText);
    paint.Text(link_left, kHistoryDateTop, link_text, kRowTextSize, link_ink);
    if (link_hovered) {
      const float underline_top = kHistoryDateTop + paint.TextHeight(kRowTextSize);
      paint.into()->AddLine(
          paint.At(link_left, underline_top),
          paint.At(link_left + paint.TextWidth(link_text, kRowTextSize), underline_top), link_ink,
          kRulePixels * paint.scale());
    }
  }
}

void DrawHistoryRows(QuestWindowState& state, const QuestWindowView& view, const QuestArt& art,
                     const SkinArt& skin, QuestWindowAction& action) {
  if (view.history.empty()) {
    DrawEmptyText(EmptyText(state, view));
    return;
  }
  for (const QuestHistoryView& entry : view.history) {
    DrawHistoryRow(state, entry, art, skin, action);
  }
}

// ── LES RECOMMANDATIONS ─────────────────────────────────────────────────────

// UNE AFFICHE : l'icône, le titre et sa pastille « Nouveau », le résumé. Ni
// cadenas ni suivi : ce n'est pas une quête.
void DrawRecommendedRow(QuestWindowState& state, const QuestRecommendedView& entry,
                        const QuestArt& art, QuestWindowAction& action) {
  const ImVec2 corner = ImGui::GetCursorScreenPos();
  const Painter paint(ImGui::GetWindowDrawList(), corner, Scale());
  const float width = ArtWidth();
  const bool selected = state.selected_recommended == entry.entry_id;
  const bool visible = ImGui::IsRectVisible(paint.At(0.0F, 0.0F), paint.At(width, kRowHeight));
  if (visible) {
    PaintRowBack(paint, art, width, kRowHeight, selected,
                 Pointed(paint, 0.0F, 0.0F, width, kRowHeight));
  }
  ImGui::PushID(static_cast<int>(entry.entry_id));
  if (ImGui::InvisibleButton("##affiche", ImVec2(Px(width), Px(kRowHeight)))) {
    state.selected_recommended = entry.entry_id;
    action.read_recommended = entry.entry_id;
  }
  if (state.scroll_to_selected && selected) {
    ImGui::ScrollToItem(ImGuiScrollFlags_KeepVisibleCenterY);
  }
  if (ImGui::IsItemHovered() && !entry.icon_label.empty() &&
      Pointed(paint, kRowIconLeft, kRowIconTop, kIconWidth, kIconHeight)) {
    SkinTooltipFormat("%s", Owned(entry.icon_label).c_str());
  }
  ImGui::PopID();
  if (!visible) {
    return;
  }
  PaintFitted(paint, entry.icon, kRowIconLeft, kRowIconTop, kIconWidth, kIconHeight);
  const float text_room = width - kRowRightPad - kRowTextLeft;
  const char* const fresh = core::Tr("Nouveau");
  const float pill_room =
      entry.fresh ? PillWidth(paint, fresh, kPillTextSize) + kRowColumnGap : 0.0F;
  const std::string title =
      quest_paint::FitToWidth(paint, entry.title, kRowTitleSize, true, text_room - pill_room);
  paint.Text(kRowTextLeft, kRowTitleTop, title, kRowTitleSize, PaperInk(kRowTitleText), true);
  if (entry.fresh) {
    const float pill_top =
        kRowTitleTop +
        std::trunc((paint.TextHeight(kRowTitleSize) - PillHeight(paint, kPillTextSize)) * 0.5F);
    static_cast<void>(
        PaintPill(paint, kRowTextLeft + paint.TextWidth(title, kRowTitleSize, true) + kRowColumnGap,
                  pill_top, fresh, kPillTextSize, kFreshFill, kFreshText));
  }
  paint.Text(kRowTextLeft, kRowSummaryTop,
             quest_paint::FitToWidth(paint, entry.summary, kRowTextSize, false, text_room),
             kRowTextSize, BodyInk());
}

void DrawRecommendedRows(QuestWindowState& state, const QuestWindowView& view, const QuestArt& art,
                         QuestWindowAction& action) {
  if (view.recommended.empty()) {
    DrawEmptyText(EmptyText(state, view));
    return;
  }
  for (const QuestRecommendedView& entry : view.recommended) {
    DrawRecommendedRow(state, entry, art, action);
  }
}

}  // namespace

void DrawListColumn(QuestWindowState& state, const QuestWindowView& view, const QuestArt& art,
                    const SkinArt& skin, float height, QuestWindowAction& action,
                    PaneRequests& requests) {
  const bool history = view.tab == QuestTab::kHistory;
  const bool recommended = view.tab == QuestTab::kRecommended;
  ImGui::BeginGroup();
  const float top = ImGui::GetCursorPosY();
  DrawCategoryRow(state, view, skin);
  // Ni l'historique ni les affiches n'ont d'ordre à choisir ni de groupes.
  if (!history && !recommended) {
    DrawOrderRow(state, skin);
  }
  // L'historique garde au pied la place de « Tout oublier ».
  const float footer = history ? SkinButtonHeight(skin) + ImGui::GetStyle().ItemSpacing.y : 0.0F;
  const float list_height = std::max(0.0F, height - (ImGui::GetCursorPosY() - top) - footer);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, Paper());
  const bool shown = ImGui::BeginChild("##quetes_liste", ImVec2(Px(kListWidth), list_height),
                                       ImGuiChildFlags_Borders);
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
  if (shown) {
    PushPaperStyle(PaintingLook());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0F));
    if (!view.catalog_problem.empty()) {
      DrawCatalogBanner(view.catalog_problem);
    }
    if (history) {
      DrawHistoryRows(state, view, art, skin, action);
    } else if (recommended) {
      DrawRecommendedRows(state, view, art, action);
    } else {
      DrawQuestRows(state, view, art, skin, action);
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(kPaperStyleColors);
    PaintWindowScrollbar(skin);
  }
  ImGui::EndChild();
  // La demande est servie, ou la quête n'est plus dans cet onglet : elle
  // retombe dans les deux cas.
  state.scroll_to_selected = false;
  if (history) {
    ImGui::BeginDisabled(view.history.empty());
    if (SkinButton(core::Tr("Tout oublier"), skin)) {
      requests.forget_all_asked = true;
    }
    ImGui::EndDisabled();
  }
  ImGui::EndGroup();
}

}  // namespace ro::ui::quest_window
