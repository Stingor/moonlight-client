#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/ui/chat_bar.h"
#include "ro/ui/chat_command_hints.h"
#include "ro/ui/chat_link.h"
#include "ro/ui/chat_whisper_list.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA RANGÉE DE SAISIE : où part ce que j'écris, ce que j'écris, et ce qu'il
// m'en reste.
//
// ⚠ LE CHAMP GRANDIT AVEC LA PHRASE, et ce n'est pas une coquetterie. Un champ
// d'une ligne qui défile cache le début de ce qu'on est en train d'écrire :
// pour un message dont la longueur est BORNÉE et dont le dépassement est refusé
// par le serveur sans un mot, ne pas voir ce qu'on a écrit est un piège. Le
// champ gagne donc des lignes, et la fenêtre gagne la hauteur — vers le HAUT,
// le bas ne bouge pas.
//
// ⚠ ET LA PLACE RESTANTE SE COMPTE EN OCTETS DU FIL, pas en caractères. Une
// lettre accentuée, un emoji et un lien d'objet ne coûtent pas la même chose, et
// c'est le SERVEUR qui tranche. `ui` ne sait pas compter cela : c'est `app` qui
// le lui dit (voir `ChatLinePreview`).

// Ce qu'une ligne de saisie peut porter, zéro terminal compris.
//
// ⚠ CE N'EST PAS LA BORNE DU SERVEUR, c'est celle de notre tampon. La borne du
// serveur — `CHAT_SIZE_MAX` — est plus basse encore une fois le nom du
// personnage compté, et elle a vocation à MONTER : voir ADR 0033, entrée D1.
inline constexpr std::size_t kChatInputSize = 512;

// Combien de lignes tapées la barre se rappelle. Assez pour retrouver ce qu'on
// vient de dire, pas assez pour que remonter le fil devienne un travail.
inline constexpr int kChatHistoryKept = 32;

// Les bornes du réglage. En deçà du plancher, la flèche ↑ ne rend plus la
// commande qu'on cherche ; au-delà du plafond, remonter le fil coûte plus que
// de retaper — et ces lignes-là se gardent d'une partie à l'autre, donc elles
// s'écrivent sur le disque.
inline constexpr int kSmallestTypedHistory = 10;
inline constexpr int kLargestTypedHistory = 200;

// LA BORNE RÉELLEMENT APPLIQUÉE, celle qu'on demande ramenée dans ses limites.
//
// 🔴 ELLE EST RAMENÉE ICI ET NON DANS LE PANNEAU DE RÉGLAGES. Le curseur du
// panneau ne peut pas sortir des bornes, mais le FICHIER, lui, s'ouvre dans un
// éditeur de texte : un zéro écrit à la main viderait l'historique à chaque
// envoi, et la flèche ↑ ne rendrait plus rien sans que rien ne l'explique.
[[nodiscard]] constexpr int TypedHistoryKept(int asked) noexcept {
  return asked < kSmallestTypedHistory  ? kSmallestTypedHistory
         : asked > kLargestTypedHistory ? kLargestTypedHistory
                                        : asked;
}

// Ce que la box « Pseudo » peut porter : un nom de personnage, et rien d'autre.
// C'est `NAME_LENGTH` du serveur, zéro terminal compris.
inline constexpr std::size_t kChatWhisperNameSize = 24;

// Sa largeur, en pixels d'interface. Assez pour la plupart des noms sans manger
// la phrase : un nom plus long s'y écrit quand même, le champ défile.
inline constexpr float kChatWhisperBoxWidth = 110.0F;

// Ce que la boîte de destination garde au moins, sans aucun mode à proposer.
inline constexpr float kChatTargetLeastText = 48.0F;

// La place restante, et sa couleur quand elle est dépassée.
inline constexpr ImU32 kChatBudgetColor = IM_COL32(150, 150, 150, 255);
inline constexpr ImU32 kChatBudgetOverColor = IM_COL32(230, 110, 90, 255);
// La marque d'un mode qui part aussi sur Discord : le bleu de Discord.
inline constexpr ImU32 kDiscordMarkColor = IM_COL32(0x72, 0x89, 0xDA, 255);

