#include "ro/ui/achievement_window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/chat_link_offer.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "achievement_paint.h"

namespace ro::ui {
namespace {

using achievement::Painter;
using achievement::Scale;
namespace paint_kit = achievement;

// ── LA DISPOSITION, en pixels d'art depuis le coin de la fenêtre ────────────

// La barre de titre, que `BeginSkinnedWindow` peint.
constexpr float kTitleBarHeight = 17.0F;
// Le bandeau, sous la barre.
constexpr float kUpperTop = 17.0F;
constexpr float kUpperHeight = 46.0F;
// Les volets, sous le bandeau.
constexpr float kPaneTop = 63.0F;
constexpr float kPaneHeight = 405.0F;
constexpr float kTreeWidth = 102.0F;
constexpr float kListLeft = 102.0F;
constexpr float kDetailLeft = 422.0F;

// ── L'ARBRE ─────────────────────────────────────────────────────────────────

// Le libellé d'une racine, celui d'une sous-ligne, et la flèche de la
// sous-ligne choisie.
constexpr float kRootTextLeft = 11.0F;
constexpr float kSubtabTextLeft = 26.0F;
constexpr float kSubtabArrowLeft = 12.0F;
constexpr float kSubtabArrowTop = 7.0F;
// Un clic dans la piste de l'ascenseur des sous-lignes avance de deux.
constexpr int kSubtabTrackStep = 2;
// L'ARBRE NE DESCEND JAMAIS PLUS BAS : toutes les racines, et la bande la plus
// haute que font des sous-lignes. Sous ce bord, la place est à la recherche.
constexpr float kTreeReach = kAchievementTabRootHeight * static_cast<float>(kAchievementTabCount) +
                             kAchievementTabSubHeight * static_cast<float>(kAchievementTabSubShown);

// ── LA RECHERCHE ET LE TRI, sous l'arbre ────────────────────────────────────

constexpr float kFinderLeft = 4.0F;
constexpr float kFinderWidth = 94.0F;
constexpr float kFinderHeight = 18.0F;
constexpr float kSearchTop = 362.0F;
constexpr float kSortTop = 384.0F;
// Dans l'ordre de `AchievementSort`.
constexpr std::array<const char*, kAchievementSortCount> kSortLabels = {
    core::TrText("Ordre du jeu"), core::TrText("Progression"), core::TrText("Score"),
    core::TrText("Nom")};

// ── LE BANDEAU ──────────────────────────────────────────────────────────────

constexpr float kRankPointsRight = 130.0F;
constexpr float kRankPointsTop = 13.0F;
constexpr float kScoreLeft = 353.0F;
constexpr float kScoreWidth = 46.0F;
constexpr float kScoreTop = 6.0F;
constexpr float kRankLeft = 16.0F;
constexpr float kRankWidth = 15.0F;
constexpr float kRankTop = 19.0F;
constexpr float kRankGaugeLeft = 47.0F;
constexpr float kRankGaugeTop = 24.0F;
constexpr float kRankGaugeWidth = 84.0F;
constexpr float kRankGaugeHeight = 10.0F;
constexpr float kTrophyLeft = 575.0F;
constexpr float kTrophyTop = 3.0F;
constexpr float kRadioLeft = 150.0F;
constexpr std::array<float, 2> kRadioTops = {10.0F, 25.0F};
constexpr float kRadioSide = 12.0F;
constexpr float kRadioLabelLeft = 169.0F;
// Le libellé est un pixel plus bas que son bouton.
constexpr float kRadioLabelDrop = 1.0F;
// La seconde colonne de filtres, dans la zone libre à droite du bouclier.
constexpr float kExtraRadioLeft = 458.0F;
constexpr float kExtraRadioLabelLeft = 477.0F;
constexpr float kTabTitleLeft = 614.0F;
constexpr float kTabTitleTop = 15.0F;
constexpr float kTabCountRight = 737.0F;
constexpr float kTabGaugeLeft = 613.0F;
constexpr float kTabGaugeTop = 29.0F;
constexpr float kTabGaugeWidth = 124.0F;
constexpr float kTabGaugeHeight = 8.0F;

// ── LA GÉOMÉTRIE PURE ───────────────────────────────────────────────────────

// Le détail, où se centrent les récompenses : un écart de huit, un pas de
// soixante-dix.
constexpr float kDetailWidth = 328.0F;
constexpr float kRewardGap = 8.0F;
constexpr float kRewardPitch = 70.0F;
// Le pouce le plus court.
constexpr int kThumbFloor = 8;
constexpr int kPercent = 100;

// La bannière : cinquante pixels sous le haut de l'écran, cinquante de haut.
constexpr float kBannerTop = 50.0F;
constexpr float kBannerHeight = 50.0F;
// La discrète : une ligne plus petite, sans gras.
constexpr float kDiscreetBannerText = 14.0F;
constexpr float kOpaque = 1.0F;
constexpr float kAlphaMax = 255.0F;

// Le surgissant du réglage de la bannière.
constexpr const char* kBannerPopupId = "##succes_banniere";
// Le libellé d'un style de bannière, traduit à la lecture.
[[nodiscard]] const char* BannerStyleLabel(AchievementBannerStyle style) {
  switch (style) {
    case AchievementBannerStyle::kNative:
      return core::Tr("Bannière du jeu");
    case AchievementBannerStyle::kDiscreet:
      return core::Tr("Bannière discrète");
    case AchievementBannerStyle::kNone:
      return core::Tr("Aucune bannière (une ligne dans la conversation)");
  }
  return core::Tr("Bannière du jeu");
}

// Assez pour « Tout réclamer (18446744073709551615) » dans toute langue.
constexpr std::size_t kClaimAllBytes = 96;
// Les actions de la barre de titre, dans leur ordre de pose.
constexpr int kClaimAllRank = 0;
constexpr int kBugReportRank = 1;

// Assez pour « 4294967295/4294967295 » entre parenthèses.
constexpr std::size_t kNumberBytes = 32;

[[nodiscard]] int SubtabCount(const AchievementWords& words, int tab) {
  if (tab < 0 || tab >= kAchievementTabCount) {
    return 0;
  }
  return static_cast<int>(words.subtabs[static_cast<std::size_t>(tab)].size());
}

[[nodiscard]] std::string_view SubtabLabel(const AchievementWords& words, int tab, int subtab) {
  const std::span<const std::string_view> labels = words.subtabs[static_cast<std::size_t>(tab)];
  if (subtab < 0 || static_cast<std::size_t>(subtab) >= labels.size()) {
    return {};
  }
  return labels[static_cast<std::size_t>(subtab)];
}

// La racine choisie : ses sous-lignes montrées, puis la bande qu'elles font.
[[nodiscard]] float SubtabBand(int subtab_count) {
  return kAchievementTabSubHeight *
         static_cast<float>(std::clamp(subtab_count, 0, kAchievementTabSubShown));
}

// L'ascenseur des sous-lignes, sur leur bord GAUCHE.
[[nodiscard]] paint_kit::ScrollbarModel SubtabScrollbar(const AchievementWindowState& state,
                                                        int subtab_count) {
  return {
      .x = 0.0F,
      .y = kAchievementTabRootHeight * static_cast<float>(state.tab + 1),
      .height = kAchievementTabSubHeight * static_cast<float>(kAchievementTabSubShown),
      .position = state.subtab_scroll,
      .range = AchievementSubtabScrollRange(subtab_count),
      .page = kAchievementTabSubShown,
      .total = subtab_count,
      .track_step = kSubtabTrackStep,
  };
}

// ── L'ARBRE D'ONGLETS ───────────────────────────────────────────────────────

void TreeInput(const Painter& paint, AchievementWindowState& state, const AchievementView& view,
               AchievementAction& action) {
  const int subtab_count = SubtabCount(view.words, state.tab);
  paint_kit::ScrollbarModel bar = SubtabScrollbar(state, subtab_count);
  // ⚠ L'ASCENSEUR AVANT L'ARBRE : il est posé sur lui, et prend la souris.
  state.subtab_scroll = paint_kit::ScrollbarInput(paint, "##arbre_ascenseur", bar);

  // ⚠ L'ARBRE S'ARRÊTE À SA PORTÉE : plus bas, la recherche et le tri ont la
  // souris, et un rectangle soumis avant eux la leur prendrait.
  const paint_kit::HitResult hit = paint.Hit("##arbre", 0.0F, 0.0F, kTreeWidth, kTreeReach);
  // MAJ + CLIC SUR UNE RACINE OU UNE SOUS-LIGNE : le lien de ce qu'elle désigne
  // dans le chat, et elle choisie au passage. ⚠ LU SUR LA GÉOMÉTRIE, AVANT LA
  // GARDE DU SURVOL : une saisie de chat qui tient le clavier rend l'arbre
  // muet — voir `chat_link_offer.h`.
  const bool shift_clicked = ShiftClickedLastItem();
  const bool tip_hovered = HoveredForLinkTooltip();
  if (!hit.hovered && !hit.pressed && !shift_clicked && !tip_hovered) {
    return;
  }
  const std::optional<AchievementTabHit> touched = HitAchievementTab(
      hit.mouse.y, state.tab, subtab_count, state.subtab_scroll, kAchievementTabCount);
  if (touched.has_value() && shift_clicked) {
    action.link_tab = touched;
  } else if (touched.has_value() && tip_hovered) {
    SkinTooltipFormat("%s", ChatLinkGestureHint());
  }
  // ⚠ LE DERNIER SURVOL RESTE : rien ne l'éteint quand la souris s'en va.
  state.hovered_tab = touched.has_value() ? touched->tab : kAchievementNothingHovered;
  state.hovered_subtab = touched.has_value() ? touched->subtab : kAchievementNothingHovered;
  if ((hit.pressed || shift_clicked) && touched.has_value()) {
    // CHOISIR UN ONGLET QUITTE LA RECHERCHE : sinon la liste resterait celle
    // des résultats, et le clic ne montrerait rien.
    state.search.fill('\0');
    state.tab = touched->tab;
    state.subtab = touched->subtab;
    state.subtab_scroll = ClampAchievementScroll(
        state.subtab_scroll, AchievementSubtabScrollRange(SubtabCount(view.words, state.tab)));
    state.list_scroll = 0;
    action.tab_changed = true;
  }
}

void TreeWheel(const Painter& paint, AchievementWindowState& state, const AchievementView& view) {
  const int range = AchievementSubtabScrollRange(SubtabCount(view.words, state.tab));
  if (range <= 0) {
    return;
  }
  const int steps = paint.WheelSteps(0.0F, 0.0F, kTreeWidth, kTreeReach);
  state.subtab_scroll = ClampAchievementScroll(state.subtab_scroll + steps, range);
}

void PaintSubtabs(const Painter& paint, const AchievementWindowState& state,
                  const AchievementView& view, const AchievementArt& art) {
  const int subtab_count = SubtabCount(view.words, state.tab);
  const float text_drop =
      std::trunc((kAchievementTabSubHeight - paint.TextHeight(paint_kit::kTextLarge)) * 0.5F);
  for (int line = 0; line < kAchievementTabSubShown; ++line) {
    const int subtab = state.subtab_scroll + line;
    if (subtab >= subtab_count) {
      break;
    }
    const float top = kAchievementTabRootHeight * static_cast<float>(state.tab + 1) +
                      kAchievementTabSubHeight * static_cast<float>(line);
    if (subtab == state.subtab) {
      paint.Art(art.tab_sub_press, 0.0F, top);
      paint.Art(art.tab_subarrow, kSubtabArrowLeft, top + kSubtabArrowTop);
    } else {
      paint.Art(subtab == state.hovered_subtab ? art.tab_sub_over : art.tab_sub_out, 0.0F, top);
    }
    paint.Text(kSubtabTextLeft, top + text_drop, SubtabLabel(view.words, state.tab, subtab),
               paint_kit::kTextLarge, paint.Ink(paint_kit::kSubtabText));
  }
}

void PaintTree(const Painter& paint, const AchievementWindowState& state,
               const AchievementView& view) {
  const AchievementArt& art = paint_kit::ArtOf(view);
  const int subtab_count = SubtabCount(view.words, state.tab);
  paint.PushClip(0.0F, 0.0F, kTreeWidth, kPaneHeight);
  paint.Art(art.bg_tab, 0.0F, 0.0F);
  const float text_drop =
      std::trunc((kAchievementTabRootHeight - paint.TextHeight(paint_kit::kTextLarge)) * 0.5F);
  for (int tab = 0; tab < kAchievementTabCount; ++tab) {
    const float top = AchievementTabTop(tab, state.tab, subtab_count);
    const std::string_view label = view.words.tabs[static_cast<std::size_t>(tab)];
    if (tab == state.tab) {
      paint.ArtCut(art.tab_press, 0.0F, top, kAchievementTabRootHeight);
      paint.Text(kRootTextLeft, top + text_drop, label, paint_kit::kTextLarge,
                 paint.Ink(paint_kit::kTabChosenText), true);
      PaintSubtabs(paint, state, view, art);
    } else {
      paint.ArtCut(tab == state.hovered_tab ? art.tab_over : art.tab_out, 0.0F, top,
                   kAchievementTabRootHeight);
      paint.Text(kRootTextLeft, top + text_drop, label, paint_kit::kTextLarge,
                 paint.Ink(paint_kit::kTabIdleText), true);
    }
  }
  paint_kit::PaintScrollbar(paint, art, SubtabScrollbar(state, subtab_count));
  paint.PopClip();
}

// ── LE BANDEAU ──────────────────────────────────────────────────────────────

// Un filtre du bandeau : le bouton, puis son libellé un pixel plus bas.
struct FilterRadio {
  AchievementFilter filter = AchievementFilter::kAll;
  float left = 0.0F;
  float top = 0.0F;
  float label_left = 0.0F;
  std::string_view label;
};

void DrawUpper(const Painter& paint, AchievementWindowState& state, const AchievementView& view,
               AchievementAction& action) {
  const AchievementArt& art = paint_kit::ArtOf(view);
  const AchievementUpperView& upper = view.upper;
  paint.Art(art.bg_upper, 0.0F, 0.0F);
  const ImU32 black_ink = paint.Ink(paint_kit::kBlack);
  const ImU32 upper_ink = paint.Ink(paint_kit::kUpperText);

  std::array<char, kNumberBytes> text{};
  std::snprintf(text.data(), text.size(), "%u/%u pts", upper.rank_points, upper.rank_span);
  paint.TextRight(kRankPointsRight, kRankPointsTop, text.data(), paint_kit::kTextMedium, black_ink);
  std::snprintf(text.data(), text.size(), "%u pts", upper.total_points);
  paint.TextCentered(kScoreLeft, kScoreWidth, kScoreTop, text.data(), paint_kit::kTextLarge,
                     upper_ink, true);
  std::snprintf(text.data(), text.size(), "%d", static_cast<int>(upper.rank));
  paint.TextCentered(kRankLeft, kRankWidth, kRankTop, text.data(), paint_kit::kTextLarge,
                     paint.Ink(paint_kit::kRankText), true);
  paint.Gauge(kRankGaugeLeft, kRankGaugeTop, kRankGaugeWidth, kRankGaugeHeight, upper.rank_points,
              upper.rank_span, paint_kit::kUpperGauge);

  // SUR LE RÉSUMÉ, le bandeau s'arrête là : ni filtres, ni trophée, ni compte.
  if (!AchievementListMode(view)) {
    return;
  }
  paint.Art(art.upper_trophy, kTrophyLeft, kTrophyTop);
  // Les deux filtres du client, puis les deux nôtres en seconde colonne.
  const std::array<FilterRadio, kAchievementFilterCount> radios = {{
      {AchievementFilter::kAll, kRadioLeft, kRadioTops[0], kRadioLabelLeft, view.words.filter_all},
      {AchievementFilter::kUnclaimed, kRadioLeft, kRadioTops[1], kRadioLabelLeft,
       view.words.filter_unclaimed},
      {AchievementFilter::kInProgress, kExtraRadioLeft, kRadioTops[0], kExtraRadioLabelLeft,
       core::Tr("En cours")},
      {AchievementFilter::kTracked, kExtraRadioLeft, kRadioTops[1], kExtraRadioLabelLeft,
       core::Tr("Suivis")},
  }};
  for (const FilterRadio& radio : radios) {
    ImGui::PushID(static_cast<int>(radio.filter));
    // ⚠ RECLIQUER LE FILTRE COCHÉ REPREND L'ONGLET : sélection et défilement
    // reviennent en tête.
    if (paint.Hit("##filtre", radio.left, radio.top, kRadioSide, kRadioSide).pressed) {
      state.filter = radio.filter;
      state.list_scroll = 0;
      action.tab_changed = true;
    }
    ImGui::PopID();
    paint.Art(state.filter == radio.filter ? art.radio_on : art.radio_off, radio.left, radio.top);
    paint.Text(radio.label_left, radio.top + kRadioLabelDrop, radio.label, paint_kit::kTextLarge,
               black_ink);
  }
  paint.Text(kTabTitleLeft, kTabTitleTop, upper.title, paint_kit::kTextMedium, upper_ink);
  std::snprintf(text.data(), text.size(), "(%u/%u)", upper.tab.done, upper.tab.total);
  paint.TextRight(kTabCountRight, kTabTitleTop, text.data(), paint_kit::kTextMedium, upper_ink);
  paint.Gauge(kTabGaugeLeft, kTabGaugeTop, kTabGaugeWidth, kTabGaugeHeight, upper.tab.done,
              upper.tab.total, paint_kit::kUpperGauge);
}

// ── LA RECHERCHE ET LE TRI ──────────────────────────────────────────────────

// Le champ de recherche et la liste des ordres, sous l'arbre, sur le résumé
// comme ailleurs. Un changement ramène la liste en tête.
void DrawFinder(const Painter& paint, AchievementWindowState& state, const SkinArt& skin,
                AchievementAction& action) {
  const float scale = paint.scale();
  const ImVec2 search_at = paint.At(kFinderLeft, kSearchTop);
  const float width = paint.At(kFinderLeft + kFinderWidth, kSearchTop).x - search_at.x;
  ImGui::PushFont(nullptr, paint_kit::kTextLarge * scale);
  // Le champ fait la hauteur de son art : la marge comble ce que la police
  // laisse.
  const float padding_y =
      std::max(0.0F, std::floor((kFinderHeight * scale - ImGui::GetFontSize()) * 0.5F));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      ImVec2(ImGui::GetStyle().FramePadding.x, padding_y));
  ImGui::SetCursorScreenPos(search_at);
  ImGui::SetNextItemWidth(width);
  bool changed = ImGui::InputTextWithHint("##succes_recherche", core::Tr("Rechercher…"),
                                          state.search.data(), state.search.size());
  ImGui::PopStyleVar();

