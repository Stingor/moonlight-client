#include "ro/ui/block_list_window.h"

#include <algorithm>
#include <cfloat>
#include <string>

#include <imgui.h>

#include "ro/core/case_fold.h"
#include "ro/core/i18n.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "ui_scale.h"
#include "ui_strings.h"

namespace ro::ui {
namespace {

// ── LES MESURES ─────────────────────────────────────────────────────────────
constexpr float kFirstWidthPixels = 420.0F;
constexpr float kFirstHeightPixels = 380.0F;
constexpr float kLeastWidthPixels = 320.0F;
constexpr float kLeastHeightPixels = 220.0F;
constexpr int kColumnCount = 4;
// Le filtre ne paraît qu'au-delà de ce nombre : sur une liste courte, il
// n'est qu'un champ de plus à comprendre.
constexpr std::size_t kFilterShownFrom = 8;

void Clear(std::array<char, kBlockNameBytes>& buffer) { buffer.fill('\0'); }

// LE CHAMP DU HAUT : un nom, et « Bloquer ».
void DrawAddRow(BlockListWindowState& state, const BlockListView& view, const SkinArt& art,
                BlockListAnswer& answer) {
  const char* const block = core::Tr("Bloquer");
  ImGui::SetNextItemWidth(std::max(0.0F, ImGui::GetContentRegionAvail().x -
                                             SkinButtonWidth(block, art) -
                                             ImGui::GetStyle().ItemSpacing.x));
  const bool submitted = ImGui::InputTextWithHint(
      "##bloquer_nom", core::Tr("Nom du personnage à bloquer"), state.name.data(),
      state.name.size(), ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::SameLine();
  const std::string_view name = TypedText(state.name);
  ImGui::BeginDisabled(name.empty() || !view.connected);
  const bool clicked = SkinButton(block, art);
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltip(view.connected ? core::Tr("Bloque ses chuchotements, et sa voix s'il est à "
                                          "portée de voix. La casse compte : « bob » n'est pas "
                                          "« Bob ».")
                               : core::Tr("Hors ligne : le serveur n'écoute pas."));
  }
  if (!name.empty() && view.connected && (clicked || submitted)) {
    answer.verb = BlockListVerb::kBlock;
    answer.name = std::string(name);
    Clear(state.name);
  }
}

// La case d'un canal, sur une ligne : rend le geste quand elle change.
void DrawChannelBox(const char* id, bool blocked, bool usable, const char* refused,
                    const SkinArt& art, BlockListVerb block, BlockListVerb allow,
                    BlockListVerb& chosen) {
  bool ticked = blocked;
  ImGui::BeginDisabled(!usable);
  if (SkinCheckBox(id, &ticked, art)) {
    chosen = ticked ? block : allow;
  }
  ImGui::EndDisabled();
  if (!usable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltip(refused);
  }
}

void DrawPlayers(const BlockListWindowState& state, const BlockListView& view, const SkinArt& art,
                 BlockListAnswer& answer) {
  if (view.players.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Vous n'avez bloqué personne."));
    return;
  }
  const std::vector<std::size_t> shown = BlockListShown(view.players, TypedText(state.filter));
  if (shown.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucun joueur bloqué ne porte ce nom."));
    return;
  }
  if (!ImGui::BeginTable("##joueurs_bloques_lignes", kColumnCount,
                         ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
    return;
  }
  ImGui::TableSetupColumn(core::Tr("Joueur"), ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(core::Tr("Chuchotements"));
  ImGui::TableSetupColumn(core::Tr("Voix"));
  ImGui::TableSetupColumn("##debloquer");
  ImGui::TableHeadersRow();
  const char* const voice_refused =
      view.voice_online ? core::Tr(
                              "Hors de portée de voix : sa voix se coupe quand vous "
                              "l'entendez.")
                        : core::Tr("La voix est fermée.");
  const char* const offline = core::Tr("Hors ligne : le serveur n'écoute pas.");
  for (const std::size_t rank : shown) {
    const BlockedPlayerView& player = view.players[rank];
    ImGui::PushID(static_cast<int>(rank));
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(player.name.c_str());
    BlockListVerb chosen = BlockListVerb::kNothing;
    ImGui::TableNextColumn();
    DrawChannelBox("##chuchotements", player.whispers, view.connected, offline, art,
                   BlockListVerb::kBlockWhispers, BlockListVerb::kAllowWhispers, chosen);
    ImGui::TableNextColumn();
    DrawChannelBox("##voix", player.voice, view.voice_online && player.voice_reachable,
                   voice_refused, art, BlockListVerb::kBlockVoice, BlockListVerb::kAllowVoice,
                   chosen);
    ImGui::TableNextColumn();
    ImGui::BeginDisabled(!view.connected && !(view.voice_online && player.voice));
    if (SkinSmallButton(core::Tr("Débloquer"), art)) {
      chosen = BlockListVerb::kAllowBoth;
    }
    ImGui::EndDisabled();
    if (chosen != BlockListVerb::kNothing) {
      answer.verb = chosen;
      answer.name = player.name;
    }
    ImGui::PopID();
  }
  ImGui::EndTable();
}

// CE QUE LA LISTE NE PEUT PAS MONTRER, dit sous elle, en gris et replié.
void DrawNotes(const BlockListView& view) {
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextWrapped("%s", view.voice_online
                               ? core::Tr("Un joueur bloqué à la voix ne paraît ici que s'il est "
                                          "en ligne.")
                               : core::Tr("La voix est fermée : ses blocages ne se montrent pas."));
  if (!view.whispers_kept) {
    ImGui::TextWrapped("%s", core::Tr("Le service web ne garde pas la liste des chuchotements : "
                                      "elle s'oubliera à la déconnexion."));
  }
  ImGui::PopStyleColor();
}

// LE PIED : « tout le monde » à gauche, « Fermer » à droite.
void DrawFooter(BlockListWindowState& state, const BlockListView& view, const SkinArt& art,
                BlockListAnswer& answer) {
  bool everyone = view.whispers_from_everyone;
  ImGui::BeginDisabled(!view.connected);
  if (SkinCheckBox(core::Tr("Bloquer tous les chuchotements"), &everyone, art)) {
    answer.verb = everyone ? BlockListVerb::kBlockEveryone : BlockListVerb::kAllowEveryone;
    answer.name.clear();
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltip(
        core::Tr("Personne ne peut plus vous chuchoter, liste ou non. Décochée, la liste "
                 "reprend : ceux qui y sont restent bloqués."));
  }
  ImGui::SameLine();
  const char* const close = core::Tr("Fermer");
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0F, ImGui::GetContentRegionAvail().x -
                                                                   SkinButtonWidth(close, art)));
  if (SkinButton(close, art)) {
    state.open = false;
  }
}

}  // namespace

void RaiseBlockListWindow(BlockListWindowState& state) noexcept {
  state.open = true;
  state.raise = true;
}

std::vector<std::size_t> BlockListShown(std::span<const BlockedPlayerView> players,
                                        std::string_view filter) {
  const std::string wanted = core::CaseFolded(filter);
  std::vector<std::size_t> shown;
  for (std::size_t rank = 0; rank < players.size(); ++rank) {
    if (wanted.empty() || core::CaseFolded(players[rank].name).find(wanted) != std::string::npos) {
      shown.push_back(rank);
    }
  }
  return shown;
}

BlockListAnswer DrawBlockListWindow(BlockListWindowState& state, const BlockListView& view,
                                    const SkinArt& art) {
  BlockListAnswer answer;
  if (!state.open) {
    return answer;
  }
  if (state.raise) {
    state.raise = false;
    ImGui::SetNextWindowFocus();
  }
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver,
                          ImVec2(0.5F, 0.5F));
  ImGui::SetNextWindowSize(ImVec2(Px(kFirstWidthPixels), Px(kFirstHeightPixels)),
                           ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(Px(kLeastWidthPixels), Px(kLeastHeightPixels)),
                                      ImVec2(FLT_MAX, FLT_MAX));
  const std::string name = std::string(core::Tr("Joueurs bloqués")) + "###" + kBlockListWindowId;
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoScrollWithMouse;
  if (BeginSkinnedWindow(name.c_str(), &state.open, art, SkinnedTitle::kBar, kFlags) &&
      state.open) {
    ImGui::TextWrapped("%s", core::Tr("Un joueur bloqué ne peut plus vous chuchoter ; bloqué à "
                                      "la voix, vous ne l'entendez plus. Il n'en est pas averti."));
    DrawAddRow(state, view, art, answer);
    if (view.players.size() >= kFilterShownFrom) {
      ImGui::SetNextItemWidth(-FLT_MIN);
      ImGui::InputTextWithHint("##bloques_filtre", core::Tr("Rechercher…"), state.filter.data(),
                               state.filter.size());
    }
    ImGui::Separator();
    const float body_height =
        std::max(0.0F, ImGui::GetContentRegionAvail().y - SkinFooterHeight(art));
    if (ImGui::BeginChild("##joueurs_bloques_corps", ImVec2(0.0F, body_height))) {
      DrawPlayers(state, view, art, answer);
      DrawNotes(view);
      PaintWindowScrollbar(art);
    }
    ImGui::EndChild();
    DrawFooter(state, view, art, answer);
  }
  EndSkinnedWindow();
  return answer;
}

}  // namespace ro::ui
