// LES CINQ CONFIRMATIONS DE L'ONGLET GUILDE.
//
// 🔴 DEUX D'ENTRE ELLES SONT LE SEUL GARDE-FOU QUI EXISTE. La dissolution passe
// par une commande qui n'a ni argument ni confirmation côté serveur, et le
// transfert de la direction est sans retour depuis le client : dans les deux
// cas, le nom retapé est la seule barrière.

#include <array>
#include <cstdio>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "guild_panes.h"
#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

constexpr float kConfirmButtonPixels = 110.0F;
constexpr float kCancelButtonPixels = 100.0F;
constexpr float kFieldPixels = 240.0F;

// « Bob : expulsion envoyée. » — le nom dans un gabarit, pour que le texte à
// traduire n'ait pas de blanc au bord. Le gabarit porte un seul « %s »,
// remplacé par le nom tel quel : une traduction venue d'un fichier ne passe
// jamais par printf.
[[nodiscard]] std::string Reported(std::string_view pattern, const std::string& name) {
  constexpr std::string_view kNameMark = "%s";
  std::string line(pattern);
  if (const std::size_t at = line.find(kNameMark); at != std::string::npos) {
    line.replace(at, kNameMark.size(), name);
  }
  return line;
}

[[nodiscard]] bool ConfirmButton(const char* label, const SkinArt& art) {
  return SkinButton(label, art, ImVec2(Px(kConfirmButtonPixels), 0.0F));
}

// « Annuler », qui referme la boîte.
void CancelButton(const SkinArt& art) {
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art, ImVec2(Px(kCancelButtonPixels), 0.0F))) {
    ImGui::CloseCurrentPopup();
  }
}

void ReasonField(GuildTabState& state, const char* id) {
  ColoredText(kGuildLabelColor, core::Tr("Motif (facultatif) :"));
  ImGui::SetNextItemWidth(Px(kFieldPixels));
  ImGui::InputText(id, state.reason.data(), state.reason.size());
  ImGui::Spacing();
}

void DrawRelationBox(GuildTabState& state, const SkinArt& art, GuildAction& action) {
  const std::string id = GuildModalName(core::Tr("Rompre la relation"), kGuildRelationModalId);
  if (!BeginSkinnedModal(id.c_str(), art)) {
    return;
  }
  std::array<char, 256> line = {};
  std::snprintf(line.data(), line.size(),
                state.target_enemy ? core::Tr("Retirer %s de la liste des ennemis ?")
                                   : core::Tr("Rompre l'alliance avec %s ?"),
                state.target_name.c_str());
  ImGui::TextUnformatted(line.data());
  ColoredText(kGuildLabelColor, core::Tr("Refusé par le serveur pendant une guerre de "
                                         "guildes\net sur les cartes verrouillées."));
  ImGui::Spacing();
  if (ConfirmButton(state.target_enemy ? core::Tr("Retirer") : core::Tr("Rompre"), art)) {
    action.verb = GuildVerb::kEndRelation;
    action.guild_id = state.target_guild;
    action.enemy = state.target_enemy;
    state.status = Reported(core::Tr("%s : demande envoyée."), state.target_name);
    ImGui::CloseCurrentPopup();
  }
  CancelButton(art);
  EndSkinnedModal();
}

void DrawLeaveBox(GuildTabState& state, const SkinArt& art, GuildAction& action) {
  const std::string id = GuildModalName(core::Tr("Quitter la guilde"), kGuildLeaveModalId);
  if (!BeginSkinnedModal(id.c_str(), art)) {
    return;
  }
  ImGui::TextUnformatted(core::Tr("Quitter définitivement la guilde ?"));
  ColoredText(kGuildLabelColor, core::Tr("Il faudra une nouvelle invitation pour y revenir."));
  ImGui::Spacing();
  ReasonField(state, "##motif_depart");
  if (ConfirmButton(core::Tr("Quitter"), art)) {
    action.verb = GuildVerb::kLeave;
    action.text = std::string(BufferText(state.reason));
    state.status = core::Tr("Départ envoyé.");
    ImGui::CloseCurrentPopup();
  }
  CancelButton(art);
  EndSkinnedModal();
}