  ImGui::SetCursorScreenPos(paint.At(kFinderLeft, kSortTop));
  ImGui::SetNextItemWidth(width);
  int sort = static_cast<int>(state.sort);
  if (SkinCombo("##succes_ordre", &sort, kSortLabels, skin)) {
    state.sort = static_cast<AchievementSort>(sort);
    changed = true;
  }
  ImGui::PopFont();
  if (changed) {
    state.list_scroll = 0;
    action.order_changed = true;
  }
}

// ── LES RÉGLAGES, sous la puce de la barre de titre : la bannière, puis le
// panneau de suivi ─────────────────────────────────────────────────────────

void DrawBannerChoice(const AchievementView& view, const SkinArt& skin, AchievementAction& action) {
  if (TitleBulletClicked()) {
    ImGui::OpenPopup(kBannerPopupId);
  }
  achievement::PushSettingsMenuFrame();
  if (!BeginSkinPopup(kBannerPopupId)) {
    achievement::PopSettingsMenuFrame();
    return;
  }
  for (std::size_t rank = 0; rank < kAchievementBannerStyleCount; ++rank) {
    const auto style = static_cast<AchievementBannerStyle>(rank);
    ImGui::PushID(static_cast<int>(rank));
    if (SkinRadio(BannerStyleLabel(style), view.banner_style == style, skin)) {
      action.banner_style = style;
      ImGui::CloseCurrentPopup();
    }
    ImGui::PopID();
  }
  ImGui::Separator();
  ImGui::TextDisabled("%s", core::Tr("Panneau de suivi"));
  action.tracker_settings = DrawTrackerSettingsItems(view.tracker_look, skin);
  EndSkinPopup();
  achievement::PopSettingsMenuFrame();
}

}  // namespace

