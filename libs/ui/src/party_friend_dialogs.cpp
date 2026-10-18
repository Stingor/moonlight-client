// LES MENUS ET LES BOÎTES DE LA FENÊTRE AMIS / GROUPE.
//
// Un menu ne fait qu'ARMER : les gestes sans retour passent par une
// confirmation, ouverte au niveau de la fenêtre — ouverte dans le menu, elle
// naîtrait sous l'identifiant de la ligne et ne paraîtrait jamais.

#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/chat_link.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "party_friend_parts.h"

namespace ro::ui {
namespace {

constexpr const char* kConfirmId = "###party_confirm";
constexpr const char* kInviteId = "###party_invite";

[[nodiscard]] std::string ModalName(const char* title, const char* id) {
  return std::string(title) + id;
}

void CopyName(std::string_view name) { ImGui::SetClipboardText(std::string(name).c_str()); }

[[nodiscard]] const char* ConfirmQuestion(PartyFriendVerb verb) {
  switch (verb) {
    case PartyFriendVerb::kMakeLeader:
      return core::Tr("Céder le commandement du groupe à ce joueur ?");
    case PartyFriendVerb::kExpel:
      return core::Tr("Expulser ce joueur du groupe ?");
    case PartyFriendVerb::kLeave:
      return core::Tr("Quitter le groupe ?");
    case PartyFriendVerb::kRemoveFriend:
      return core::Tr("Retirer ce joueur de votre liste d'amis ?");
    case PartyFriendVerb::kNothing:
    case PartyFriendVerb::kWhisper:
    case PartyFriendVerb::kWhisperWindow:
    case PartyFriendVerb::kCreateParty:
    case PartyFriendVerb::kInvite:
    case PartyFriendVerb::kAddFriend:
    case PartyFriendVerb::kAnswerParty:
    case PartyFriendVerb::kAnswerFriend:
    case PartyFriendVerb::kPartyOptions:
    case PartyFriendVerb::kTarget:
    case PartyFriendVerb::kEntityMenu:
    case PartyFriendVerb::kInviteMvp:
      break;
  }
  return core::Tr("Confirmer ?");
}

}  // namespace

void AskPartyConfirm(PartyFriendWindowState& state, PartyFriendVerb verb, std::uint32_t account_id,
                     std::uint32_t char_id, std::string_view name) {
  state.confirm = verb;
  state.confirm_account = account_id;
  state.confirm_char = char_id;
  state.confirm_name = std::string(name);
  state.ask_confirm = true;
}

void DrawPartyRowMenu(PartyFriendWindowState& state, const PartyFriendView& view,
                      const PartyMemberView& member, PartyFriendAction& action) {
  if (!BeginSkinPopup(kPartyRowMenuId)) {
    return;
  }
  const std::string name(member.name);
  ImGui::TextDisabled("%s", name.c_str());
  ImGui::Separator();

  if (member.is_me) {
    if (!member.offline && ImGui::Selectable(core::Tr("Cibler"))) {
      action.verb = PartyFriendVerb::kTarget;
      action.account_id = member.account_id;
    }
  } else {
    // Pas de sprite : le menu d'entité est hors d'atteinte, on comble avec ce
    // qui voyage par nom.
    if (!member.offline && ImGui::Selectable(core::Tr("Chuchoter"))) {
      action.verb = PartyFriendVerb::kWhisper;
      action.account_id = member.account_id;
      action.name = name;
    }
    if (!member.offline && ImGui::Selectable(core::Tr("Chuchoter dans une fenêtre…"))) {
      action.verb = PartyFriendVerb::kWhisperWindow;
      action.account_id = member.account_id;
      action.name = name;
    }
    const bool friends_full = static_cast<int>(view.friends.size()) >= kFriendsMost;
    if (!member.is_friend && !friends_full && ImGui::Selectable(core::Tr("Ajouter à mes amis"))) {
      action.verb = PartyFriendVerb::kAddFriend;
      action.name = name;
    }
    // LE CARNET DE CHASSE MVP : la même entrée, et le même grisage, que dans le
    // menu d'un pseudo ou d'un membre de guilde. Connecté ou non : l'invitation
    // attend son retour.
    if (!view.mvp_invites_withdrawn && MvpInviteMenuItem(member.mvp_refusal)) {
      action.verb = PartyFriendVerb::kInviteMvp;
      action.name = name;
    }
    if (ImGui::Selectable(core::Tr("Copier le nom"))) {
      CopyName(member.name);
    }
  }

  const bool leads_other = view.i_am_leader && !member.is_me;
  if (leads_other || member.is_me) {
    ImGui::Separator();
  }
  if (leads_other) {
    const char* refusal = PartyLeaderRefusal(view, member);
    if (ImGui::Selectable(
            core::Tr("Nommer chef de groupe"), false,
            refusal != nullptr ? ImGuiSelectableFlags_Disabled : ImGuiSelectableFlags_None)) {
      AskPartyConfirm(state, PartyFriendVerb::kMakeLeader, member.account_id, member.char_id,
                      member.name);
    }
    SkinTooltip(refusal);
    if (ImGui::Selectable(core::Tr("Expulser du groupe"))) {
      AskPartyConfirm(state, PartyFriendVerb::kExpel, member.account_id, member.char_id,
                      member.name);
    }
  }
  if (member.is_me && ImGui::Selectable(core::Tr("Quitter le groupe"))) {
    AskPartyConfirm(state, PartyFriendVerb::kLeave, member.account_id, member.char_id, member.name);
  }
  EndSkinPopup();
}

void DrawFriendRowMenu(PartyFriendWindowState& state, const PartyFriendView& view,
                       const FriendView& row, PartyFriendAction& action) {
  if (!BeginSkinPopup(kPartyRowMenuId)) {
    return;
  }
  const std::string name(row.name);
  ImGui::TextDisabled("%s", name.c_str());
  ImGui::Separator();
  if (row.online && ImGui::Selectable(core::Tr("Chuchoter"))) {
    action.verb = PartyFriendVerb::kWhisper;
    action.account_id = row.account_id;
    action.name = name;
  }
  if (row.online && ImGui::Selectable(core::Tr("Chuchoter dans une fenêtre…"))) {
    action.verb = PartyFriendVerb::kWhisperWindow;
    action.account_id = row.account_id;
    action.name = name;
  }
  // Le serveur n'accepte d'invitation que du chef.
  if (row.online && !row.in_my_party && view.in_party && view.i_am_leader &&
      ImGui::Selectable(core::Tr("Inviter dans le groupe"))) {
    action.verb = PartyFriendVerb::kInvite;
    action.name = name;
  }
  if (!view.mvp_invites_withdrawn && MvpInviteMenuItem(row.mvp_refusal)) {
    action.verb = PartyFriendVerb::kInviteMvp;
    action.name = name;
  }
  if (ImGui::Selectable(core::Tr("Copier le nom"))) {
    CopyName(row.name);
  }
  ImGui::Separator();
  if (ImGui::Selectable(core::Tr("Retirer de mes amis"))) {
    AskPartyConfirm(state, PartyFriendVerb::kRemoveFriend, row.account_id, row.char_id, row.name);
  }
  EndSkinPopup();
}

void DrawPartyConfirm(PartyFriendWindowState& state, const SkinArt& art,
                      PartyFriendAction& action) {
  const std::string id = ModalName(core::Tr("Confirmation"), kConfirmId);
  if (state.ask_confirm) {
    AskModal(id.c_str());
    state.ask_confirm = false;
  }
  if (!BeginSkinnedModal(id.c_str(), art)) {
    return;
  }
  ImGui::TextUnformatted(ConfirmQuestion(state.confirm));
  if (!state.confirm_name.empty() && state.confirm != PartyFriendVerb::kLeave) {
    ImGui::Spacing();
    ImGui::TextUnformatted(state.confirm_name.c_str());
  }
  if (state.confirm == PartyFriendVerb::kLeave) {
    ImGui::Spacing();
    ImGui::TextDisabled("%s",
                        core::Tr("Si vous êtes chef, le commandement passera à un membre proche."));
  }
  ImGui::Spacing();
  if (SkinButton(core::Tr("Confirmer"), art)) {
    action.verb = state.confirm;
    action.account_id = state.confirm_account;
    action.char_id = state.confirm_char;
    action.name = state.confirm_name;
    state.confirm = PartyFriendVerb::kNothing;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art)) {
    state.confirm = PartyFriendVerb::kNothing;
    ImGui::CloseCurrentPopup();
  }
  EndSkinnedModal();
}

