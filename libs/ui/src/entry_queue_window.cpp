// La liste des terrains, leur détail et l'aide. Voir l'en-tête.

#include "ro/ui/entry_queue_window.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "entry_queue_parts.h"

namespace ro::ui {
namespace {

constexpr const char* kListTitle = core::TrText("Champ de bataille");
constexpr const char* kHelpTitle = core::TrText("Aide : inscription au champ de bataille");
// La boîte de confirmation, nommée DANS la fenêtre qui la demande.
constexpr const char* kConfirmId = "##entry_queue_confirm";
// Le menu du lien d'une récompense, nommé dans la liste.
constexpr const char* kRewardMenuId = "##entry_queue_reward_menu";

// LA MINIMAP DU TERRAIN, en pixels d'interface : la moitié de l'image du jeu,
// assez pour reconnaître la carte sans écraser le détail.
constexpr float kMinimapSidePixels = 128.0F;
// La colonne de la liste ne descend pas sous cette largeur, en hauteurs de
// police : trois noms courts ne doivent pas en faire un ruban.
constexpr float kListLeastFonts = 8.0F;
// Ce que la liste ajoute au plus long nom, en hauteurs de police.
constexpr float kListMarginFonts = 1.5F;
// La largeur du détail, en hauteurs de police.
constexpr float kDetailFonts = 20.0F;
// L'écart entre une étiquette et sa valeur, en hauteurs de police.
constexpr float kLabelGapFonts = 0.75F;
// Dans les étiquettes du détail, la première des trois issues : la victoire.
constexpr std::size_t kFirstOutcomeLabel = 3;

constexpr std::size_t kTextCapacity = 256;
using Text = std::array<char, kTextCapacity>;

constexpr std::uint32_t kSecondsPerMinute = 60;
constexpr std::uint32_t kSecondsPerHour = 3600;

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

[[nodiscard]] std::string Owned(std::string_view text) { return std::string(text); }

[[nodiscard]] bool IsApply(EntryQueueVerb verb) {
  return verb == EntryQueueVerb::kApplySolo || verb == EntryQueueVerb::kApplyParty ||
         verb == EntryQueueVerb::kApplyGuild;
}

// UNE INSCRIPTION, telle que son bouton la montre.
struct ApplyButton {
  EntryQueueVerb verb;
  const char* label;
  // Ce qu'elle fait, quand elle peut partir.
  const char* help;
  bool allowed;
};

// LA QUESTION DE LA CONFIRMATION, pour ce terrain.
//
// ⚠ UN GABARIT LITTÉRAL PAR CAS, et non un gabarit rangé dans une table : le
// compilateur ne vérifie les arguments que d'un format qu'il voit.
[[nodiscard]] Text ApplyQuestion(EntryQueueVerb verb, std::string_view field_name) {
  const std::string name = Owned(field_name);
  Text question{};
  switch (verb) {
    case EntryQueueVerb::kApplyParty:
      std::snprintf(question.data(), question.size(), core::Tr("Inscrire votre party à « %s » ?"),
                    name.c_str());
      break;
    case EntryQueueVerb::kApplyGuild:
      std::snprintf(question.data(), question.size(), core::Tr("Inscrire votre guilde à « %s » ?"),
                    name.c_str());
      break;
    case EntryQueueVerb::kApplySolo:
    case EntryQueueVerb::kNothing:
    case EntryQueueVerb::kCancel:
    case EntryQueueVerb::kAccept:
    case EntryQueueVerb::kDecline:
    case EntryQueueVerb::kNoticeSeen:
      std::snprintf(question.data(), question.size(), core::Tr("Vous inscrire seul à « %s » ?"),
                    name.c_str());
      break;
  }
  return question;
}

// POURQUOI UNE INSCRIPTION NE PEUT PAS PARTIR ; nul si elle peut. La première
// raison qui tient l'emporte : la liaison d'abord, puis la file, puis le
// terrain.
//
// ⚠ AUCUNE GARDE D'EFFECTIF NI DE NIVEAU : le serveur juge, et il le dit.
[[nodiscard]] const char* ApplyBlocker(const EntryQueueView& view, bool allowed) {
  if (!view.link_ready) {
    return EntryQueueLinkPendingText();
  }
  if (view.queued || view.invited || view.accepted) {
    return core::Tr("Vous êtes déjà en file.");
  }
  if (view.applying) {
    return core::Tr("Inscription envoyée, en attente du serveur…");
  }
  if (!allowed) {
    return core::Tr("Ce terrain n'accepte pas ce mode d'inscription.");
  }
  return nullptr;
}

// Une ligne « étiquette  valeur », la valeur repliée sur la largeur du détail.
//
// ⚠ DEUX REPÈRES DIFFÉRENTS : `value_from_group` se compte depuis le bord du
// groupe — `SameLine` y ajoute lui-même le décalage du groupe —, `wrap_at` depuis
// le bord de la fenêtre, comme le veut `PushTextWrapPos`.
void DetailRow(const char* label, std::string_view value, float value_from_group, float wrap_at) {
  ImGui::TextUnformatted(label);
  ImGui::SameLine(value_from_group);
  ImGui::PushTextWrapPos(wrap_at);
  ImGui::TextUnformatted(value.data(), value.data() + value.size());
  ImGui::PopTextWrapPos();
}

// LES RÉCOMPENSES D'UNE ISSUE : l'étiquette, puis une ligne par récompense.
// Un objet est une cellule d'objet cliquable, icône comprise ; un texte libre
// se lit seulement. Aucune : « Aucune ». Mêmes repères que `DetailRow` ; `left`
// est le bord du groupe, compté depuis la fenêtre.
void RewardRows(const char* label, std::span<const EntryQueueRewardView> rewards, float left,
                float value_from_group, float wrap_at, ItemLinkMenu& menu, ChatLinkAsk& asked) {
  ImGui::TextUnformatted(label);
  if (rewards.empty()) {
    ImGui::SameLine(value_from_group);
    ImGui::TextUnformatted(core::Tr("Aucune"));
    return;
  }
  for (std::size_t rank = 0; rank < rewards.size(); ++rank) {
    const EntryQueueRewardView& reward = rewards[rank];
    if (rank == 0) {
      ImGui::SameLine(value_from_group);
    } else {
      ImGui::SetCursorPosX(left + value_from_group);
    }
    if (reward.item_id != 0) {
      DrawItemLinkCell(
          ItemLinkCell{.item_id = reward.item_id, .text = reward.text, .icon = reward.icon}, menu,
          asked);
      continue;
    }
    ImGui::PushTextWrapPos(wrap_at);
    ImGui::TextUnformatted(reward.text.data(), reward.text.data() + reward.text.size());
    ImGui::PopTextWrapPos();
  }
}

// LA LISTE DES TERRAINS, en colonne, à la largeur du plus long nom.
void DrawFieldList(EntryQueueWindowState& state, const EntryQueueView& view) {
  const float font = ImGui::GetFontSize();
  float widest = font * kListLeastFonts;
  for (const EntryQueueFieldView& field : view.fields) {
    widest =
        std::max(widest, ImGui::CalcTextSize(field.display_name.data(),
                                             field.display_name.data() + field.display_name.size())
                                 .x +
                             font * kListMarginFonts);
  }
  ImGui::BeginGroup();
  for (std::size_t rank = 0; rank < view.fields.size(); ++rank) {
    ImGui::PushID(static_cast<int>(rank));
    if (ImGui::Selectable(Owned(view.fields[rank].display_name).c_str(), state.selected == rank,
                          ImGuiSelectableFlags_None, ImVec2(widest, 0.0F))) {
      state.selected = rank;
    }
    ImGui::PopID();
  }
  ImGui::EndGroup();
}

// Pose la prochaine pièce, large de `width`, au milieu du détail.
void CenterInDetail(float left, float detail_width, float width) {
  ImGui::SetCursorPosX(left + std::max(0.0F, (detail_width - width) * 0.5F));
}

// LE DÉTAIL DU TERRAIN CHOISI : sa minimap et son nom centrés, puis son format,
// sa condition, son objectif et ses récompenses. Ce que le joueur fait du lien
// d'une récompense s'écrit dans `asked`.
void DrawFieldDetail(const EntryQueueFieldView& field, ItemLinkMenu& menu, ChatLinkAsk& asked) {
  const float font = ImGui::GetFontSize();
  const float left = ImGui::GetCursorPosX();
  const float detail_width = font * kDetailFonts;
  const float wrap_at = left + detail_width;
  ImGui::BeginGroup();
  // Le détail garde sa largeur quel que soit le terrain : la fenêtre ne saute
  // pas d'un choix à l'autre.
  ImGui::Dummy(ImVec2(detail_width, 0.0F));
  if (field.minimap.usable()) {
    const float side = kMinimapSidePixels * Scale();
    CenterInDetail(left, detail_width, side);
    const ImVec2 corner = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddImage(field.minimap.id, corner,
                                         ImVec2(corner.x + side, corner.y + side));
    ImGui::Dummy(ImVec2(side, side));
  }
  const char* name_begin = field.display_name.data();
  const char* name_end = name_begin + field.display_name.size();
  CenterInDetail(left, detail_width, ImGui::CalcTextSize(name_begin, name_end).x);
  ImGui::TextUnformatted(name_begin, name_end);
  ImGui::Separator();

  // ⚠ LA COLONNE DES VALEURS SE CALE SUR LA PLUS LONGUE ÉTIQUETTE, mesurée dans
  // la langue courante : aucune ne mord sur sa valeur.
  const std::array<const char*, 6> labels = {core::Tr("Format"),    core::Tr("Condition"),
                                             core::Tr("Objectif"),  core::Tr("Victoire"),
                                             core::Tr("Match nul"), core::Tr("Défaite")};
  float widest = 0.0F;
  for (const char* label : labels) {
    widest = std::max(widest, ImGui::CalcTextSize(label).x);
  }
  const float value_column = widest + font * kLabelGapFonts;

  const std::string_view none = core::Tr("Aucune");
  if (!field.team_text.empty()) {
    DetailRow(labels[0], field.team_text, value_column, wrap_at);
  }
  DetailRow(labels[1], field.condition_text.empty() ? none : field.condition_text, value_column,
            wrap_at);
  if (!field.victory.empty()) {
    DetailRow(labels[2], field.victory, value_column, wrap_at);
  }
  ImGui::Spacing();
  ImGui::TextDisabled("%s", core::Tr("Récompenses"));
  const std::array<std::span<const EntryQueueRewardView>, 3> outcomes = {
      field.reward_win, field.reward_draw, field.reward_lose};
  for (std::size_t rank = 0; rank < outcomes.size(); ++rank) {
    ImGui::PushID(static_cast<int>(rank));
    RewardRows(labels[kFirstOutcomeLabel + rank], outcomes[rank], left, value_column, wrap_at, menu,
               asked);
    ImGui::PopID();
  }
  if (!field.help.empty()) {
    ImGui::Spacing();
    ImGui::PushTextWrapPos(wrap_at);
    ImGui::TextDisabled("%s", Owned(field.help).c_str());
    ImGui::PopTextWrapPos();
  }
  ImGui::EndGroup();
}

// LES TROIS INSCRIPTIONS, grisées quand elles ne peuvent pas partir, et la
// confirmation qui les garde. Rend celle que le joueur a confirmée.
[[nodiscard]] EntryQueueAction DrawApplyButtons(EntryQueueWindowState& state,
                                                const EntryQueueView& view,
                                                const EntryQueueFieldView& field,
                                                const SkinArt& art) {
  const std::array<ApplyButton, 3> buttons = {
      ApplyButton{.verb = EntryQueueVerb::kApplySolo,
                  .label = core::Tr("Solo"),
                  .help = core::Tr("Vous inscrire seul."),
                  .allowed = field.private_apply},
      ApplyButton{.verb = EntryQueueVerb::kApplyParty,
                  .label = core::Tr("Party"),
                  .help = core::Tr("Inscrire toute votre party : réservé à son chef."),
                  .allowed = field.party_apply},
      ApplyButton{.verb = EntryQueueVerb::kApplyGuild,
                  .label = core::Tr("Guilde"),
                  .help = core::Tr("Inscrire votre guilde : réservé au maître de guilde."),
                  .allowed = field.guild_apply},
  };
  EntryQueueAction action;
  for (std::size_t rank = 0; rank < buttons.size(); ++rank) {
    const ApplyButton& button = buttons[rank];
    if (rank > 0) {
      ImGui::SameLine();
    }
    const char* blocker = ApplyBlocker(view, button.allowed);
    ImGui::BeginDisabled(blocker != nullptr);
    if (SkinButton(button.label, art)) {
      AskEntryQueueVerb(state, button.verb);
    }
    ImGui::EndDisabled();
    TellEntryQueueWhy(blocker != nullptr ? blocker : button.help);
  }
  for (const ApplyButton& button : buttons) {
    if (state.confirming != button.verb) {
      continue;
    }
    const Text question = ApplyQuestion(button.verb, field.display_name);
    // 🔴 LA RÉPONSE SE REJUGE AU « OUI » : la file a pu bouger pendant que la
    // boîte attendait, et une inscription devenue impossible ne part pas.
    if (ConfirmEntryQueueVerb(state, button.verb, question.data(), core::Tr("S'inscrire"),
                              core::Tr("Annuler"), art) &&
        ApplyBlocker(view, button.allowed) == nullptr) {
      action.verb = button.verb;
      action.field_rank = state.selected;
    }
  }
  return action;
}

[[nodiscard]] EntryQueueAction DrawListWindow(EntryQueueWindowState& state,
                                              const EntryQueueView& view, const SkinArt& art) {
  EntryQueueAction action;
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5F, 0.5F));
  static WindowName name(kListTitle, kEntryQueueWindowId);
  PoseBugReportButton(state.bug_report);
  const bool begun =
      BeginSkinnedWindow(name.Get(), &state.open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
  if (!begun) {
    EndSkinnedWindow();
    return action;
  }
  const bool report_bug = state.bug_report && TitleActionClicked(kEntryQueueBugReportRank);
  // Une liste qui a raccourci ne garde pas un choix hors d'elle.
  if (state.selected >= view.fields.size()) {
    state.selected = 0;
  }

  // Ce que le joueur fait du lien d'une récompense.
  ChatLinkAsk link;

  // En file, la liste le rappelle, et mène à la fenêtre de la file.
  if (view.queued) {
    ImGui::Text(core::Tr("Vous êtes en file pour « %s »."), Owned(view.queued_name).c_str());
    ImGui::SameLine();
    if (SkinSmallButton(core::Tr("Voir la file"), art)) {
      state.standby_open = true;
    }
    ImGui::Separator();
  }

  if (view.fields.empty()) {
    // Plus de terrain, plus rien à confirmer : la boîte ne se dessinerait plus.
    if (IsApply(state.confirming)) {
      state.confirming = EntryQueueVerb::kNothing;
    }
    ImGui::TextUnformatted(core::Tr("Aucun terrain n'est proposé pour le moment."));
  } else {
    DrawFieldList(state, view);
    ImGui::SameLine();
    DrawFieldDetail(view.fields[state.selected], state.reward_menu, link);
    ImGui::Separator();
    action = DrawApplyButtons(state, view, view.fields[state.selected], art);
    ImGui::SameLine();
  }
  if (SkinButton(core::Tr("Aide"), art)) {
    state.help_open = !state.help_open;
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Fermer"), art)) {
    state.open = false;
  }
  // Le menu, au niveau de la fenêtre et après son dernier bouton : voir
  // `DrawItemLinkMenu`.
  DrawItemLinkMenu(kRewardMenuId, state.reward_menu, link);
  action.link = link;
  EndSkinnedWindow();
  if (report_bug) {
    action.report_bug = true;
    action.field_rank = state.selected;
  }
  return action;
}

// L'AIDE : les cas où l'inscription et l'entrée sont refusées.
void DrawHelpWindow(EntryQueueWindowState& state, const SkinArt& art) {
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5F, 0.5F));
  static WindowName name(kHelpTitle, kEntryQueueHelpId);
  const bool begun =
      BeginSkinnedWindow(name.Get(), &state.help_open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
  if (!begun) {
    EndSkinnedWindow();
    return;
  }
  // LES RÈGLES D'INSCRIPTION, une ligne chacune : un titre, puis cinq cas
  // numérotés, dont deux se prolongent d'une ligne sans numéro.
  const std::array<const char*, 9> lines = {
      core::Tr("[Règles d'inscription au champ de bataille]"),
      core::Tr("L'inscription et l'entrée au champ de bataille sont impossibles dans les cas "
               "suivants"),
      core::Tr("1. On ne peut pas s'inscrire à plusieurs types de bataille à la fois."),
      core::Tr("2. On ne peut pas s'inscrire à la fois en solo, en party et en guilde."),
      core::Tr("3. Seul le chef de la party peut l'inscrire."),
      core::Tr("Les membres de la party hors ligne n'entrent pas dans la file."),
      core::Tr("4. On peut demander l'entrée dans l'arène depuis n'importe quelle carte, sauf "
               "celles qui interdisent la téléportation et le warp."),
      core::Tr("À la fin de la bataille, votre personnage revient à l'endroit où il était ou, si "
               "c'est impossible, à son point de sauvegarde."),
      core::Tr("5. Les récompenses se consultent et se choisissent dans la salle d'attente de "
               "l'arène."),
  };
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetFontSize() * kEntryQueueWrapFonts);
  for (const char* line : lines) {
    ImGui::TextUnformatted(line);
  }
  ImGui::PopTextWrapPos();
  ImGui::Spacing();
  if (SkinButton(core::Tr("Fermer"), art)) {
    state.help_open = false;
  }
  EndSkinnedWindow();
}

