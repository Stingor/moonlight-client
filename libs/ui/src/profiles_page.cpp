#include "ro/ui/profiles_page.h"

#include <array>
#include <cstdio>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/list_keys.h"
#include "ro/ui/skin_widgets.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// La largeur de la liste et du champ du nom, en pixels d'interface.
constexpr float kFieldPixels = 200.0F;
constexpr std::size_t kLineBytes = 160;

[[nodiscard]] bool InRange(const ProfilesPageView& view, int rank) {
  return rank >= 0 && static_cast<std::size_t>(rank) < view.labels.size() &&
         static_cast<std::size_t>(rank) < view.names.size();
}

[[nodiscard]] const char* LabelAt(const ProfilesPageView& view, int rank) {
  return InRange(view, rank) ? view.labels[static_cast<std::size_t>(rank)] : "?";
}

[[nodiscard]] std::string NameAt(const ProfilesPageView& view, int rank) {
  return InRange(view, rank) ? view.names[static_cast<std::size_t>(rank)] : std::string();
}

// Ce nom désigne-t-il déjà un profil, par son nom ou par son libellé ?
[[nodiscard]] bool Taken(const ProfilesPageView& view, std::string_view name) {
  for (std::size_t rank = 0; rank < view.names.size() && rank < view.labels.size(); ++rank) {
    if (view.names[rank] == name || std::string_view(view.labels[rank]) == name) {
      return true;
    }
  }
  return false;
}

// UN BOUTON DE LA PAGE, grisé quand il ne peut rien, et son infobulle.
[[nodiscard]] bool PageButton(const char* label, bool usable, const char* tip, const SkinArt& art) {
  ImGui::BeginDisabled(!usable);
  const bool clicked = SkinSmallButton(label, art);
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltip(tip);
  }
  return clicked;
}

void DrawHeader(const ProfilesPageView& view) {
  ImGui::SeparatorText(view.title);
  std::array<char, kLineBytes> line{};
  if (view.character != nullptr) {
    std::snprintf(line.data(), line.size(), core::Tr("Profil de %s : %s"), view.character,
                  LabelAt(view, view.active));
  } else {
    std::snprintf(line.data(), line.size(), core::Tr("Profil posé : %s"),
                  LabelAt(view, view.active));
  }
  ImGui::TextUnformatted(line.data());
  ImGui::SameLine();
  SkinHelpMarker(view.help);
}

// La liste, et ce qu'on fait du profil qu'on y désigne.
void DrawChoiceLine(const ProfilesPageView& view, ProfilesPageState& state, int selected,
                    const SkinArt& art, ProfilesPageAction& asked) {
  const int count = static_cast<int>(view.labels.size());
  ImGui::SetNextItemWidth(Px(kFieldPixels));
  int step = 0;
  const bool open = BeginSkinCombo("##profile", LabelAt(view, selected), art, &step);
  if (const int to = SkinComboStepped(selected, step, count); to != selected) {
    state.selected = to;
    state.confirming = ProfileGesture::kNone;
  }
  if (open) {
    ComboKeys keys(selected, count);
    for (int rank = 0; rank < count; ++rank) {
      ImGui::PushID(rank);
      if (keys.Took(rank, ImGui::Selectable(LabelAt(view, rank), rank == selected))) {
        state.selected = rank;
        state.confirming = ProfileGesture::kNone;
      }
      ImGui::PopID();
    }
    EndSkinCombo();
  }
  ImGui::SameLine();
  if (PageButton(core::Tr("Charger##profile"), selected != view.active,
                 core::Tr("Pose ce profil, et le retient pour ce personnage."), art)) {
    asked.gesture = ProfileGesture::kLoad;
    asked.target = NameAt(view, selected);
    state.selected = -1;
  }
  ImGui::SameLine();
  // ⚠ LE PROFIL POSÉ S'ENREGISTRE DE LUI-MÊME : l'écraser ne changerait rien.
  if (PageButton(core::Tr("Écraser##profile"), selected != view.active,
                 core::Tr("Remplace ce profil par l'état présent, sans le poser."), art)) {
    state.confirming = ProfileGesture::kOverwrite;
  }
  ImGui::SameLine();
  if (PageButton(core::Tr("Réinitialiser##profile"), true,
                 core::Tr("Remet ce profil aux réglages d'origine du client."), art)) {
    state.confirming = ProfileGesture::kReset;
  }
  ImGui::SameLine();
  if (PageButton(core::Tr("Supprimer##profile"), selected != view.fixed,
                 core::Tr("Supprime ce profil. Les personnages qui le posaient reprennent "
                          "« Défaut »."),
                 art)) {
    state.confirming = ProfileGesture::kDelete;
  }
}

