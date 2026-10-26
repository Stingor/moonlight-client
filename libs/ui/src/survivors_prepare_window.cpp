#include "ro/ui/survivors_prepare_window.h"

#include <array>
#include <cstdio>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/survivors_ranking_window.h"
#include "ro/ui/survivors_records_window.h"
#include "ro/ui/window_bounds.h"
#include "ro/ui/window_name.h"

#include "survivors_meta_text.h"
#include "survivors_text.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

inline constexpr const char* kPrepareTitle = core::TrText("Survivants de Midgard");

constexpr float kFirstWidth = 420.0F;
constexpr float kFirstHeight = 460.0F;
constexpr float kBoltIconSide = 24.0F;
constexpr ImU32 kRefusalColor = IM_COL32(255, 120, 90, 255);
constexpr ImU32 kOnlineColor = IM_COL32(120, 220, 120, 255);
constexpr std::int64_t kSecondsPerHour = 3'600;
constexpr std::int64_t kHoursPerDay = 24;
constexpr std::size_t kLineSize = 160;

void DrawBoltLine(const SurvivorsBoltChoice& bolt) {
  if (bolt.icon != 0) {
    ImGui::Image(bolt.icon, ImVec2(kBoltIconSide, kBoltIconSide));
    ImGui::SameLine();
  }
}

// Le choix du premier bolt ; l'identifiant cliqué, s'il change.
[[nodiscard]] std::optional<std::uint16_t> DrawBolts(const SurvivorsPrepareView& view,
                                                     const SkinArt& art) {
  std::optional<std::uint16_t> chosen;
  ImGui::SeparatorText(core::Tr("Premier sort"));
  for (const SurvivorsBoltChoice& bolt : view.bolts) {
    ImGui::PushID(bolt.skill_id);
    DrawBoltLine(bolt);
    const bool is_chosen = bolt.skill_id == view.chosen_skill_id;
    if (SkinRadio(bolt.name, is_chosen, art) && !is_chosen) {
      chosen = bolt.skill_id;
    }
    ImGui::Indent();
    FaintText(SurvivorsText(bolt.description));
    ImGui::Unindent();
    ImGui::PopID();
  }
  return chosen;
}

void DrawControls(const SurvivorsPrepareView& view) {
  ImGui::SeparatorText(core::Tr("Contrôles"));
  ImGui::Text(core::Tr("%s %s %s %s ou les flèches : se déplacer"), view.key_north, view.key_west,
              view.key_south, view.key_east);
  ImGui::TextUnformatted(core::Tr("Bouton gauche tenu : marcher vers le curseur"));
  ImGui::TextUnformatted(core::Tr("Les sorts partent seuls."));
  ImGui::TextUnformatted(core::Tr("1 à 4 : choisir une carte au niveau supérieur"));
  ImGui::TextUnformatted(core::Tr("Échap : pause"));
}

// L'onglet classé s'offre-t-il ? Le classement ouvert, et rien qui le grise.
[[nodiscard]] bool RankedOffered(const SurvivorsPrepareMeta& meta) noexcept {
  return meta.ranking.has_value() && meta.ranking->refusal == nullptr;
}

// L'onglet dessiné : le tableau classé retenu sans classement ouvert se
// montre en partie libre, celle qui partira.
[[nodiscard]] SurvivorsBoardTab BoardShown(const SurvivorsPrepareMeta& meta) noexcept {
  if (meta.board == SurvivorsBoardTab::kRanked && !meta.ranking.has_value()) {
    return SurvivorsBoardTab::kFree;
  }
  return meta.board;
}

// L'onglet qui suit sous Tab : libre, défi, puis classé quand il s'offre.
[[nodiscard]] SurvivorsBoardTab NextBoard(const SurvivorsPrepareMeta& meta) noexcept {
  switch (BoardShown(meta)) {
    case SurvivorsBoardTab::kFree:
      return SurvivorsBoardTab::kChallenge;
    case SurvivorsBoardTab::kChallenge:
      return RankedOffered(meta) ? SurvivorsBoardTab::kRanked : SurvivorsBoardTab::kFree;
    case SurvivorsBoardTab::kRanked:
      break;
  }
  return SurvivorsBoardTab::kFree;
}

// Un onglet en bouton d'état ; le clic sur un autre onglet que le retenu le
// demande.
void BoardTab(const char* label, SurvivorsBoardTab tab, const SurvivorsPrepareMeta& meta,
              SurvivorsPrepareAsk& asked, const SkinArt& art) {
  const bool chosen = BoardShown(meta) == tab;
  if (SkinToggleButton(label, chosen, art) && !chosen) asked.choose_board = tab;
}

