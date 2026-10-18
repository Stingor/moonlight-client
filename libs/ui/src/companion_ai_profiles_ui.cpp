// L'EN-TÊTE DES PROFILS DE LA FENÊTRE « IA DU COMPAGNON » : le profil édité,
// Nouveau, Renommer, Dupliquer, Supprimer et Importer…, les trois contextes, et
// la proposition de recopie. Voir `ro/ui/companion_ai_window.h`.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/list_keys.h"
#include "ro/ui/skin_widgets.h"

#include "companion_ai_window_parts.h"
#include "ui_scale.h"

namespace ro::ui::companion_ai_parts {
namespace {

// La largeur des listes de l'en-tête et du champ du nom, en pixels de police.
constexpr float kProfileComboPixels = 200.0F;
// La liste d'import, dont chaque ligne porte aussi un personnage et une classe.
constexpr float kImportComboPixels = 2.0F * kProfileComboPixels;
constexpr std::size_t kLineBytes = 160;
// Le fond de la bannière de proposition.
constexpr ImU32 kBannerColor = IM_COL32(70, 110, 160, 90);

constexpr std::array<const char*, kCompanionAiContexts> kContextLabels = {"PvE", "PvP", "WoE"};

[[nodiscard]] const char* LabelAt(const CompanionProfilesView& profiles, int rank) {
  return rank >= 0 && static_cast<std::size_t>(rank) < profiles.profiles.size()
             ? profiles.profiles[static_cast<std::size_t>(rank)].label.c_str()
             : "?";
}

[[nodiscard]] std::string KeyAt(const CompanionProfilesView& profiles, int rank) {
  return rank >= 0 && static_cast<std::size_t>(rank) < profiles.profiles.size()
             ? profiles.profiles[static_cast<std::size_t>(rank)].key
             : std::string();
}

// UN AUTRE PROFIL ÉDITÉ : ce qui était déplié ne le concerne plus.
void Edit(CompanionAiWindowState& state, std::string key) {
  state.edited_profile = std::move(key);
  state.expanded_rule = -1;
  state.expanded_tactic = -1;
  state.expanded_conduct = -1;
  state.profile_dialog = ProfileDialog::kNone;
}

// UN BOUTON DE L'EN-TÊTE, grisé avec sa raison.
[[nodiscard]] bool HeaderButton(const char* label, const char* refusal, const char* tip,
                                const SkinArt& art) {
  ImGui::BeginDisabled(refusal != nullptr);
  const bool clicked = SkinSmallButton(label, art);
  ImGui::EndDisabled();
  SkinTooltip(refusal != nullptr ? refusal : tip);
  return clicked;
}

void Ask(CompanionAiWindowState& state, ProfileDialog dialog, const std::string& name) {
  state.profile_dialog = dialog;
  state.profile_name = {};
  name.copy(state.profile_name.data(), state.profile_name.size() - 1);
  state.import_choice = 0;
}

void DrawButtons(CompanionAiWindowState& state, const CompanionAiView& view, const SkinArt& art) {
  const CompanionProfilesView& profiles = view.profiles;
  const char* full = profiles.full ? core::Tr("Ce type a déjà tous ses profils.") : nullptr;
  const char* basic =
      view.basic ? core::Tr("L'IA basique ne se renomme ni ne se supprime.") : nullptr;
  if (HeaderButton(core::Tr("Nouveau"), full, core::Tr("Un profil neuf, copie de l'IA basique."),
                   art)) {
    Ask(state, ProfileDialog::kNew, std::string());
  }
  ImGui::SameLine();
  if (HeaderButton(core::Tr("Renommer"), basic, core::Tr("Renomme le profil édité."), art)) {
    // LA SAISIE PART DU NOM MONTRÉ, traduit pour « Mon profil ».
    Ask(state, ProfileDialog::kRename, LabelAt(profiles, profiles.edited));
  }
  ImGui::SameLine();
  if (HeaderButton(core::Tr("Dupliquer"), full, core::Tr("Un profil neuf, copie de celui-ci."),
                   art)) {
    Ask(state, ProfileDialog::kDuplicate, std::string());
  }
  ImGui::SameLine();
  if (HeaderButton(core::Tr("Supprimer"), basic, core::Tr("Supprime le profil édité."), art)) {
    Ask(state, ProfileDialog::kDelete, std::string());
  }
  ImGui::SameLine();
  const char* no_import = full != nullptr ? full
                          : profiles.imports.empty()
                              ? core::Tr("Aucun autre profil n'est connu du fichier.")
                              : nullptr;
  if (HeaderButton(core::Tr("Importer…"), no_import,
                   core::Tr("Recopie le profil d'un autre type, ou le profil actif d'un autre "
                            "personnage."),
                   art)) {
    Ask(state, ProfileDialog::kImport, std::string());
  }
}

// LA SAISIE QU'UN BOUTON A OUVERTE, et sa validation.
void DrawDialog(CompanionAiWindowState& state, const CompanionAiView& view, const SkinArt& art,
                CompanionAiWindowAction& action) {
  const CompanionProfilesView& profiles = view.profiles;
  if (state.profile_dialog == ProfileDialog::kNone) {
    return;
  }
  ImGui::PushID("profile_dialog");
  bool confirmed = false;
  if (state.profile_dialog == ProfileDialog::kDelete) {
    std::array<char, kLineBytes> line{};
    std::snprintf(line.data(), line.size(), core::Tr("Supprimer le profil « %s » ?"),
                  LabelAt(profiles, profiles.edited));
    ImGui::TextUnformatted(line.data());
    confirmed = SkinSmallButton(core::Tr("Confirmer"), art);
  } else {
    if (state.profile_dialog == ProfileDialog::kImport) {
      const int last = static_cast<int>(profiles.imports.size()) - 1;
      state.import_choice = std::clamp(state.import_choice, 0, std::max(last, 0));
      ImGui::SetNextItemWidth(Px(kImportComboPixels));
      const char* shown =
          profiles.imports.empty()
              ? "?"
              : profiles.imports[static_cast<std::size_t>(state.import_choice)].c_str();
      if (BeginSkinCombo("##import", shown, art)) {
        ComboKeys keys(state.import_choice, static_cast<int>(profiles.imports.size()));
        for (std::size_t index = 0; index < profiles.imports.size(); ++index) {
          ImGui::PushID(static_cast<int>(index));
          if (keys.Took(static_cast<int>(index),
                        ImGui::Selectable(profiles.imports[index].c_str(),
                                          state.import_choice == static_cast<int>(index)))) {
            state.import_choice = static_cast<int>(index);
          }
          ImGui::PopID();
        }
        EndSkinCombo();
      }
    }
    ImGui::SetNextItemWidth(Px(kProfileComboPixels));
    const bool entered = ImGui::InputTextWithHint(
        "##profile_name", core::Tr("Nom du profil"), state.profile_name.data(),
        state.profile_name.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    const std::string typed = TrimmedCharacterName(state.profile_name.data());
    ImGui::SameLine();
    ImGui::BeginDisabled(typed.empty());
    confirmed = SkinSmallButton(core::Tr("Valider"), art) || (entered && !typed.empty());
    ImGui::EndDisabled();
    action.name = typed;
  }
  ImGui::SameLine();
  const bool cancelled = SkinSmallButton(core::Tr("Annuler"), art);
  if (confirmed) {
    switch (state.profile_dialog) {
      case ProfileDialog::kNew:
        action.verb = CompanionAiVerb::kNewProfile;
        break;
      case ProfileDialog::kRename:
        action.verb = CompanionAiVerb::kRenameProfile;
        break;
      case ProfileDialog::kDuplicate:
        action.verb = CompanionAiVerb::kDuplicateProfile;
        break;
      case ProfileDialog::kImport:
        action.verb = CompanionAiVerb::kImportProfile;
        action.choice = state.import_choice;
        break;
      case ProfileDialog::kDelete:
        action.verb = CompanionAiVerb::kDeleteProfile;
        break;
      case ProfileDialog::kNone:
        break;
    }
  }
  if (confirmed || cancelled) {
    state.profile_dialog = ProfileDialog::kNone;
    state.profile_name = {};
  }
  if (!confirmed) {
    action.name.clear();
  }
  ImGui::PopID();
}

// LES TROIS CONTEXTES : le profil actif de ce personnage dans chacun, ou son
// repli ; celui de la carte est marqué.
void DrawContexts(const CompanionAiView& view, const SkinArt& art,
                  CompanionAiWindowAction& action) {
  const CompanionProfilesView& profiles = view.profiles;
  for (std::size_t context = 0; context < kCompanionAiContexts; ++context) {
    ImGui::PushID(static_cast<int>(context));
    const bool current = static_cast<int>(context) == profiles.current_context;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(kContextLabels[context]);
    ImGui::SameLine();
    const int chosen = profiles.chosen[context];
    ImGui::SetNextItemWidth(Px(kProfileComboPixels));
    const char* shown =
        chosen >= 0 ? LabelAt(profiles, chosen) : profiles.fallback[context].c_str();
    if (BeginSkinCombo("##context", shown, art)) {
      // Le rang zéro est le profil par défaut, les profils suivent.
      ComboKeys keys(chosen < 0 ? 0 : chosen + 1, static_cast<int>(profiles.profiles.size()) + 1);
      if (keys.Took(0, ImGui::Selectable(profiles.fallback[context].c_str(), chosen < 0) &&
                           chosen >= 0)) {
        action.verb = CompanionAiVerb::kSetContextProfile;
        action.context = static_cast<int>(context);
        action.choice = -1;
      }
      for (std::size_t rank = 0; rank < profiles.profiles.size(); ++rank) {
        ImGui::PushID(static_cast<int>(rank));
        if (keys.Took(static_cast<int>(rank) + 1,
                      ImGui::Selectable(profiles.profiles[rank].label.c_str(),
                                        chosen == static_cast<int>(rank)) &&
                          chosen != static_cast<int>(rank))) {
          action.verb = CompanionAiVerb::kSetContextProfile;
          action.context = static_cast<int>(context);
          action.choice = static_cast<int>(rank);
        }
        ImGui::PopID();
      }
      EndSkinCombo();
    }
    if (current) {
      ImGui::SameLine();
      ImGui::TextDisabled("%s", core::Tr("(carte actuelle)"));
    }
    ImGui::PopID();
  }
}

// UN PROFIL QU'AUCUN CONTEXTE NE PREND n'agit nulle part : la ligne le dit, et
// chaque bouton le fait prendre par un contexte.
void DrawUnused(const CompanionProfilesView& profiles, const SkinArt& art,
                CompanionAiWindowAction& action) {
  if (!profiles.unused) {
    return;
  }
  ImGui::PushID("unused");
  companion_ai_parts::WrappedNote(
      core::Tr("Ce profil n'est actif dans aucun contexte : il n'agit pas."));
  const std::array<const char*, kCompanionAiContexts> uses = {
      core::Tr("Utiliser en PvE"), core::Tr("Utiliser en PvP"), core::Tr("Utiliser en WoE")};
  for (std::size_t context = 0; context < kCompanionAiContexts; ++context) {
    ImGui::PushID(static_cast<int>(context));
    if (context != 0) {
      ImGui::SameLine();
    }
    if (SkinSmallButton(uses[context], art)) {
      action.verb = CompanionAiVerb::kSetContextProfile;
      action.context = static_cast<int>(context);
      action.choice = profiles.edited;
    }
    ImGui::PopID();
  }
  ImGui::PopID();
}

}  // namespace

void DrawProposalBanner(const CompanionProfilesView& profiles, const SkinArt& art,
                        CompanionAiWindowAction& action) {
  if (profiles.proposal.empty()) {
    return;
  }
  ImGui::PushID("proposal");
  // LE FOND SE PEINT SOUS LE TEXTE : le contenu va sur le calque du dessus, le
  // fond, dont la taille n'est connue qu'à la fin, sur celui du dessous.
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->ChannelsSplit(2);
  draw->ChannelsSetCurrent(1);
  const ImVec2 top = ImGui::GetCursorScreenPos();
  companion_ai_parts::WrappedNote(profiles.proposal.c_str());
  if (SkinSmallButton(core::Tr("Recopier"), art)) {
    action.verb = CompanionAiVerb::kAnswerProposal;
    action.accept = true;
  }
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Non merci"), art)) {
    action.verb = CompanionAiVerb::kAnswerProposal;
    action.accept = false;
  }
  const ImVec2 bottom(top.x + ImGui::GetContentRegionAvail().x, ImGui::GetItemRectMax().y);
  draw->ChannelsSetCurrent(0);
  draw->AddRectFilled(top, bottom, kBannerColor);
  draw->ChannelsMerge();
  ImGui::Separator();
  ImGui::PopID();
}

void DrawProfileHeader(CompanionAiWindowState& state, const CompanionAiView& view,
                       const SkinArt& art, CompanionAiWindowAction& action) {
  const CompanionProfilesView& profiles = view.profiles;
  ImGui::PushID("profile_header");
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(core::Tr("Profil édité :"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(Px(kProfileComboPixels));
  if (BeginSkinCombo("##edited", LabelAt(profiles, profiles.edited), art)) {
    ComboKeys keys(profiles.edited, static_cast<int>(profiles.profiles.size()));
    for (std::size_t rank = 0; rank < profiles.profiles.size(); ++rank) {
      ImGui::PushID(static_cast<int>(rank));
      if (keys.Took(static_cast<int>(rank),
                    ImGui::Selectable(profiles.profiles[rank].label.c_str(),
                                      profiles.edited == static_cast<int>(rank)) &&
                        profiles.edited != static_cast<int>(rank))) {
        Edit(state, KeyAt(profiles, static_cast<int>(rank)));
      }
      ImGui::PopID();
    }
    EndSkinCombo();
  }
  DrawButtons(state, view, art);
  DrawDialog(state, view, art, action);
  DrawUnused(profiles, art, action);
  ImGui::SeparatorText(core::Tr("Profil actif de ce personnage"));
  DrawContexts(view, art, action);
  ImGui::PopID();
}

}  // namespace ro::ui::companion_ai_parts