// LA QUESTION D'UN GESTE QU'ON NE DÉFAIT PAS.
void DrawConfirmLine(const ProfilesPageView& view, ProfilesPageState& state, int selected,
                     const SkinArt& art, ProfilesPageAction& asked) {
  if (state.confirming == ProfileGesture::kNone ||
      (state.confirming == ProfileGesture::kDelete && selected == view.fixed) ||
      (state.confirming == ProfileGesture::kOverwrite && selected == view.active)) {
    return;
  }
  // Un `snprintf` par question : le format reste un littéral que le compilateur
  // confronte à ses arguments.
  std::array<char, kLineBytes> line{};
  const char* const label = LabelAt(view, selected);
  if (state.confirming == ProfileGesture::kDelete) {
    std::snprintf(line.data(), line.size(), core::Tr("Supprimer le profil « %s » ?"), label);
  } else if (state.confirming == ProfileGesture::kOverwrite) {
    std::snprintf(line.data(), line.size(),
                  core::Tr("Écraser le profil « %s » par l'état présent ?"), label);
  } else {
    std::snprintf(line.data(), line.size(), core::Tr("Réinitialiser le profil « %s » ?"), label);
  }
  ImGui::TextUnformatted(line.data());
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Confirmer"), art)) {
    asked.gesture = state.confirming;
    asked.target = NameAt(view, selected);
    state.confirming = ProfileGesture::kNone;
    if (asked.gesture == ProfileGesture::kDelete) {
      state.selected = -1;
    }
  }
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Annuler"), art)) {
    state.confirming = ProfileGesture::kNone;
  }
}

// Le nom, et ce qui s'en sert : enregistrer sous, dupliquer, renommer.
void DrawNameLine(const ProfilesPageView& view, ProfilesPageState& state, int selected,
                  const SkinArt& art, ProfilesPageAction& asked) {
  ImGui::SetNextItemWidth(Px(kFieldPixels));
  ImGui::InputTextWithHint("##profile_name", core::Tr("Nom du profil"), state.name.data(),
                           state.name.size());
  const std::string_view typed(state.name.data());
  const bool taken = Taken(view, typed);
  const bool fresh = !typed.empty() && !taken && !view.full;

  const auto ask = [&](ProfileGesture gesture) {
    asked.gesture = gesture;
    asked.target = NameAt(view, selected);
    asked.new_name = std::string(typed);
    state.name = {};
    state.selected = -1;
  };
  ImGui::SameLine();
  // ⚠ UN NOM DÉJÀ PRIS ÉCRASE CE PROFIL : c'est ainsi qu'on met à jour un
  // profil qu'on n'a pas posé.
  if (PageButton(core::Tr("Enregistrer sous##profile"), !typed.empty() && (taken || !view.full),
                 core::Tr("Enregistre l'état présent sous ce nom, et le pose. Un nom déjà pris "
                          "remplace ce profil."),
                 art)) {
    ask(ProfileGesture::kSaveAs);
  }
  ImGui::SameLine();
  if (PageButton(core::Tr("Dupliquer##profile"), fresh,
                 core::Tr("Copie le profil choisi sous ce nom, sans le poser."), art)) {
    ask(ProfileGesture::kDuplicate);
  }
  ImGui::SameLine();
  if (PageButton(core::Tr("Renommer##profile"), !typed.empty() && !taken && selected != view.fixed,
                 core::Tr("Renomme le profil choisi. Les personnages qui le posaient suivent."),
                 art)) {
    ask(ProfileGesture::kRename);
  }
}

}  // namespace

ProfilesPageAction DrawProfilesPage(const ProfilesPageView& view, ProfilesPageState& state,
                                    const SkinArt& art) {
  ProfilesPageAction asked;
  if (view.labels.empty()) {
    return asked;
  }
  ImGui::PushID(view.id);
  const int count = static_cast<int>(view.labels.size());
  const int selected = state.selected >= 0 && state.selected < count ? state.selected : view.active;
  DrawHeader(view);
  DrawChoiceLine(view, state, selected, art, asked);
  DrawConfirmLine(view, state, selected, art, asked);
  DrawNameLine(view, state, selected, art, asked);
  ImGui::PopID();
  return asked;
}

}  // namespace ro::ui