// Les onglets, en boutons d'état : le tableau retenu appartient à `app`.
void DrawBoardTabs(const SurvivorsPrepareMeta& meta, SurvivorsPrepareAsk& asked,
                   const SkinArt& art) {
  BoardTab(core::Tr("Partie libre"), SurvivorsBoardTab::kFree, meta, asked, art);
  ImGui::SameLine();
  BoardTab(core::Tr("Défi de la semaine"), SurvivorsBoardTab::kChallenge, meta, asked, art);
  if (meta.ranking.has_value()) {
    ImGui::SameLine();
    const bool refused = meta.ranking->refusal != nullptr;
    if (refused) ImGui::BeginDisabled();
    BoardTab(core::Tr("Partie classée"), SurvivorsBoardTab::kRanked, meta, asked, art);
    if (refused) ImGui::EndDisabled();
    SkinTooltip(SurvivorsText(meta.ranking->refusal));
    ColoredText(kOnlineColor, core::Tr("Classement en ligne"));
  }
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
    asked.choose_board = NextBoard(meta);
  }
}

// La Cursed Water : une fiole par rang, de 0 au plus haut, et le bonus du rang.
void DrawCursedWater(const SurvivorsPrepareMeta& meta, SurvivorsPrepareAsk& asked,
                     const SkinArt& art) {
  if (meta.most_cursed_water <= 0) return;
  ImGui::SeparatorText(core::Tr("Cursed Water"));
  std::array<char, kLineSize> line = {};
  for (int rank = 0; rank <= meta.most_cursed_water; ++rank) {
    if (rank > 0) ImGui::SameLine();
    ImGui::PushID(rank);
    std::snprintf(line.data(), line.size(), "%d", rank);
    if (SkinSmallToggleButton(line.data(), rank == meta.cursed_water, art) &&
        rank != meta.cursed_water) {
      asked.choose_cursed_water = rank;
    }
    ImGui::PopID();
  }
  std::snprintf(line.data(), line.size(), core::Tr("Score et Zeny +%d %%, monstres plus durs"),
                meta.cursed_water_bonus_pct);
  FaintText(line.data());
}

void DrawFreeBoard(const SurvivorsPrepareView& view, const SurvivorsPrepareMeta& meta,
                   SurvivorsPrepareAsk& asked, const SkinArt& art) {
  asked.choose_skill_id = DrawBolts(view, art);
  DrawCursedWater(meta, asked, art);
  ImGui::Separator();
  ImGui::Text(core::Tr("Bénédictions : %d rang(s), %d vie(s), %d charge(s)"), meta.blessing_ranks,
              meta.lives, meta.charges);
}

void DrawChallengeBoard(const SurvivorsPrepareMeta& meta) {
  ImGui::TextUnformatted(SurvivorsWeekText(meta.week_year, meta.week_number).data());
  const std::int64_t hours = meta.week_left_s / kSecondsPerHour;
  ImGui::Text(core::Tr("Se termine dans %d j %d h"), static_cast<int>(hours / kHoursPerDay),
              static_cast<int>(hours % kHoursPerDay));
  ImGui::SeparatorText(core::Tr("Sort imposé"));
  DrawBoltLine(meta.challenge_bolt);
  ImGui::TextUnformatted(meta.challenge_bolt.name);
  ImGui::Separator();
  FaintText(core::Tr("Départ neutre : sans Bénédictions, sans charges, toutes les cartes."));
  if (meta.ranking.has_value()) FaintText(core::Tr("Classé si le serveur répond."));
  if (meta.week_best_score.has_value()) {
    ImGui::Text(core::Tr("Meilleur score de la semaine : %s"),
                SurvivorsZeny(*meta.week_best_score).data());
  } else {
    FaintText(core::Tr("Aucune partie cette semaine."));
  }
}

// La partie classée : départ neutre, le bolt au choix, la graine du serveur.
void DrawRankedBoard(const SurvivorsPrepareView& view, const SurvivorsPrepareRanking& ranking,
                     SurvivorsPrepareAsk& asked, const SkinArt& art) {
  asked.choose_skill_id = DrawBolts(view, art);
  ImGui::Separator();
  FaintText(core::Tr("Départ neutre : sans Bénédictions, sans charges, toutes les cartes."));
  FaintText(core::Tr("Graine tirée par le serveur."));
  std::array<char, kLineSize> line = {};
  if (ranking.best_rank > 0) {
    std::snprintf(line.data(), line.size(), core::Tr("Votre meilleur : %s, %s"),
                  SurvivorsRankText(ranking.best_rank).data(),
                  SurvivorsZeny(ranking.best_score).data());
    ImGui::TextUnformatted(line.data());
  } else {
    FaintText(core::Tr("Vous n'êtes pas encore classé."));
  }
}

