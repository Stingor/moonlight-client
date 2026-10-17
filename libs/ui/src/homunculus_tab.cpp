// L'ONGLET HOMONCULE : l'en-tête et son nom-bouton, les quatre jauges,
// l'intimité, nourrir et l'auto-alimentation, l'abandon confirmé, les huit
// statistiques, la section « Staff » sous son droit, puis la liste repliable
// des compétences.
//
// Tout l'onglet tient dans la fiche à sa largeur la plus étroite : libellés
// courts, et boutons ajustés à leur texte plutôt qu'à une largeur ronde.

#include "ro/ui/homunculus_tab.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <initializer_list>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/companion_ai_window.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/staff_skill_id.h"

#include "homunculus_staff.h"
#include "ui_scale.h"

namespace ro::ui {
namespace {

// ── LA PALETTE, réglée pour le corps clair ──────────────────────────────────
constexpr ImU32 kLabelColor = IM_COL32(89, 89, 107, 255);
constexpr ImU32 kValueColor = IM_COL32(26, 26, 33, 255);
constexpr ImU32 kGreenColor = IM_COL32(26, 128, 38, 255);
constexpr ImU32 kRedColor = IM_COL32(153, 31, 31, 255);
constexpr ImU32 kGaugeTextColor = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kGaugeShadowColor = IM_COL32(0, 0, 0, 200);
// Les couleurs des jauges.
constexpr ImU32 kHpGaugeColor = IM_COL32(179, 51, 51, 255);
constexpr ImU32 kSpGaugeColor = IM_COL32(51, 89, 179, 255);
constexpr ImU32 kExpGaugeColor = IM_COL32(140, 115, 38, 255);
constexpr ImU32 kFedGaugeColor = IM_COL32(89, 153, 64, 255);
constexpr ImU32 kHungryGaugeColor = IM_COL32(191, 51, 51, 255);

// ── LES MESURES, en pixels d'interface ──────────────────────────────────────
constexpr float kGaugePixels = 15.0F;
// Le texte d'une jauge remonte un peu : le centrage d'ImGui prend la boîte de
// la police, dont le jambage descendant fait paraître le texte trop bas.
constexpr float kGaugeTextLiftPixels = 2.0F;
constexpr float kGaugeShadowPixels = 1.0F;
constexpr float kRenameButtonPixels = 60.0F;
constexpr int kRenameButtons = 2;
constexpr float kAbandonButtonPixels = 100.0F;
constexpr float kFeedButtonPixels = 62.0F;
constexpr float kConfirmCancelPixels = 70.0F;
constexpr float kSkillIconPixels = 20.0F;
constexpr float kDragIconPixels = 24.0F;
constexpr float kRaiseButtonPixels = 22.0F;
constexpr float kPercent = 100.0F;
// Satiété à partir de laquelle la jauge rougit et l'alerte s'écrit.
constexpr int kHungryHunger = 10;
// La satiété se lit sur cent, l'intimité sur mille : les échelles du serveur.
constexpr int kHomunculusHungerGaugeMost = 100;
constexpr int kHomunculusIntimacyGaugeMost = 1000;
constexpr int kStatColumns = 4;
constexpr int kStatRows = 8;
constexpr int kSkillColumns = 3;

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
// pourquoi il l'est.
void Tooltip(const char* text) {
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltipFormat("%s", text);
  }
}

// UNE INFOBULLE DE PLUSIEURS LIGNES : chaque ligne est sa propre entrée du
// catalogue de traduction.
[[nodiscard]] std::string Lines(std::initializer_list<const char*> lines) {
  std::string joined;
  for (const char* line : lines) {
    if (!joined.empty()) {
      joined += '\n';
    }
    joined += line;
  }
  return joined;
}

[[nodiscard]] std::string_view BufferText(std::span<const char> buffer) {
  const auto end = std::ranges::find(buffer, '\0');
  return {buffer.data(), static_cast<std::size_t>(end - buffer.begin())};
}

void CopyIntoBuffer(std::span<char> buffer, std::string_view text) {
  const std::size_t kept = std::min(text.size(), buffer.size() - 1);
  std::copy_n(text.begin(), kept, buffer.begin());
  buffer[kept] = '\0';
}

// Le nom montré : celui de l'homoncule, ou à défaut celui de son espèce.
[[nodiscard]] std::string Shown(const HomunculusTabView& view) {
  if (!view.name.empty()) {
    return std::string(view.name);
  }
  return view.species.empty() ? std::string("?") : std::string(view.species);
}

const char* IntimacyText(HomunculusIntimacyLabel label) {
  switch (label) {
    case HomunculusIntimacyLabel::kHateWithPassion:
      return core::Tr("Haine profonde");
    case HomunculusIntimacyLabel::kHate:
      return core::Tr("Haine");
    case HomunculusIntimacyLabel::kAwkward:
      return core::Tr("Distant");
    case HomunculusIntimacyLabel::kShy:
      return core::Tr("Timide");
    case HomunculusIntimacyLabel::kNeutral:
      return core::Tr("Neutre");
    case HomunculusIntimacyLabel::kCordial:
      return core::Tr("Cordial");
    case HomunculusIntimacyLabel::kLoyal:
      return core::Tr("Loyal");
    case HomunculusIntimacyLabel::kUnknown:
      break;
  }
  return core::Tr("Inconnue");
}

// UNE JAUGE : la barre, puis son texte dessiné à la main — blanc plein, ombré,
// centré. Celui d'ImGui suit le bord du remplissage et finit, illisible, sur le
// fond sombre de la barre.
void Gauge(const char* label, std::int64_t current, std::int64_t most, ImU32 color) {
  std::array<char, 96> text = {};
  const float ratio =
      most > 0
          ? std::clamp(static_cast<float>(static_cast<double>(current) / static_cast<double>(most)),
                       0.0F, 1.0F)
          : 0.0F;
  if (most > 0) {
    std::snprintf(text.data(), text.size(), "%s  %lld / %lld  (%.1f %%)", label,
                  static_cast<long long>(current), static_cast<long long>(most),
                  static_cast<double>(ratio * kPercent));
  } else {
    std::snprintf(text.data(), text.size(), "%s  %lld", label, static_cast<long long>(current));
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
  const float shadow = Px(kGaugeShadowPixels);
  into->AddText(ImVec2(at.x + shadow, at.y + shadow), kGaugeShadowColor, text.data());
  into->AddText(at, kGaugeTextColor, text.data());
}

void DrawRenaming(HomunculusTabState& state, const SkinArt& art, HomunculusTabAction& action) {
  const float full = ImGui::GetContentRegionAvail().x;
  // Les deux boutons et leurs espacements : le champ prend tout le reste.
  ImGui::SetNextItemWidth(full - (Px(kRenameButtonPixels) * kRenameButtons) -
                          ((kRenameButtons + 1) * ImGui::GetStyle().ItemSpacing.x));
  ImGui::InputText("##homun_name", state.new_name.data(), state.new_name.size());
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Valider"), art, ImVec2(Px(kRenameButtonPixels), 0.0F)) &&
      state.new_name[0] != '\0') {
    action.verb = HomunculusVerb::kRename;
    action.name = std::string(BufferText(state.new_name));
    state.status = core::Tr("Renommage envoyé.");
    state.renaming = false;
  }
  ImGui::SameLine();
  if (SkinSmallButton((std::string(core::Tr("Annuler")) + "##homun_rename_cancel").c_str(), art,
                      ImVec2(Px(kRenameButtonPixels), 0.0F))) {
    state.renaming = false;
  }
  WrappedColored(kLabelColor, core::Tr("Donne à ton homoncule un nom de 23 lettres au plus."));
}

// L'EN-TÊTE : le nom est le bouton de renommage tant que le serveur l'accepte,
// et « Abandonner… », irréversible, se tient au bord droit, loin de la case
// qu'on coche sans y penser.
void DrawHeader(HomunculusTabState& state, const HomunculusTabView& view, const SkinArt& art,
                HomunculusTabAction& action) {
  if (state.renaming) {
    DrawRenaming(state, art, action);
  } else {
    const std::string shown = Shown(view);
    if (!view.renamed) {
      // Le suffixe garde l'identifiant du bouton quand le nom change.
      const std::string button = shown + "##homun_rename";
      ImGui::BeginDisabled(!view.active());
      if (SkinSmallButton(button.c_str(), art)) {
        CopyIntoBuffer(state.new_name, view.name);
        state.renaming = true;
      }
      ImGui::EndDisabled();
      Tooltip(view.active() ? core::Tr("Cliquer pour renommer.")
                            : core::Tr("Impossible : l'homoncule est au repos ou hors de combat."));
    } else {
      ImGui::AlignTextToFramePadding();
      TextColored(kValueColor, shown.c_str());
      Tooltip(core::Tr("Le serveur n'accepte le renommage qu'UNE fois."));
    }
    const float width = Px(kAbandonButtonPixels);
    ImGui::SameLine();
    const float room = ImGui::GetContentRegionAvail().x;
    if (room > width) {
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + room - width);
    }
    ImGui::BeginDisabled(!view.active());
    if (SkinSmallButton(core::Tr("Abandonner…"), art, ImVec2(width, 0.0F))) {
      state.delete_asked = true;
      state.delete_confirm[0] = '\0';
    }
    ImGui::EndDisabled();
    if (!view.active()) {
      Tooltip(core::Tr("Impossible : l'homoncule est au repos ou hors de combat."));
    }
  }

