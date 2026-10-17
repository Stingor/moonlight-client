#include "ro/ui/companion_ai_window.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <string>
#include <utility>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "companion_ai_window_parts.h"
#include "ui_scale.h"

namespace ro::ui {
namespace {

using companion_ai_parts::DrawRulesSection;
using companion_ai_parts::DrawSettingsSection;
using companion_ai_parts::DrawTacticsSection;

// Sa première place et sa première taille, en pixels de police.
constexpr float kSpawnXPixels = 380.0F;
constexpr float kSpawnYPixels = 90.0F;
constexpr float kSpawnWidthPixels = 520.0F;
constexpr float kSpawnHeightPixels = 560.0F;
constexpr std::size_t kLineBytes = 160;
// La largeur du champ où se tape le nom d'un ami, en pixels de police.
constexpr float kFriendFieldPixels = 260.0F;
// 🔴 LA LARGEUR DES CURSEURS, LISTES ET CHAMPS, en pixels de police : fixe,
// comme dans les outils du staff. Celle d'ImGui suit la fenêtre, et une
// fenêtre élargie étirait chaque contrôle sur toute sa largeur.
constexpr float kControlWidthPixels = 190.0F;

[[nodiscard]] bool Present(const CompanionAiView& view, int companion) {
  return companion == kCompanionAiHomunculus ? view.homunculus_present : view.mercenary_present;
}

// L'ONGLET DE CHAQUE COMPAGNON, quand les deux sont là. Un compagnon parti
// laisse la place à l'autre.
void DrawCompanionChoice(CompanionAiWindowState& state, const CompanionAiView& view,
                         const SkinArt& art) {
  if (!Present(view, state.companion)) {
    ChangeAiWindowCompanion(
        state, view.homunculus_present ? kCompanionAiHomunculus : kCompanionAiMercenary);
  }
  if (!view.homunculus_present || !view.mercenary_present) {
    return;
  }
  if (SkinSmallToggleButton(core::Tr("Homoncule"), state.companion == kCompanionAiHomunculus,
                            art)) {
    ChangeAiWindowCompanion(state, kCompanionAiHomunculus);
  }
  ImGui::SameLine();
  if (SkinSmallToggleButton(core::Tr("Mercenaire"), state.companion == kCompanionAiMercenary,
                            art)) {
    ChangeAiWindowCompanion(state, kCompanionAiMercenary);
  }
}

// LE PROFIL EN VIGUEUR, l'en-tête des profils, et « Repartir de l'IA basique »
// avec sa confirmation.
void DrawProfile(CompanionAiWindowState& state, const CompanionAiView& view, const SkinArt& art,
                 CompanionAiWindowAction& action) {
  std::array<char, kLineBytes> line{};
  std::snprintf(line.data(), line.size(), core::Tr("%s — profil : %s"), view.companion_name.c_str(),
                view.profile.c_str());
  ImGui::TextUnformatted(line.data());
  if (!view.editable) {
    return;
  }
  companion_ai_parts::DrawProposalBanner(view.profiles, art, action);
  companion_ai_parts::DrawProfileHeader(state, view, art, action);
  ImGui::Separator();
  if (view.basic) {
    companion_ai_parts::WrappedNote(
        core::Tr("L'IA basique ne se modifie pas : « Nouveau » ou "
                 "« Dupliquer » en font une copie à régler."));
    return;
  }
  // 🔴 UNE CONFIRMATION NE VAUT QUE POUR LE COMPAGNON QUI L'A DEMANDÉE : elle
  // tombe dès que la vue en décrit un autre.
  if (state.reset_asked &&
      (state.reset_companion != view.companion || state.reset_class != view.companion_class)) {
    state.reset_asked = false;
  }
  if (!state.reset_asked) {
    if (SkinSmallButton(core::Tr("Repartir de l'IA basique…"), art)) {
      state.reset_asked = true;
      state.reset_companion = view.companion;
      state.reset_class = view.companion_class;
    }
    SkinTooltip(core::Tr("Remplace tout ce profil par l'IA basique, après confirmation."));
    return;
  }
  companion_ai_parts::WrappedNote(
      core::Tr("Les réglages, les tactiques, les règles et les amis de ce "
               "profil seront remplacés par ceux de l'IA basique. Les "
               "lignes du fichier écartées à la lecture y restent."));
  if (SkinSmallButton(core::Tr("Confirmer"), art)) {
    action.verb = CompanionAiVerb::kResetToBasic;
    action.companion_class = state.reset_class;
    state.reset_asked = false;
  }
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Annuler"), art)) {
    state.reset_asked = false;
  }
}

void DrawFriendsSection(CompanionAiWindowState& state, const CompanionAiView& view,
                        const SkinArt& art, CompanionAiWindowAction& action) {
  companion_ai_parts::WrappedNote(
      core::Tr("Un ami est défendu comme le maître, et sert de cible « un "
               "ami » aux règles. Il est reconnu au nom de son personnage, "
               "et s'ajoute aussi par le menu d'entité d'un joueur."));
  if (view.friends.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucun ami."));
  }
  for (std::size_t index = 0; index < view.friends.size(); ++index) {
    ImGui::PushID(static_cast<int>(index));
    ImGui::TextUnformatted(view.friends[index].c_str());
    ImGui::SameLine();
    if (SkinSmallButton(core::Tr("Retirer"), art)) {
      action.verb = CompanionAiVerb::kRemoveFriend;
      action.index = index;
    }
    ImGui::PopID();
  }
  ImGui::SetNextItemWidth(Px(kFriendFieldPixels));
  const bool entered =
      ImGui::InputText("##friend_name", state.friend_name.data(), state.friend_name.size(),
                       ImGuiInputTextFlags_EnterReturnsTrue);
  // ⚠ LU APRÈS LA SAISIE : la lettre tapée à l'image de l'Entrée en fait partie.
  const std::string typed = TrimmedCharacterName(state.friend_name.data());
  const bool full = view.friends.size() >= kCompanionAiFriendMost;
  const bool known = std::ranges::find(view.friends, typed) != view.friends.end();
  ImGui::SameLine();
  const bool refused = typed.empty() || full || known;
  ImGui::BeginDisabled(refused);
  const bool added = SkinSmallButton(core::Tr("Ajouter"), art);
  ImGui::EndDisabled();
  if (refused && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltipFormat("%s", full    ? core::Tr("La liste d'amis du compagnon est pleine.")
                            : known ? core::Tr("Ce nom est déjà dans la liste.")
                                    : core::Tr("Tapez le nom exact d'un personnage."));
  }
  if ((added || entered) && !refused) {
    action.verb = CompanionAiVerb::kAddFriend;
    action.name = typed;
    state.friend_name = {};
  }
}

void DrawSections(CompanionAiWindowState& state, const CompanionAiView& view, const SkinArt& art,
                  CompanionAiWindowAction& action) {
  if (!BeginSkinTabBar("companion_ai_sections", art)) {
    return;
  }
  // LE CONTENU DE CHAQUE ONGLET SE GRISE, pas la barre : l'IA basique se lit
  // onglet par onglet sans se modifier.
  const bool locked = !view.editable || view.basic;
  // La demande d'onglet se consomme ici : ImGui choisit en dessinant.
  const int asked = std::exchange(state.section_asked, -1);
  const auto flags = [asked](CompanionAiSection section) {
    return asked == static_cast<int>(section) ? ImGuiTabItemFlags_SetSelected
                                              : ImGuiTabItemFlags_None;
  };
  if (ImGui::BeginTabItem(core::Tr("Réglages"), nullptr, flags(CompanionAiSection::kSettings))) {
    ImGui::BeginDisabled(locked);
    DrawSettingsSection(view, art, action);
    ImGui::EndDisabled();
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem(core::Tr("Tactiques"), nullptr, flags(CompanionAiSection::kTactics))) {
    ImGui::BeginDisabled(locked);
    DrawTacticsSection(state, view, art, action);
    ImGui::EndDisabled();
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem(core::Tr("Joueurs"), nullptr, flags(CompanionAiSection::kPlayers))) {
    ImGui::BeginDisabled(locked);
    companion_ai_parts::DrawPlayersSection(state, view, art, action);
    ImGui::EndDisabled();
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem(core::Tr("Règles"), nullptr, flags(CompanionAiSection::kRules))) {
    ImGui::BeginDisabled(locked);
    DrawRulesSection(state, view, art, action);
    ImGui::EndDisabled();
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem(core::Tr("Amis"), nullptr, flags(CompanionAiSection::kFriends))) {
    ImGui::BeginDisabled(locked);
    DrawFriendsSection(state, view, art, action);
    ImGui::EndDisabled();
    ImGui::EndTabItem();
  }
  EndSkinTabBar();
}

}  // namespace