// Le compteur discret des résultats en attente d'envoi ; au survol, le
// personnage et la date de chacun.
void DrawPending(const SurvivorsPrepareRanking& ranking) {
  if (ranking.pending.empty()) return;
  std::array<char, kLineSize> line = {};
  std::snprintf(line.data(), line.size(), core::Tr("%d résultat(s) en attente d'envoi"),
                static_cast<int>(ranking.pending.size()));
  FaintText(line.data());
  if (!ImGui::IsItemHovered()) return;
  std::string detail;
  for (const SurvivorsPendingRow& row : ranking.pending) {
    if (!detail.empty()) detail += '\n';
    std::snprintf(line.data(), line.size(), "%s — %s", row.character,
                  SurvivorsDateOfUtc(row.ended_at_utc_s).data());
    detail += line.data();
  }
  SkinTooltip(detail.c_str());
}

// Le pied de l'accueil : le solde et les quatre lieux de la méta.
void DrawPlaces(const SurvivorsPrepareMeta& meta, SurvivorsPrepareAsk& asked, const SkinArt& art) {
  ImGui::Separator();
  ImGui::Text(core::Tr("Solde : %s Zeny"), SurvivorsZeny(meta.zeny).data());
  if (SkinSmallButton(core::Tr("Église"), art)) asked.open_place = SurvivorsMetaPlace::kChurch;
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Kafra"), art)) asked.open_place = SurvivorsMetaPlace::kKafra;
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Codex"), art)) asked.open_place = SurvivorsMetaPlace::kCodex;
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Records"), art)) asked.open_place = SurvivorsMetaPlace::kRecords;
  if (!meta.ranking.has_value()) return;
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Classement"), art)) asked.open_ranking = true;
  DrawPending(*meta.ranking);
}

}  // namespace

SurvivorsPrepareAsk DrawSurvivorsPrepareWindow(SurvivorsPrepareState& state,
                                               const SurvivorsPrepareView& view,
                                               const SkinArt& art) {
  SurvivorsPrepareAsk asked;
  if (!state.open) {
    return asked;
  }
  static WindowName name(kPrepareTitle, kSurvivorsPrepareId);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidth, kFirstHeight), ImGuiCond_FirstUseEver);
  ConstrainNextWindowToScreen(name.Get());
  if (!BeginSkinnedWindow(name.Get(), &state.open, art)) {
    EndSkinnedWindow();
    return asked;
  }
  FaintText(core::Tr("Survivez quinze minutes à la horde de Prontera Field."));
  if (!view.meta.has_value()) {
    asked.choose_skill_id = DrawBolts(view, art);
  } else {
    DrawBoardTabs(*view.meta, asked, art);
    switch (BoardShown(*view.meta)) {
      case SurvivorsBoardTab::kFree:
        DrawFreeBoard(view, *view.meta, asked, art);
        break;
      case SurvivorsBoardTab::kChallenge:
        DrawChallengeBoard(*view.meta);
        break;
      case SurvivorsBoardTab::kRanked:
        DrawRankedBoard(view, *view.meta->ranking, asked, art);
        break;
    }
  }
  DrawControls(view);
  if (view.meta.has_value()) DrawPlaces(*view.meta, asked, art);
  ImGui::Separator();
  // Une partie classée que le classement grise ne part pas : sa raison se dit
  // sous « Partir ».
  const char* ranked_refusal =
      view.meta.has_value() && BoardShown(*view.meta) == SurvivorsBoardTab::kRanked
          ? view.meta->ranking->refusal
          : nullptr;
  const bool refused = view.refusal != nullptr || ranked_refusal != nullptr;
  if (refused) {
    ImGui::BeginDisabled();
  }
  bool start = SkinButton(core::Tr("Partir"), art, ImVec2(-1.0F, 0.0F));
  if (refused) {
    ImGui::EndDisabled();
    const char* reason = view.refusal != nullptr ? view.refusal : ranked_refusal;
    ColoredText(kRefusalColor, SurvivorsText(reason));
  }
  if (view.meta.has_value() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
       ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
    start = true;
  }
  EndSkinnedWindow();
  asked.start = start && !refused;
  return asked;
}

}  // namespace ro::ui
