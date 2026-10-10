// Voir l'en-tête.

#include "ro/ui/mercenary_tab.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/companion_ai_window.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/staff_skill_id.h"

namespace ro::ui {
namespace {

// ── LA PALETTE, celle de l'onglet de l'homoncule ────────────────────────────
// Le libellé d'un couple « libellé : valeur », et tout texte secondaire.
constexpr ImU32 kLabelColor = IM_COL32(89, 89, 107, 255);
// La valeur, plus sombre que son libellé.
constexpr ImU32 kValueColor = IM_COL32(26, 26, 33, 255);
// Ce qui détruit.
constexpr ImU32 kRedColor = IM_COL32(153, 31, 31, 255);
// Les jauges, et leur texte blanc à l'ombre portée : il traverse deux fonds de
// luminosité opposée dès qu'une jauge est à moitié pleine.
constexpr ImU32 kHpGaugeColor = IM_COL32(179, 51, 51, 255);
constexpr ImU32 kSpGaugeColor = IM_COL32(51, 89, 179, 255);
constexpr ImU32 kGaugeTextColor = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kGaugeShadowColor = IM_COL32(0, 0, 0, 200);

// ── LES MESURES, en pixels d'interface ──────────────────────────────────────
constexpr float kGaugePixels = 15.0F;
// Le texte d'une jauge remonte : le centrage prend la boîte de la police, dont
// le jambage descendant fait paraître le texte trop bas.
constexpr float kGaugeTextLiftPixels = 2.0F;
constexpr float kGaugeShadowPixels = 1.0F;
constexpr float kDismissButtonPixels = 100.0F;
constexpr float kCancelButtonPixels = 70.0F;
constexpr float kRowIconPixels = 20.0F;
constexpr float kDragIconPixels = 24.0F;
constexpr float kPercent = 100.0F;

// Les statistiques vont par paires « libellé / valeur » sur chaque ligne.
constexpr int kStatColumns = 4;
constexpr int kStatsPerRow = 2;

float Px(float pixels) { return pixels * ImGui::GetStyle().FontScaleMain; }

void TextColored(ImU32 color, const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::TextUnformatted(text);
  ImGui::PopStyleColor();
}

void WrappedColored(ImU32 color, const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::TextWrapped("%s", text);
  ImGui::PopStyleColor();
}

// L'infobulle du dernier élément, même grisé : c'est souvent elle qui dit
// POURQUOI il l'est.
void Tooltip(const char* text) {
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltipFormat("%s", text);
  }
}

std::string_view BufferText(const std::array<char, kMercenaryConfirmSize>& buffer) {
  const auto end = std::ranges::find(buffer, '\0');
  return {buffer.data(), static_cast<std::size_t>(end - buffer.begin())};
}

// UNE JAUGE : le libellé s'écrit par-dessus, centré, et non à la mode d'ImGui
// qui le pose au bord du remplissage, sur le fond sombre où il se perd.
void Gauge(const char* label, int current, int most, ImU32 color) {
  const float ratio =
      most > 0 ? std::clamp(static_cast<float>(current) / static_cast<float>(most), 0.0F, 1.0F)
               : 0.0F;
  std::array<char, 96> text = {};
  if (most > 0) {
    std::snprintf(text.data(), text.size(), "%s  %d / %d  (%.1f %%)", label, current, most,
                  static_cast<double>(ratio * kPercent));
  } else {
    std::snprintf(text.data(), text.size(), "%s  %d", label, current);
  }
  ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
  ImGui::ProgressBar(ratio, ImVec2(-1.0F, Px(kGaugePixels)), "");
  ImGui::PopStyleColor();
  const ImVec2 low = ImGui::GetItemRectMin();
  const ImVec2 high = ImGui::GetItemRectMax();
  const ImVec2 size = ImGui::CalcTextSize(text.data());
  const ImVec2 at((low.x + high.x - size.x) * 0.5F,
                  ((low.y + high.y - size.y) * 0.5F) - Px(kGaugeTextLiftPixels));
  ImDrawList* into = ImGui::GetWindowDrawList();
  const float drop = Px(kGaugeShadowPixels);
  into->AddText(ImVec2(at.x + drop, at.y + drop), kGaugeShadowColor, text.data());
  into->AddText(at, kGaugeTextColor, text.data());
}

// « 1 h 05 min », « 12 min 30 s », « 45 s ».
std::string ContractText(std::uint32_t seconds) {
  constexpr std::uint32_t kSecondsPerMinute = 60;
  constexpr std::uint32_t kSecondsPerHour = 3600;
  std::array<char, 48> text = {};
  const auto hours = static_cast<int>(seconds / kSecondsPerHour);
  const auto minutes = static_cast<int>((seconds % kSecondsPerHour) / kSecondsPerMinute);
  const auto rest = static_cast<int>(seconds % kSecondsPerMinute);
  if (hours > 0) {
    std::snprintf(text.data(), text.size(), core::Tr("%d h %02d min"), hours, minutes);
  } else if (minutes > 0) {
    std::snprintf(text.data(), text.size(), core::Tr("%d min %02d s"), minutes, rest);
  } else {
    std::snprintf(text.data(), text.size(), core::Tr("%d s"), rest);
  }
  return text.data();
}

// « Libellé : valeur », le libellé plus sombre que la valeur, comme la ligne
// d'intimité de l'homoncule. Le libellé traduit porte ses deux-points, que
// chaque langue place à sa façon.
void LabelledLine(const char* label, const std::string& value) {
  TextColored(kValueColor, label);
  ImGui::SameLine();
  ImGui::TextUnformatted(value.c_str());
}

// ── EN-TÊTE ─────────────────────────────────────────────────────────────────
void DrawHeader(MercenaryTabState& state, const MercenaryTabView& view, const SkinArt& art) {
  const std::string shown = view.name.empty() ? std::string("?") : std::string(view.name);
  ImGui::AlignTextToFramePadding();
  TextColored(kValueColor, shown.c_str());
  // « Renvoyer » collé au bord droit : il est IRRÉVERSIBLE.
  const float width = Px(kDismissButtonPixels);
  ImGui::SameLine();
  const float avail = ImGui::GetContentRegionAvail().x;
  if (avail > width) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - width);
  }
  if (SkinSmallButton(core::Tr("Renvoyer…"), art, ImVec2(width, 0.0F))) {
    state.dismiss_asked = true;
    state.dismiss_confirm.fill('\0');
  }
  // Espèce et niveau sur une ligne.
  std::array<char, 128> line = {};
  const std::string kind(view.kind);
  std::snprintf(line.data(), line.size(), "%s  ·  %s %d", kind.c_str(), core::Tr("Niveau"),
                view.level);
  TextColored(kLabelColor, line.data());
  ImGui::Separator();
}