const char* CompanionTacticGenericLabel(int generic) {
  switch (generic) {
    case kTacticGenericChests:
      return core::Tr("Treasure Chest des châteaux");
    case kTacticGenericStill:
      return core::Tr("Immobiles (plantes, champignons)");
    case kTacticGenericMinions:
      return core::Tr("Sbires d'un monstre");
    default:
      break;
  }
  return core::Tr("Tout autre monstre");
}

void companion_ai_parts::WrappedNote(const char* text) {
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  const float widest_x = window->DC.IdealMaxPos.x;
  ImGui::TextWrapped("%s", text);
  window->DC.IdealMaxPos.x = widest_x;
}

CompanionAiWindowAction DrawCompanionAiWindow(CompanionAiWindowState& state,
                                              const CompanionAiView& view, const SkinArt& art) {
  CompanionAiWindowAction action;
  action.companion = view.companion;
  if (!state.open || (!view.homunculus_present && !view.mercenary_present)) {
    return action;
  }
  if (state.focus_asked) {
    state.focus_asked = false;
    ImGui::SetNextWindowFocus();
  }
  ImGui::SetNextWindowPos(ImVec2(Px(kSpawnXPixels), Px(kSpawnYPixels)), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(Px(kSpawnWidthPixels), Px(kSpawnHeightPixels)),
                           ImGuiCond_FirstUseEver);
  const std::string title = std::string(core::Tr("IA du compagnon")) + kCompanionAiWindowId;
  bool open = true;
  const bool measuring = state.widest_pixels <= 0.0F;
  if (!measuring) {
    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0F, 0.0F), ImVec2(state.widest_pixels, FLT_MAX));
  }
  PoseBugReportButton(state.bug_report);
  SetNextWindowPinnable(&state.pinned);
  const bool begun = BeginSkinnedWindow(title.c_str(), &open, art, SkinnedTitle::kBar,
                                        ImGuiWindowFlags_NoCollapse);
  action.report_bug = state.bug_report && TitleActionClicked(0);
  if (!open) {
    state.open = false;
    action.closed = true;
  }
  // 🔴 TANT QUE LA LIGNE LA PLUS LARGE N'EST PAS MESURÉE, LA FENÊTRE EST
  // CACHÉE : sans plafond, elle paraîtrait à sa largeur d'ouverture, puis
  // se resserrerait à l'image suivante. ⚠ CACHÉE APRÈS `Begin`, et non
  // transparente : une transparence nulle au `Begin` fait sauter tout le
  // contenu, qui ne se mesurerait jamais.
  if (measuring) {
    ImGui::GetCurrentWindow()->Hidden = true;
  }
  if (begun) {
    ImGui::PushItemWidth(Px(kControlWidthPixels));
    DrawCompanionChoice(state, view, art);
    // ⚠ LA VUE PEUT RETARDER D'UNE IMAGE sur l'onglet choisi : elle décrit
    // encore l'autre compagnon, et rien ne se dessine plutôt qu'un geste ne
    // parte vers le mauvais.
    if (view.companion == state.companion) {
      DrawProfile(state, view, art, action);
      ImGui::Separator();
      if (!view.editable) {
        companion_ai_parts::WrappedNote(
            core::Tr("La classe du compagnon n'est pas encore connue : son "
                     "profil se règle dès qu'il est en vue."));
      }
      DrawSections(state, view, art, action);
    }
    ImGui::PopItemWidth();
    // La ligne la plus large de l'image précédente, plus les marges et la
    // barre de défilement. ⚠ NULLE À LA PREMIÈRE IMAGE de la fenêtre, qui
    // n'a encore rien mesuré : elle ne compte pas.
    const ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->ContentSizeIdeal.x > 0.0F) {
      state.widest_pixels = std::max(
          state.widest_pixels,
          window->ContentSizeIdeal.x + (window->WindowPadding.x * 2.0F) + window->ScrollbarSizes.x);
    }
  }
  EndSkinnedWindow();
  // LE GESTE VISE LE PROFIL QUE LA VUE ÉDITAIT, même si l'en-tête vient d'en
  // choisir un autre.
  const std::span<const ProfileChoiceView> profiles = view.profiles.profiles;
  if (view.profiles.edited >= 0 &&
      static_cast<std::size_t>(view.profiles.edited) < profiles.size()) {
    action.profile = profiles[static_cast<std::size_t>(view.profiles.edited)].key;
  }
  return action;
}

