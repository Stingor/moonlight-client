// L'onglet des titres de la fiche : celui qu'on porte, ceux qu'on possède, et
// le clic qui en change.

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/core/ascii.h"
#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"

#include "character_panes.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// LES TROIS TEINTES DE L'ONGLET. ⚠ ELLES NE SONT PAS DANS LA PALETTE : ce sont
// celles de cet onglet seul, et le vert en particulier est une nuance à
// lui, plus claire que celui des bonus.
constexpr ImVec4 kTitleCaptionColor(0.0F, 0.0F, 0.0F, 1.0F);
constexpr ImVec4 kEquippedTitleColor(0.15F, 0.55F, 0.20F, 1.0F);
constexpr ImVec4 kTitleFaintColor(0.35F, 0.35F, 0.42F, 1.0F);

// LA MARQUE DE LA LIGNE PORTÉE, et le blanc qui aligne les autres sur elle.
constexpr std::string_view kEquippedMark = "> ";
constexpr std::string_view kUnequippedMark = "   ";

}  // namespace

void DrawCharacterTitles(std::span<char> filter, const CharacterTitlesView& titles,
                         CharacterWindowAction& action) {
  ImGui::AlignTextToFramePadding();
  ColoredText(kTitleCaptionColor, core::Tr("Titre équipé :"));
  ImGui::SameLine();
  if (titles.equipped != 0) {
    ColoredText(kEquippedTitleColor, titles.equipped_label);
  } else {
    ColoredText(kTitleFaintColor, core::Tr("aucun"));
  }

  ImGui::Spacing();
  // Le filtre court sur toute la largeur : c'est lui qui retrouve un titre
  // quand la liste s'allonge.
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  ImGui::InputTextWithHint("##filtre", core::Tr("filtrer par nom…"), filter.data(), filter.size());
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // ⚠ LE CHOIX S'APPLIQUE APRÈS LA LISTE, pas pendant : la ligne cochée doit
  // rester celle que le serveur a dite tant qu'il n'a pas répondu.
  std::optional<std::int32_t> chosen;
  const std::string_view needle(filter.data());
  const bool filtering = !needle.empty();

  // « AUCUN TITRE » RETIRE CELUI QU'ON PORTE. ⚠ CACHÉ PENDANT UN FILTRE : ce
  // n'est pas un titre, et aucun libellé cherché ne le désigne.
  if (!filtering) {
    if (ImGui::Selectable(core::Tr("Aucun titre"), titles.equipped == 0)) {
      chosen = 0;
    }
    ImGui::Spacing();
  }

  if (titles.owned.empty()) {
    ColoredText(kTitleFaintColor,
                core::Tr("Aucun titre décroché. Complète des succès qui récompensent un titre."));
  }

  for (const CharacterTitleView& title : titles.owned) {
    if (filtering && !core::AsciiContainsFolded(title.label, needle)) {
      continue;
    }
    ImGui::PushID(title.title_id);
    const bool equipped = title.title_id == titles.equipped;
    std::string row(equipped ? kEquippedMark : kUnequippedMark);
    row += title.label;
    if (ImGui::Selectable(row.c_str(), equipped)) {
      chosen = title.title_id;
    }
    SkinTooltip(equipped ? core::Tr("Titre actuellement équipé (clic : garder)")
                         : core::Tr("Clic : équiper ce titre"));
    ImGui::PopID();
  }

  if (chosen.has_value() && *chosen != titles.equipped) {
    action.equip_title = chosen;
  }
}

}  // namespace ro::ui
