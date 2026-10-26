#include "ro/ui/survivors_summary_window.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/ui/skin_look.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/survivors_hud.h"
#include "ro/ui/window_name.h"

#include "survivors_meta_text.h"
#include "survivors_text.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

inline constexpr const char* kSummaryTitle = core::TrText("Survivants de Midgard");
inline constexpr const char* kSummaryId = "survivors_summary";

// Le texte du voile et du bandeau : grand, centré, sur un ruban sombre.
constexpr float kBannerTextScale = 3.0F;
constexpr float kBannerPadding = 24.0F;
constexpr ImU32 kBannerBack = IM_COL32(0, 0, 0, 160);
constexpr ImU32 kDeathColor = IM_COL32(230, 40, 40, 255);
// L'or vif sur le ruban sombre du bandeau ; l'or sombre dans le corps clair
// de la fenêtre, où l'or vif ne se lit plus.
constexpr ImU32 kBannerVictoryColor = IM_COL32(255, 215, 90, 255);
constexpr ImU32 kVictoryColor = kSkinGoldTextColor;
constexpr ImU32 kHintColor = IM_COL32(220, 220, 220, 255);

constexpr float kSummaryWidth = 520.0F;
constexpr float kListHeight = 160.0F;
constexpr float kRowIconSide = 24.0F;
constexpr float kCardSide = 56.0F;
constexpr float kCardGap = 6.0F;
constexpr std::size_t kLineSize = 96;
// La largeur réservée au compte d'une espèce : six chiffres.
constexpr const char* kCountSample = "000000";
constexpr ImGuiWindowFlags kModalFlags =
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

// Le voile ou le bandeau, sur la liste de premier plan ; vrai sur un clic.
[[nodiscard]] bool DrawBanner(const char* text, ImU32 color, bool accepts_input) {
  ImDrawList& draw = *ImGui::GetForegroundDrawList();
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  const ImVec2 center = screen->GetCenter();
  ImFont* font = ImGui::GetFont();
  const float size = ImGui::GetFontSize() * kBannerTextScale;
  const ImVec2 text_size = font->CalcTextSizeA(size, FLT_MAX, 0.0F, text);
  const char* hint = core::Tr("Cliquez pour continuer.");
  const ImVec2 hint_size = ImGui::CalcTextSize(hint);
  const float top = center.y - text_size.y * 0.5F - kBannerPadding;
  const float bottom = center.y + text_size.y * 0.5F + hint_size.y + kBannerPadding;
  draw.AddRectFilled(ImVec2(screen->Pos.x, top), ImVec2(screen->Pos.x + screen->Size.x, bottom),
                     kBannerBack);
  draw.AddText(font, size, ImVec2(center.x - text_size.x * 0.5F, center.y - text_size.y * 0.5F),
               color, text);
  draw.AddText(ImVec2(center.x - hint_size.x * 0.5F, center.y + text_size.y * 0.5F), kHintColor,
               hint);
  return accepts_input && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
}

// La marque de la partie, en tête : classée ou libre.
void DrawRankingMark(const SurvivorsSummaryRanking& ranking) {
  if (ranking.ranked) {
    ColoredText(kVictoryColor, core::Tr("Partie classée"));
  } else {
    FaintText(core::Tr("Partie libre"));
  }
}

// Le score du serveur et la ligne du rang d'une partie classée.
void DrawStanding(const SurvivorsSummaryRanking& ranking) {
  if (!ranking.ranked) return;
  ImGui::SeparatorText(core::Tr("Classement"));
  std::array<char, kLineSize> line = {};
  if (ranking.server_score.has_value()) {
    std::snprintf(line.data(), line.size(), core::Tr("Score : %s"),
                  SurvivorsZeny(*ranking.server_score).data());
    ImGui::TextUnformatted(line.data());
  }
  const std::array<char, kSurvivorsStandingTextSize> standing =
      SurvivorsStandingText(ranking.standing);
  switch (ranking.standing.status) {
    case SurvivorsRankStatus::kPersonalBest:
      ColoredText(kVictoryColor, standing.data());
      break;
    case SurvivorsRankStatus::kAccepted:
      ImGui::TextUnformatted(standing.data());
      break;
    case SurvivorsRankStatus::kSending:
    case SurvivorsRankStatus::kPending:
    case SurvivorsRankStatus::kLost:
      ImGui::PushTextWrapPos(0.0F);
      FaintText(standing.data());
      ImGui::PopTextWrapPos();
      break;
    case SurvivorsRankStatus::kImplausible:
    case SurvivorsRankStatus::kRefused:
      ColoredText(kDeathColor, standing.data());
      break;
  }
}