namespace achievement {

const AchievementArt& ArtOf(const AchievementView& view) {
  static const AchievementArt kNoArt{};
  return view.art != nullptr ? *view.art : kNoArt;
}

}  // namespace achievement

// ── LA GÉOMÉTRIE PURE ───────────────────────────────────────────────────────

bool AchievementListMode(const AchievementView& view) {
  return view.tab != kAchievementSummaryTab || view.searching;
}

float AchievementTabTop(int tab, int chosen_tab, int chosen_subtab_count) {
  float top = kAchievementTabRootHeight * static_cast<float>(tab);
  if (tab > chosen_tab) {
    top += SubtabBand(chosen_subtab_count);
  }
  return top;
}

std::optional<AchievementTabHit> HitAchievementTab(float y, int chosen_tab, int chosen_subtab_count,
                                                   int subtab_scroll, int tab_count) {
  if (y < 0.0F) {
    return std::nullopt;
  }
  const float band_top = kAchievementTabRootHeight * static_cast<float>(chosen_tab + 1);
  const float band = SubtabBand(chosen_subtab_count);
  if (y > band_top && y < band_top + band) {
    return AchievementTabHit{
        .tab = chosen_tab,
        .subtab = subtab_scroll + static_cast<int>((y - band_top) / kAchievementTabSubHeight)};
  }
  const float root_y = y >= band_top + band ? y - band : y;
  const int tab = static_cast<int>(root_y / kAchievementTabRootHeight);
  // Sous la dernière racine, il n'y a rien à choisir.
  if (tab >= tab_count) {
    return std::nullopt;
  }
  return AchievementTabHit{.tab = tab, .subtab = kAchievementWholeTab};
}