  // Espèce, niveau et état sur une ligne.
  std::array<char, 128> line = {};
  std::snprintf(line.data(), line.size(), "%s  ·  %s %d",
                view.species.empty() ? "?" : std::string(view.species).c_str(), core::Tr("Niveau"),
                view.level);
  TextColored(kLabelColor, line.data());
  if (view.resting || !view.alive) {
    ImGui::SameLine();
    if (view.resting) {
      TextColored(kLabelColor, core::Tr("Au repos."));
    } else {
      TextColored(kRedColor, core::Tr("Hors de combat (0 PV)."));
    }
  }
  ImGui::Separator();
}

void DrawGauges(const HomunculusTabView& view) {
  Gauge(core::Tr("PV"), view.hp, view.max_hp, kHpGaugeColor);
  Gauge(core::Tr("SP"), view.sp, view.max_sp, kSpGaugeColor);
  // L'expérience courante ET la requise : la jauge seule ne dit que le rapport.
  if (view.exp_next > 0) {
    Gauge("EXP", view.exp, view.exp_next, kExpGaugeColor);
  } else {
    std::array<char, 96> line = {};
    std::snprintf(line.data(), line.size(), "EXP : %lld  —  %s", static_cast<long long>(view.exp),
                  core::Tr("niveau maximum"));
    TextColored(kGreenColor, line.data());
  }
  // Satiété : sur cent, comme le serveur.
  const bool hungry = view.hunger <= kHungryHunger;
  Gauge(core::Tr("Satiété"), view.hunger, kHomunculusHungerGaugeMost,
        hungry ? kHungryGaugeColor : kFedGaugeColor);
  if (hungry) {
    WrappedColored(kRedColor, core::Tr("Ton homoncule meurt de faim. Nourris-le, sinon il te "
                                       "quittera."));
  }
  // Intimité : le palier suffit, la valeur brute passe en infobulle.
  std::array<char, 64> label = {};
  std::snprintf(label.data(), label.size(), "%s :", core::Tr("Intimité"));
  TextColored(kValueColor, label.data());
  ImGui::SameLine();
  ImGui::TextUnformatted(IntimacyText(view.intimacy_label));
  std::array<char, 48> raw = {};
  std::snprintf(raw.data(), raw.size(), "%d / %d", view.intimacy, kHomunculusIntimacyGaugeMost);
  Tooltip(raw.data());
}

