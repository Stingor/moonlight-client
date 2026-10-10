#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <span>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/achievement_window.h"
#include "ro/ui/skin_widgets.h"

#include "achievement_paint.h"

// LES TROIS VOLETS DE LA FENÊTRE DES SUCCÈS : la liste, le détail, le résumé.
namespace ro::ui::achievement {
namespace {

// Assez pour « 4294967295/4294967295 ».
constexpr std::size_t kNumberBytes = 32;

// ── LA LISTE ────────────────────────────────────────────────────────────────

constexpr float kListWidth = 320.0F;
constexpr float kListHeight = 405.0F;
// Les rangées ; l'ascenseur à leur droite.
constexpr float kRowsWidth = 309.0F;
// Un clic dans la piste de l'ascenseur avance d'une page moins une rangée.
constexpr int kListTrackStep = kAchievementRowsShown - 1;
// Dans une rangée : l'icône et l'épingle, la zone qui épingle, le nom et le
// score, le résumé, l'insigne, le coffre, la date.
constexpr float kIconLeft = 8.0F;
constexpr float kIconTop = 3.0F;
constexpr float kPinZoneInset = 5.0F;
constexpr float kPinZoneSide = 30.0F;
constexpr float kNameLeft = 53.0F;
constexpr float kNameTop = 14.0F;
constexpr float kRowScoreLeft = 258.0F;
constexpr float kRowScoreWidth = 40.0F;
constexpr float kRowSummaryLeft = 10.0F;
constexpr float kRowSummaryTop = 54.0F;
constexpr float kBadgeLeft = 250.0F;
constexpr float kBadgeTop = 5.0F;
constexpr float kChestLeft = 263.0F;
constexpr float kChestTop = 49.0F;
constexpr float kDateMiddle = 276.0F;
constexpr float kDateTop = 55.0F;

// ── LE DÉTAIL ───────────────────────────────────────────────────────────────

constexpr float kDetailWidth = 328.0F;
constexpr float kDetailHeight = 405.0F;
constexpr float kStampLeft = 264.0F;
constexpr float kStampTop = 3.0F;
constexpr float kTitleLeft = 15.0F;
constexpr float kTitleTop = 14.0F;
// Une boîte de texte : le texte à trois pixels du bord, quatre du haut, et une
// ligne tous les dix-huit ; le trait qui barre à deux du bord, dix du haut.
constexpr float kBoxTextLeft = 3.0F;
constexpr float kBoxTextTop = 4.0F;
constexpr float kBoxLineHeight = 18.0F;
constexpr float kStrikeLeft = 2.0F;
constexpr float kStrikeTop = 10.0F;
constexpr float kStrikeThickness = 1.0F;
// Le trait déborde du texte d'un pixel.
constexpr float kStrikeOverrun = 1.0F;
// La description.
constexpr float kDescriptionLeft = 12.0F;
constexpr float kDescriptionTop = 34.0F;
constexpr float kDescriptionWidth = 300.0F;
constexpr float kDescriptionHeight = 60.0F;
// Les objectifs : la boîte, la puce, la jauge, selon qu'ils se comptent.
constexpr float kObjectiveLeft = 28.0F;
constexpr float kObjectiveWidth = 280.0F;
constexpr float kObjectiveHeight = 40.0F;
constexpr float kBulletLeft = 20.0F;
constexpr float kListedTop = 97.0F;
constexpr float kListedPitch = 19.0F;
constexpr float kListedBulletTop = 103.0F;
constexpr float kCountedTop = 105.0F;
constexpr float kCountedPitch = 36.0F;
constexpr float kCountedBulletTop = 111.0F;
constexpr float kCountedGaugeTop = 123.0F;
constexpr float kObjectiveGaugeWidth = 254.0F;
constexpr float kObjectiveGaugeHeight = 10.0F;
// Le bouton de réclamation.
constexpr float kReceiveLeft = 90.0F;
constexpr float kReceiveTop = 372.0F;
constexpr float kReceiveWidth = 148.0F;
constexpr float kReceiveHeight = 20.0F;
// Les récompenses : le cadre, et l'icône de l'objet dedans.
constexpr float kRewardTop = 304.0F;
constexpr float kRewardFrame = 62.0F;
constexpr float kItemIconLeft = 20.0F;
constexpr float kItemIconTop = 15.0F;
constexpr float kItemIconSide = 24.0F;

// ── LE RÉSUMÉ ───────────────────────────────────────────────────────────────

constexpr float kSummaryWidth = 648.0F;
constexpr float kSummaryHeight = 404.0F;
constexpr float kSummaryTitleLeft = 82.0F;
constexpr float kSummaryTitleTop = 27.0F;
constexpr float kOverallCountRight = 600.0F;
constexpr float kOverallCountTop = 28.0F;
constexpr float kOverallGaugeLeft = 43.0F;
constexpr float kOverallGaugeTop = 49.0F;
constexpr float kOverallGaugeWidth = 558.0F;
constexpr float kSummaryGaugeHeight = 10.0F;
// Les catégories, deux par ligne.
constexpr std::size_t kCategoryColumns = 2;
constexpr float kCategoryLabelLeft = 82.0F;
constexpr float kCategoryLabelPitch = 302.0F;
constexpr float kCategoryLabelTop = 107.0F;
constexpr float kCategoryCountRight = 296.0F;
constexpr float kCategoryCountTop = 108.0F;
constexpr float kCategoryColumnPitch = 304.0F;
constexpr float kCategoryLinePitch = 50.0F;
constexpr float kCategoryGaugeLeft = 43.0F;
constexpr float kCategoryGaugeTop = 123.0F;
constexpr float kCategoryGaugeWidth = 254.0F;
// Les succès récents : l'en-tête, puis deux colonnes repérées par l'insigne.
constexpr float kRecentHeaderLeft = 9.0F;
constexpr float kRecentHeaderTop = 292.0F;
constexpr float kRecentBadgeFirst = 260.0F;
constexpr float kRecentPitch = 316.0F;
constexpr float kRecentBadgeTop = 316.0F;
constexpr float kRecentIconBack = 242.0F;
constexpr float kRecentIconTop = 314.0F;
constexpr float kRecentScoreAfter = 23.0F;
constexpr float kRecentNameBack = 198.0F;
constexpr float kRecentLineTop = 325.0F;
constexpr float kRecentSummaryBack = 241.0F;
constexpr float kRecentSummaryTop = 365.0F;
constexpr float kRecentChestAfter = 12.0F;
constexpr float kRecentChestTop = 360.0F;
constexpr float kRecentDateBack = 3.0F;
constexpr float kRecentDateTop = 366.0F;

// UNE BOÎTE DE TEXTE : repliée au mot dans sa largeur, autant de lignes que sa
// hauteur en tient. Contournée de blanc, ou barrée, au choix.
struct TextBox {
  float x = 0.0F;
  float y = 0.0F;
  float width = 0.0F;
  float height = 0.0F;
  ImU32 color = kBlack;
  bool outlined = false;
  bool struck = false;
};

void PaintTextBox(const Painter& paint, const TextBox& box, std::string_view text) {
  const std::vector<std::string_view> lines =
      WrapLines(paint, text, kTextLarge, box.width - kBoxTextLeft);
  const auto shown = std::min(lines.size(), static_cast<std::size_t>(box.height / kBoxLineHeight));
  const ImU32 ink = paint.Ink(box.color);
  for (std::size_t line = 0; line < shown; ++line) {
    const float top = box.y + kBoxLineHeight * static_cast<float>(line);
    if (box.outlined) {
      paint.TextOutlined(box.x + kBoxTextLeft, top + kBoxTextTop, lines[line], kTextLarge, ink,
                         paint.Laid(kWhite));
    } else {
      paint.Text(box.x + kBoxTextLeft, top + kBoxTextTop, lines[line], kTextLarge, ink);
    }
    if (box.struck) {
      paint.Fill(box.x + kStrikeLeft, top + kStrikeTop,
                 paint.TextWidth(lines[line], kTextLarge) + kStrikeOverrun, kStrikeThickness, ink);
    }
  }
}

[[nodiscard]] ImU32 ObjectiveColor(AchievementObjectiveState state) {
  switch (state) {
    case AchievementObjectiveState::kReached:
      return kPaleText;
    case AchievementObjectiveState::kStarted:
      return kObjectiveStartedText;
    case AchievementObjectiveState::kUntouched:
      break;
  }
  return kObjectiveUntouchedText;
}

void DrawObjectives(const Painter& paint, const AchievementDetailView& detail,
                    const AchievementArt& art, AchievementAction& action) {
  const std::size_t count = std::min(detail.objectives.size(), kAchievementObjectiveRows);
  const float first = detail.counted ? kCountedTop : kListedTop;
  const float pitch = detail.counted ? kCountedPitch : kListedPitch;
  for (std::size_t rank = 0; rank < count; ++rank) {
    const AchievementObjectiveView& objective = detail.objectives[rank];
    const float step = pitch * static_cast<float>(rank);
    const float top = first + step;
    if (detail.counted) {
      paint.Art(art.detail_bullet, kBulletLeft, kCountedBulletTop + step);
      if (objective.count.has_value()) {
        paint.Gauge(kObjectiveLeft, kCountedGaugeTop + step, kObjectiveGaugeWidth,
                    kObjectiveGaugeHeight, *objective.count, objective.target, kObjectiveGauge);
      }
    } else {
      paint.Art(art.detail_bullet, kBulletLeft, kListedBulletTop + step);
    }
    const bool reached = objective.state == AchievementObjectiveState::kReached;
    PaintTextBox(paint,
                 {.x = kObjectiveLeft,
                  .y = top,
                  .width = kObjectiveWidth,
                  .height = kObjectiveHeight,
                  .color = ObjectiveColor(objective.state),
                  .struck = reached},
                 objective.text);
    // ⚠ LA ZONE CLIQUABLE S'ARRÊTE AU PAS DES LIGNES : les boîtes, plus hautes,
    // se chevauchent, et le clic irait à la mauvaise.
    if (objective.shortcut.has_value()) {
      ImGui::PushID(static_cast<int>(rank));
      if (paint
              .Hit("##objectif", kObjectiveLeft, top, kObjectiveWidth,
                   std::min(kObjectiveHeight, pitch))
              .pressed) {
        action.jump_to = *objective.shortcut;
      }
      ImGui::PopID();
    }
  }
}

void DrawReceive(const Painter& paint, const AchievementDetailView& detail,
                 const AchievementArt& art, AchievementAction& action) {
  switch (detail.receive) {
    case AchievementReceive::kHidden:
      return;
    case AchievementReceive::kDisabled:
      paint.Art(art.receive_disabled, kReceiveLeft, kReceiveTop);
      return;
    case AchievementReceive::kEnabled:
      break;
  }
  const HitResult hit =
      paint.Hit("##recevoir", kReceiveLeft, kReceiveTop, kReceiveWidth, kReceiveHeight, kOnRelease);
  const SkinImage& face = hit.held      ? art.receive_press
                          : hit.hovered ? art.receive_over
                                        : art.receive_out;
  paint.Art(face, kReceiveLeft, kReceiveTop);
  if (hit.pressed) {
    action.claim = detail.achievement_id;
  }
}

[[nodiscard]] const SkinImage& RewardFrame(AchievementRewardKind kind, const AchievementArt& art) {
  switch (kind) {
    case AchievementRewardKind::kTitle:
      return art.reward_title;
    case AchievementRewardKind::kBuff:
      return art.reward_buff;
    case AchievementRewardKind::kItem:
      break;
  }
  return art.reward_item;
}

void DrawRewards(const Painter& paint, const AchievementDetailView& detail,
                 const AchievementArt& art, AchievementAction& action) {
  const std::size_t count = detail.rewards.size();
  for (std::size_t rank = 0; rank < count; ++rank) {
    const AchievementRewardView& reward = detail.rewards[rank];
    const float left = AchievementRewardX(rank, count, kRewardFrame);
    paint.Art(RewardFrame(reward.kind, art), left, kRewardTop);
    const bool item = reward.kind == AchievementRewardKind::kItem;
    if (item) {
      paint.Image(reward.item_icon, left + kItemIconLeft, kRewardTop + kItemIconTop);
    }
    ImGui::PushID(static_cast<int>(rank));
    const HitResult hit =
        paint.Hit("##recompense", left, kRewardTop, kRewardFrame, kRewardFrame,
                  ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    ImGui::PopID();
    if (hit.hovered && !reward.tooltip.empty()) {
      SkinTooltip(std::string(reward.tooltip).c_str());
    }
    // Le clic droit sur l'icône de l'objet le décrit.
    const bool on_icon = hit.mouse.x >= kItemIconLeft &&
                         hit.mouse.x < kItemIconLeft + kItemIconSide &&
                         hit.mouse.y >= kItemIconTop && hit.mouse.y < kItemIconTop + kItemIconSide;
    if (item && hit.hovered && on_icon && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      action.describe_item = reward.item_id;
    }
  }
}

// LE COFFRE D'UNE RÉCOMPENSE À PRENDRE SE CLIQUE : il la réclame, sans choisir
// la rangée. ⚠ SOUMIS AVANT LA LISTE, qui sinon prendrait la souris. Un coffre
// qui attend l'achèvement reste un dessin.
void ChestInput(const Painter& paint, const AchievementWindowState& state,
                std::span<const AchievementRowView> rows, const AchievementArt& art,
                AchievementAction& action) {
  const SkinImage& chest = art.rewardbox_unclaimed;
  for (int line = 0; line < kAchievementRowsShown; ++line) {
    const auto index = static_cast<std::size_t>(line + state.list_scroll);
    if (index >= rows.size()) {
      break;
    }
    const AchievementRowView& row = rows[index];
    if (row.chest != AchievementChest::kUnclaimed) {
      continue;
    }
    const float top = kAchievementRowHeight * static_cast<float>(line);
    ImGui::PushID(static_cast<int>(row.achievement_id));
    const HitResult hit =
        paint.Hit("##coffre", kChestLeft, top + kChestTop, chest.width, chest.height, kOnRelease);
    if (hit.hovered) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      SkinTooltip(core::Tr("Réclamer la récompense"));
    }
    ImGui::PopID();
    if (hit.pressed) {
      action.claim = row.achievement_id;
    }
  }
}

}  // namespace

// ── LA LISTE ────────────────────────────────────────────────────────────────

void DrawAchievementList(const Painter& paint, AchievementWindowState& state,
                         const AchievementView& view, AchievementAction& action) {
  const AchievementArt& art = ArtOf(view);
  const std::span<const AchievementRowView> rows = view.rows;
  const int range = AchievementListScrollRange(rows.size());
  state.list_scroll = ClampAchievementScroll(state.list_scroll, range);
  ScrollbarModel bar{
      .x = kRowsWidth,
      .y = 0.0F,
      .height = kListHeight,
      .position = state.list_scroll,
      .range = range,
      .page = kAchievementRowsShown,
      .total = static_cast<int>(rows.size()),
      .track_step = kListTrackStep,
  };
  state.list_scroll = ScrollbarInput(paint, "##liste_ascenseur", bar);
  ChestInput(paint, state, rows, art, action);

  // UN CLIC : sur la zone de l'icône, il épingle et ne choisit pas ; ailleurs
  // dans une rangée montrée, il la choisit.
  const HitResult hit = paint.Hit("##liste", 0.0F, 0.0F, kRowsWidth, kListHeight);
  const std::optional<std::size_t> hovered_row =
      hit.hovered ? AchievementListRowAt(hit.mouse.y, state.list_scroll, rows.size())
                  : std::nullopt;
  if (hovered_row.has_value()) {
    const float row_top = kAchievementRowHeight *
                          static_cast<float>(static_cast<int>(*hovered_row) - state.list_scroll);
    const float inside_y = hit.mouse.y - row_top;
    const bool on_pin = hit.mouse.x >= kPinZoneInset &&
                        hit.mouse.x < kPinZoneInset + kPinZoneSide && inside_y >= kPinZoneInset &&
                        inside_y < kPinZoneInset + kPinZoneSide;
    // L'ICÔNE SUIT LE SUCCÈS, et rien d'autre ne le dit : l'infobulle l'annonce.
    if (on_pin) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      SkinTooltip(rows[*hovered_row].tracked ? core::Tr("Ne plus suivre ce succès")
                                             : core::Tr("Suivre ce succès à l'écran"));
    }
    if (hit.pressed) {
      const std::uint32_t chosen = rows[*hovered_row].achievement_id;
      if (on_pin) {
        action.toggle_tracking = chosen;
      } else {
        state.selected = chosen;
        action.select = chosen;
      }
    }
  }
  if (range > 0) {
    const int steps = paint.WheelSteps(0.0F, 0.0F, kListWidth, kListHeight);
    state.list_scroll =
        ClampAchievementScroll(state.list_scroll + steps * kAchievementWheelRows, range);
  }