// ── JAUGES ET CONTRAT ───────────────────────────────────────────────────────
void DrawGaugesAndContract(const MercenaryTabView& view) {
  Gauge(core::Tr("PV"), view.hp, view.max_hp, kHpGaugeColor);
  Gauge(core::Tr("SP"), view.sp, view.max_sp, kSpGaugeColor);
  TextColored(kValueColor, core::Tr("Expiration :"));
  ImGui::SameLine();
  const std::string left = ContractText(view.contract_left_s);
  if (view.contract_ending) {
    TextColored(kRedColor, left.c_str());
  } else {
    ImGui::TextUnformatted(left.c_str());
  }
  if (!view.expires_at.empty()) {
    const std::string tip =
        std::string(core::Tr("Fin du contrat :")) + " " + std::string(view.expires_at);
    Tooltip(tip.c_str());
  }
  LabelledLine(core::Tr("Fidélité :"), std::to_string(view.faith));
  LabelledLine(core::Tr("Appels :"), std::to_string(view.calls));
  LabelledLine(core::Tr("Victimes :"), std::to_string(view.kills));
}

// ── LA CONFIRMATION DU RENVOI ───────────────────────────────────────────────
void DrawDismissConfirmation(MercenaryTabState& state, const MercenaryTabView& view,
                             const SkinArt& art, MercenaryTabAction& action) {
  if (!state.dismiss_asked) {
    return;
  }
  ImGui::Spacing();
  WrappedColored(kRedColor, core::Tr("Supprimer un mercenaire efface aussi son historique de "
                                     "progression. Veux-tu vraiment le supprimer ?"));
  WrappedColored(kValueColor, core::Tr("Retape le nom du mercenaire pour confirmer :"));
  ImGui::SetNextItemWidth(-1.0F);
  ImGui::InputText("##merc_dismiss", state.dismiss_confirm.data(), state.dismiss_confirm.size());
  const bool matches = !view.name.empty() && BufferText(state.dismiss_confirm) == view.name;
  ImGui::BeginDisabled(!matches);
  if (SkinSmallButton(core::Tr("Renvoyer"), art, ImVec2(Px(kDismissButtonPixels), 0.0F))) {
    action.dismiss = true;
    state.status = core::Tr("Renvoi envoyé.");
    state.dismiss_asked = false;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Annuler"), art, ImVec2(Px(kCancelButtonPixels), 0.0F))) {
    state.dismiss_asked = false;
  }
}