// CE QUE `app` SAIT DE LA LIGNE EN COURS DE FRAPPE, et que `ui` ne peut pas
// deviner : la destination se lit dans les sigils, qui vivent dans `net`, et la
// place restante se compte dans la page de codes du fil.
// UN MODE D'ENVOI, tel que le sélecteur le propose.
//
// ⚠ `ui` NE SAIT PAS CE QU'EST UN GROUPE NI UNE GUILDE. Elle reçoit un libellé
// déjà traduit et une teinte, et rend le RANG de celui qu'on a choisi — même
// frontière que pour les genres d'une ligne.
struct ChatSendMode {
  std::string_view label;
  // La couleur du texte tant que ce mode est armé — celle que prendra la ligne
  // une fois partie, pour que la saisie et le fil parlent la même teinte.
  // Parler à sa guilde sans le voir est la faute qu'on ne fait qu'une fois.
  // Zéro : le texte ordinaire.
  ImU32 color = 0;
  // LE FOND SUR LEQUEL CETTE COULEUR EST PENSÉE. ⚠ LES TEINTES DU FIL SONT
  // CLAIRES, faites pour son fond sombre : posées sur la liste ou le champ
  // clairs, elles ne se liraient plus. Zéro : le fond ordinaire suffit.
  ImU32 backdrop = 0;
  // CE MODE PART AUSSI SUR DISCORD : une marque le dit à côté du sélecteur.
  bool relayed = false;
};

struct ChatLinePreview {
  // Où cette ligne partira, DÉJÀ TRADUIT — « Alentour », « Groupe », un pseudo.
  std::string_view target;
  // ── LE MODE ARMÉ ───────────────────────────────────────
  // Ce que le sélecteur propose, et le rang de celui qui tient.
  //
  // 🔴 UN SIGIL TAPÉ L'EMPORTE SUR LE MODE ARMÉ, pour cette ligne-là et
  // elle seule. Les deux chemins existent dans le client, et le joueur qui a
  // les sigils dans les doigts ne doit pas avoir à désarmer quoi que ce soit
  // pour dire un mot à voix haute. C'est `app` qui tranche — les sigils vivent
  // dans `net`.
  std::span<const ChatSendMode> modes;
  int mode = 0;
  // Les derniers correspondants, le plus récent en tête, pour la liste de la
  // box. Vide tant que personne n'a chuchoté dans un sens ou dans l'autre :
  // la liste ne se déroule alors pas.
  std::span<const ChatRecentWhisper> recent_whispers;
  // De quoi expliquer les sigils, DÉJÀ COMPOSÉ. ⚠ LES CARACTÈRES SONT CEUX DU
  // CLIENT, pas ceux d'une documentation : une aide qui annoncerait le mauvais
  // sigil enverrait le joueur droit dans le mur.
  std::string_view help;
  // Ce qu'il reste de place sur le fil ; NÉGATIF quand la ligne est trop longue.
  int bytes_left = 0;
  // Faux tant que `app` ne sait pas compter (hors connexion, par exemple) : rien
  // ne s'affiche alors, plutôt qu'un nombre qui ment.
  bool budget_known = false;
  // Ce que dit l'infobulle de la marque Discord, DÉJÀ TRADUIT ; vide sans marque.
  std::string_view relay_tip;
  // LES COMMANDES QUE LA LISTE AU-DESSUS DU CHAMP PROPOSE, déjà traduites et
  // déjà filtrées par le groupe du joueur. Vide : la liste ne s'ouvre jamais.
  std::span<const ChatCommandHint> commands;
  // CETTE LIGNE-CI PART AUSSI SUR DISCORD : la marque se peint.
  //
  // 🔴 C'EST LA DESTINATION RÉSOLUE QUI DÉCIDE, PAS LE MODE ARMÉ : un sigil
  // tapé l'emporte sur le mode, et une ligne de groupe ne part pas sur Discord
  // même quand « Alentour » est armé. `app` le calcule, comme `target`.
  bool relayed = false;
};

// L'historique des lignes tapées.
//
// ⚠ IL SURVIT À LA PARTIE, et c'est `app` qui l'emporte : `ui` ne lit ni
// n'écrit rien sur disque. Ce qu'on retrouve à la flèche ↑ au démarrage suivant
// est ce que ce vecteur portait à la fermeture — les `@commandes` comprises,
// qui sont ce qu'on retape le plus.
struct ChatInputHistory {
  std::vector<std::string> lines;
  // Combien on en garde. ⚠ RÉGLABLE, ET BORNÉ À LA POSE : c'est `app` qui pose
  // le réglage du joueur, et une valeur lue d'un fichier ne se croit pas.
  int kept = kChatHistoryKept;
  // ⚠ VAUT `lines.size()` QUAND ON EST SUR LE BROUILLON, et c'est ce qui permet
  // de RESSORTIR de l'historique par le bas pour retrouver la phrase commencée,
  // au lieu d'une ligne vide.
  int at = 0;
  std::string draft;
};