// La première demande de l'image l'emporte : un seul geste à la fois. Le
// rapport de bug et la description d'un objet, eux, s'ajoutent à ce geste au
// lieu de le remplacer.
void Keep(EntryQueueAction& kept, const EntryQueueAction& found) {
  const bool report_bug = kept.report_bug || found.report_bug;
  const ChatLinkAsk link =
      kept.link.gesture != ChatLinkGesture::kNothing || kept.link.hovering ? kept.link : found.link;
  if (kept.verb == EntryQueueVerb::kNothing) {
    const std::size_t field_rank = kept.report_bug ? kept.field_rank : found.field_rank;
    kept = found;
    if (found.verb == EntryQueueVerb::kNothing) {
      kept.field_rank = field_rank;
    }
  }
  kept.report_bug = report_bug;
  kept.link = link;
}

}  // namespace

const char* EntryQueueLinkPendingText() {
  return core::Tr("Liaison avec le serveur en cours de vérification : réessayez dans un instant.");
}

void TellEntryQueueWhy(const char* why) {
  if (why != nullptr && why[0] != '\0' &&
      ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltipFormat("%s", why);
  }
}

void AskEntryQueueVerb(EntryQueueWindowState& state, EntryQueueVerb verb) {
  state.confirming = verb;
  AskModal(kConfirmId);
}