int AchievementSubtabScrollRange(int subtab_count) {
  return std::max(0, subtab_count - kAchievementTabSubShown);
}

int AchievementSubtabScrollShowing(int subtab, int subtab_count) {
  return ClampAchievementScroll(subtab - (kAchievementTabSubShown - 1),
                                AchievementSubtabScrollRange(subtab_count));
}

void OpenAchievementTab(AchievementWindowState& state, AchievementTabHit place) {
  const int tab = std::clamp(place.tab, 0, kAchievementTabCount - 1);
  const int subtab_count = kAchievementSubtabCounts[static_cast<std::size_t>(tab)];
  // ⚠ UN SOUS-ONGLET QUE L'ONGLET N'A PAS rend l'onglet entier.
  const int subtab =
      place.subtab >= 0 && place.subtab < subtab_count ? place.subtab : kAchievementWholeTab;
  state.search.fill('\0');
  state.open = true;
  state.tab = tab;
  state.subtab = subtab;
  state.subtab_scroll = AchievementSubtabScrollShowing(subtab, subtab_count);
  state.list_scroll = 0;
}

int AchievementListScrollRange(std::size_t row_count) {
  return std::max(0, static_cast<int>(row_count) - kAchievementRowsShown);
}

int ClampAchievementScroll(int scroll, int range) {
  return std::clamp(scroll, 0, std::max(0, range));
}