  paint.PushClip(0.0F, 0.0F, kListWidth, kListHeight);
  paint.Art(art.bg_list, 0.0F, 0.0F);
  std::array<char, kNumberBytes> score{};
  const ImU32 name_ink = paint.Ink(kNameText);
  for (int line = 0; line < kAchievementRowsShown; ++line) {
    const auto index = static_cast<std::size_t>(line + state.list_scroll);
    if (index >= rows.size()) {
      break;
    }
    const AchievementRowView& row = rows[index];
    const float top = kAchievementRowHeight * static_cast<float>(line);
    const bool chosen = state.selected.has_value() && *state.selected == row.achievement_id;
    const SkinImage& back = chosen ? (row.completed ? art.list_complete_press : art.list_press)
                                   : (row.completed ? art.list_complete_out : art.list_out);
    paint.Art(back, 0.0F, top);
    paint.Art(row.icon, kIconLeft, top + kIconTop);
    if (row.tracked) {
      paint.Image(art.list_pin, kIconLeft, top + kIconTop);
    }
    paint.Text(kNameLeft, top + kNameTop, row.name, kTextLarge, name_ink, true);
    std::snprintf(score.data(), score.size(), "%3d", static_cast<int>(row.score));
    paint.TextCentered(kRowScoreLeft, kRowScoreWidth, top + kNameTop, score.data(), kTextLarge,
                       name_ink, true);
    paint.Text(kRowSummaryLeft, top + kRowSummaryTop, row.summary, kTextLarge, name_ink);
    if (row.completed) {
      paint.Image(art.badge_complete, kBadgeLeft, top + kBadgeTop);
    }
    if (row.chest == AchievementChest::kWaiting) {
      paint.Image(art.rewardbox_default, kChestLeft, top + kChestTop);
    } else if (row.chest == AchievementChest::kUnclaimed) {
      paint.Image(art.rewardbox_unclaimed, kChestLeft, top + kChestTop);
    }
    if (!row.date.empty()) {
      const float half = std::trunc(paint.TextWidth(row.date, kTextSmall) * 0.5F);
      paint.TextOutlined(kDateMiddle - half, top + kDateTop, row.date, kTextSmall,
                         paint.Ink(kDateText), paint.Laid(kWhite));
    }
  }
  bar.position = state.list_scroll;
  PaintScrollbar(paint, art, bar);
  paint.PopClip();
}

