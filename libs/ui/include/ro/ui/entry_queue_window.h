#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "ro/ui/chat_link.h"
#include "ro/ui/item_link_cell.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── L'INSCRIPTION AU CHAMP DE BATAILLE ──────────────────────────────────────
//
// Quatre fenêtres : la liste des terrains et le détail du terrain choisi ;
// « en attente », tant qu'on est en file ; « terrain prêt », la modale de
// l'invitation et son compte à rebours ; l'aide. Et la boîte qui dit un
// résultat.
//
// 🔴 `ui` NE TIENT AUCUN ÉTAT DE JEU : `app` compose la vue à chaque image,
// textes déjà traduits, et reçoit en retour le verbe que le joueur a choisi. La
// fenêtre ne garde que de l'état d'INTERFACE — ce qui est ouvert, ce qui est
// sélectionné.

// LES IDENTITÉS DES FENÊTRES : clés de leur place et de la pile d'Échap. Non
// traduites.
inline constexpr const char* kEntryQueueWindowId = "entry_queue";
inline constexpr const char* kEntryQueueStandbyId = "entry_queue_standby";
inline constexpr const char* kEntryQueueReadyId = "entry_queue_ready";
inline constexpr const char* kEntryQueueHelpId = "entry_queue_help";

// UN TERRAIN, tel que la liste et le détail le montrent. ⚠ DES VUES : elles
// pointent dans les tampons de `app`, valables le temps de l'image.
// UNE RÉCOMPENSE : son icône, inutilisable pour un texte libre, et ce qu'elle
// dit, déjà composé et traduit. Un objet se montre en lien, avec les gestes de
// tout lien d'objet (`ItemLinkCell`).
struct EntryQueueRewardView {
  SkinImage icon;
  std::string_view text;
  // L'objet de la base ; zéro : un texte libre, qui ne se clique pas.
  std::uint32_t item_id = 0;
};

struct EntryQueueFieldView {
  std::string_view display_name;
  std::string_view victory;
  // Ce qu'il faut savoir du terrain ; vide : rien.
  std::string_view help;
  std::span<const EntryQueueRewardView> reward_win;
  std::span<const EntryQueueRewardView> reward_draw;
  std::span<const EntryQueueRewardView> reward_lose;
  // La condition d'entrée, déjà composée et traduite ; vide : aucune.
  std::string_view condition_text;
  // L'effectif par camp, déjà composé et traduit ; vide : non dit.
  std::string_view team_text;
  bool private_apply = false;
  bool party_apply = false;
  bool guild_apply = false;
  // LE REFUS DE CHAQUE INSCRIPTION, déjà jugé par `app` et traduit ; vide :
  // elle peut passer à sa confirmation. 🔴 UN REFUS PASSE AVANT LA QUESTION :
  // le clic ne demande pas « voulez-vous ? » à qui ne le peut pas.
  std::string_view solo_refusal;
  std::string_view party_refusal;
  std::string_view guild_refusal;
  // La minimap du terrain ; inutilisable, le détail s'en passe.
  SkinImage minimap;
};

// CE QUE `app` SAIT DE LA FILE, à cette image.
struct EntryQueueView {
  std::span<const EntryQueueFieldView> fields;
  // En file ; la demande d'inscription est partie sans réponse ; le terrain est
  // prêt ; « j'entre » est parti.
  bool queued = false;
  bool applying = false;
  bool invited = false;
  bool accepted = false;
  // Le terrain où l'on est en file, tel que le joueur le lit.
  std::string_view queued_name;
  // L'effectif total de la file ; négatif : inconnu.
  int count = -1;
  std::uint32_t waited_s = 0;
  // Le terrain de l'invitation, tel que le joueur le lit.
  std::string_view invited_name;
  // Ce qui reste de l'invitation, en secondes.
  int invitation_s_left = 0;
  // Le résultat à dire dans la boîte, déjà traduit ; vide : aucun.
  std::string_view notice;
  // La liaison accepte un envoi : sans elle, aucun bouton qui envoie n'est
  // actif.
  bool link_ready = false;
};

enum class EntryQueueVerb : std::uint8_t {
  kNothing,
  kApplySolo,
  kApplyParty,
  kApplyGuild,
  kCancel,
  kAccept,
  kDecline,
  // La boîte de résultat a été fermée : `app` peut oublier son texte.
  kNoticeSeen,
};

// CE QUE LE JOUEUR A DEMANDÉ À CETTE IMAGE. `field_rank` désigne une entrée de
// `EntryQueueView::fields`, pour les trois inscriptions.
struct EntryQueueAction {
  EntryQueueVerb verb = EntryQueueVerb::kNothing;
  std::size_t field_rank = 0;
  // POUR UNE INSCRIPTION : le joueur a dit « oui » à la confirmation. Sans ce
  // oui, elle n'est qu'une demande que `app` juge pour en dire le refus, et qui
  // ne part jamais.
  bool confirmed = false;
  // Le bouton de rapport de bug d'une des deux fenêtres a été cliqué. Il
  // s'ajoute au verbe : `field_rank` dit aussi le terrain choisi.
  bool report_bug = false;
  // Ce que le joueur a fait du lien d'un objet en récompense : survol, geste,
  // entrée du menu. Il s'ajoute au verbe, comme le rapport de bug.
  ChatLinkAsk link;
};

// L'ÉTAT D'INTERFACE, d'une image à l'autre.
struct EntryQueueWindowState {
  // La liste et son détail ; « en attente » ; l'aide.
  bool open = false;
  bool standby_open = false;
  bool help_open = false;
  // Le terrain choisi dans la liste.
  std::size_t selected = 0;
  // La modale « terrain prêt » est demandée pour l'invitation en cours.
  bool asked_ready = false;
  // La boîte de résultat est demandée pour le résultat en cours.
  bool asked_notice = false;
  // L'inscription qui attend sa confirmation ; `kNothing` : aucune.
  EntryQueueVerb confirming = EntryQueueVerb::kNothing;
  // Le bouton de rapport de bug, au titre de la liste et d'« en attente » :
  // posé par `app` à chaque image, selon le réglage commun à toutes les
  // fenêtres.
  bool bug_report = false;
  // Le menu du lien d'une récompense.
  ItemLinkMenu reward_menu;
};

// TOUT CE QUE L'INSCRIPTION DESSINE : la liste et le détail, « en attente »,
// « terrain prêt », l'aide et la boîte de résultat.
[[nodiscard]] EntryQueueAction DrawEntryQueueWindows(EntryQueueWindowState& state,
                                                     const EntryQueueView& view,
                                                     const SkinArt& art);

// LE TEMPS PASSÉ EN FILE, lisible : « 4 min 05 s », « 12 s ».
[[nodiscard]] std::string FormatQueueWait(std::uint32_t waited_s);

}  // namespace ro::ui