std::optional<std::size_t> AchievementListRowAt(float y, int list_scroll, std::size_t row_count) {
  if (y < 0.0F) {
    return std::nullopt;
  }
  const int line = static_cast<int>(y / kAchievementRowHeight);
  if (line >= kAchievementRowsShown) {
    return std::nullopt;
  }
  const int row = line + std::max(0, list_scroll);
  if (static_cast<std::size_t>(row) >= row_count) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(row);
}

float AchievementRewardX(std::size_t index, std::size_t count, float frame_width) {
  const auto shown = static_cast<float>(count);
  const float first =
      std::floor((kDetailWidth - kRewardGap * shown - frame_width * shown + kRewardGap) * 0.5F);
  return first + kRewardPitch * static_cast<float>(index);
}

std::optional<float> AchievementGaugeFill(float interior_width, std::int64_t current,
                                          std::int64_t maximum) {
  if (current < 0 || maximum < 0 || current > maximum) {
    return std::nullopt;
  }
  if (maximum == 0) {
    return 0.0F;
  }
  const float percent =
      static_cast<float>(current) * static_cast<float>(kPercent) / static_cast<float>(maximum);
  if (percent <= 0.0F) {
    return 0.0F;
  }
  const int filled = static_cast<int>(interior_width) * static_cast<int>(percent) / kPercent;
  return static_cast<float>(filled);
}