void ChangeAiWindowCompanion(CompanionAiWindowState& state, int companion) {
  state.companion = companion;
  state.expanded_rule = -1;
  state.expanded_tactic = -1;
  state.expanded_conduct = -1;
  state.reset_asked = false;
  state.reset_class = 0;
  state.friend_name = {};
  state.edited_profile.clear();
  state.profile_dialog = ProfileDialog::kNone;
  state.profile_name = {};
}

void FollowAcceptedAiGesture(CompanionAiWindowState& state, const CompanionAiWindowAction& action,
                             std::size_t rule_count) {
  const auto index = static_cast<int>(action.index);
  int& rule = state.expanded_rule;
  switch (action.verb) {
    case CompanionAiVerb::kMoveRule: {
      // Retirée de `index`, puis insérée au rang `to`.
      const auto to = static_cast<int>(action.to);
      if (rule == index) {
        rule = to;
      } else if (index < rule && rule <= to) {
        --rule;
      } else if (to <= rule && rule < index) {
        ++rule;
      }
      break;
    }
    case CompanionAiVerb::kRemoveRule:
      rule = rule == index ? -1 : (rule > index ? rule - 1 : rule);
      break;
    case CompanionAiVerb::kDuplicateRule:
      if (rule > index) {
        ++rule;
      }
      break;
    case CompanionAiVerb::kAddRule:
      rule = static_cast<int>(rule_count) - 1;
      break;
    case CompanionAiVerb::kRemoveTactic: {
      int& tactic = state.expanded_tactic;
      tactic = tactic == index ? -1 : (tactic > index ? tactic - 1 : tactic);
      break;
    }
    case CompanionAiVerb::kResetToBasic:
      rule = -1;
      state.expanded_tactic = -1;
      state.expanded_conduct = -1;
      break;
    case CompanionAiVerb::kRemoveConduct: {
      int& conduct = state.expanded_conduct;
      conduct = conduct == index ? -1 : (conduct > index ? conduct - 1 : conduct);
      break;
    }
    default:
      break;
  }
}