void DrawActions(HomunculusTabState& state, const HomunculusTabView& view, const SkinArt& art,
                 HomunculusTabAction& action) {
  // Le serveur refuse de nourrir un homoncule qui n'est pas actif.
  ImGui::BeginDisabled(!view.active());
  if (SkinSmallButton(core::Tr("Nourrir"), art, ImVec2(Px(kFeedButtonPixels), 0.0F))) {
    action.verb = HomunculusVerb::kFeed;
    state.status = core::Tr("Demande de nourrissage envoyée.");
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  // ⚠ LA CASE MONTRE CE QUE LE SERVEUR A ACCUSÉ : un clic demande, l'écho
  // change. Au repos, le serveur se tait.
  bool auto_feed = view.auto_feed;
  ImGui::BeginDisabled(view.resting);
  if (SkinCheckBox(core::Tr("Alimentation automatique"), &auto_feed, art)) {
    action.verb = HomunculusVerb::kAutoFeed;
    action.auto_feed = auto_feed;
  }
  ImGui::EndDisabled();
  Tooltip(core::Tr("L'homoncule reçoit le dixième de l'expérience du maître."));

  if (state.delete_asked) {
    ImGui::Spacing();
    WrappedColored(kRedColor,
                   core::Tr("Tu as décidé de supprimer cet homoncule. Une fois supprimé, "
                            "l'homoncule et son histoire seront effacés et ne pourront jamais être "
                            "restaurés. Veux-tu continuer ?"));
    WrappedColored(kValueColor, core::Tr("Retape le nom de l'homoncule pour confirmer :"));
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputText("##homun_del", state.delete_confirm.data(), state.delete_confirm.size());
    const std::string_view expected = view.name.empty() ? view.species : view.name;
    const bool confirmed = !expected.empty() && BufferText(state.delete_confirm) == expected;
    ImGui::BeginDisabled(!confirmed || !view.active());
    if (SkinSmallButton(core::Tr("Abandonner"), art, ImVec2(Px(kAbandonButtonPixels), 0.0F))) {
      action.verb = HomunculusVerb::kDelete;
      state.status = core::Tr("Suppression envoyée.");
      state.delete_asked = false;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (SkinSmallButton((std::string(core::Tr("Annuler")) + "##homun_del_cancel").c_str(), art,
                        ImVec2(Px(kConfirmCancelPixels), 0.0F))) {
      state.delete_asked = false;
    }
  }
  if (!state.status.empty()) {
    TextColored(kLabelColor, state.status.c_str());
  }
}

// LES HUIT STATISTIQUES, dans l'ordre de la colonne du client. Deux d'entre
// elles ne sont pas ce qu'elles semblent : leurs infobulles le disent.
void DrawStats(const HomunculusTabView& view) {
  if (!ImGui::BeginTable("cs_homun_stats", kStatColumns,
                         ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
    return;
  }
  ImGui::TableSetupColumn("l0", ImGuiTableColumnFlags_WidthFixed);
  ImGui::TableSetupColumn("v0", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("l1", ImGuiTableColumnFlags_WidthFixed);
  ImGui::TableSetupColumn("v1", ImGuiTableColumnFlags_WidthStretch);
  // ASPD : absente du paquet, le client la calcule — son infobulle porte le
  // délai brut que le serveur envoie.
  std::array<char, 128> amotion = {};
  std::snprintf(amotion.data(), amotion.size(),
                core::Tr("Délai d'attaque brut : %d ms — c'est LUI que le serveur envoie."),
                view.amotion_ms);
  struct Row {
    const char* label;
    int value;
    std::string tip;
    // Seule MDEF porte son unité : c'est le seul nombre qui EST le pourcentage
    // appliqué.
    bool percent;
  };
  const std::array<Row, kStatRows> rows = {{
      {"ATK", view.atk,
       Lines({core::Tr("Attaque de base + attaque d'arme (batk + atk2), plafonnée à 32767."),
              core::Tr("C'est le dégât AVANT défense de la cible.")}),
       false},
      {"MATK", view.matk,
       Lines({core::Tr("Attaque magique MAXIMALE. Sur ce serveur HOMSET_SAME_MATK est actif :"),
              core::Tr("le minimum est égal au maximum, il n'y a donc pas de fourchette.")}),
       false},
      {"HIT", view.hit,
       Lines({core::Tr("Précision = min(niveau, 600) + DEX."),
              core::Tr("Chance de toucher = 80 + HIT - FLEE de la cible, bornée à 5..100 %.")}),
       false},
      {"CRI", view.crit,
       Lines({core::Tr("/!\\ CE N'EST PAS UN TAUX DE CRITIQUE. Le serveur envoie ici LUK/3 + 1"),
              core::Tr("(option HOMSET_DISPLAY_LUK). Un homoncule ne fait JAMAIS de critique :"),
              core::Tr("enable_critical ne couvre que les joueurs et les mercenaires."),
              core::Tr("À lire comme un indicateur de LUK — donc ici LUK vaut environ (valeur - "
                       "1) x 3.")}),
       false},
      {"DEF", view.def,
       Lines({core::Tr("Somme de DEUX choses de natures différentes : DEF dure + VIT."),
              core::Tr("• DEF dure = réduction en POURCENTAGE (dégâts x (100 - DEF) / 100)."),
              core::Tr("• VIT alimente la DEF douce, une réduction PLATE soustraite ensuite,"),
              core::Tr("avec une part aléatoire : DEF2 + rnd(0, (DEF2/20)²)."),
              core::Tr("Le nombre affiché ne se lit donc ni en % ni en plat — c'est un total"),
              core::Tr("d'affichage hérité d'Aegis.")}),
       false},
      {"MDEF", view.mdef,
       Lines({core::Tr("MDEF dure SEULE, en POURCENTAGE (dégâts magiques x (100 - MDEF) / 100)."),
              core::Tr("/!\\ La MDEF douce (INT + VIT/2), soustraite à plat après, n'est PAS"),
              core::Tr("comptée ici — la résistance réelle est supérieure à ce chiffre.")}),
       true},
      {"FLEE", view.flee,
       Lines({core::Tr("Esquive = min(niveau, 600) + AGI. Elle se retranche directement de la"),
              core::Tr("précision de l'attaquant. -10 % par assaillant au-delà du deuxième."),
              core::Tr("L'esquive parfaite (LUK) n'est pas transmise pour un homoncule.")}),
       false},
      {"ASPD", view.aspd,
       Lines({core::Tr("Vitesse d'attaque = (2000 - amotion) / 10, calculée par le client."),
              amotion.data()}),
       false},
  }};
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const Row& row = rows[index];
    if (index % 2 == 0) {
      ImGui::TableNextRow();
    }
    // Libellé collé à sa valeur, valeur alignée à droite de sa moitié.
    ImGui::TableNextColumn();
    TextColored(kLabelColor, row.label);
    Tooltip(row.tip.c_str());
    ImGui::TableNextColumn();
    std::array<char, 16> value = {};
    std::snprintf(value.data(), value.size(), row.percent ? "%d %%" : "%d", row.value);
    const float slack = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(value.data()).x;
    if (slack > 0.0F) {
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + slack);
    }
    ImGui::TextUnformatted(value.data());
    Tooltip(row.tip.c_str());
  }
  ImGui::EndTable();
  std::array<char, 128> range = {};
  std::snprintf(range.data(), range.size(), core::Tr("Portée d'attaque : %d"),
                view.attack_range_cells);
  TextColored(kLabelColor, range.data());
  Tooltip(
      core::Tr("En cases. 1 = corps à corps ; au-delà, l'homoncule frappe à distance sans se "
               "déplacer."));
}

[[nodiscard]] std::string SkillLabel(const HomunculusSkillRow& skill) {
  return skill.name.empty() ? std::string("?") : std::string(skill.name);
}

// LES GESTES D'UNE CELLULE, rappelés après l'icône ET après le nom : on attrape
// une compétence par l'un comme par l'autre.
//
// ⚠ UNE COMPÉTENCE QUI VISE UNE CIBLE OU UNE CASE S'ARME : le clic suivant la
// donne à l'IA de l'homoncule, qui s'approche jusqu'à la portée mesurée depuis
// lui ; le joueur ne marche pas.
void SkillGestures(const HomunculusSkillRow& skill, bool resting, bool dragging, bool staff_allowed,
                   HomunculusTabAction& action) {
  const bool active = !skill.passive && skill.level > 0;
  // Glisser une compétence ACTIVE vers une barre ; une passive n'irait nulle
  // part. ⚠ NI L'IMAGE NI LE TEXTE NE DÉPOSENT D'IDENTIFIANT : sans
  // `SourceAllowNullID`, la source se refuserait sans rien dire.
  if (active && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
    const SkillDrag carried{.skill_id = skill.skill_id,
                            .level = static_cast<std::uint16_t>(skill.level)};
    ImGui::SetDragDropPayload(kSkillDragPayload, &carried, sizeof(carried));
    if (skill.icon.usable()) {
      ImGui::Image(skill.icon.id, ImVec2(Px(kDragIconPixels), Px(kDragIconPixels)));
      ImGui::SameLine();
    }
    ImGui::TextUnformatted(SkillLabel(skill).c_str());
    ImGui::EndDragDropSource();
  }
  if (!ImGui::IsItemHovered()) {
    return;
  }
  // Clic droit : la description ; double-clic : lancer. ⚠ RIEN PENDANT UN
  // GLISSER, sinon relâcher sur la barre lancerait aussi.
  if (!dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
    action.verb = HomunculusVerb::kDescribeSkill;
    action.skill_id = skill.skill_id;
  }
  if (active && !dragging && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    action.verb = HomunculusVerb::kUseSkill;
    action.skill_id = skill.skill_id;
    action.skill_level = skill.level;
  }
  // Une seule infobulle, qui porte ce que les colonnes ne montrent pas.
  const std::string sp = skill.sp_cost > 0 ? std::to_string(skill.sp_cost) : std::string("-");
  const std::string range =
      skill.range_cells > 0 ? std::to_string(skill.range_cells) : std::string("-");
  const char* nature = skill.passive ? core::Tr("passif") : core::Tr("actif");
  std::array<char, 128> facts = {};
  std::snprintf(facts.data(), facts.size(), core::Tr("%s  ·  SP %s  ·  portée %s"), nature,
                sp.c_str(), range.c_str());
  std::string tip;
  if (!active) {
    tip = Lines({facts.data(), core::Tr("Clic droit : description")});
  } else if (resting) {
    // Le repos d'abord : rien ne part, ciblé ou non.
    tip = Lines({facts.data(), core::Tr("Glisser : poser dans la barre"),
                 core::Tr("L'homoncule est au repos : il ne lance rien."),
                 core::Tr("Clic droit : description")});
  } else if (!skill.self_cast) {
    tip = Lines({facts.data(), core::Tr("Double-clic : lancer  ·  Glisser : poser dans la barre"),
                 core::Tr("Ce skill vise une cible ou une case : le clic suivant la désigne."),
                 core::Tr("Clic droit : description")});
  } else {
    tip = Lines({facts.data(), core::Tr("Double-clic : lancer  ·  Glisser : poser dans la barre"),
                 core::Tr("Clic droit : description")});
  }
  SkinTooltipWithStaffSkillId(tip.c_str(), staff_allowed,
                              StaffSkillId{.skill_id = skill.skill_id, .idname = skill.idname});
}

void DrawSkillRows(HomunculusTabState& state, const HomunculusTabView& view, const SkinArt& art,
                   HomunculusTabAction& action) {
  if (!ImGui::BeginTable(
          "cs_homun_skill_rows", kSkillColumns,
          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit)) {
    return;
  }
  ImGui::TableSetupColumn(core::Tr("Skill"), ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(core::Tr("Niv"));
  ImGui::TableSetupColumn("");
  ImGui::TableHeadersRow();
  for (const HomunculusSkillRow& skill : view.skills) {
    ImGui::PushID(skill.skill_id);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    // Calculé AVANT tout dessin : l'icône et le nom doivent voir le même état.
    const bool dragging = ImGui::GetDragDropPayload() != nullptr;
    if (skill.icon.usable()) {
      ImGui::Image(skill.icon.id, ImVec2(Px(kSkillIconPixels), Px(kSkillIconPixels)));
      SkillGestures(skill, view.resting, dragging, view.staff_allowed, action);
      ImGui::SameLine();
    }
    ImGui::TextUnformatted(SkillLabel(skill).c_str());
    SkillGestures(skill, view.resting, dragging, view.staff_allowed, action);

    ImGui::TableNextColumn();
    ImGui::Text("%d", skill.level);
    ImGui::TableNextColumn();
    // Le « + » seulement quand le serveur accepterait : rien d'affiché au
    // maximum, le bouton absent le dit déjà.
    if (skill.upgradable && view.skill_points > 0 && !view.resting) {
      if (SkinSmallButton("+", art, ImVec2(Px(kRaiseButtonPixels), 0.0F))) {
        action.verb = HomunculusVerb::kSkillUp;
        action.skill_id = skill.skill_id;
        state.status = core::Tr("Montée envoyée.");
      }
      Tooltip(Lines({core::Tr("Dépense un point (envoi immédiat)."),
                     core::Tr("Le serveur exige en plus un niveau d'homoncule minimum, que le "
                              "client ne connaît pas.")})
                  .c_str());
    }
    ImGui::PopID();
  }
  ImGui::EndTable();
}

// LES COMPÉTENCES, repliables : une fois les points dépensés et les
// compétences posées dans une barre, il n'y a plus rien à y faire. Le titre
// porte le compte des points, seul rappel quand la section est repliée.
void DrawSkills(HomunculusTabState& state, const HomunculusTabView& view, const SkinArt& art,
                HomunculusTabAction& action) {
  std::array<char, 96> title = {};
  if (view.skill_points > 0) {
    std::snprintf(title.data(), title.size(), core::Tr("Skills  -  %d point(s)###homun_skills"),
                  view.skill_points);
  } else {
    std::snprintf(title.data(), title.size(), "%s", core::Tr("Skills###homun_skills"));
  }
  // ⚠ À CHAQUE IMAGE, sinon ImGui garde son propre état et le nôtre ne pilote
  // rien. Le retour fait foi.
  ImGui::SetNextItemOpen(state.skills_open);
  state.skills_open = ImGui::CollapsingHeader(title.data());
  if (!state.skills_open) {
    return;
  }
  if (view.skills.empty()) {
    TextColored(kLabelColor, core::Tr("Aucun skill reçu du serveur."));
    return;
  }
  if (ImGui::BeginChild("cs_homun_skills", ImVec2(0.0F, 0.0F), ImGuiChildFlags_Borders)) {
    DrawSkillRows(state, view, art, action);
  }
  ImGui::EndChild();
}

}  // namespace

HomunculusTabAction DrawHomunculusTab(HomunculusTabState& state, const HomunculusTabView& view,
                                      const SkinArt& art) {
  HomunculusTabAction action;
  if (!view.sheet_known) {
    TextColored(kLabelColor, core::Tr("Aucun homoncule."));
    ImGui::Spacing();
    WrappedColored(kValueColor,
                   core::Tr("Un Alchimiste ayant appris Bioéthique peut en invoquer un avec une "
                            "Embryon. Cet onglet reprend la fiche d'état (Alt+R) et l'arbre de "
                            "skills."));
    return action;
  }
  DrawHeader(state, view, art, action);
  DrawGauges(view);
  DrawActions(state, view, art, action);
  switch (DrawCompanionAiLine(view.ai_profile, view.ai_proposal, art)) {
    case CompanionAiLineVerb::kOpen:
      action.verb = HomunculusVerb::kOpenAi;
      break;
    case CompanionAiLineVerb::kAcceptProposal:
      action.verb = HomunculusVerb::kAcceptAiProposal;
      break;
    case CompanionAiLineVerb::kDeclineProposal:
      action.verb = HomunculusVerb::kDeclineAiProposal;
      break;
    case CompanionAiLineVerb::kNothing:
      break;
  }
  ImGui::Spacing();
  ImGui::Separator();
  DrawStats(view);
  ImGui::Spacing();
  ImGui::Separator();
  // ⚠ AVANT LES COMPÉTENCES : leur liste prend toute la hauteur qui reste.
  DrawHomunculusStaff(state, view, art, action);
  DrawSkills(state, view, art, action);
  return action;
}

}  // namespace ro::ui