void DrawHeadline(const SurvivorsSummaryView& view) {
  if (view.ranking.has_value()) DrawRankingMark(*view.ranking);
  switch (view.outcome) {
    case SurvivorsEndOutcome::kDied:
      ColoredText(kDeathColor, core::Tr("Vous êtes mort"));
      break;
    case SurvivorsEndOutcome::kVictory:
      ColoredText(kVictoryColor, core::Tr("Victoire !"));
      break;
    case SurvivorsEndOutcome::kAbandoned:
      FaintText(core::Tr("Partie interrompue"));
      break;
  }
  const std::array<char, kSurvivorsClockTextSize> clock = SurvivorsClockText(view.survived_ms);
  ImGui::Text(core::Tr("Temps : %s"), clock.data());
  ImGui::Text(core::Tr("Niveau : %d"), view.level);
  ImGui::Text(core::Tr("Job : %s"), view.job_name);
  ImGui::Text(core::Tr("Zeny : %u"), view.zeny);
}

// La liste défile dans sa hauteur fixe ; ImGui y réserve la gouttière de
// l'ascenseur, que le compte aligné à droite ne franchit pas.
void DrawFoes(const SurvivorsSummaryView& view, const SkinArt& art) {
  ImGui::SeparatorText(core::Tr("Tués par espèce"));
  ImGui::BeginChild("##foes", ImVec2(0.0F, kListHeight));
  for (const SurvivorsFoeRow& foe : view.foes) {
    ImGui::Text("%s", foe.name);
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(kCountSample).x);
    ImGui::Text("%llu", static_cast<unsigned long long>(foe.kills));
  }
  PaintWindowScrollbar(art);
  ImGui::EndChild();
}

void DrawDamage(const SurvivorsSummaryView& view) {
  ImGui::SeparatorText(core::Tr("Dégâts par skill"));
  std::uint64_t most = 0;
  for (const SurvivorsSkillDamageRow& skill : view.skills) {
    most = std::max(most, skill.damage);
  }
  std::array<char, kLineSize> line = {};
  for (const SurvivorsSkillDamageRow& skill : view.skills) {
    if (skill.icon != 0) {
      ImGui::Image(skill.icon, ImVec2(kRowIconSide, kRowIconSide));
    } else {
      ImGui::Dummy(ImVec2(kRowIconSide, kRowIconSide));
    }
    ImGui::SameLine();
    std::snprintf(line.data(), line.size(), "%s — %llu", skill.name,
                  static_cast<unsigned long long>(skill.damage));
    const float share =
        most == 0
            ? 0.0F
            : static_cast<float>(static_cast<double>(skill.damage) / static_cast<double>(most));
    ImGui::ProgressBar(share, ImVec2(-1.0F, kRowIconSide), line.data());
  }
}

// Une illustration ramenée dans le carré d'une carte, son rapport gardé.
void DrawIllustration(ImTextureID illustration, ImVec2 size) {
  if (illustration == 0) {
    ImGui::Dummy(ImVec2(kCardSide, kCardSide));
    return;
  }
  const float wide = size.x > 0.0F ? size.x : kCardSide;
  const float high = size.y > 0.0F ? size.y : kCardSide;
  const float scale = std::min(kCardSide / wide, kCardSide / high);
  ImGui::Image(illustration, ImVec2(wide * scale, high * scale));
}

void DrawCards(const SurvivorsSummaryView& view) {
  ImGui::SeparatorText(core::Tr("Cartes portées"));
  if (view.cards.empty()) {
    FaintText(core::Tr("Aucune carte"));
    return;
  }
  std::array<char, kLineSize> line = {};
  bool first = true;
  for (const SurvivorsCardRow& card : view.cards) {
    if (!first) ImGui::SameLine(0.0F, kCardGap);
    first = false;
    ImGui::BeginGroup();
    DrawIllustration(card.illustration, card.illustration_size);
    std::snprintf(line.data(), line.size(), core::Tr("Lv %d"), card.level);
    ImGui::TextUnformatted(line.data());
    ImGui::EndGroup();
    SkinTooltip(card.name);
  }
}

// Ce que la partie a rapporté au carnet.
void DrawCredit(const SurvivorsCreditRows& credit) {
  ImGui::SeparatorText(core::Tr("Carnet"));
  std::array<char, kLineSize> line = {};
  std::snprintf(line.data(), line.size(), core::Tr("+%s Zeny (solde %s)"),
                SurvivorsZeny(credit.zeny_gained).data(),
                SurvivorsZeny(credit.zeny_balance).data());
  ImGui::TextUnformatted(line.data());
  switch (credit.record) {
    case SurvivorsRecordNews::kPlaced:
      switch (credit.board) {
        case SurvivorsBoardTab::kFree:
          std::snprintf(line.data(), line.size(),
                        core::Tr("Nouveau record ! Rang %d des parties libres"),
                        credit.record_rank);
          break;
        case SurvivorsBoardTab::kChallenge:
          std::snprintf(line.data(), line.size(), core::Tr("Nouveau record ! Rang %d du défi"),
                        credit.record_rank);
          break;
        case SurvivorsBoardTab::kRanked:
          std::snprintf(line.data(), line.size(),
                        core::Tr("Nouveau record ! Rang %d des parties classées"),
                        credit.record_rank);
          break;
      }
      ColoredText(kVictoryColor, line.data());
      break;
    case SurvivorsRecordNews::kBelowBoard:
      if (credit.score_to_beat.has_value()) {
        std::snprintf(line.data(), line.size(), core::Tr("Score à battre : %s"),
                      SurvivorsZeny(*credit.score_to_beat).data());
        FaintText(line.data());
      }
      break;
    case SurvivorsRecordNews::kNotRanked:
      FaintText(core::Tr("Partie interrompue : hors des records."));
      break;
    case SurvivorsRecordNews::kRefused:
      FaintText(core::Tr("Hors des records du défi : départ non neutre ou autre semaine."));
      break;
  }
}