RoundedBox AchievementGaugeInterior(ImVec2 gauge_min, ImVec2 gauge_max) {
  return GaugeInterior(gauge_min, gauge_max, /*frame_rounding=*/0.0F, paint_kit::kGaugeInsetPixels);
}

std::optional<AxisBox> AchievementGaugeFillBox(const RoundedBox& interior, std::int64_t current,
                                               std::int64_t maximum) {
  const std::optional<float> filled =
      AchievementGaugeFill(interior.max.x - interior.min.x, current, maximum);
  if (!filled.has_value()) {
    return std::nullopt;
  }
  return AxisBox{interior.min, ImVec2(interior.min.x + *filled, interior.max.y)};
}

AchievementThumb AchievementScrollThumb(float track_height, int position, int range, int page,
                                        int total) {
  const int track = std::max(0, static_cast<int>(track_height - 2.0F * paint_kit::kScrollArrow));
  if (range <= 0 || total <= 0) {
    return {.top = paint_kit::kScrollArrow, .length = static_cast<float>(track)};
  }
  const int length = std::min(track, std::max(kThumbFloor, track * page / total));
  const int top = (track - length) * std::clamp(position, 0, range) / range;
  return {.top = paint_kit::kScrollArrow + static_cast<float>(top),
          .length = static_cast<float>(length)};
}

