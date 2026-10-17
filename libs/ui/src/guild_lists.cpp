// LES SOUS-ONGLETS « RELATIONS » ET « EXPULSIONS » : deux listes, l'une qu'on
// peut défaire, l'autre qu'on ne peut que lire.

#include <algorithm>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"

#include "guild_panes.h"
#include "ui_scale.h"

namespace ro::ui {
namespace {

constexpr float kCrossPixels = 20.0F;
constexpr float kCrossGapPixels = 6.0F;
constexpr float kRowLeastPixels = 60.0F;
constexpr float kBanNameColumnPixels = 130.0F;

}  // namespace

void DrawGuildRelations(GuildTabState& state, const GuildView& view, const SkinArt& art) {
  // ROMPRE EXIGE LE DRAPEAU DE MAÎTRE chez le serveur : la croix et le menu
  // n'existent que pour lui, comme le « Delete » du client.
  const bool may_break = view.master;
  for (const bool enemies : {false, true}) {
    GuildTextColored(enemies ? kGuildRedColor : kGuildGreenColor,
                     enemies ? core::Tr("Ennemis") : core::Tr("Alliés"));
    int shown = 0;
    for (std::size_t index = 0; index < view.relations.size(); ++index) {
      const GuildRelationView& relation = view.relations[index];
      if (relation.enemy != enemies) {
        continue;
      }
      const char* break_label =
          enemies ? core::Tr("Retirer l'hostilité…") : core::Tr("Rompre l'alliance…");
      const auto ask_break = [&state, &relation]() {
        state.target_guild = relation.guild_id;
        state.target_enemy = relation.enemy;
        state.target_name = std::string(relation.name);
        state.ask_relation = true;
      };
      ImGui::PushID(static_cast<int>(index));
      const float cross = Px(kCrossPixels);
      const float width =
          std::max(Px(kRowLeastPixels), ImGui::GetContentRegionAvail().x -
                                            (may_break ? cross + Px(kCrossGapPixels) : 0.0F));
      const std::string name(relation.name);
      ImGui::Selectable(name.c_str(), false, 0, ImVec2(width, 0.0F));
      if (may_break) {
        if (BeginSkinPopupContextItem("relation")) {
          GuildTextColored(kGuildLabelColor, name.c_str());
          ImGui::Separator();
          if (ImGui::MenuItem(break_label)) {
            ask_break();
            ImGui::CloseCurrentPopup();
          }
          EndSkinPopup();
        }
        ImGui::SameLine();
        if (SkinSmallButton("x", art, ImVec2(cross, 0.0F))) {
          ask_break();
        }
        GuildTooltip(break_label);
      }
      ImGui::PopID();
      ++shown;
    }
    if (shown == 0) {
      // ⚠ LE RETRAIT HORS DU TEXTE À TRADUIRE : une clé n'a pas de blanc au bord.
      GuildTextColored(kGuildLabelColor, (std::string("   ") + core::Tr("aucune")).c_str());
    }
    ImGui::Spacing();
  }
  PaintWindowScrollbar(art);
}

// PUREMENT INFORMATIF : rien ne permet de réintégrer quelqu'un depuis le client,
// et le serveur ne garde ni date ni auteur.
void DrawGuildBans(const GuildView& view) {
  if (!view.bans_known) {
    GuildTextColored(kGuildLabelColor,
                     core::Tr("Liste non encore reçue — clic sur « Actualiser »."));
    return;
  }
  if (view.bans.empty()) {
    GuildTextColored(kGuildLabelColor, core::Tr("Aucune expulsion enregistrée."));
    return;
  }
  const auto flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                     ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY;
  if (!ImGui::BeginTable("expulsions", 2, flags)) {
    return;
  }
  ImGui::TableSetupColumn(core::Tr("Personnage"), ImGuiTableColumnFlags_WidthFixed,
                          Px(kBanNameColumnPixels));
  ImGui::TableSetupColumn(core::Tr("Motif"), ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableHeadersRow();
  for (const GuildBanView& ban : view.bans) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    const std::string name = ban.name.empty() ? "?" : std::string(ban.name);
    ImGui::TextUnformatted(name.c_str());
    ImGui::TableNextColumn();
    if (ban.reason.empty()) {
      GuildTextColored(kGuildLabelColor, core::Tr("(aucun motif)"));
    } else {
      const std::string reason(ban.reason);
      ImGui::TextUnformatted(reason.c_str());
    }
  }
  ImGui::EndTable();
}

}  // namespace ro::ui