void DrawDisbandBox(GuildTabState& state, const GuildView& view, const SkinArt& art,
                    GuildAction& action) {
  const std::string id = GuildModalName(core::Tr("Dissoudre la guilde"), kGuildDisbandModalId);
  if (!BeginSkinnedModal(id.c_str(), art)) {
    return;
  }
  std::array<char, 256> line = {};
  const std::string name(view.name);
  std::snprintf(line.data(), line.size(), core::Tr("Dissoudre « %s » ?"), name.c_str());
  ColoredText(kGuildRedColor, line.data());
  ColoredText(kGuildLabelColor, core::Tr("Irréversible : la guilde, ses postes, son storage "
                                         "et ses\nskills disparaissent."));
  ImGui::Spacing();
  // LES CONDITIONS RÉELLES DU SERVEUR, dites AVANT : deux des refus sont peu
  // bavards, et l'un est muet.
  ColoredText(kGuildBlackColor, core::Tr("Le serveur refusera si :"));
  ImGui::BulletText("%s", core::Tr("tu n'es pas le maître de guilde ;"));
  ImGui::BulletText("%s", core::Tr("il reste un autre membre — il faut être SEUL ;"));
  ImGui::BulletText("%s", core::Tr("la carte interdit les modifications de guilde\n(mapflag "
                                   "guildlock) ;"));
  ImGui::BulletText("%s", core::Tr("une instance de guilde est en cours."));
  ColoredText(kGuildLabelColor,
              core::Tr("L'instance fait échouer la dissolution SANS aucun message."));
  ImGui::Spacing();
  ColoredText(kGuildLabelColor, core::Tr("Retape le nom de la guilde pour confirmer :"));
  ImGui::SetNextItemWidth(Px(kFieldPixels));
  ImGui::InputTextWithHint("##dissolution", core::Tr("Nom exact de la guilde"),
                           state.disband_confirm.data(), state.disband_confirm.size());
  ImGui::Spacing();
  const bool matches = !view.name.empty() && BufferText(state.disband_confirm) == view.name;
  ImGui::BeginDisabled(!matches);
  if (ConfirmButton(core::Tr("Dissoudre"), art)) {
    action.verb = GuildVerb::kDisband;
    state.status = core::Tr("Dissolution demandée.");
    state.disband_confirm.fill('\0');
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndDisabled();
  CancelButton(art);
  EndSkinnedModal();
}

void DrawExpelBox(GuildTabState& state, const SkinArt& art, GuildAction& action) {
  const std::string id = GuildModalName(core::Tr("Expulser de la guilde"), kGuildExpelModalId);
  if (!BeginSkinnedModal(id.c_str(), art)) {
    return;
  }
  std::array<char, 256> line = {};
  std::snprintf(line.data(), line.size(), core::Tr("Expulser %s de la guilde ?"),
                state.target_name.c_str());
  ImGui::TextUnformatted(line.data());
  ImGui::Spacing();
  ReasonField(state, "##motif_expulsion");
  if (ConfirmButton(core::Tr("Expulser"), art)) {
    action.verb = GuildVerb::kExpel;
    action.account_id = state.target_account;
    action.char_id = state.target_char;
    action.text = std::string(BufferText(state.reason));
    state.status = Reported(core::Tr("%s : expulsion envoyée."), state.target_name);
    ImGui::CloseCurrentPopup();
  }
  CancelButton(art);
  EndSkinnedModal();
}

void DrawTransferBox(GuildTabState& state, const GuildView& view, const SkinArt& art,
                     GuildAction& action) {
  const std::string id = GuildModalName(core::Tr("Transférer la direction"), kGuildTransferModalId);
  if (!BeginSkinnedModal(id.c_str(), art)) {
    return;
  }
  std::array<char, 256> line = {};
  const std::string name(view.name);
  std::snprintf(line.data(), line.size(), core::Tr("Faire de %s le maître de « %s » ?"),
                state.target_name.c_str(), name.c_str());
  ColoredText(kGuildRedColor, line.data());
  ColoredText(kGuildLabelColor,
              core::Tr("Tu perds le poste de maître sur-le-champ : postes, emblème,\n"
                       "annonce, invitations et dissolution passent à ce membre.\n"
                       "Lui seul pourra te rendre la direction."));
  ImGui::Spacing();
  ColoredText(kGuildBlackColor, core::Tr("Le serveur refusera si :"));
  ImGui::BulletText("%s", core::Tr("une guerre de guildes est en cours ;"));
  ImGui::BulletText("%s", core::Tr("le précédent transfert est trop récent (délai serveur) ;"));
  ImGui::BulletText("%s", core::Tr("une instance de guilde est en cours — refus SANS message."));
  ImGui::Spacing();
  ColoredText(kGuildLabelColor, core::Tr("Retape le nom du membre pour confirmer :"));
  ImGui::SetNextItemWidth(Px(kFieldPixels));
  ImGui::InputTextWithHint("##transfert", core::Tr("Nom exact du membre"),
                           state.transfer_confirm.data(), state.transfer_confirm.size());
  ImGui::Spacing();
  const bool matches =
      !state.target_name.empty() && BufferText(state.transfer_confirm) == state.target_name;
  ImGui::BeginDisabled(!matches);
  if (ConfirmButton(core::Tr("Transférer"), art)) {
    action.verb = GuildVerb::kTransferMaster;
    action.account_id = state.target_account;
    action.char_id = state.target_char;
    state.status = Reported(core::Tr("%s : transfert de la direction envoyé."), state.target_name);
    state.transfer_confirm.fill('\0');
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndDisabled();
  CancelButton(art);
  EndSkinnedModal();
}

}  // namespace

void DrawGuildConfirmations(GuildTabState& state, const GuildView& view, const SkinArt& art,
                            GuildAction& action) {
  // LES DEMANDES VENUES D'UN MENU s'ouvrent ICI, au niveau de l'onglet, pour que
  // l'identifiant soit celui de la boîte.
  if (state.ask_expel) {
    state.ask_expel = false;
    AskModal(GuildModalName(core::Tr("Expulser de la guilde"), kGuildExpelModalId).c_str());
  }
  if (state.ask_relation) {
    state.ask_relation = false;
    AskModal(GuildModalName(core::Tr("Rompre la relation"), kGuildRelationModalId).c_str());
  }
  if (state.ask_transfer) {
    state.ask_transfer = false;
    AskModal(GuildModalName(core::Tr("Transférer la direction"), kGuildTransferModalId).c_str());
  }
  DrawRelationBox(state, art, action);
  DrawLeaveBox(state, art, action);
  DrawDisbandBox(state, view, art, action);
  DrawExpelBox(state, art, action);
  DrawTransferBox(state, view, art, action);
}

}  // namespace ro::ui