std::vector<std::string_view> SplitAchievementBanner(std::string_view text) {
  std::vector<std::string_view> lines;
  std::size_t start = 0;
  while (start < text.size()) {
    std::size_t stop = start;
    for (std::size_t glyphs = 0; glyphs < kAchievementBannerLineChars && stop < text.size();
         ++glyphs) {
      stop = std::min(text.size(), stop + paint_kit::Utf8Length(text[stop]));
    }
    lines.push_back(text.substr(start, stop - start));
    start = stop;
  }
  return lines;
}

// ── LA FENÊTRE ──────────────────────────────────────────────────────────────

AchievementAction DrawAchievementWindow(AchievementWindowState& state, const AchievementView& view,
                                        const SkinArt& skin) {
  AchievementAction action;
  if (!state.open) {
    return action;
  }
  const float scale = Scale();
  // Centrée à chaque ouverture : le client recrée sa fenêtre.
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                          ImVec2(0.5F, 0.5F));
  ImGui::SetNextWindowSize(
      ImVec2(kAchievementWindowWidth * scale, kAchievementWindowHeight * scale), ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  const std::string name = std::string(view.words.window_title) + "###" + kAchievementWindowId;
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                      ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoScrollWithMouse;
  // LA BARRE DE TITRE : « Tout réclamer », éteint quand rien n'attend, et la
  // puce qui règle la bannière. ⚠ Le libellé vit jusqu'à la fin de la
  // fenêtre : la barre le garde par pointeur.
  std::array<char, kClaimAllBytes> claim_all{};
  std::snprintf(claim_all.data(), claim_all.size(), core::Tr("Tout réclamer (%zu)"),
                view.claimable);
  SetNextWindowTitleAction(
      claim_all.data(),
      core::Tr("Réclame une à une toutes les récompenses qui attendent, tous onglets confondus."),
      view.claimable > 0);
  PoseBugReportButton(state.bug_report);
  SetNextWindowBulletClickable(core::Tr("Bannière des succès"));
  const bool body_shown =
      BeginSkinnedWindow(name.c_str(), &state.open, skin, SkinnedTitle::kBar, kFlags);
  // ⚠ LUS MÊME REPLIÉE : la barre garde ses boutons.
  if (view.claimable > 0 && TitleActionClicked(kClaimAllRank)) {
    action.claim_all = true;
  }
  action.report_bug = state.bug_report && TitleActionClicked(kBugReportRank);
  if (body_shown) {
    DrawBannerChoice(view, skin, action);
    ImGuiWindow* const window = ImGui::GetCurrentWindow();
    // L'ORIGINE EST LE COIN DE LA FENÊTRE, barre comprise, comme les mesures
    // du client ; le corps commence où la barre réelle s'arrête.
    // LE CORPS EST DU PAPIER, celui des bulles et descriptions : l'art et la
    // palette du client, choisis pour du blanc, s'y posent.
    const Painter paint(
        window->DrawList,
        ImVec2(window->Pos.x, window->Pos.y + window->TitleBarHeight - kTitleBarHeight * scale),
        scale, PaintingLook().paper);
    const float body_height = kAchievementWindowHeight - kTitleBarHeight;
    paint.PushClip(0.0F, kTitleBarHeight, kAchievementWindowWidth, body_height);
    paint.Fill(0.0F, kTitleBarHeight, kAchievementWindowWidth, body_height, paint.paper());

    const Painter tree = paint.Child(0.0F, kPaneTop);
    TreeInput(tree, state, view, action);
    TreeWheel(tree, state, view);
    DrawUpper(paint.Child(0.0F, kUpperTop), state, view, action);
    PaintTree(tree, state, view);
    DrawFinder(tree, state, skin, action);
    if (AchievementListMode(view)) {
      paint_kit::DrawAchievementList(paint.Child(kListLeft, kPaneTop), state, view, action);
      paint_kit::DrawAchievementDetail(paint.Child(kDetailLeft, kPaneTop), view, action);
    } else {
      paint_kit::DrawAchievementSummary(paint.Child(kListLeft, kPaneTop), view, action);
    }
    paint.PopClip();
    // Le curseur revient dans la fenêtre : rien ne s'étend au-delà.
    ImGui::SetCursorScreenPos(paint.At(0.0F, kUpperTop + kUpperHeight));
    ImGui::Dummy(ImVec2(0.0F, 0.0F));
  }
  EndSkinnedWindow();
  ImGui::PopStyleVar();
  return action;
}

