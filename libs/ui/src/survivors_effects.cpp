#include "ro/ui/survivors_effects.h"

#include <algorithm>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/ui/skin_look.h"

#include "ui_text.h"

namespace ro::ui {
namespace {

constexpr std::uint32_t kMsPerSecond = 1000;

// La colonne des effets en cours : une case par effet, ses secondes dessous.
constexpr float kEffectSide = 28.0F;
constexpr float kEffectGap = 4.0F;
constexpr ImU32 kEffectBack = IM_COL32(0, 0, 0, 140);
constexpr ImU32 kEffectEdge = IM_COL32(255, 255, 255, 90);
// Le voile de la part écoulée : assez sombre pour se voir, assez clair pour
// laisser l'icône lisible dessous.
constexpr ImU32 kEffectVeil = IM_COL32(0, 0, 0, 150);
// Le texte sur la scène : blanc à l'ombre noire décalée d'un pixel.
constexpr ImU32 kSceneTextColor = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kSceneShadowColor = IM_COL32(0, 0, 0, 220);
constexpr float kShadowOffset = 1.0F;
constexpr std::size_t kSecondsTextSize = 16;

// LES AVIS : l'or vif, posé sur la scène sombre, au-dessus de la tête du
// héros, montant un peu le long de leur vie.
constexpr ImU32 kNoticeColor = IM_COL32(255, 215, 90, 255);
constexpr float kNoticeAboveFeet = 110.0F;
constexpr float kNoticeRise = 16.0F;
// La part de sa vie où l'avis s'efface.
constexpr float kNoticeFadeShare = 0.25F;
constexpr ImU32 kOpaque = 255;
constexpr float kWhole = 1.0F;

// La ligne d'un effet permanent dans la pause : l'icône, le nom, le niveau
// et l'effet.
constexpr float kRowIconSide = 20.0F;
constexpr int kRowColumns = 4;
constexpr std::size_t kLevelTextSize = 32;

// La couleur dont l'opacité est multipliée par `alpha`, de 0 à 1.
[[nodiscard]] ImU32 Faded(ImU32 color, float alpha) {
  const float own = static_cast<float>((color & IM_COL32_A_MASK) >> IM_COL32_A_SHIFT);
  const auto alpha_byte = static_cast<ImU32>(own * std::clamp(alpha, 0.0F, 1.0F));
  return (color & ~IM_COL32_A_MASK) | (std::min<ImU32>(alpha_byte, kOpaque) << IM_COL32_A_SHIFT);
}

// Un texte et son ombre, effacés ensemble par `alpha`.
void ShadowedText(ImDrawList& draw, ImVec2 at, ImU32 color, float alpha, const char* text) {
  draw.AddText(ImVec2(at.x + kShadowOffset, at.y + kShadowOffset), Faded(kSceneShadowColor, alpha),
               text);
  draw.AddText(at, Faded(color, alpha), text);
}

}  // namespace

std::uint32_t SurvivorsEffectSecondsLeft(std::uint32_t remaining_ms) noexcept {
  return remaining_ms / kMsPerSecond + (remaining_ms % kMsPerSecond != 0 ? 1U : 0U);
}

bool SurvivorsEffectIconLit(std::uint32_t remaining_ms) noexcept {
  if (remaining_ms > kSurvivorsEffectBlinkMs) return true;
  return (remaining_ms / kSurvivorsEffectBlinkHalfPeriodMs) % 2U == 0U;
}

float SurvivorsEffectVeil(std::uint32_t remaining_ms, std::uint32_t full_ms) noexcept {
  if (full_ms == 0) return 1.0F;
  const float left =
      static_cast<float>(std::min(remaining_ms, full_ms)) / static_cast<float>(full_ms);
  return 1.0F - left;
}

float SurvivorsNoticeAlpha(float fraction) noexcept {
  if (fraction <= 0.0F || fraction >= 1.0F) return 0.0F;
  return std::min((1.0F - fraction) / kNoticeFadeShare, 1.0F);
}

// ⚠ UN FORMAT LITTÉRAL PAR SORTE, et non une table de formats : un format lu
// dans une table n'est plus vérifié par le compilateur contre ses arguments.
SurvivorsFigureText SurvivorsEffectFigureText(SurvivorsEffectFigure figure, std::int32_t value) {
  SurvivorsFigureText text = {};
  char* out = text.data();
  const std::size_t size = text.size();
  const int shown = value;
  switch (figure) {
    case SurvivorsEffectFigure::kDamagePct:
      std::snprintf(out, size, core::Tr("Puissance +%d %%"), shown);
      break;
    case SurvivorsEffectFigure::kMaxHp:
      std::snprintf(out, size, core::Tr("PV max +%d"), shown);
      break;
    case SurvivorsEffectFigure::kArmor:
      std::snprintf(out, size, core::Tr("Armure +%d"), shown);
      break;
    case SurvivorsEffectFigure::kRegenPerMinute:
      std::snprintf(out, size, core::Tr("PV +%d par minute"), shown);
      break;
    case SurvivorsEffectFigure::kWalkPct:
      std::snprintf(out, size, core::Tr("Marche +%d %%"), shown);
      break;
    case SurvivorsEffectFigure::kCooldownPct:
      std::snprintf(out, size, core::Tr("Recharge –%d %%"), shown);
      break;
    case SurvivorsEffectFigure::kAmount:
      std::snprintf(out, size, core::Tr("Projectiles +%d"), shown);
      break;
    case SurvivorsEffectFigure::kAreaPct:
      std::snprintf(out, size, core::Tr("Zone +%d %%"), shown);
      break;
    case SurvivorsEffectFigure::kDurationPct:
      std::snprintf(out, size, core::Tr("Durée +%d %%"), shown);
      break;
    case SurvivorsEffectFigure::kLuckPct:
      std::snprintf(out, size, core::Tr("Chance +%d %%"), shown);
      break;
    case SurvivorsEffectFigure::kMagnetPct:
      std::snprintf(out, size, core::Tr("Aimant +%d %%"), shown);
      break;
    case SurvivorsEffectFigure::kExpPct:
      std::snprintf(out, size, core::Tr("Expérience +%d %%"), shown);
      break;
    case SurvivorsEffectFigure::kHeal:
      std::snprintf(out, size, core::Tr("PV +%d"), shown);
      break;
    case SurvivorsEffectFigure::kZeny:
      std::snprintf(out, size, core::Tr("Zeny +%d"), shown);
      break;
    case SurvivorsEffectFigure::kPurifyCells:
      std::snprintf(out, size, core::Tr("Purifie les monstres à %d cases"), shown);
      break;
    case SurvivorsEffectFigure::kGatherGems:
      std::snprintf(out, size, "%s", core::Tr("Attire toutes les gemmes"));
      break;
    case SurvivorsEffectFigure::kRerollsLeft:
      std::snprintf(out, size, core::Tr("Relances restantes : %d"), shown);
      break;
    case SurvivorsEffectFigure::kSkipsLeft:
      std::snprintf(out, size, core::Tr("Passes restantes : %d"), shown);
      break;
    case SurvivorsEffectFigure::kBanishesLeft:
      std::snprintf(out, size, core::Tr("Bannissements restants : %d"), shown);
      break;
    case SurvivorsEffectFigure::kLivesLeft:
      std::snprintf(out, size, core::Tr("Vies restantes : %d"), shown);
      break;
  }
  return text;
}

SurvivorsNoticeText SurvivorsNoticeTextOf(const SurvivorsHudNotice& notice) {
  const SurvivorsFigureText effect = SurvivorsEffectFigureText(notice.figure, notice.value);
  const char* name = notice.name != nullptr ? notice.name : "";
  SurvivorsNoticeText text = {};
  if (notice.duration_ms == 0) {
    std::snprintf(text.data(), text.size(), core::Tr("%s : %s"), name, effect.data());
  } else {
    std::snprintf(text.data(), text.size(), core::Tr("%s : %s, %u s"), name, effect.data(),
                  static_cast<unsigned>(SurvivorsEffectSecondsLeft(notice.duration_ms)));
  }
  return text;
}

float DrawSurvivorsHudEffects(ImDrawList& draw, std::span<const SurvivorsHudEffect> effects,
                              float right, float top) {
  const float line_height = ImGui::GetTextLineHeight();
  std::array<char, kSecondsTextSize> seconds = {};
  for (const SurvivorsHudEffect& effect : effects) {
    const ImVec2 top_left(right - kEffectSide, top);
    const ImVec2 bottom_right(right, top + kEffectSide);
    draw.AddRectFilled(top_left, bottom_right, kEffectBack);
    if (effect.icon != 0 && SurvivorsEffectIconLit(effect.remaining_ms)) {
      draw.AddImage(effect.icon, top_left, bottom_right);
    }
    // Le voile descend du haut à mesure que l'effet s'écoule : la part claire
    // de l'icône est ce qui reste.
    const float veil = SurvivorsEffectVeil(effect.remaining_ms, effect.full_ms);
    if (veil > 0.0F) {
      draw.AddRectFilled(top_left, ImVec2(bottom_right.x, top_left.y + kEffectSide * veil),
                         kEffectVeil);
    }
    draw.AddRect(top_left, bottom_right, kEffectEdge);
    std::snprintf(seconds.data(), seconds.size(), "%u",
                  static_cast<unsigned>(SurvivorsEffectSecondsLeft(effect.remaining_ms)));
    const ImVec2 size = ImGui::CalcTextSize(seconds.data());
    ShadowedText(draw, ImVec2(right - kEffectSide * 0.5F - size.x * 0.5F, bottom_right.y),
                 kSceneTextColor, kWhole, seconds.data());
    top = bottom_right.y + line_height + kEffectGap;
  }
  return top;
}

void DrawSurvivorsHudNotices(ImDrawList& draw, std::span<const SurvivorsHudNotice> notices,
                             ImVec2 anchor) {
  const float line_height = ImGui::GetTextLineHeight();
  float bottom = anchor.y - kNoticeAboveFeet;
  // Le plus récent en bas : la liste arrive du plus ancien au plus récent.
  for (auto notice = notices.rbegin(); notice != notices.rend(); ++notice) {
    const float alpha = SurvivorsNoticeAlpha(notice->fraction);
    if (alpha <= 0.0F) continue;
    const SurvivorsNoticeText text = SurvivorsNoticeTextOf(*notice);
    const ImVec2 size = ImGui::CalcTextSize(text.data());
    const float top = bottom - size.y - kNoticeRise * notice->fraction;
    ShadowedText(draw, ImVec2(anchor.x - size.x * 0.5F, top), kNoticeColor, alpha, text.data());
    bottom -= line_height;
  }
}

void DrawSurvivorsPauseEffects(std::span<const SurvivorsPauseEffectRow> rows) {
  if (rows.empty()) return;
  ImGui::SeparatorText(core::Tr("Effets permanents"));
  if (!ImGui::BeginTable("##effects", kRowColumns, ImGuiTableFlags_SizingFixedFit)) return;
  std::array<char, kLevelTextSize> level = {};
  for (const SurvivorsPauseEffectRow& row : rows) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (row.icon != 0) {
      ImGui::Image(row.icon, ImVec2(kRowIconSide, kRowIconSide));
    } else {
      ImGui::Dummy(ImVec2(kRowIconSide, kRowIconSide));
    }
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(row.name != nullptr ? row.name : "");
    ImGui::TableNextColumn();
    if (row.level > 0) {
      std::snprintf(level.data(), level.size(), core::Tr("Lv %d"), row.level);
      ColoredText(kSkinGoldTextColor, level.data());
    }
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(SurvivorsEffectFigureText(row.figure, row.value).data());
  }
  ImGui::EndTable();
}

}  // namespace ro::ui