// Range une ligne dans l'historique de frappe. Deux règles d'usage : la même
// phrase deux fois de suite n'y entre qu'une, et l'envoi ramène le fil à son
// présent.
//
// ⚠ PUBLIQUE PARCE QU'UNE LIGNE PEUT PARTIR SANS QUE LE CHAMP L'AIT VUE : Entrée
// valide aussi depuis DEHORS, quand la barre n'a pas le clavier. Deux chemins
// d'envoi, un seul historique.
void RememberTypedLine(ChatInputHistory& history, std::string_view line);

// Ce que la rangée garde d'une image à l'autre.
struct ChatInputState {
  std::array<char, kChatInputSize> text = {};
  // Posé par le jeu quand le joueur appuie sur Entrée : la saisie prend le
  // clavier à l'image suivante.
  bool wants_focus = false;
  // ⚠ ET À LAQUELLE DES DEUX BOÎTES. La rangée en porte deux — le pseudo et la
  // phrase — et le joueur a laissé son curseur dans l'une d'elles. Un envoi,
  // un emoji pioché ou un lien posé lui rendent le clavier LÀ, et non d'office
  // dans la phrase : le client d'origine ne déplace pas le curseur de qui que
  // ce soit sous prétexte qu'un bouton a été cliqué.
  bool focus_on_whisper = false;
  // Le champ gagne-t-il des lignes ? Décoché, il en garde une, qui défile.
  bool grow = true;
  // ── LE BATTLE MODE ────────────────────────────────────────
  // Le réglage, posé à chaque image par la fenêtre, et l'état qu'il commande :
  // la barre est-elle dépliée ? Les règles vivent dans `chat_bar.h`.
  //
  // ⚠ `unfolded` NE VEUT RIEN DIRE HORS BATTLE MODE : la barre y est permanente.
  // Il ne se lit donc jamais seul — voir `ChatBarShown`.
  bool battle_mode = false;
  bool unfolded = false;
  // ── LES TOUCHES REÇUES HORS DU DESSIN ─────────────────────
  // 🔴 ELLES NE PEUVENT PAS ÊTRE TRAITÉES PAR LE CHAMP. Repliée, la rangée
  // n'est pas dessinée du tout — rien n'y consommerait la touche, et la barre
  // ne se serait jamais ouverte. Dépliée mais sans le clavier, le champ ne voit
  // rien non plus. La boucle dépose donc l'intention, et la fenêtre l'applique
  // en tête d'image.
  bool enter_pending = false;
  bool escape_pending = false;
  // CE QUE LE JOUEUR A TAPÉ PENDANT QUE LA BARRE N'AVAIT PAS ENCORE LE CLAVIER.
  //
  // 🔴 MIS DE CÔTÉ, PAS ÉCRIT DANS LE TAMPON. ImGui vide sa file de caractères
  // à la fin de chaque image : une lettre tapée à l'image où l'on demande le
  // focus serait perdue, puisque le champ ne s'active qu'à la suivante. Et
  // l'écrire directement dans `text` serait pire — le champ activé par le
  // chemin du clavier SÉLECTIONNE TOUT dès que son tampon a changé, et la
  // frappe suivante effacerait la lettre. On la remet donc dans la file
  // d'ImGui au moment où le champ s'active : elle s'insère alors au curseur,
  // comme n'importe quelle frappe.
  std::vector<ImWchar> typed;
  ChatInputHistory history;
  // La liste des commandes au-dessus du champ : voir `chat_command_hints.h`.
  ChatCommandListState commands;
  // ── Mesures d'image ───────────────────────────────────────────────────────
  // Combien de rangées le texte DEMANDE, et combien la fenêtre peut en céder.
  // L'écart est ce que la fenêtre devra gagner, ou rendre.
  int rows_wanted = 1;
  int rows_allowed = 1;
  // L'image où `ChatInputHeight` a mesuré. ⚠ LA RÉSERVATION ET LE DESSIN
  // DOIVENT PARLER DU MÊME NOMBRE DE LIGNES : mesurer deux fois dans la même
  // image, avec un texte qui a pu changer entre-temps, ferait déborder la rangée
  // ou laisserait un trou sous elle.
  int measured_frame = -1;
  int rows_drawn = 1;
  // Où se trouve le curseur, relevé à l'image PRÉCÉDENTE. C'est ce qui décide
  // si une flèche rappelle l'historique ou déplace le curseur : le rappel
  // d'ImGui tourne APRÈS le traitement des touches, donc sa valeur de cette
  // image-ci arrive trop tard pour trancher.
  int cursor_row = 0;
  int cursor_rows = 1;
  // ── LA BOX « PSEUDO » ────────────────────────────────────
  // Le destinataire armé. Rempli, tout ce qu'on tape sans sigil part en
  // chuchotement — c'est ainsi que le client chuchote sans taper `/w`.
  //
  // ⚠ IL NE SE VIDE PAS À L'ENVOI, et c'est le comportement du client : on
  // chuchote rarement une seule phrase. C'est au joueur de l'effacer, et la
  // couleur de l'étiquette le lui rappelle à chaque ligne.
  std::array<char, kChatWhisperNameSize> whisper_to = {};
  // La liste des derniers correspondants sous la box : voir
  // `chat_whisper_list.h`.
  ChatWhisperListState whisper_list;