// ── LA BANNIÈRE ─────────────────────────────────────────────────────────────

std::uint32_t AchievementBannerLifeMs(AchievementBannerStyle style) {
  switch (style) {
    case AchievementBannerStyle::kNative:
      return kAchievementBannerMs;
    case AchievementBannerStyle::kDiscreet:
      return kAchievementDiscreetBannerMs;
    case AchievementBannerStyle::kNone:
      break;
  }
  return 0;
}

float AchievementDiscreetBannerAlpha(std::uint32_t elapsed_ms) {
  if (elapsed_ms >= kAchievementDiscreetBannerMs) {
    return 0.0F;
  }
  const std::uint32_t left_ms = kAchievementDiscreetBannerMs - elapsed_ms;
  if (left_ms >= kAchievementDiscreetFadeMs) {
    return kOpaque;
  }
  return static_cast<float>(left_ms) / static_cast<float>(kAchievementDiscreetFadeMs);
}

namespace {

// LA DISCRÈTE : la première ligne seule, plus petite et sans gras, qui
// s'efface à la fin de sa vie.
void DrawDiscreetBanner(const Painter& paint, float screen_width, std::string_view text,
                        std::uint32_t elapsed_ms) {
  const std::vector<std::string_view> lines = SplitAchievementBanner(text);
  const float alpha = AchievementDiscreetBannerAlpha(elapsed_ms);
  if (lines.empty() || alpha <= 0.0F) {
    return;
  }
  const auto opacity = static_cast<ImU32>(std::lround(alpha * kAlphaMax));
  const ImU32 white = (paint_kit::kWhite & ~IM_COL32_A_MASK) | (opacity << IM_COL32_A_SHIFT);
  const ImU32 black = (paint_kit::kBlack & ~IM_COL32_A_MASK) | (opacity << IM_COL32_A_SHIFT);
  const std::string_view line = lines.front();
  const float top =
      kBannerTop + std::trunc((kBannerHeight - paint.TextHeight(kDiscreetBannerText)) * 0.5F);
  const float width = paint.TextWidth(line, kDiscreetBannerText);
  paint.TextShadowed(std::trunc((screen_width - width) * 0.5F), top, line, kDiscreetBannerText,
                     white, black);
}

}  // namespace

void DrawAchievementBanner(std::string_view text, AchievementBannerStyle style,
                           std::uint32_t elapsed_ms) {
  if (text.empty() || style == AchievementBannerStyle::kNone) {
    return;
  }
  const float scale = Scale();
  const Painter paint(ImGui::GetForegroundDrawList(), ImVec2(0.0F, 0.0F), scale);
  const float screen_width = ImGui::GetIO().DisplaySize.x / scale;
  if (style == AchievementBannerStyle::kDiscreet) {
    DrawDiscreetBanner(paint, screen_width, text, elapsed_ms);
    return;
  }
  const std::vector<std::string_view> lines = SplitAchievementBanner(text);
  const float line_height = paint.TextHeight(paint_kit::kTextBanner);
  float top = kBannerTop +
              std::trunc((kBannerHeight - line_height * static_cast<float>(lines.size())) * 0.5F);
  for (const std::string_view line : lines) {
    const float width = paint.TextWidth(line, paint_kit::kTextBanner, true);
    paint.TextShadowed(std::trunc((screen_width - width) * 0.5F), top, line, paint_kit::kTextBanner,
                       paint_kit::kWhite, paint_kit::kBlack, true);
    top += line_height;
  }
}

}  // namespace ro::ui
