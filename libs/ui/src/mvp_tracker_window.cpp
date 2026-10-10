// Le carnet de chasse MVP : la fenêtre, son titre replié, sa puce et son
// épingle, le panneau du groupe, la modale d'invitation et la saisie manuelle.

#include "ro/ui/mvp_tracker_window.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "mvp_tracker_parts.h"

namespace ro::ui {

namespace {

using namespace mvp_tracker;

// L'INVITATION REÇUE, en modale : elle s'impose, fenêtre fermée comme ouverte —
// c'est souvent elle qui donne une raison d'ouvrir le carnet.
//
// ⚠ ÉCHAP NE LA CONGÉDIE PAS : seuls ses boutons en sortent.
//
// 🔴 ELLE NE S'OUVRE QUE SUR UNE PILE VIDE : un surgissant déjà ouvert — une
// saisie, un menu, une confirmation — serait fermé par elle, avec ce qu'on y
// tapait. Elle attend donc qu'il se ferme ; et un autre qui s'ouvre par-dessus
// elle la ferme, puis elle revient quand la pile s'est vidée. Ouverte, elle ne
// se redemande pas : la pile n'est plus vide.
//
// 🔴 HORS LIAISON VÉRIFIÉE, « PLUS TARD » EN SORT, sans rien envoyer : on ne
// peut ni accepter ni refuser, et la modale bloquerait toute l'interface
// jusqu'au retour de la liaison. Remise, elle ne se redemande qu'à ce retour.
void DrawInvitePopup(MvpTrackerWindowState& state, const MvpTrackerView& view, const SkinArt& art,
                     MvpTrackerAction& action) {
  if (view.link_ready) {
    state.invite_deferred = false;
  }
  // ⚠ CARNET ÉTEINT, AUCUNE INVITATION : celle qu'on montrait se referme.
  const bool invited = view.enabled && view.pending_invite != 0;
  const std::string title = std::string(core::Tr("Invitation reçue")) + "###mvp_invite";
  if (invited && !state.invite_deferred &&
      !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
    AskModal(title.c_str());
  }
  if (!BeginSkinnedModal(title.c_str(), art, ImGuiWindowFlags_AlwaysAutoResize,
                         ModalEscape::kIgnored)) {
    return;
  }
  if (!invited) {
    // Répondue ailleurs, changement de personnage ou carnet éteint : plus rien
    // à décider.
    ImGui::CloseCurrentPopup();
    EndSkinnedModal();
    return;
  }
  // `Text` et non `TextWrapped` : dans une boîte qui s'ajuste, le repli fixerait
  // la largeur au lieu de la suivre. Le nom de groupe est borné par le serveur.
  ImGui::Text(core::Tr("Le groupe de chasse « %s » vous invite à partager son carnet MVP."),
              std::string(view.pending_invite_name).c_str());
  ImGui::Spacing();
  // 🔴 LA MODALE ATTEND LE SERVEUR : elle ne se ferme qu'avec l'invitation, que
  // `app` efface sur sa réponse. Un refus s'écrit donc ICI, sous la question
  // qu'il concerne, et non dans un carnet peut-être fermé.
  if (view.invite_refusal != 0) {
    ImGui::PushStyleColor(ImGuiCol_Text, kMvpRefusalTextColor);
    if (const char* why = MvpResultText(view.invite_refusal); why != nullptr) {
      ImGui::TextUnformatted(why);
    } else {
      ImGui::Text(core::Tr("Refusé par le serveur (code %u)."),
                  static_cast<unsigned>(view.invite_refusal));
    }
    ImGui::PopStyleColor();
    ImGui::Spacing();
  }
  // L'invitation n'existe plus côté serveur : il n'y a plus rien à répondre,
  // seulement à congédier, sans rien envoyer.
  if (view.invite_gone) {
    if (SkinButton(core::Tr("Fermer"), art)) {
      action.dismiss_invite = true;
    }
    EndSkinnedModal();
    return;
  }
  // Accepter en étant déjà membre est refusé par le serveur : on le dit, et on
  // grise plutôt que d'essuyer le refus.
  const bool already = view.group_id != 0;
  if (already) {
    ImGui::TextDisabled("%s",
                        core::Tr("Quittez d'abord votre groupe actuel pour pouvoir accepter."));
    ImGui::Spacing();
  }
  // Une réponse partie se grise le temps que `app` juge raisonnable : un second
  // clic enverrait une seconde commande. Hors liaison vérifiée, rien ne part.
  ImGui::BeginDisabled(already || view.invite_waiting || !view.link_ready);
  if (SkinButton(core::Tr("Accepter"), art)) {
    action.accept_invite = true;
  }
  ImGui::EndDisabled();
  TellLinkPending(view);
  ImGui::SameLine();
  ImGui::BeginDisabled(view.invite_waiting || !view.link_ready);
  if (SkinButton(core::Tr("Refuser"), art)) {
    action.decline_invite = true;
  }
  ImGui::EndDisabled();
  TellLinkPending(view);
  // 🔴 UN REFUS OFFRE TOUJOURS UNE SORTIE : la modale bloque toute l'interface,
  // chat compris, et un refus qui se répète la laisserait sans issue.
  if (view.invite_refusal != 0) {
    ImGui::SameLine();
    if (SkinButton(core::Tr("Fermer"), art)) {
      action.dismiss_invite = true;
    }
  }
  if (!view.link_ready) {
    ImGui::SameLine();
    if (SkinButton(core::Tr("Plus tard"), art)) {
      state.invite_deferred = true;
      ImGui::CloseCurrentPopup();
    }
  }
  if (view.invite_waiting) {
    ImGui::TextDisabled("%s", core::Tr("En attente de la réponse du serveur…"));
  }
  EndSkinnedModal();
}

// Le dernier résultat qui n'est pas un succès, le temps que `app` l'a jugé
// lisible. Un refus en rouge FONCÉ — le fond de la fenêtre est clair — ; une
// saisie sans effet, qui n'a rien refusé, en teinte neutre.
void DrawLastResult(const MvpTrackerView& view) {
  if (view.result_code == 0 || !view.result_shown) {
    return;
  }
  ImGui::PushStyleColor(ImGuiCol_Text, MvpResultRefuses(view.result_code)
                                           ? kMvpRefusalTextColor
                                           : kMvpNeutralResultTextColor);
  if (const char* why = MvpResultText(view.result_code); why != nullptr) {
    ImGui::TextWrapped("%s", why);
  } else {
    ImGui::TextWrapped(core::Tr("Refusé par le serveur (code %u)."),
                       static_cast<unsigned>(view.result_code));
  }
  ImGui::PopStyleColor();
}

// UN CHAMP DE NOM BORNÉ AU FIL : la saisie s'arrête là où le serveur couperait
// en silence, mesurée en octets CP1252 et non en UTF-8. Entrée vaut le bouton.
//
// ⚠ LA BORNE VOYAGE PAR `UserData`, le temps de l'appel : le rappel est une
// fonction sans état.
int CutToWire(ImGuiInputTextCallbackData* data) {
  const std::size_t most = *static_cast<const std::size_t*>(data->UserData);
  const MvpWireExcess excess =
      MvpWireExcessOf(std::string_view(data->Buf, static_cast<std::size_t>(data->BufTextLen)),
                      static_cast<std::size_t>(data->CursorPos), most);
  if (excess.bytes > 0) {
    data->DeleteChars(static_cast<int>(excess.at), static_cast<int>(excess.bytes));
  }
  return 0;
}

[[nodiscard]] bool WireNameField(const char* id, const char* hint, std::span<char> buffer,
                                 std::size_t most_wire_chars) {
  return ImGui::InputTextWithHint(
      id, hint, buffer.data(), buffer.size(),
      ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackEdit, CutToWire,
      &most_wire_chars);
}

void DrawNoGroup(MvpTrackerWindowState& state, const MvpTrackerView& view, const SkinArt& art,
                 MvpTrackerAction& action) {
  ImGui::TextDisabled("%s", core::Tr("Vous n'êtes dans aucun groupe de chasse."));
  ImGui::SetNextItemWidth(Px(kMvpGroupNameWidthPixels));
  const bool entered = WireNameField("##mvp_group_name", core::Tr("Nom du groupe"),
                                     state.group_name, kMvpGroupNameMostWireChars);
  // Entrée qui n'envoie rien — champ vide, liaison non vérifiée — : le champ
  // garde le clavier.
  if (entered && (state.group_name[0] == '\0' || !view.link_ready)) {
    ImGui::SetKeyboardFocusHere(-1);
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltipFormat(core::Tr("Nom du groupe : %d caractères au plus."),
                      static_cast<int>(kMvpGroupNameMostWireChars));
  }
  ImGui::SameLine();
  // 🔴 GRISÉ, LE GESTE NE PART PAS, et Entrée non plus : le nom reste dans son
  // champ, au lieu de se vider pour une commande que le serveur jetterait.
  ImGui::BeginDisabled(!view.link_ready);
  const bool clicked = SkinButton(core::Tr("Créer"), art);
  ImGui::EndDisabled();
  TellLinkPending(view);
  if ((clicked || entered) && view.link_ready && state.group_name[0] != '\0') {
    action.create_group = state.group_name.data();
    state.group_name.fill('\0');
  }
}

// LES MEMBRES. La PREMIÈRE ligne d'un compte porte ce qui appartient au
// COMPTE — la marque de propriétaire, le bouton d'exclusion — ; les suivantes
// sont d'autres têtes du même compte, en retrait.
void DrawMembers(const MvpTrackerView& view, const SkinArt& art, MvpTrackerAction& action) {
  if (!ImGui::BeginTable("##mvp_members", kMvpMemberColumnCount,
                         ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
    return;
  }
  ImGui::TableSetupColumn(core::Tr("Membre"));
  ImGui::TableSetupColumn(core::Tr("Niveau"), ImGuiTableColumnFlags_WidthFixed,
                          Px(kMvpLevelColumnPixels));
  ImGui::TableSetupColumn(core::Tr("État"), ImGuiTableColumnFlags_WidthFixed,
                          Px(kMvpStateColumnPixels));
  ImGui::TableSetupColumn("##act", ImGuiTableColumnFlags_WidthFixed, Px(kMvpActColumnPixels));
  ImGui::TableHeadersRow();
  for (std::size_t rank = 0; rank < view.members.size(); ++rank) {
    const MvpMemberView& member = view.members[rank];
    ImGui::TableNextRow();
    // Sous un NOM : le numéro seul rendrait l'identifiant que l'en-tête pose
    // déjà sur la colonne de même rang.
    ImGui::PushID("member");
    ImGui::PushID(static_cast<int>(rank));
    const bool first_of_account = rank == 0 || view.members[rank - 1].user_id != member.user_id;

    ImGui::TableSetColumnIndex(0);
    if (!first_of_account) {
      ImGui::Indent(Px(kMvpSecondHeadIndentPixels));
    }
    if (member.name.empty()) {
      ImGui::TextUnformatted(core::Tr("(compte sans personnage)"));
    } else {
      Unformatted(member.name);
    }
    if (!first_of_account) {
      ImGui::Unindent(Px(kMvpSecondHeadIndentPixels));
    }
    // CHACUNE DE NOS LIGNES se marque : toutes les sessions de notre compte
    // sont « nous ».
    if (member.our_account) {
      ImGui::SameLine();
      ImGui::TextDisabled("%s", core::Tr("(vous)"));
    }
    if (first_of_account && member.user_id == view.owner_user_id) {
      ImGui::SameLine();
      ImGui::TextDisabled("%s", core::Tr("(propriétaire)"));
    }

    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%d", member.level);

    ImGui::TableSetColumnIndex(2);
    if (member.online) {
      ImGui::TextUnformatted(core::Tr("en ligne"));
    } else {
      ImGui::TextDisabled("%s", core::Tr("absent"));
    }

    // Exclure, c'est exclure le COMPTE, donc toutes ses têtes d'un coup.
    // ⚠ PAR LE NOM D'UN DE SES PERSONNAGES : un compte sans personnage n'en a
    // aucun à désigner, et le bouton n'y serait qu'un clic mort.
    // Seul le propriétaire exclut : su d'avance, le bouton se grise et dit
    // pourquoi ; inconnu, il reste offert et le serveur tranche. Notre propre
    // compte ne s'exclut pas : on QUITTE.
    ImGui::TableSetColumnIndex(3);
    if (first_of_account && member.user_id != view.owner_user_id && !member.our_account &&
        !member.name.empty()) {
      const bool forbidden = view.owner_known && !view.i_own;
      ImGui::BeginDisabled(forbidden || !view.link_ready);
      if (SkinSmallButton(core::Tr("Exclure"), art)) {
        action.kick_member = member.name;
      }
      ImGui::EndDisabled();
      TellLinkPending(view);
      if (view.link_ready && forbidden &&
          ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        SkinTooltipFormat("%s", core::Tr("Seul le propriétaire du groupe peut exclure un membre."));
      }
    }

    ImGui::PopID();
    ImGui::PopID();
  }
  ImGui::EndTable();
}

void DrawGroupPanel(MvpTrackerWindowState& state, const MvpTrackerView& view, const SkinArt& art,
                    MvpTrackerAction& action) {
  DrawLastResult(view);
  if (view.group_id == 0) {
    DrawNoGroup(state, view, art, action);
    return;
  }
  // 🔴 ON COMPTE LES COMPTES, pas les lignes : chaque personnage en ligne a la
  // sienne, et trois têtes d'un joueur ne font pas « 3 membres ».
  int accounts = 0;
  for (std::size_t rank = 0; rank < view.members.size(); ++rank) {
    if (rank == 0 || view.members[rank].user_id != view.members[rank - 1].user_id) {
      ++accounts;
    }
  }
  const std::string header = MvpGroupHeader(view.group_name, accounts);
  if (!ImGui::CollapsingHeader(header.c_str())) {
    return;
  }
  DrawMembers(view, art, action);

  ImGui::SetNextItemWidth(Px(kMvpInviteNameWidthPixels));
  const bool entered = WireNameField("##mvp_invite", core::Tr("Nom du personnage"),
                                     state.invite_name, kMvpNameMostWireChars);
  if (entered && (state.invite_name[0] == '\0' || !view.link_ready)) {
    ImGui::SetKeyboardFocusHere(-1);
  }
  ImGui::SameLine();
  // Grisé, le nom reste dans son champ : voir « Créer ».
  ImGui::BeginDisabled(!view.link_ready);
  const bool clicked = SkinButton(core::Tr("Inviter"), art);
  ImGui::EndDisabled();
  TellLinkPending(view);
  if ((clicked || entered) && view.link_ready && state.invite_name[0] != '\0') {
    action.invite_member = state.invite_name.data();
    state.invite_name.fill('\0');
  }
  SkinTooltip(
      core::Tr("Guilde et liste d'amis uniquement.\nInviter quelqu'un, c'est inviter tous "
               "ses personnages et tous ses comptes de jeu."));
  ImGui::SameLine();
  // Les deux confirmations s'excluent : une seule question à la fois. Hors
  // liaison vérifiée, ni la question ni sa confirmation ne s'offrent.
  ImGui::BeginDisabled(!view.link_ready);
  if (SkinButton(core::Tr("Quitter"), art)) {
    state.confirm_leave = true;
    state.confirm_dissolve = false;
  }
  ImGui::EndDisabled();
  TellLinkPending(view);
  // Dissoudre n'est offert qu'au propriétaire SU : le serveur refuserait les
  // autres. La propriété perdue emporte la question.
  if (!view.owner_known || !view.i_own) {
    state.confirm_dissolve = false;
  } else {
    ImGui::SameLine();
    ImGui::BeginDisabled(!view.link_ready);
    if (SkinButton(core::Tr("Dissoudre"), art)) {
      state.confirm_dissolve = true;
      state.confirm_leave = false;
    }
    ImGui::EndDisabled();
    TellLinkPending(view);
  }
  if (state.confirm_dissolve) {
    ImGui::PushID("dissolve");
    ImGui::TextWrapped("%s", core::Tr("Dissoudre le groupe ? Tous ses membres en sortiront, et ses "
                                      "invitations seront annulées."));
    ImGui::BeginDisabled(!view.link_ready);
    if (SkinButton(core::Tr("Confirmer"), art)) {
      action.dissolve_group = true;
      state.confirm_dissolve = false;
    }
    ImGui::EndDisabled();
    TellLinkPending(view);
    ImGui::SameLine();
    if (SkinButton(core::Tr("Annuler"), art)) {
      state.confirm_dissolve = false;
    }
    ImGui::PopID();
  }
  if (state.confirm_leave) {
    ImGui::TextWrapped("%s", core::Tr("Quitter le groupe ? Si vous en êtes le dernier membre, il "
                                      "sera détruit avec ses invitations."));
    ImGui::BeginDisabled(!view.link_ready);
    if (SkinButton(core::Tr("Confirmer"), art)) {
      action.leave_group = true;
      state.confirm_leave = false;
    }
    ImGui::EndDisabled();
    TellLinkPending(view);
    ImGui::SameLine();
    if (SkinButton(core::Tr("Annuler"), art)) {
      state.confirm_leave = false;
    }
  }
}

// LA SAISIE D'UNE HEURE DE MORT : un surgissant, pas une modale. La source
// « saisie » est la plus faible ; l'arbitrage est au serveur.
void DrawManualPopup(MvpTrackerWindowState& state, const MvpTrackerView& view, const SkinArt& art,
                     MvpTrackerAction& action) {
  if (!BeginSkinPopup("##mvp_manual")) {
    return;
  }
  const MvpSlotView* slot = FindSlotView(view.slots, state.manual_slot);
  if (slot == nullptr) {
    EndSkinPopup();
    return;
  }
  // Le MVP et sa carte sont nommés : plusieurs créneaux portent le même nom.
  ImGui::Text(core::Tr("Heure de mort de %s (%s)"), std::string(slot->label).c_str(),
              std::string(MvpMapShown(*slot)).c_str());
  // On ouvre la saisie pour taper : le champ prend le clavier d'emblée.
  if (ImGui::IsWindowAppearing()) {
    ImGui::SetKeyboardFocusHere();
  }
  ImGui::SetNextItemWidth(Px(kMvpManualTimeWidthPixels));
  const bool entered = ImGui::InputTextWithHint("##mvp_manual_time", "1430 / 14h30 / -2350",
                                                state.manual_time.data(), state.manual_time.size(),
                                                ImGuiInputTextFlags_EnterReturnsTrue);
  const std::int64_t when = ParseMvpKillTime(state.manual_time.data(), view.server_now_s);
  // Entrée qui n'envoie rien — heure illisible, liaison non vérifiée — : le
  // champ garde le clavier.
  if (entered && (when == 0 || !view.link_ready)) {
    ImGui::SetKeyboardFocusHere(-1);
  }
  ImGui::TextDisabled("%s", core::Tr("Un « - » de tête désigne la veille."));
  if (state.manual_time[0] != '\0' && when == 0) {
    ImGui::TextDisabled("%s", core::Tr("Heure illisible."));
  }
  // Ce que la saisie a compris, veille comprise, avant que le groupe ne le
  // reçoive.
  if (when != 0) {
    ImGui::TextDisabled(core::Tr("Heure lue : %s"),
                        MvpKillTimeEcho(when, view.server_now_s).c_str());
  }
  // Grisé faute de liaison, le surgissant reste ouvert avec l'heure tapée.
  ImGui::BeginDisabled(when == 0 || !view.link_ready);
  const bool clicked = SkinButton(core::Tr("Envoyer"), art);
  ImGui::EndDisabled();
  TellLinkPending(view);
  if ((clicked || entered) && view.link_ready && when != 0) {
    action.manual_slot = state.manual_slot;
    action.manual_kill_time_s = when;
    state.manual_time.fill('\0');
    ImGui::CloseCurrentPopup();
  }
  EndSkinPopup();
}

void DrawWindow(MvpTrackerWindowState& state, const MvpTrackerView& view, const SkinArt& art,
                MvpTrackerAction& action) {
  ImGui::SetNextWindowSize(ImVec2(Px(kMvpFirstWidthPixels), Px(kMvpFirstHeightPixels)),
                           ImGuiCond_FirstUseEver);
  // UNE OUVERTURE DEMANDÉE MET LE CARNET DEVANT, même déjà ouvert sous une autre
  // fenêtre ; et déplié quand elle vise une ligne, qu'on doit voir.
  if (state.raise) {
    ImGui::SetNextWindowFocus();
    if (state.focus_slot != kMvpNoSlotView) {
      ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
    }
    state.raise = false;
  }
  // La puce mène aux réglages du carnet ; l'épingle le sort de la pile d'Échap.
  SetNextWindowBulletClickable(core::Tr("Réglages du carnet"));
  SetNextWindowPinnable(&state.pinned);
  // 🔴 REPLIÉE, LA BARRE DIT LA PROCHAINE ÉCHÉANCE : c'est tout ce qui reste à
  // l'écran. Le pli n'est connu qu'APRÈS le `Begin` : on se fie à l'image
  // précédente, une image de retard invisible à l'œil. ⚠ `###` FIGE L'IDENTITÉ,
  // sans quoi la fenêtre perdrait place, taille et épingle à chaque échéance.
  state.title = (state.was_collapsed ? MvpCollapsedTitle(view.slots, view.server_now_s)
                                     : std::string(core::Tr("Carnet de chasse MVP"))) +
                "###" + kMvpTrackerWindowId;
  const bool begun = BeginSkinnedWindow(state.title.c_str(), &state.open, art);
  state.was_collapsed = !begun && state.open;
  // ⚠ LU HORS DU `if (begun)` : la puce se clique aussi sur une fenêtre repliée.
  if (TitleBulletClicked()) {
    action.open_settings = true;
  }
  if (!begun) {
    EndSkinnedWindow();
    return;
  }
  DrawGroupPanel(state, view, art, action);
  ImGui::Separator();
  DrawTable(state, view, art, action);
  DrawManualPopup(state, view, art, action);
  // Le rectangle du carnet, relevé DEDANS : c'est lui qui décide si un lâcher
  // est « dehors ».
  state.window_pos = ImGui::GetWindowPos();
  state.window_size = ImGui::GetWindowSize();
  EndSkinnedWindow();
}

}  // namespace

MvpTrackerAction DrawMvpTrackerWindow(MvpTrackerWindowState& state, const MvpTrackerView& view,
                                      const SkinArt& art) {
  MvpTrackerAction action;
  state.wanted_sprites.clear();
  state.wanted_map.clear();
  // 🔴 EN TÊTE, HORS DE TOUT DESSIN : le groupe peut disparaître fenêtre fermée
  // ou repliée, et « Quitter le groupe ? » ne doit pas revenir sur le suivant.
  if (view.group_id == 0) {
    state.confirm_leave = false;
    state.confirm_dissolve = false;
  }
  // 🔴 LE TIRAGE D'UNE LIGNE ORPHELIN DE SON BOUTON s'annule ici, carnet éteint
  // compris : ImGui vide la souris quand l'appli perd le focus, et le
  // relâchement qui l'aurait clos ne se verra jamais. Gardé, il ferait suivre la
  // ligne au prochain glisser, n'importe où.
  if (state.line_drag >= 0 && !ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
      !ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    state.line_drag = -1;
  }
  // 🔴 ÉTEINT, LE CARNET NE DESSINE RIEN : ni fenêtre, ni alerte, ni ligne
  // détachée, et aucun glisser ne survit. Seule l'invitation qu'on montrait
  // passe encore, pour se refermer.
  if (!view.enabled) {
    state.drag_slot = kMvpNoSlotView;
    state.row_dragged = false;
    state.line_drag = -1;
    DrawInvitePopup(state, view, art, action);
    return action;
  }
  // L'alerte et les lignes détachées vivent hors de la fenêtre : survivre à sa
  // fermeture est leur propos.
  DrawAlerts(view, action);
  DrawPinnedLines(state, view, action);
  // Le glisser d'un rang continue quand le carnet vient de se fermer sous la
  // souris : il se termine ici, pas dans la table.
  // ⚠ RELEVÉ AVANT LA FIN : le relâchement efface le glisser, et le nom qui se
  // trouve sous la souris le prendrait sinon pour son clic.
  state.row_dragged = state.drag_slot != kMvpNoSlotView;
  FinishRowDrag(state, view, action);
  DrawInvitePopup(state, view, art, action);
  if (state.open) {
    DrawWindow(state, view, art, action);
  }
  return action;
}

}  // namespace ro::ui