bool ConfirmEntryQueueVerb(EntryQueueWindowState& state, EntryQueueVerb verb, const char* question,
                           const char* yes, const char* no, const SkinArt& art) {
  if (state.confirming != verb) {
    return false;
  }
  const ModalAnswer answer = DrawModalBox(kConfirmId, question, yes, no, art);
  // ⚠ UNE BOÎTE FERMÉE SANS RÉPONSE — un autre surgissant l'a remplacée — ne
  // laisse pas le verbe suspendu : il attendrait une réponse qui ne vient plus.
  if (answer != ModalAnswer::kWaiting || !ImGui::IsPopupOpen(kConfirmId)) {
    state.confirming = EntryQueueVerb::kNothing;
  }
  return answer == ModalAnswer::kYes;
}

EntryQueueAction DrawEntryQueueWindows(EntryQueueWindowState& state, const EntryQueueView& view,
                                       const SkinArt& art) {
  // 🔴 UNE CONFIRMATION SANS SA FENÊTRE S'OUBLIE : la boîte vit dans la
  // fenêtre, et rouverte plus tard elle ne doit pas reparaître d'elle-même.
  if (!state.open && IsApply(state.confirming)) {
    state.confirming = EntryQueueVerb::kNothing;
  }
  if (!state.standby_open && state.confirming == EntryQueueVerb::kCancel) {
    state.confirming = EntryQueueVerb::kNothing;
  }
  EntryQueueAction action;
  if (state.open) {
    Keep(action, DrawListWindow(state, view, art));
  }
  if (state.standby_open) {
    Keep(action, DrawEntryQueueStandby(state, view, art));
  }
  if (state.help_open) {
    DrawHelpWindow(state, art);
  }
  // Les deux modales, hors de toute fenêtre : elles s'imposent, liste fermée
  // comme ouverte.
  Keep(action, DrawEntryQueueReady(state, view, art));
  Keep(action, DrawEntryQueueNotice(state, view, art));
  return action;
}

std::string FormatQueueWait(std::uint32_t waited_s) {
  const std::uint32_t hours = waited_s / kSecondsPerHour;
  const std::uint32_t minutes = waited_s % kSecondsPerHour / kSecondsPerMinute;
  const std::uint32_t seconds = waited_s % kSecondsPerMinute;
  Text text{};
  if (hours > 0) {
    std::snprintf(text.data(), text.size(), "%u h %02u min %02u s", hours, minutes, seconds);
  } else if (minutes > 0) {
    std::snprintf(text.data(), text.size(), "%u min %02u s", minutes, seconds);
  } else {
    std::snprintf(text.data(), text.size(), "%u s", seconds);
  }
  return text.data();
}

}  // namespace ro::ui