// ── LE DÉTAIL ───────────────────────────────────────────────────────────────

void DrawAchievementDetail(const Painter& paint, const AchievementView& view,
                           AchievementAction& action) {
  const AchievementArt& art = ArtOf(view);
  paint.PushClip(0.0F, 0.0F, kDetailWidth, kDetailHeight);
  paint.Art(art.bg_detail, 0.0F, 0.0F);
  if (view.detail.has_value()) {
    const AchievementDetailView& detail = *view.detail;
    if (detail.completed) {
      paint.Art(art.detail_stamp, kStampLeft, kStampTop);
    }
    paint.Text(kTitleLeft, kTitleTop, detail.name, kTextLarge, paint.Ink(kNameText), true);
    PaintTextBox(paint,
                 {.x = kDescriptionLeft,
                  .y = kDescriptionTop,
                  .width = kDescriptionWidth,
                  .height = kDescriptionHeight,
                  .color = kPaleText,
                  .outlined = true},
                 detail.details);
    DrawObjectives(paint, detail, art, action);
    DrawRewards(paint, detail, art, action);
    DrawReceive(paint, detail, art, action);
  }
  paint.PopClip();
}

// ── LE RÉSUMÉ ───────────────────────────────────────────────────────────────

void DrawAchievementSummary(const Painter& paint, const AchievementView& view,
                            AchievementAction& action) {
  const AchievementArt& art = ArtOf(view);
  const AchievementSummaryView& summary = view.summary;
  paint.PushClip(0.0F, 0.0F, kSummaryWidth, kSummaryHeight);
  paint.Art(art.bg_summary, 0.0F, 0.0F);
  const ImU32 name_ink = paint.Ink(kNameText);
  const ImU32 count_ink = paint.Ink(kDateText);
  paint.Text(kSummaryTitleLeft, kSummaryTitleTop, view.words.summary_title, kTextLarge, name_ink,
             true);
  std::array<char, kNumberBytes> text{};
  std::snprintf(text.data(), text.size(), "%u/%u", summary.overall.done, summary.overall.total);
  paint.TextRight(kOverallCountRight, kOverallCountTop, text.data(), kTextMedium, count_ink);
  paint.Gauge(kOverallGaugeLeft, kOverallGaugeTop, kOverallGaugeWidth, kSummaryGaugeHeight,
              summary.overall.done, summary.overall.total, kObjectiveGauge);

  for (std::size_t category = 0; category < kAchievementCategoryCount; ++category) {
    const auto column = static_cast<float>(category % kCategoryColumns);
    const float drop = kCategoryLinePitch * static_cast<float>(category / kCategoryColumns);
    const AchievementTally& tally = summary.categories[category];
    // Les catégories sont les onglets qui suivent « Tous ».
    paint.Text(kCategoryLabelLeft + kCategoryLabelPitch * column, kCategoryLabelTop + drop,
               view.words.tabs[static_cast<std::size_t>(kAchievementFirstCategoryTab) + category],
               kTextLarge, name_ink, true);
    std::snprintf(text.data(), text.size(), "%u/%u", tally.done, tally.total);
    paint.TextRight(kCategoryCountRight + kCategoryColumnPitch * column, kCategoryCountTop + drop,
                    text.data(), kTextMedium, count_ink);
    paint.Gauge(kCategoryGaugeLeft + kCategoryColumnPitch * column, kCategoryGaugeTop + drop,
                kCategoryGaugeWidth, kSummaryGaugeHeight, tally.done, tally.total, kObjectiveGauge);
  }

  paint.Text(kRecentHeaderLeft, kRecentHeaderTop, view.words.recent, kTextLarge,
             paint.Ink(kRecentHeaderText), true);
  const std::size_t recent_count = std::min(summary.recent.size(), kAchievementRecentCount);
  for (std::size_t rank = 0; rank < recent_count; ++rank) {
    const AchievementRecentView& recent = summary.recent[rank];
    const float badge = kRecentBadgeFirst + kRecentPitch * static_cast<float>(rank);
    paint.Art(recent.icon, badge - kRecentIconBack, kRecentIconTop);
    paint.Image(art.badge_complete, badge, kRecentBadgeTop);
    std::snprintf(text.data(), text.size(), "%d", static_cast<int>(recent.score));
    paint.Text(badge + kRecentScoreAfter, kRecentLineTop, text.data(), kTextLarge, name_ink, true);
    paint.Text(badge - kRecentSummaryBack, kRecentSummaryTop, recent.summary, kTextLarge,
               paint.Ink(kPaleText));
    if (recent.unclaimed) {
      paint.Image(art.rewardbox_unclaimed, badge + kRecentChestAfter, kRecentChestTop);
    }
    if (!recent.date.empty()) {
      paint.TextOutlined(badge - kRecentDateBack, kRecentDateTop, recent.date, kTextSmall,
                         count_ink, paint.Laid(kWhite));
    }
    // LE NOM EST UN BOUTON-TEXTE, de la largeur de son texte : il mène au succès.
    const float name_left = badge - kRecentNameBack;
    ImGui::PushID(static_cast<int>(rank));
    const HitResult hit = paint.Hit("##recent", name_left, kRecentLineTop,
                                    paint.TextWidth(recent.name, kTextLarge, true),
                                    paint.TextHeight(kTextLarge), kOnRelease);
    ImGui::PopID();
    if (hit.pressed) {
      action.jump_to = recent.achievement_id;
    }
    paint.TextOutlined(name_left, kRecentLineTop, recent.name, kTextLarge,
                       hit.hovered ? count_ink : paint.Ink(kRecentNameText), paint.Laid(kWhite),
                       true);
  }
  paint.PopClip();
}

}  // namespace ro::ui::achievement