  // DU TEXTE À POSER EN FIN DE SAISIE, dès la prochaine image.
  //
  // 🔴 EN ATTENTE, ET NON ÉCRIT DIRECTEMENT. Le sélecteur d'emoji vit
  // dans la BANDE D'ONGLETS, dessinée avant la rangée de saisie : à ce moment le
  // champ n'existe pas encore pour cette image, et son identifiant non plus. On
  // dépose donc, et la rangée ramasse — c'est elle qui sait prévenir le widget.
  //
  // ⚠ ON Y AJOUTE, ON NE L'ÉCRASE JAMAIS : voir `AppendPendingInsert`.
  std::string pending_insert;
  // Le champ de la phrase avait le clavier à l'image précédente : c'est la
  // dernière saisie active, celle où un lien posé doit atterrir.
  bool was_active = false;
};

// AJOUTE `piece` AU BOUT DU DÉPÔT, après ce qui y attend déjà.
//
// 🔴 PLUSIEURS ÉCRIVAINS DANS LA MÊME IMAGE, et le premier compte : une ligne
// retenue pour sa longueur revient par le dépôt, puis un emoji pioché ou un
// lien posé s'y ajoute. Écraser le dépôt perdrait la ligne retenue.
//
// Faux, et rien ne change, quand la saisie ne pourrait plus recevoir le tout :
// le dépôt se pose en bloc ou pas du tout, et un ajout de trop emporterait ce
// qui attendait avant lui.
[[nodiscard]] bool AppendPendingInsert(ChatInputState& state, std::string_view piece);

// CE QU'UNE PALETTE POSE DANS UNE SAISIE — un emoji, ou le nom `:nom:` d'une
// émoticone —, déposé au bout.
//
// ⚠ UN NOM `:nom:` EST UN MOT : il prend une espace devant, sauf quand la
// saisie est vide ou en finit déjà par une. Un emoji se colle tel quel. Faux,
// et rien ne change, quand il ne tiendrait plus — voir `AppendPendingInsert`.
[[nodiscard]] bool AppendPickedToChatInput(ChatInputState& state, std::string_view piece);

// LA LARGEUR DE LA BOÎTE DE DESTINATION : celle du PLUS LONG libellé de mode,
// flèche et marges comprises.
//
// 🔴 ELLE NE SUIT PAS CE QUE LA BOÎTE MONTRE. Taillée sur son texte, elle
// change de largeur à chaque mode armé — et la boîte montre AUSSI le pseudo
// qu'on chuchote, donc elle grandissait à chaque lettre tapée dans la box
// « Pseudo », qui se dérobait sous le curseur en même temps que le champ. La
// place du plus long est la seule qui ne bouge jamais, et elle vaut dans
// toutes les langues : un libellé traduit est plus long que le français aussi
// souvent qu'il est plus court.
[[nodiscard]] float ChatTargetBoxWidth(const ChatLinePreview& preview, const SkinArt& art);

// La hauteur que la rangée prendra CETTE image, plafonnée par `most`.
//
// À appeler AVANT de dimensionner ce qui la précède : depuis que le champ
// grandit avec la phrase, la rangée n'a plus de hauteur fixe.
[[nodiscard]] float ChatInputHeight(ChatInputState& state, const ChatLinePreview& preview,
                                    float most, const SkinArt& art);