std::string TrimmedCharacterName(std::string_view typed) {
  constexpr std::string_view kBlanks = " \t\r\n";
  const std::size_t first = typed.find_first_not_of(kBlanks);
  if (first == std::string_view::npos) {
    return std::string();
  }
  const std::size_t last = typed.find_last_not_of(kBlanks);
  return std::string(typed.substr(first, last - first + 1));
}

CompanionAiLineVerb DrawCompanionAiLine(std::string_view profile, std::string_view proposal,
                                        const SkinArt& art) {
  if (profile.empty()) {
    return CompanionAiLineVerb::kNothing;
  }
  CompanionAiLineVerb verb = CompanionAiLineVerb::kNothing;
  ImGui::PushID("companion_ai_line");
  std::array<char, kLineBytes> line{};
  std::snprintf(line.data(), line.size(), core::Tr("IA : %.*s"), static_cast<int>(profile.size()),
                profile.data());
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(line.data());
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Régler…"), art)) {
    verb = CompanionAiLineVerb::kOpen;
  }
  SkinTooltip(core::Tr("Ouvre la fenêtre « IA du compagnon »."));
  if (!proposal.empty()) {
    ImGui::TextWrapped("%.*s", static_cast<int>(proposal.size()), proposal.data());
    if (SkinSmallButton(core::Tr("Recopier"), art)) {
      verb = CompanionAiLineVerb::kAcceptProposal;
    }
    ImGui::SameLine();
    if (SkinSmallButton(core::Tr("Non merci"), art)) {
      verb = CompanionAiLineVerb::kDeclineProposal;
    }
  }
  ImGui::PopID();
  return verb;
}

}  // namespace ro::ui