// ── STATISTIQUES ────────────────────────────────────────────────────────────
struct StatRow {
  const char* label = nullptr;
  int value = 0;
  // Vide pour une ligne sans infobulle.
  std::string tip;
  // La valeur EST le pourcentage appliqué : elle porte son unité.
  bool percent = false;
};

void DrawStats(const MercenaryTabView& view) {
  std::array<char, 192> aspd_tip = {};
  std::snprintf(aspd_tip.data(), aspd_tip.size(),
                core::Tr("Vitesse d'attaque = (2000 - amotion) / 10, calculée par le client.\n"
                         "Délai d'attaque brut : %d ms — c'est LUI que le serveur envoie."),
                view.amotion_ms);
  const std::array<StatRow, kStatColumns * kStatsPerRow> rows = {
      StatRow{
          .label = "ATK",
          .value = view.atk,
          .tip = core::Tr("Tirée au hasard par le serveur dans la fourchette de l'arme, à "
                          "chaque envoi :\ndeux lectures successives ne la donnent pas égale.")},
      StatRow{.label = "MATK", .value = view.matk, .tip = core::Tr("Attaque magique maximale.")},
      StatRow{.label = "HIT", .value = view.hit},
      StatRow{.label = "CRI", .value = view.crit, .tip = core::Tr("Taux de critique.")},
      StatRow{.label = "DEF",
              .value = view.def,
              .tip = core::Tr("DEF dure seule, en POURCENTAGE (dégâts x (100 - DEF) / 100)."),
              .percent = true},
      StatRow{.label = "MDEF",
              .value = view.mdef,
              .tip = core::Tr(
                  "MDEF dure seule, en POURCENTAGE (dégâts magiques x (100 - MDEF) / 100)."),
              .percent = true},
      StatRow{.label = "FLEE", .value = view.flee},
      StatRow{.label = "ASPD", .value = view.aspd, .tip = aspd_tip.data()},
  };
  if (ImGui::BeginTable("merc_stats", kStatColumns,
                        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
    // Les libellés prennent la largeur du texte, les valeurs le reste.
    ImGui::TableSetupColumn("l0", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("v0", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("l1", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("v1", ImGuiTableColumnFlags_WidthStretch);
    int rank = 0;
    for (const StatRow& row : rows) {
      if (rank % kStatsPerRow == 0) {
        ImGui::TableNextRow();
      }
      ++rank;
      ImGui::TableNextColumn();
      TextColored(kLabelColor, row.label);
      if (!row.tip.empty()) {
        Tooltip(row.tip.c_str());
      }
      // La valeur alignée à DROITE de sa moitié, collée au libellé suivant.
      ImGui::TableNextColumn();
      const std::string value = std::to_string(row.value) + (row.percent ? " %" : "");
      const float gap = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(value.c_str()).x;
      if (gap > 0.0F) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + gap);
      }
      ImGui::TextUnformatted(value.c_str());
      if (!row.tip.empty()) {
        Tooltip(row.tip.c_str());
      }
    }
    ImGui::EndTable();
  }
  std::array<char, 96> range = {};
  std::snprintf(range.data(), range.size(), core::Tr("Portée d'attaque : %d"), view.range_cells);
  TextColored(kLabelColor, range.data());
  Tooltip(
      core::Tr("En cases. 1 = corps à corps ; au-delà, le mercenaire frappe à distance sans "
               "se déplacer."));
}

// ── COMPÉTENCES ─────────────────────────────────────────────────────────────

// Les gestes s'attachent au DERNIER élément soumis : rappelés après l'icône ET
// après le nom, pour que la cellule entière réagisse.
void SkillGestures(const MercenarySkillRow& skill, bool dragging, bool staff_allowed,
                   MercenaryTabAction& action) {
  // Glisser vers une barre ce qui part d'ici : une case qui ne partirait jamais
  // n'a rien à faire dans une barre. `SourceAllowNullID` parce que ni l'image
  // ni le texte ne portent d'identifiant.
  if (skill.castable && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
    const SkillDrag carried{.skill_id = skill.skill_id, .level = skill.level};
    ImGui::SetDragDropPayload(kSkillDragPayload, &carried, sizeof(carried));
    if (skill.icon.usable()) {
      ImGui::Image(skill.icon.id, ImVec2(Px(kDragIconPixels), Px(kDragIconPixels)));
      ImGui::SameLine();
    }
    const std::string name(skill.name);
    ImGui::TextUnformatted(name.c_str());
    ImGui::EndDragDropSource();
  }
  if (!ImGui::IsItemHovered()) {
    return;
  }
  // Rien ne se déclenche pendant un glisser : relâcher sur la barre lancerait
  // AUSSI la compétence.
  if (!dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
    action.describe_skill_id = skill.skill_id;
  }
  if (skill.castable && !dragging && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    action.cast = MercenarySkillCast{.skill_id = skill.skill_id, .level = skill.level};
  }
  // Une seule infobulle : nature, coût, portée, puis les gestes possibles.
  const std::string sp = skill.sp_cost > 0 ? std::to_string(skill.sp_cost) : std::string("-");
  const std::string range =
      skill.range_cells > 0 ? std::to_string(skill.range_cells) : std::string("-");
  std::array<char, 384> tip = {};
  if (skill.castable && skill.needs_aim) {
    std::snprintf(tip.data(), tip.size(),
                  core::Tr("%s  ·  SP %s  ·  portée %s\n"
                           "Double-clic : lancer  ·  Glisser : poser dans la barre\n"
                           "Ce skill vise une cible ou une case : le clic suivant la désigne.\n"
                           "Clic droit : description"),
                  core::Tr("actif"), sp.c_str(), range.c_str());
  } else if (skill.castable) {
    std::snprintf(tip.data(), tip.size(),
                  core::Tr("%s  ·  SP %s  ·  portée %s\n"
                           "Double-clic : lancer  ·  Glisser : poser dans la barre\n"
                           "Clic droit : description"),
                  core::Tr("actif"), sp.c_str(), range.c_str());
  } else {
    std::snprintf(tip.data(), tip.size(),
                  core::Tr("%s  ·  SP %s  ·  portée %s\nClic droit : description"),
                  skill.active ? core::Tr("actif") : core::Tr("passif"), sp.c_str(), range.c_str());
  }
  SkinTooltipWithStaffSkillId(tip.data(), staff_allowed,
                              StaffSkillId{.skill_id = skill.skill_id, .idname = skill.idname});
}

void DrawSkills(MercenaryTabState& state, const MercenaryTabView& view,
                MercenaryTabAction& action) {
  // Le retour de l'en-tête fait foi : posé à chaque image, sans quoi ImGui
  // garderait son propre état et le nôtre ne piloterait rien.
  ImGui::SetNextItemOpen(state.skills_open);
  state.skills_open = ImGui::CollapsingHeader(core::Tr("Skills###merc_skills"));
  if (!state.skills_open) {
    return;
  }
  if (view.skills.empty()) {
    TextColored(kLabelColor, core::Tr("Aucun skill reçu du serveur."));
    return;
  }
  if (ImGui::BeginChild("merc_skills", ImVec2(0.0F, 0.0F), ImGuiChildFlags_Borders)) {
    if (ImGui::BeginTable("merc_skill_rows", 2,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                              ImGuiTableFlags_SizingFixedFit)) {
      ImGui::TableSetupColumn(core::Tr("Skill"), ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn(core::Tr("Niv"));
      ImGui::TableHeadersRow();
      for (const MercenarySkillRow& skill : view.skills) {
        ImGui::PushID(skill.skill_id);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        // Calculé AVANT tout dessin : l'icône et le nom doivent voir le même.
        const bool dragging = ImGui::GetDragDropPayload() != nullptr;
        if (skill.icon.usable()) {
          ImGui::Image(skill.icon.id, ImVec2(Px(kRowIconPixels), Px(kRowIconPixels)));
          SkillGestures(skill, dragging, view.staff_allowed, action);
          ImGui::SameLine();
        }
        const std::string name(skill.name);
        ImGui::TextUnformatted(name.c_str());
        SkillGestures(skill, dragging, view.staff_allowed, action);
        ImGui::TableNextColumn();
        ImGui::Text("%d", skill.level);
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
  }
  ImGui::EndChild();
}

}  // namespace

MercenaryTabAction DrawMercenaryTab(MercenaryTabState& state, const MercenaryTabView& view,
                                    const SkinArt& art) {
  MercenaryTabAction action;
  DrawHeader(state, view, art);
  DrawGaugesAndContract(view);
  DrawDismissConfirmation(state, view, art, action);
  if (!state.status.empty()) {
    TextColored(kLabelColor, state.status.c_str());
  }
  switch (DrawCompanionAiLine(view.ai_profile, view.ai_proposal, art)) {
    case CompanionAiLineVerb::kOpen:
      action.open_ai = true;
      break;
    case CompanionAiLineVerb::kAcceptProposal:
      action.ai_proposal_answer = true;
      break;
    case CompanionAiLineVerb::kDeclineProposal:
      action.ai_proposal_answer = false;
      break;
    case CompanionAiLineVerb::kNothing:
      break;
  }
  ImGui::Spacing();
  ImGui::Separator();
  DrawStats(view);
  ImGui::Spacing();
  ImGui::Separator();
  DrawSkills(state, view, action);
  return action;
}

}  // namespace ro::ui
