#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/ui/damage_meter_window.h"
#include "ro/ui/skin_tooltip.h"

#include "damage_meter_parts.h"
#include "ui_scale.h"

namespace ro::ui::damage_meter {
namespace {

// ── LE VOLET, en pixels à l'échelle 100 % ───────────────────────────────────
constexpr float kRowRoomPixels = 4.0F;
constexpr float kHeadingRoomPixels = 4.0F;
constexpr float kColumnGapPixels = 6.0F;
constexpr float kTagGapPixels = 3.0F;
constexpr float kStripePixels = 2.0F;
constexpr float kRowRoundingPixels = 2.0F;
// La barre qui mesure un coup au premier, derrière sa ligne.
constexpr float kBarOpacity = 0.16F;
// UN COUP QUI VIENT D'ENTRER S'ALLUME, puis s'éteint : on voit qu'il est là
// sans avoir à relire la liste.
constexpr std::uint32_t kFreshMs = 2000;
constexpr float kFreshOpacity = 0.35F;
constexpr float kStripeOpacity = 0.85F;
constexpr ImU32 kCriticalColor = IM_COL32(255, 120, 80, 255);
constexpr ImU32 kHoveredRowColor = IM_COL32(255, 255, 255, 18);

constexpr std::uint32_t kMsPerSecond = 1000;
constexpr std::uint32_t kSecondsPerMinute = 60;
constexpr std::uint32_t kSecondsPerHour = 3600;

// Ce qui suit un libellé trop long pour sa place.
constexpr const char* kEllipsis = "...";

[[nodiscard]] float RowHeight() { return ImGui::GetFontSize() + Px(kRowRoomPixels); }

[[nodiscard]] float HeadingHeight() {
  return ImGui::GetFontSize() * kLabelTextScale + Px(kHeadingRoomPixels);
}

// L'âge d'un coup, à la grosse maille : la seconde, puis la minute, puis
// l'heure. ⚠ ARRONDI VERS LE BAS : « 1 min » ne doit pas se lire à 31 s.
[[nodiscard]] std::string ShortAge(std::uint32_t age_ms) {
  const std::uint32_t seconds = age_ms / kMsPerSecond;
  std::array<char, 24> text{};
  if (seconds < kSecondsPerMinute) {
    std::snprintf(text.data(), text.size(), core::Tr("%u s"), seconds);
  } else if (seconds < kSecondsPerHour) {
    std::snprintf(text.data(), text.size(), core::Tr("%u min"), seconds / kSecondsPerMinute);
  } else {
    std::snprintf(text.data(), text.size(), core::Tr("%u h"), seconds / kSecondsPerHour);
  }
  return text.data();
}

// Ce qui a frappé, niveau compris : « Bash 10 ».
[[nodiscard]] std::string WhatOf(const DamageMeterRecordView& record) {
  std::string what(record.what);
  if (record.skill_level != 0) {
    what += ' ';
    what += std::to_string(record.skill_level);
  }
  return what;
}

// Le texte rogné à la largeur donnée, suivi de points de suspension. ⚠ PAR
// OCTETS ENTIERS D'UN CARACTÈRE : un nom coréen ou accentué coupé au milieu
// d'une séquence UTF-8 écrirait un losange.
[[nodiscard]] std::string Fit(ImFont* font, float size, std::string text, float room) {
  if (TextWidth(font, size, text.c_str()) <= room) {
    return text;
  }
  constexpr unsigned char kContinuationMask = 0xC0;
  constexpr unsigned char kContinuationBits = 0x80;
  while (!text.empty()) {
    // Un caractère entier : ses octets de suite, puis celui qui l'ouvre.
    while (!text.empty() &&
           (static_cast<unsigned char>(text.back()) & kContinuationMask) == kContinuationBits) {
      text.pop_back();
    }
    if (!text.empty()) {
      text.pop_back();
    }
    if (TextWidth(font, size, (text + kEllipsis).c_str()) <= room) {
      return text + kEllipsis;
    }
  }
  return kEllipsis;
}

// L'infobulle d'un rang : tout ce que la ligne a dû taire.
void DescribeRecord(const DamageMeterRecordView& record, DamageMeterMode mode) {
  if (!BeginSkinTooltip()) {
    return;
  }
  ImGui::TextUnformatted(AmountText(mode, record.amount).c_str());
  ImGui::TextUnformatted(WhatOf(record).c_str());
  // L'AUTRE BOUT : la cible de ce qu'on fait, la source de ce qu'on reçoit.
  const bool received =
      mode == DamageMeterMode::kDamageTaken || mode == DamageMeterMode::kHealingTaken;
  if (!record.other.empty() && received) {
    ImGui::Text(core::Tr("De : %s"), std::string(record.other).c_str());
  } else if (!record.other.empty()) {
    ImGui::Text(core::Tr("Sur : %s"), std::string(record.other).c_str());
  }
  if (record.area_cycle) {
    ImGui::TextUnformatted(
        core::Tr("La somme d'un battement du sort de zone, toutes cibles confondues."));
  }
  if (record.hits > 1) {
    ImGui::Text(core::Tr("En %u coups"), static_cast<unsigned>(record.hits));
  }
  if (record.critical) {
    ImGui::TextUnformatted(core::Tr("Coup critique"));
  }
  const std::uint32_t seconds = record.age_ms / kMsPerSecond;
  ImGui::Text(core::Tr("Il y a %u min %02u s"), seconds / kSecondsPerMinute,
              seconds % kSecondsPerMinute);
  if (record.this_fight) {
    ImGui::TextUnformatted(core::Tr("Pendant le combat en cours"));
  }
  EndSkinTooltip();
}

}  // namespace

float RecordsHeight(std::size_t rows) {
  return HeadingHeight() + RowHeight() * static_cast<float>(rows);
}

std::size_t RecordRowsFitting(float height) {
  const float room = height - HeadingHeight();
  if (room <= 0.0F) {
    return 0;
  }
  return std::min(kDamageMeterRecordRows, static_cast<std::size_t>(room / RowHeight()));
}

void DrawRecords(const DamageMeterView& view, float width, std::size_t rows) {
  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImFont* regular = ImGui::GetFont();
  ImFont* bold = NumberFont(view);
  const float base_px = ImGui::GetFontSize();
  const float label_px = base_px * kLabelTextScale;
  const float height = RecordsHeight(rows);
  const ImU32 text = view.look.text_color;

  // L'intitulé, du même gris et de la même taille que ceux des tuiles.
  PaintText(into, regular, label_px, origin, kLabelColor, core::Tr("PALMARÈS"));
  const float rows_top = origin.y + HeadingHeight();

  if (view.records.empty()) {
    const char* hint = core::Tr("Aucun coup porté pour l'instant.");
    PaintText(into, regular, base_px,
              ImVec2(origin.x + (width - TextWidth(regular, base_px, hint)) * 0.5F,
                     rows_top + (height - HeadingHeight() - base_px) * 0.5F),
              kQuietColor, hint);
    ImGui::Dummy(ImVec2(width, height));
    return;
  }

  // ── LES COLONNES, mesurées sur ce qu'elles portent ────────────────────────
  // Le rang à droite d'une colonne de « 10 » ; les dégâts à droite d'une
  // colonne de leur plus large ; l'âge à droite de la fenêtre.
  const std::size_t shown = std::min(view.records.size(), rows);
  const float rank_width = TextWidth(regular, label_px, "10");
  float damage_width = 0.0F;
  for (std::size_t rank = 0; rank < shown; ++rank) {
    const std::string amount = FormatDamageAmount(view.records[rank].amount);
    damage_width = std::max(damage_width, TextWidth(bold, base_px, amount.c_str()));
  }
  const float gap = Px(kColumnGapPixels);
  const float damage_right = origin.x + Px(kStripePixels) + gap + rank_width + gap + damage_width;
  const std::int64_t top_damage = std::max<std::int64_t>(view.records.front().amount, 1);
  const float row_height = RowHeight();

  for (std::size_t rank = 0; rank < shown; ++rank) {
    const DamageMeterRecordView& record = view.records[rank];
    const ImVec2 low(origin.x, rows_top + row_height * static_cast<float>(rank));
    const ImVec2 high(origin.x + width, low.y + row_height);
    const float text_y = low.y + (row_height - base_px) * 0.5F;

    // Le survol, porté par un bouton invisible : il donne l'infobulle.
    ImGui::SetCursorScreenPos(low);
    ImGui::PushID(static_cast<int>(rank));
    ImGui::InvisibleButton("##rang", ImVec2(width, row_height));
    ImGui::PopID();
    const bool hovered = ImGui::IsItemHovered();

    // La barre qui le mesure au premier, puis l'éclat d'un coup tout neuf.
    const float share = static_cast<float>(record.amount) / static_cast<float>(top_damage);
    into->AddRectFilled(low, ImVec2(low.x + width * std::clamp(share, 0.0F, 1.0F), high.y),
                        Tint(view.look.plot_color, kBarOpacity), Px(kRowRoundingPixels));
    if (record.age_ms < kFreshMs) {
      const float fading = 1.0F - static_cast<float>(record.age_ms) / static_cast<float>(kFreshMs);
      into->AddRectFilled(low, high, Tint(text, kFreshOpacity * fading), Px(kRowRoundingPixels));
    }
    if (hovered) {
      into->AddRectFilled(low, high, kHoveredRowColor, Px(kRowRoundingPixels));
    }
    // Le liseré du combat en cours.
    if (record.this_fight) {
      into->AddRectFilled(low, ImVec2(low.x + Px(kStripePixels), high.y),
                          Tint(text, kStripeOpacity));
    }

    // Le rang.
    const std::string place = std::to_string(rank + 1);
    const float rank_right = low.x + Px(kStripePixels) + gap + rank_width;
    PaintText(into, regular, label_px,
              ImVec2(rank_right - TextWidth(regular, label_px, place.c_str()),
                     low.y + (row_height - label_px) * 0.5F),
              kLabelColor, place.c_str());

    // Les dégâts, alignés à droite de leur colonne.
    const std::string amount = FormatDamageAmount(record.amount);
    PaintText(into, bold, base_px,
              ImVec2(damage_right - TextWidth(bold, base_px, amount.c_str()), text_y),
              Tint(text, 0.95F), amount.c_str());

    // L'âge, à droite de la fenêtre.
    const std::string age = ShortAge(record.age_ms);
    const float age_x = high.x - TextWidth(regular, label_px, age.c_str());
    PaintText(into, regular, label_px, ImVec2(age_x, low.y + (row_height - label_px) * 0.5F),
              kQuietColor, age.c_str());

    // Entre les deux : les marques, puis ce qui a frappé et sur qui.
    float x = damage_right + Px(kTagGapPixels);
    if (record.critical) {
      const char* mark = core::Tr("crit");
      PaintText(into, regular, label_px, ImVec2(x, text_y), kCriticalColor, mark);
      x += TextWidth(regular, label_px, mark) + Px(kTagGapPixels);
    }
    if (record.hits > 1) {
      std::array<char, 16> times{};
      std::snprintf(times.data(), times.size(), core::Tr("×%u"),
                    static_cast<unsigned>(record.hits));
      PaintText(into, regular, label_px, ImVec2(x, text_y), kLabelColor, times.data());
      x += TextWidth(regular, label_px, times.data()) + Px(kTagGapPixels);
    }
    x += gap - Px(kTagGapPixels);
    std::string what = WhatOf(record);
    if (!record.other.empty()) {
      what += " · ";
      what += record.other;
    }
    const float room = age_x - gap - x;
    if (room > 0.0F) {
      PaintText(into, regular, base_px, ImVec2(x, text_y), kSubColor,
                Fit(regular, base_px, std::move(what), room).c_str());
    }

    if (hovered) {
      DescribeRecord(record, view.mode);
    }
  }

  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(width, height));
}

}  // namespace ro::ui::damage_meter