// Dessine la rangée, tout en bas du conteneur courant. Rend la ligne validée —
// déjà vidée de la saisie — ou rien.
//
// ⚠ LA BOÎTE DE DESTINATION PORTE LE BOUTON FLÈCHE DU CLIENT — `txtbox_btn_*`,
// les pièces mêmes dont sa liste déroulante est faite. Le reste de la rangée
// n'est que des champs clairs sur un cadre sombre, comme dans le client.
// CE QUE LA RANGÉE DEMANDE. ⚠ REND DES INTENTIONS, N'APPLIQUE RIEN : `ui` ne
// sait pas ce qu'un mode veut dire, et ne possède pas la liste des récents.
// LES MODIFICATEURS TENUS QUAND LA LIGNE A ÉTÉ VALIDÉE. ⚠ DES TOUCHES, PAS UNE
// DESTINATION : ce qu'elles veulent dire se décide chez l'appelant.
struct ChatSubmitKeys {
  bool ctrl = false;
  bool alt = false;
};

struct ChatInputAsk {
  // Le rang du mode qu'on vient d'armer, ou moins de zéro.
  int chose_mode = -1;
  // Les touches tenues quand la ligne rendue a été validée. Alt+Entrée valide
  // aussi : ImGui ne le fait pas de lui-même.
  ChatSubmitKeys keys;
  // Le joueur a demandé à oublier les destinataires récents.
  bool forget_whispers = false;
  // ÉCHAP VIENT DE QUITTER L'UNE DES DEUX BOÎTES : en battle mode, c'est la
  // sortie. ⚠ LA TOUCHE EST ALORS CONSOMMÉE par l'appelant — sans quoi elle
  // refermerait aussi la fenêtre derrière.
  bool folded = false;
  // Un geste sur une PASTILLE de lien, avant même que la ligne soit partie.
  ChatLinkAsk link;
};

// UN LIEN DÉJÀ POSÉ DANS LA SAISIE, tel que la rangée doit le peindre.
//
// 🔴 LE NATIF NE MET PAS DU TEXTE DANS SA LIGNE DE SAISIE : il y accroche un
// vrai bouton par lien (`UIItemTagButton`), coloré et cliquable AVANT l'envoi. Un
// `InputText` d'ImGui n'a pas de texte riche ; la seule façon d'y colorer un
// fragment est de le RECOUVRIR du fond du champ puis de le réécrire dans la
// couleur des liens. D'où une pastille peinte par-dessus, après le widget.
struct ChatInputLink {
  // Le libellé, tel qu'il figure DANS le texte du champ — c'est par lui qu'on le
  // retrouve, et deux exemplaires du même objet en portent deux.
  std::string_view label;
  ChatLinkKind kind = ChatLinkKind::kItem;
  std::uint32_t item_id = 0;
  // Le jeton du lien, rendu tel quel dans la demande. Voir `ChatLinkSpan`.
  std::uint32_t token = 0;
  // `kMvp` : ce que son menu sait d'avance, nul pour tout le reste. Voir
  // `ChatLinkSpan::mvp`.
  const MvpLinkMenuFacts* mvp = nullptr;
  // `kNaviSearch` : ce que son menu sait d'avance. Voir `ChatLinkSpan::navi`.
  NaviSearchMenuFacts navi;
};

// `links` sont les liens posés, DANS L'ORDRE DE POSE : le balayage les consomme
// de gauche à droite, un à la fois, exactement comme la substitution à l'envoi.
[[nodiscard]] std::optional<std::string> DrawChatInput(ChatInputState& state,
                                                       const ChatLinePreview& preview,
                                                       ChatInputAsk* asked, const SkinArt& art,
                                                       std::span<const ChatInputLink> links = {},
                                                       ItemLinkFacts link_facts = {});

// CE QUE LA BARRE FAIT DES TOUCHES QU'ELLE A REÇUES HORS DU DESSIN.
//
// ⚠ EN TÊTE D'IMAGE, AVANT TOUT DESSIN : la décision change la HAUTEUR de la
// fenêtre — une barre qui se déplie prend sa place — et il est trop tard une
// fois le cadre ouvert. C'est aussi le seul moment où l'on sait qu'une AUTRE
// zone de texte écrit déjà : sans ce test, chaque Entrée lui volerait le
// clavier.
struct ChatBarAnswer {
  // La ligne qu'Entrée a validée DEPUIS DEHORS, déjà vidée de la saisie et
  // rangée dans l'historique de frappe.
  std::optional<std::string> said;
  // Échap vient de replier la barre. ⚠ L'APPELANT DOIT ALORS AVALER LA TOUCHE :
  // une seule frappe ne ferme qu'une chose.
  bool folded = false;
};
[[nodiscard]] ChatBarAnswer PumpChatBarKeys(ChatInputState& state);

}  // namespace ro::ui
