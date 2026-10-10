#pragma once

#include <imgui.h>

#include "ro/ui/party_friend_window.h"

// LES MORCEAUX DE LA FENÊTRE AMIS / GROUPE, chacun dans son fichier.
//
// En-tête PRIVÉ : la coque (`party_friend_window.cpp`) décide des onglets et de
// l'ordre ; les lignes et les boîtes décident du contenu.
namespace ro::ui {

// Une longueur de référence, mise à l'échelle de l'interface.
[[nodiscard]] float PartyPx(float pixels);

// Le membre qui porte le joueur, ou nul.
[[nodiscard]] const PartyMemberView* PartyMe(const PartyFriendView& view);

// Les lignes. Chacune pose son menu contextuel, qui ne fait qu'ARMER : la
// confirmation s'ouvre au niveau de la fenêtre.
void DrawPartyRow(PartyFriendWindowState& state, const PartyFriendView& view,
                  const PartyMemberView& member, const PartyFriendLook& look,
                  PartyFriendAction& action);
void DrawFriendRow(PartyFriendWindowState& state, const PartyFriendView& view,
                   const FriendView& row, const PartyFriendLook& look, PartyFriendAction& action);

// Le menu d'une ligne sans sprite — ou de la ligne du joueur.
inline constexpr const char* kPartyRowMenuId = "##rowmenu";
void DrawPartyRowMenu(PartyFriendWindowState& state, const PartyFriendView& view,
                      const PartyMemberView& member, PartyFriendAction& action);
void DrawFriendRowMenu(PartyFriendWindowState& state, const PartyFriendView& view,
                       const FriendView& row, PartyFriendAction& action);

// Arme la confirmation d'un geste sans retour.
void AskPartyConfirm(PartyFriendWindowState& state, PartyFriendVerb verb, std::uint32_t account_id,
                     std::uint32_t char_id, std::string_view name);

void DrawPartyConfirm(PartyFriendWindowState& state, const SkinArt& art, PartyFriendAction& action);
void DrawPartyInvite(const PartyFriendView& view, const SkinArt& art, PartyFriendAction& action);

}  // namespace ro::ui