void DrawPartyInvite(const PartyFriendView& view, const SkinArt& art, PartyFriendAction& action) {
  const PartyInviteView& invite = view.invite;
  const std::string id = ModalName(core::Tr("Demande reçue"), kInviteId);
  if (invite.active && !ImGui::IsPopupOpen(id.c_str())) {
    AskModal(id.c_str());
  }
  if (!BeginSkinnedModal(id.c_str(), art)) {
    return;
  }
  if (!invite.active) {
    ImGui::CloseCurrentPopup();
    EndSkinnedModal();
    return;
  }
  const std::string who(invite.name);
  ImGui::TextUnformatted(who.c_str());
  ImGui::Spacing();
  ImGui::TextUnformatted(invite.is_friend ? core::Tr("souhaite vous ajouter à sa liste d'amis.")
                                          : core::Tr("vous invite à rejoindre ce groupe."));
  ImGui::Spacing();
  const auto answer = [&](bool accept) {
    action.verb = invite.is_friend ? PartyFriendVerb::kAnswerFriend : PartyFriendVerb::kAnswerParty;
    action.party_id = invite.party_id;
    action.account_id = invite.account_id;
    action.char_id = invite.char_id;
    action.name = who;
    action.accept = accept;
    ImGui::CloseCurrentPopup();
  };
  if (SkinButton(core::Tr("Accepter"), art)) {
    answer(true);
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Refuser"), art)) {
    answer(false);
  }
  EndSkinnedModal();
}

}  // namespace ro::ui