void DrawDropped(const SurvivorsCreditRows& credit) {
  if (credit.dropped.empty()) return;
  ImGui::SeparatorText(core::Tr("Cartes tombées"));
  bool first = true;
  for (const SurvivorsDroppedCardRow& card : credit.dropped) {
    if (!first) ImGui::SameLine(0.0F, kCardGap);
    first = false;
    ImGui::BeginGroup();
    DrawIllustration(card.illustration, card.illustration_size);
    if (card.first_time) ColoredText(kVictoryColor, core::Tr("NOUVELLE"));
    ImGui::EndGroup();
    SkinTooltip(card.name);
  }
}

void DrawFeats(const SurvivorsCreditRows& credit) {
  std::array<char, kLineSize> line = {};
  if (!credit.feats.empty()) ImGui::SeparatorText(core::Tr("Exploits"));
  for (const SurvivorsFeatEntry& feat : credit.feats) {
    if (!feat.reached) {
      ImGui::Text("%s — %s", feat.name, SurvivorsFeatProgressText(feat).data());
      continue;
    }
    std::snprintf(line.data(), line.size(), core::Tr("Exploit atteint : %s"), feat.name);
    if (feat.fresh) {
      ColoredText(kVictoryColor, line.data());
    } else {
      FaintText(line.data());
    }
  }
}

// Les succès gagnés, avec ou sans crédit : un carnet qui ne s'écrit pas ne
// les efface pas.
void DrawTrophies(std::span<const SurvivorsTrophyRow> trophies) {
  if (!trophies.empty()) ImGui::SeparatorText(core::Tr("Succès"));
  for (const SurvivorsTrophyRow& trophy : trophies) {
    if (trophy.icon != 0) {
      ImGui::Image(trophy.icon, ImVec2(kRowIconSide, kRowIconSide));
    } else {
      ImGui::Dummy(ImVec2(kRowIconSide, kRowIconSide));
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(SurvivorsText(trophy.name));
  }
}

}  // namespace

SurvivorsSummaryAsk DrawSurvivorsSummaryWindow(const SurvivorsSummaryView& view,
                                               const SkinArt& art) {
  SurvivorsSummaryAsk asked;
  if (view.phase == SurvivorsEndPhase::kDeathVeil) {
    asked.proceed = DrawBanner(core::Tr("Vous êtes mort"), kDeathColor, view.accepts_input);
    return asked;
  }
  if (view.phase == SurvivorsEndPhase::kVictoryBanner) {
    asked.proceed = DrawBanner(core::Tr("Victoire !"), kBannerVictoryColor, view.accepts_input);
    return asked;
  }
  static WindowName name(kSummaryTitle, kSummaryId);
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
  if (!BeginSkinnedWindow(name.Get(), nullptr, art, SkinnedTitle::kBar, kModalFlags)) {
    EndSkinnedWindow();
    return asked;
  }
  ImGui::Dummy(ImVec2(kSummaryWidth, 0.0F));
  DrawHeadline(view);
  if (view.ranking.has_value()) DrawStanding(*view.ranking);
  DrawFoes(view, art);
  DrawDamage(view);
  DrawCards(view);
  if (view.credit.has_value()) {
    DrawCredit(*view.credit);
    DrawDropped(*view.credit);
    DrawFeats(*view.credit);
  }
  DrawTrophies(view.trophies);
  ImGui::Separator();
  if (!view.accepts_input) {
    ImGui::BeginDisabled();
  }
  const bool replay = SkinButton(core::Tr("Rejouer"), art);
  ImGui::SameLine();
  const bool back = SkinButton(core::Tr("Retour"), art);
  bool church = false;
  if (view.credit.has_value()) {
    ImGui::SameLine();
    church = SkinButton(core::Tr("Église"), art);
  }
  bool ranking = false;
  if (view.ranking.has_value() && view.ranking->ranked && view.ranking->board_reachable) {
    ImGui::SameLine();
    ranking = SkinButton(core::Tr("Classement"), art);
  }
  if (!view.accepts_input) {
    ImGui::EndDisabled();
  }
  EndSkinnedWindow();
  asked.replay = replay && view.accepts_input;
  asked.back = back && view.accepts_input;
  asked.church = church && view.accepts_input;
  if (ranking && view.accepts_input) asked.open_ranking = view.ranking->tab;
  return asked;
}

}  // namespace ro::ui
